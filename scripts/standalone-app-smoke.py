#!/usr/bin/env python3
import argparse
import subprocess
import sys
import time
from pathlib import Path

SIZES = [(320, 240), (641, 479)]


def find_executable(build_dir: Path, executable_name: str) -> Path:
    candidates = [path for path in build_dir.rglob(executable_name) if path.is_file()]
    if sys.platform == "win32" and not candidates:
        candidates = [path for path in build_dir.rglob(f"{executable_name}.exe") if path.is_file()]
    if not candidates:
        raise SystemExit(f"missing executable: {executable_name} under {build_dir}")
    candidates.sort(key=lambda path: (len(path.parts), str(path)))
    return candidates[0]


def run_size(sender_executable: Path, receiver_executable: Path, width: int, height: int) -> None:
    source = f"juce_nozzle_app_smoke_{width}x{height}_{int(time.time() * 1000)}"
    sender_args = [
        str(sender_executable),
        "--smoke-sender",
        "--source", source,
        "--width", str(width),
        "--height", str(height),
        "--frames", "90",
        "--interval-ms", "16",
    ]
    receiver_args = [
        str(receiver_executable),
        "--smoke-receiver",
        "--source", source,
        "--width", str(width),
        "--height", str(height),
        "--timeout-ms", "15000",
    ]

    sender = subprocess.Popen(sender_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        time.sleep(0.25)
        receiver = subprocess.run(receiver_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=20)
        try:
            sender_stdout, sender_stderr = sender.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            sender.terminate()
            sender_stdout, sender_stderr = sender.communicate(timeout=5)
            raise SystemExit(f"sender timed out for {width}x{height}\nstdout:\n{sender_stdout}\nstderr:\n{sender_stderr}")
    finally:
        if sender.poll() is None:
            sender.terminate()
            sender.communicate(timeout=5)

    print(sender_stdout, end="")
    print(sender_stderr, end="", file=sys.stderr)
    print(receiver.stdout, end="")
    print(receiver.stderr, end="", file=sys.stderr)

    if sender.returncode != 0:
        raise SystemExit(f"sender exited {sender.returncode} for {width}x{height}")
    if receiver.returncode != 0:
        raise SystemExit(f"receiver exited {receiver.returncode} for {width}x{height}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run separate-process standalone sender/receiver smoke.")
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    sender = find_executable(build_dir, "Nozzle Sender Standalone")
    receiver = find_executable(build_dir, "Nozzle Receiver Standalone")
    print(f"sender_executable={sender}")
    print(f"receiver_executable={receiver}")

    for width, height in SIZES:
        run_size(sender, receiver, width, height)

    print("standalone app smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
