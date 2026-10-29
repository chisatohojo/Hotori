"""Compile the production C++ with host Arduino fakes, then execute it."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX") or shutil.which("g++"))
    args = parser.parse_args()
    if not args.cxx:
        parser.error("Pass --cxx with a host g++ executable, or set CXX/PATH.")
    runtime = Path(__file__).resolve().parents[1]
    output = runtime.parent / "build" / "hotori_runtime_tests"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / ("test_runtime.exe" if os.name == "nt" else "test_runtime")
    sources = [runtime / "tests" / "test_runtime.cpp",
               runtime / "tests" / "fakes" / "Arduino.cpp", *sorted(runtime.glob("*.cpp"))]
    command = [args.cxx, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic",
               "-I" + str(runtime / "tests" / "fakes"), "-I" + str(runtime),
               *map(str, sources), "-o", str(executable)]
    env = dict(os.environ)
    env["PATH"] = str(Path(args.cxx).resolve().parent) + os.pathsep + env.get("PATH", "")
    subprocess.run(command, check=True, env=env)
    subprocess.run([str(executable)], check=True, env=env)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode or 1)
