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
        if dir in ["java", "bin", "lib", "share"] or "zip" in dir:
            continue
        os.chdir(install_dir / dir)
        with open("group_info.txt") as f:
            group = f.read()
        files = glob.glob("**", recursive=True)

        # Identify how these libraries were built
        debug_or_not = ""
        static_or_not = ""
        platform = ""
        arch = ""
        for file in files:
            if "include" in file or "sources" in file:
                continue
            if "debug" in file:
                debug_or_not = "debug"
            if "static" in file:
                static_or_not = "static"
                # We need a complete platform path to calculate OS and arch
            if len(platform_parts := file.replace("\\", "/").split("/")) >= 3:
                platform = platform_parts[0]
                arch = platform_parts[1]
        artifact_name = (
            f"_GROUP_{group}_ID_{dir}_CLS_{platform}{arch}{static_or_not}{debug_or_not}"
        )

        with zipfile.ZipFile(install_dir / f"{artifact_name}.zip", "w") as archive:
            for file in files:
                if file == "group_info.txt" or "include" in file or "sources" in file:
                    continue
                archive.write(file)
            archive.write(repo_root / "ThirdPartyNotices.txt", "ThirdPartyNotices.txt")
            archive.write(repo_root / "LICENSE.md", "LICENSE.md")

        if os.path.exists(install_dir / dir / "include"):
            create_archive(repo_root, install_dir, "include", dir, "headers", group)
        if os.path.exists(install_dir / dir / "sources"):
            create_archive(repo_root, install_dir, "sources", dir, "sources", group)


def create_archive(
    repo_root: Path,
    install_dir: Path,
    directory: str,
    artifact_id: str,
    classifier: str,
    group: str,
):
    os.chdir(install_dir / artifact_id / directory)
    files = glob.glob("**", recursive=True)
    artifact_name = f"_GROUP_{group}_ID_{artifact_id}_CLS_{classifier}"
    with zipfile.ZipFile(install_dir / f"{artifact_name}.zip", "w") as archive:
        for file in files:
            archive.write(file)
        archive.write(repo_root / "ThirdPartyNotices.txt", "ThirdPartyNotices.txt")
        archive.write(repo_root / "LICENSE.md", "LICENSE.md")


if __name__ == "__main__":
    main(sys.argv[1:])
