#!/usr/bin/env python3
import argparse
import json
import platform
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

SIZES = [(320, 240), (641, 479)]


def executable_candidates(build_dir: Path, executable_name: str) -> list[Path]:
    candidates = [path for path in build_dir.rglob(executable_name) if path.is_file()]
    if sys.platform == "win32" and not candidates:
        candidates = [path for path in build_dir.rglob(f"{executable_name}.exe") if path.is_file()]
    if sys.platform == "darwin" and not candidates:
        candidates = [path for path in build_dir.rglob(f"{executable_name}.app/Contents/MacOS/{executable_name}") if path.is_file()]
    candidates.sort(key=lambda path: (len(path.parts), str(path)))
    return candidates


def find_executable(build_dir: Path, executable_name: str) -> Path:
    candidates = executable_candidates(build_dir, executable_name)
    if not candidates:
        raise SystemExit(f"missing executable: {executable_name} under {build_dir}")
    return candidates[0]


def git_sha(repo_dir: Path) -> str:
    try:
        return subprocess.check_output(["git", "-C", str(repo_dir), "rev-parse", "HEAD"], text=True).strip()
    except subprocess.CalledProcessError:
        return "unknown"


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def run_process(args: list[str], timeout: int, stdout_path: Path, stderr_path: Path) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
    write_text(stdout_path, completed.stdout)
    write_text(stderr_path, completed.stderr)
    return completed


def wait_sender(sender: subprocess.Popen[str], stdout_path: Path, stderr_path: Path, label: str) -> None:
    try:
        stdout, stderr = sender.communicate(timeout=60)
    except subprocess.TimeoutExpired:
        sender.terminate()
        stdout, stderr = sender.communicate(timeout=5)
        write_text(stdout_path, stdout)
        write_text(stderr_path, stderr)
        raise SystemExit(f"{label} sender timed out\nstdout:\n{stdout}\nstderr:\n{stderr}")
    write_text(stdout_path, stdout)
    write_text(stderr_path, stderr)
    if sender.returncode != 0:
        raise SystemExit(f"{label} sender exited {sender.returncode}\nstdout:\n{stdout}\nstderr:\n{stderr}")


def load_evidence(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def require_pass(evidence_path: Path, label: str) -> None:
    evidence = load_evidence(evidence_path)
    verdict = evidence.get("verdict")
    checks = evidence.get("checks", {})
    required_checks = [
        "dimensions",
        "top_left_red",
        "top_right_green",
        "bottom_left_blue",
        "bottom_right_white",
        "orientation",
        "channel_order",
    ]
    failures = [name for name in required_checks if checks.get(name) != "PASS"]
    if verdict != "PASS" or failures:
        raise SystemExit(f"{label} evidence failed: verdict={verdict} failed_checks={failures} path={evidence_path}")


def require_existing_executable(path: Path, label: str) -> None:
    if not path.is_file():
        raise SystemExit(f"missing {label} executable: {path}")


def run_juce_pair(sender_executable: Path, receiver_executable: Path, width: int, height: int, evidence_dir: Path) -> None:
    source = f"juce_nozzle_app_smoke_{width}x{height}_{int(time.time() * 1000)}"
    label = f"juce_sender_to_juce_receiver_{width}x{height}"
    evidence_path = evidence_dir / f"{label}.json"
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
        "--evidence", str(evidence_path),
    ]

    sender = subprocess.Popen(sender_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        time.sleep(0.25)
        receiver = run_process(receiver_args, 20, evidence_dir / f"{label}.receiver.stdout.log", evidence_dir / f"{label}.receiver.stderr.log")
        wait_sender(sender, evidence_dir / f"{label}.sender.stdout.log", evidence_dir / f"{label}.sender.stderr.log", label)
    finally:
        if sender.poll() is None:
            sender.terminate()
            sender.communicate(timeout=5)

    print(receiver.stdout, end="")
    print(receiver.stderr, end="", file=sys.stderr)

    if receiver.returncode != 0:
        raise SystemExit(f"{label} receiver exited {receiver.returncode}")
    require_pass(evidence_path, label)


def run_sender_to_viewer(sender_executable: Path, viewer_executable: Path, width: int, height: int, evidence_dir: Path) -> None:
    source = f"juce_nozzle_viewer_smoke_{width}x{height}_{int(time.time() * 1000)}"
    label = f"juce_sender_to_nozzle_viewer_{width}x{height}"
    evidence_path = evidence_dir / f"{label}.json"
    sender_args = [
        str(sender_executable),
        "--smoke-sender",
        "--source", source,
        "--width", str(width),
        "--height", str(height),
        "--frames", "120",
        "--interval-ms", "16",
    ]
    viewer_args = [
        str(viewer_executable),
        "--smoke-receiver",
        "--source", source,
        "--width", str(width),
        "--height", str(height),
        "--timeout-ms", "15000",
        "--evidence", str(evidence_path),
    ]
    sender = subprocess.Popen(sender_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        time.sleep(0.25)
        viewer = run_process(viewer_args, 20, evidence_dir / f"{label}.receiver.stdout.log", evidence_dir / f"{label}.receiver.stderr.log")
        wait_sender(sender, evidence_dir / f"{label}.sender.stdout.log", evidence_dir / f"{label}.sender.stderr.log", label)
    finally:
        if sender.poll() is None:
            sender.terminate()
            sender.communicate(timeout=5)
    print(viewer.stdout, end="")
    print(viewer.stderr, end="", file=sys.stderr)
    if viewer.returncode != 0:
        raise SystemExit(f"{label} receiver exited {viewer.returncode}")
    require_pass(evidence_path, label)


def run_tester_to_receiver(tester_executable: Path, receiver_executable: Path, width: int, height: int, evidence_dir: Path) -> None:
    source = f"nozzle_tester_to_juce_smoke_{width}x{height}_{int(time.time() * 1000)}"
    label = f"nozzle_tester_sender_to_juce_receiver_{width}x{height}"
    sender_evidence_path = evidence_dir / f"{label}.sender.json"
    receiver_evidence_path = evidence_dir / f"{label}.receiver.json"
    sender_args = [
        str(tester_executable),
        "sender",
        "--name", source,
        "--width", str(width),
        "--height", str(height),
        "--format", "rgba8_unorm",
        "--frames", "120",
        "--delay-ms", "16",
        "--hold-ms", "500",
        "--sender-pattern", "juce-quadrants",
        "--evidence", str(sender_evidence_path),
    ]
    receiver_args = [
        str(receiver_executable),
        "--smoke-receiver",
        "--source", source,
        "--width", str(width),
        "--height", str(height),
        "--timeout-ms", "15000",
        "--evidence", str(receiver_evidence_path),
    ]
    sender = subprocess.Popen(sender_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        time.sleep(0.25)
        receiver = run_process(receiver_args, 20, evidence_dir / f"{label}.receiver.stdout.log", evidence_dir / f"{label}.receiver.stderr.log")
        wait_sender(sender, evidence_dir / f"{label}.sender.stdout.log", evidence_dir / f"{label}.sender.stderr.log", label)
    finally:
        if sender.poll() is None:
            sender.terminate()
            sender.communicate(timeout=5)
    print(receiver.stdout, end="")
    print(receiver.stderr, end="", file=sys.stderr)
    if receiver.returncode != 0:
        raise SystemExit(f"{label} receiver exited {receiver.returncode}")
    require_pass(receiver_evidence_path, label)


def write_summary(evidence_dir: Path, repo_root: Path, juce_nozzle_dir: Path, viewer_dir: Optional[Path], tester_dir: Optional[Path]) -> None:
    summary = {
        "schema_version": "0.1.0",
        "tool": "juce-nozzle standalone-app-smoke",
        "os": platform.platform(),
        "python": sys.version,
        "repo_shas": {
            "juce-nozzle": git_sha(juce_nozzle_dir),
            "nozzle": git_sha(juce_nozzle_dir / "nozzle"),
        },
        "evidence_files": sorted(path.name for path in evidence_dir.glob("*.json") if path.name != "summary.json"),
        "log_files": sorted(path.name for path in evidence_dir.glob("*.log")),
    }
    if viewer_dir is not None:
        summary["repo_shas"]["nozzle-viewer"] = git_sha(viewer_dir)
    if tester_dir is not None:
        summary["repo_shas"]["nozzle-tester"] = git_sha(tester_dir)
    write_text(evidence_dir / "summary.json", json.dumps(summary, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run separate-process standalone sender/receiver smoke.")
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--evidence-dir", default=None)
    parser.add_argument("--viewer-executable", default=None)
    parser.add_argument("--nozzle-tester-cli", default=None)
    parser.add_argument("--viewer-repo-dir", default=None)
    parser.add_argument("--tester-repo-dir", default=None)
    parser.add_argument("--skip-juce-pair", action="store_true")
    parser.add_argument("--require-external", action="store_true", help="Fail unless both nozzle-viewer and nozzle-tester executables are provided.")
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    repo_root = Path(__file__).resolve().parents[1]
    evidence_dir = Path(args.evidence_dir) if args.evidence_dir else build_dir / "standalone-app-smoke-evidence"
    evidence_dir.mkdir(parents=True, exist_ok=True)
    sender = find_executable(build_dir, "Nozzle Sender Standalone")
    receiver = find_executable(build_dir, "Nozzle Receiver Standalone")
    print(f"sender_executable={sender}")
    print(f"receiver_executable={receiver}")

    viewer = Path(args.viewer_executable) if args.viewer_executable else None
    tester = Path(args.nozzle_tester_cli) if args.nozzle_tester_cli else None
    viewer_dir = Path(args.viewer_repo_dir) if args.viewer_repo_dir else None
    tester_dir = Path(args.tester_repo_dir) if args.tester_repo_dir else None
    if args.require_external and (viewer is None or tester is None):
        raise SystemExit("--require-external requires --viewer-executable and --nozzle-tester-cli")
    if viewer is not None:
        require_existing_executable(viewer, "nozzle-viewer")
    if tester is not None:
        require_existing_executable(tester, "nozzle-tester-cli")

    for width, height in SIZES:
        if not args.skip_juce_pair:
            run_juce_pair(sender, receiver, width, height, evidence_dir)
        if viewer is not None:
            run_sender_to_viewer(sender, viewer, width, height, evidence_dir)
        if tester is not None:
            run_tester_to_receiver(tester, receiver, width, height, evidence_dir)

    write_summary(evidence_dir, repo_root, repo_root, viewer_dir, tester_dir)
    print(f"standalone app smoke PASS evidence_dir={evidence_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
