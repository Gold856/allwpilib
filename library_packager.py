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
    out = subprocess.run(["git", "describe"], capture_output=True, text=True)
    version = out.stdout[1:].strip()
    directories = glob.glob("*", root_dir=args.install_directory)
    for dir in directories:
        if dir in ["include", "java", "jni", "share"]:
            continue
        os.chdir(args.install_directory / dir)
        files = glob.glob("**", recursive=True)
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
        with zipfile.ZipFile(
            args.install_directory / f"{artifact_name}.zip",
            "w",
        ) as archive:
            for file in files:
                archive.write(file)
            archive.write(dirname / "ThirdPartyNotices.txt", "ThirdPartyNotices.txt")
            archive.write(dirname / "LICENSE.md", "LICENSE.md")


if __name__ == "__main__":
    main(sys.argv[1:])
