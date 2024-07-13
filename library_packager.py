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
        if dir in ["include", "java", "share", "sources"]:
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
            if len(platform_parts := file.replace("\\", "/").split("/")) >= 3:
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

    create_archive(dirname, install_dir, version, "include", "headers")
    create_archive(dirname, install_dir, version, "sources", "sources")


def create_archive(
    dirname: Path, install_dir: Path, version: str, directory: str, classifier: str
):
    os.chdir(install_dir / directory)
    directories = glob.glob("*", root_dir=install_dir / directory)
    for dir in directories:
        library_dir = install_dir / directory / dir
        os.chdir(library_dir)

        files = glob.glob("**", recursive=True)
        artifact_name = f"{dir}-cpp-{version}-{classifier}"
        with zipfile.ZipFile(install_dir / f"{artifact_name}.zip", "w") as archive:
            for file in files:
                archive.write(file)
            archive.write(dirname / "ThirdPartyNotices.txt", "ThirdPartyNotices.txt")
            archive.write(dirname / "LICENSE.md", "LICENSE.md")


if __name__ == "__main__":
    main(sys.argv[1:])
