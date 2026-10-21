#!/usr/bin/env python3

import os
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPILER = ROOT / "build" / "aicompiler"
LLVM_BIN = Path("/opt/homebrew/opt/llvm/bin")
CLANG = LLVM_BIN / "clang"
SOURCE = ROOT / "examples" / "demo2.src"

REPEATS = 15
INPUT = "3\n4\n"


def run(command, **kwargs):
    return subprocess.run(
        [str(part) for part in command],
        check=True,
        text=True,
        capture_output=True,
        **kwargs,
    )


def main():
    for tool in (COMPILER, CLANG):
        if not tool.exists():
            print(f"ERROR: Required tool not found: {tool}")
            return 1

    if not SOURCE.exists():
        print(f"ERROR: Example source not found: {SOURCE}")
        return 1

    with tempfile.TemporaryDirectory(prefix="aicompiler_bench_") as temp:
        temp = Path(temp)
        ir_file = temp / "program.ll"

        try:
            generated = run(
                [COMPILER, "--llvm", SOURCE],
                cwd=ROOT,
            )
            ir_file.write_text(generated.stdout)

            executables = {}
            for level in ("O0", "O2"):
                executable = temp / f"program_{level}"
                run([
                    CLANG,
                    f"-{level}",
                    ir_file,
                    "-o",
                    executable,
                ])
                executables[level] = executable

            results = {}
            expected_output = None

            # Warm up each executable and check equivalent behavior.
            for level, executable in executables.items():
                completed = run([executable], input=INPUT)
                if expected_output is None:
                    expected_output = completed.stdout
                elif completed.stdout != expected_output:
                    print("ERROR: O0 and O2 produced different output.")
                    print(f"O0: {expected_output!r}")
                    print(f"O2: {completed.stdout!r}")
                    return 1

            for level, executable in executables.items():
                timings_ms = []
                for _ in range(REPEATS):
                    start = time.perf_counter_ns()
                    completed = subprocess.run(
                        [str(executable)],
                        input=INPUT,
                        text=True,
                        capture_output=True,
                    )
                    elapsed = time.perf_counter_ns() - start

                    if completed.returncode != 0:
                        print(f"ERROR: {level} executable failed.")
                        print(completed.stderr)
                        return 1

                    if completed.stdout != expected_output:
                        print(f"ERROR: Output changed during {level} run.")
                        return 1

                    timings_ms.append(elapsed / 1_000_000)

                results[level] = statistics.median(timings_ms)

        except subprocess.CalledProcessError as exc:
            print("ERROR: A compiler or execution command failed.")
            print("Command:", " ".join(map(str, exc.cmd)))
            if exc.stderr:
                print(exc.stderr)
            return 1

        print("LLVM NATIVE RUNTIME BENCHMARK")
        print("=============================")
        print(f"Source: {SOURCE.name}")
        print(f"Successful repetitions per level: {REPEATS}")
        print("Input: 3, 4")
        print("Output equivalence: PASS")
        print()
        print(f"LLVM -O0 median: {results['O0']:.4f} ms")
        print(f"LLVM -O2 median: {results['O2']:.4f} ms")

        if results["O2"] > 0:
            change = (
                (results["O0"] - results["O2"]) / results["O0"]
            ) * 100
            print(f"Observed time reduction: {change:.2f}%")
        else:
            print("Observed time reduction: unavailable")

        print()
        print("Caution: process startup dominates tiny programs.")
        print("Treat these timings as an experiment, not a reliable")
        print("general speedup claim or a measurement of rule-based")
        print("optimization. Both builds use the same generated IR.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
