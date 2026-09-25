#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare complete BLC/LUT class sources with framework-only substitutes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent

SOURCES = [
    "include/libcamera/internal/software_isp/swisp_stats.h",
    "include/libcamera/internal/software_isp/debayer_params.h",
    "src/ipa/simple/ipa_context.h",
    "src/ipa/simple/algorithms/blc.h",
    "src/ipa/simple/algorithms/lut.h",
    "src/ipa/simple/algorithms/blc.cpp",
    "src/ipa/simple/algorithms/lut.cpp",
]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract(path: Path) -> str:
    # Remove only build includes and pragma once. Keep class declarations,
    # function bodies, namespaces, registration, and logging verbatim.
    lines = path.read_text().splitlines(keepends=True)
    return ''.join(
        '\n' if re.match(r'^\s*#\s*(?:include\b|pragma once\b)', line) else line
        for line in lines
    )


def translation_unit(source: Path) -> str:
    chunks = [(HERE / "framework.h").read_text()]
    for index, relative in enumerate(SOURCES):
        if index == 3:
            chunks.append((HERE / "algorithm-interface.h").read_text())
        chunks.append(f'\n#line 1 {json.dumps(relative)}\n')
        chunks.append(extract(source / relative))
    chunks.append('\n#line 1 "cases.cpp"\n')
    chunks.append((HERE / "cases.cpp").read_text())
    return '\n'.join(chunks)


def build_and_run(source: Path, work: Path, name: str, compiler: str,
                  sanitizers: str) -> dict:
    unit = work / f"{name}.cpp"
    binary = work / name
    unit.write_text(translation_unit(source))
    command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic"]
    if sanitizers != "none":
        command.extend([f"-fsanitize={sanitizers}", "-fno-omit-frame-pointer"])
    command.extend(["-g", str(unit), "-o", str(binary)])
    subprocess.run(command, check=True)
    result = subprocess.run([str(binary)] + (["--startup-only"] if baseline else []),
                            text=True, capture_output=True, check=False)
    print(result.stdout, end='')
    print(result.stderr, end='')
    if result.returncode != 0:
        raise RuntimeError("actual-source regression suite failed")
    return {"command": command, "returncode": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr,
            "translation_unit_sha256": digest(unit)}



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', default='c++')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('use a new output directory')
    args.output.mkdir(parents=True)
    before = build_and_run(args.baseline, args.output, 'baseline', args.compiler, 'address,undefined')
    after = build_and_run(args.candidate, args.output, 'candidate', args.compiler, 'address,undefined')
    if before['stdout'] != after['stdout']:
        raise RuntimeError('BLC/LUT observable outputs differ')
    report = {'schema': 1, 'hardware_tested': False, 'passed': True,
              'baseline': before, 'candidate': after,
              'cases': after['stdout'].count('PASS '),
              'sources': {label: {name: digest(root / name) for name in SOURCES}
                          for label, root in [('baseline', args.baseline), ('candidate', args.candidate)]}}
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
