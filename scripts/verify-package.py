#!/usr/bin/env python3
import argparse
import plistlib
import zipfile
from typing import Optional

MAC_EXECUTABLE_SUFFIXES = [
    "plugins/Nozzle Receiver.vst3/Contents/MacOS/Nozzle Receiver",
    "plugins/Nozzle Receiver.component/Contents/MacOS/Nozzle Receiver",
    "standalone/Nozzle Receiver.app/Contents/MacOS/Nozzle Receiver",
    "standalone/Nozzle Sender Standalone.app/Contents/MacOS/Nozzle Sender Standalone",
    "standalone/Nozzle Receiver Standalone.app/Contents/MacOS/Nozzle Receiver Standalone",
]

MAC_PLIST_EXPECTATIONS = {
    "plugins/Nozzle Receiver.vst3/Contents/Info.plist": "Nozzle Receiver",
    "plugins/Nozzle Receiver.component/Contents/Info.plist": "Nozzle Receiver",
    "standalone/Nozzle Receiver.app/Contents/Info.plist": "Nozzle Receiver",
    "standalone/Nozzle Sender Standalone.app/Contents/Info.plist": "Nozzle Sender Standalone",
    "standalone/Nozzle Receiver Standalone.app/Contents/Info.plist": "Nozzle Receiver Standalone",
}

WINDOWS_FILE_SUFFIXES = [
    "plugins/Nozzle Receiver.vst3/Contents/x86_64-win/Nozzle Receiver.vst3",
    "standalone/Nozzle Receiver.exe",
    "standalone/Nozzle Sender Standalone.exe",
    "standalone/Nozzle Receiver Standalone.exe",
]

COMMON_SUFFIXES = [
    "plugins/Nozzle Receiver.vst3/",
    "standalone/",
    "README.md",
    "LICENSE",
    "THIRD-PARTY-NOTICES.md",
    "docs/plugin-host-boundary.md",
    "docs/standalone-smoke.md",
    "docs/juce-license-boundary.md",
]


def find_suffix(names: list[str], suffix: str) -> Optional[str]:
    for name in names:
        if name.endswith(suffix):
            return name
    return None


def require_suffix(names: list[str], suffix: str) -> str:
    match = find_suffix(names, suffix)
    if match is None:
        raise SystemExit(f"missing package entry: {suffix}")
    return match


def require_executable_mode(archive: zipfile.ZipFile, names: list[str], suffix: str) -> None:
    name = require_suffix(names, suffix)
    info = archive.getinfo(name)
    mode = (info.external_attr >> 16) & 0o777
    if mode == 0:
        raise SystemExit(f"missing unix mode metadata for executable: {suffix}")
    if (mode & 0o111) == 0:
        raise SystemExit(f"package entry is not executable: {suffix} mode={oct(mode)}")


def require_plist(archive: zipfile.ZipFile, names: list[str], suffix: str, executable: str) -> None:
    name = require_suffix(names, suffix)
    with archive.open(name) as file:
        data = plistlib.load(file)
    actual_executable = data.get("CFBundleExecutable")
    if actual_executable != executable:
        raise SystemExit(f"{suffix} CFBundleExecutable={actual_executable!r}, expected {executable!r}")
    if not data.get("CFBundleIdentifier"):
        raise SystemExit(f"{suffix} has empty CFBundleIdentifier")


def verify_common(names: list[str]) -> None:
    for suffix in COMMON_SUFFIXES:
        require_suffix(names, suffix)


def verify_macos(archive: zipfile.ZipFile, names: list[str]) -> None:
    for suffix in MAC_EXECUTABLE_SUFFIXES:
        require_executable_mode(archive, names, suffix)
    for suffix, executable in MAC_PLIST_EXPECTATIONS.items():
        require_plist(archive, names, suffix, executable)


def verify_windows(names: list[str]) -> None:
    for suffix in WINDOWS_FILE_SUFFIXES:
        require_suffix(names, suffix)


def verify_linux(names: list[str]) -> None:
    require_suffix(names, "plugins/Nozzle Receiver.vst3/")
    require_suffix(names, "standalone/Nozzle Receiver")


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify juce-nozzle package contents.")
    parser.add_argument("--platform", required=True)
    parser.add_argument("zip_path")
    args = parser.parse_args()

    with zipfile.ZipFile(args.zip_path) as archive:
        names = archive.namelist()
        verify_common(names)
        if args.platform == "macos-universal":
            verify_macos(archive, names)
        elif args.platform == "windows-x64":
            verify_windows(names)
        elif args.platform == "linux-x64":
            verify_linux(names)
        else:
            raise SystemExit(f"unsupported platform: {args.platform}")

    print(f"verify-package: PASS {args.zip_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
