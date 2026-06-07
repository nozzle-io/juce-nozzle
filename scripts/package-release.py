#!/usr/bin/env python3
import argparse
import shutil
import sys
import zipfile
from pathlib import Path

PLUGIN_NAME = "Nozzle Receiver"


def copy_tree(source: Path, target: Path) -> None:
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target, symlinks=True)


def find_one(root: Path, pattern: str) -> Path:
    matches = sorted(root.rglob(pattern))
    if not matches:
        raise SystemExit(f"missing artifact matching {pattern} under {root}")
    return matches[0]


def find_standalone(root: Path, platform: str) -> Path:
    if platform == "macos-universal":
        return find_one(root, f"{PLUGIN_NAME}.app")
    if platform == "windows-x64":
        return find_one(root, f"{PLUGIN_NAME}.exe")
    matches = [path for path in root.rglob(PLUGIN_NAME) if path.is_file()]
    if not matches:
        raise SystemExit(f"missing standalone executable {PLUGIN_NAME} under {root}")
    return sorted(matches)[0]


def copy_artifact(source: Path, target_dir: Path) -> None:
    target = target_dir / source.name
    if source.is_dir():
        copy_tree(source, target)
    else:
        shutil.copy2(source, target)


def zip_directory(source_dir: Path, zip_path: Path) -> None:
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(source_dir.rglob("*")):
            archive.write(path, path.relative_to(source_dir.parent))


def main() -> int:
    parser = argparse.ArgumentParser(description="Stage and zip juce-nozzle release artifacts.")
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--platform", required=True, choices=["macos-universal", "windows-x64", "linux-x64"])
    parser.add_argument("--package-name", required=True)
    parser.add_argument("--output-dir", default="dist")
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    output_dir = Path(args.output_dir)
    package_root = output_dir / args.package_name
    plugin_dir = package_root / "plugins"
    standalone_dir = package_root / "standalone"

    if package_root.exists():
        shutil.rmtree(package_root)
    plugin_dir.mkdir(parents=True)
    standalone_dir.mkdir(parents=True)

    copy_artifact(find_one(build_dir, f"{PLUGIN_NAME}.vst3"), plugin_dir)
    if args.platform == "macos-universal":
        copy_artifact(find_one(build_dir, f"{PLUGIN_NAME}.component"), plugin_dir)
    copy_artifact(find_standalone(build_dir, args.platform), standalone_dir)

    for name in ["README.md", "LICENSE", "THIRD-PARTY-NOTICES.md"]:
        shutil.copy2(name, package_root / name)
    copy_tree(Path("docs"), package_root / "docs")

    zip_path = output_dir / f"{args.package_name}.zip"
    zip_directory(package_root, zip_path)
    print(zip_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
