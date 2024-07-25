import argparse
import glob
import os
import subprocess
import sys
import zipfile
from pathlib import Path


def main(argv):
    repo_root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--install_directory",
        help="Required.",
        required=True,
        type=Path,
    )
    subparsers = parser.add_subparsers(dest="subcommand", required=True)
    subparsers.add_parser(
        "package", help="Packages all the files in the install directory."
    )
    sign_parser = subparsers.add_parser("sign", help="Signs macOS apps.")
    sign_parser.add_argument(
        "--developer_id",
        help="The Apple Developer ID used to sign macOS apps.",
        required=True,
        type=str,
    )

    args = parser.parse_args(argv)
    install_dir: Path = args.install_directory
    if args.subcommand == "package":
        package_artifacts(install_dir, repo_root)
    elif args.subcommand == "sign":
        sign_apps(args.developer_id)


def sign_apps(developer_id: str):
    apps = glob.glob("**/*.app", recursive=True) + glob.glob(
        "**/*.dylib", recursive=True
    )
    for app in apps:
        subprocess.run(
            [
                "codesign",
                "--force",
                "--strict",
                "--timestamp",
                "--options=runtime",
                "--verbose",
                "-s",
                developer_id,
                app,
            ]
        )


def package_artifacts(install_dir: Path, repo_root: Path):
    out = subprocess.run(["git", "describe"], capture_output=True, text=True)
    print("git:")
    print(out.stdout)
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
            archive.write(repo_root / "ThirdPartyNotices.txt", "ThirdPartyNotices.txt")
            archive.write(repo_root / "LICENSE.md", "LICENSE.md")

    create_archive(repo_root, install_dir, version, "include", "headers")
    create_archive(repo_root, install_dir, version, "sources", "sources")


def create_archive(
    repo_root: Path, install_dir: Path, version: str, directory: str, classifier: str
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
            archive.write(repo_root / "ThirdPartyNotices.txt", "ThirdPartyNotices.txt")
            archive.write(repo_root / "LICENSE.md", "LICENSE.md")


if __name__ == "__main__":
    main(sys.argv[1:])
