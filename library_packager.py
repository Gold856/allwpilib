import argparse
import glob
import os
import subprocess
import sys
import zipfile
from pathlib import Path


def main(argv):
    dirname = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--install_directory",
        help="Required.",
        type=Path,
    )
    args = parser.parse_args(argv)
    install_dir: Path = args.install_directory
    out = subprocess.run(["git", "describe"], capture_output=True, text=True)
    version = out.stdout[1:].strip()

    directories = glob.glob("*", root_dir=install_dir)
    for dir in directories:
        # TODO: Remove
        if dir in ["include", "java", "jni", "share"]:
            continue
        os.chdir(install_dir / dir)
        files = glob.glob("**", recursive=True)

        # Identify how these libraries were built
        debug_or_not = ""
        static_or_not = ""
        platform = ""
        arch = ""
        for file in files:
            if "debug" in file:
                debug_or_not = "debug"
            if "static" in file:
                static_or_not = "static"
            # We need a complete platform path to calculate OS and arch
            if len(platform_parts := file.split("\\")) >= 3:
                platform = platform_parts[0]
                arch = platform_parts[1]
        artifact_name = (
            f"{dir}-cpp-{version}-{platform}{arch}{static_or_not}{debug_or_not}"
        )

        with zipfile.ZipFile(install_dir / f"{artifact_name}.zip", "w") as archive:
            for file in files:
                archive.write(file)
            archive.write(dirname / "ThirdPartyNotices.txt", "ThirdPartyNotices.txt")
            archive.write(dirname / "LICENSE.md", "LICENSE.md")

    os.chdir(install_dir / "include")
    header_directories = glob.glob("*", root_dir=install_dir / "include")
    for header_dir in header_directories:
        library_header_dir = install_dir / "include" / header_dir
        os.chdir(library_header_dir)

        headers = glob.glob("**", recursive=True, root_dir=library_header_dir)
        artifact_name = f"{header_dir}-cpp-{version}-headers"
        with zipfile.ZipFile(install_dir / f"{artifact_name}.zip", "w") as archive:
            for header in headers:
                archive.write(header)


if __name__ == "__main__":
    main(sys.argv[1:])
