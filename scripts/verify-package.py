#!/usr/bin/env python3
import argparse
import zipfile


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify juce-nozzle package contents.")
    parser.add_argument("--platform", required=True)
    parser.add_argument("zip_path")
    args = parser.parse_args()

    with zipfile.ZipFile(args.zip_path) as archive:
        names = archive.namelist()

    required_suffixes = [
        "plugins/Nozzle Receiver.vst3/",
        "standalone/",
        "README.md",
        "LICENSE",
        "THIRD-PARTY-NOTICES.md",
    ]
    if args.platform == "macos-universal":
        required_suffixes += [
            "plugins/Nozzle Receiver.vst3/Contents/MacOS/Nozzle Receiver",
            "plugins/Nozzle Receiver.component/Contents/MacOS/Nozzle Receiver",
            "standalone/Nozzle Receiver.app/Contents/MacOS/Nozzle Receiver",
        ]
    elif args.platform == "windows-x64":
        required_suffixes += [
            "plugins/Nozzle Receiver.vst3/",
            "standalone/Nozzle Receiver.exe",
        ]
    elif args.platform == "linux-x64":
        required_suffixes += [
            "plugins/Nozzle Receiver.vst3/",
            "standalone/Nozzle Receiver",
        ]
    else:
        raise SystemExit(f"unsupported platform: {args.platform}")

    missing = [suffix for suffix in required_suffixes if not any(name.endswith(suffix) for name in names)]
    if missing:
        raise SystemExit("missing package entries: " + ", ".join(missing))
    print(f"verify-package: PASS {args.zip_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
