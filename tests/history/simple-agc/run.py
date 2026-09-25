#!/usr/bin/env python3
"""Compile actual Simple IPA AGC functions against bounded test dependencies."""
import argparse
import hashlib
from extract import extract
import json
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPOSITORY = HERE.parents[2]
RELATIVE = Path('src/ipa/simple/algorithms/agc.cpp')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--compiler', default='c++')
    parser.add_argument('--sanitizers', choices=['address,undefined', 'none'],
                        default='address,undefined')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or output == REPOSITORY or REPOSITORY in output.parents:
        parser.error('--output must be a new directory outside the repository')
    extractor = HERE / 'extract.py'
    prelude = (HERE / 'prelude.h').read_text()
    inputs = {}
    definitions = {}
    expected_candidate = 'aca866817fb1f775e3cb9ebfa84c38e56db96f629746a249f0ab51e5437419b3'
    for label, root in [('baseline', args.baseline), ('candidate', args.source)]:
        source = root.resolve() / RELATIVE
        text = source.read_text()
        if label == 'candidate' and sha(source.read_bytes()) != expected_candidate:
            raise ValueError('historical harness requires the original runtime-0018 source; use native tests for refactors')
        inputs[label] = {'path': str(source), 'sha256': sha(source.read_bytes()),
                         'header_sha256': sha(source.with_suffix('.h').read_bytes())}
        signatures = ['int Agc::configure(', 'void Agc::updateExposure(',
                      'void Agc::process(']
        body = '\n'.join(extract(text, signature) for signature in signatures)
        definitions[label] = body
        prelude += '\nnamespace ' + label + ' {\n'
        prelude += ('struct Agc { bool startup_=true; unsigned startupComputations_=0; '
                    'int configure(IPAContext &,const IPAConfigInfo &); '
                    'void updateExposure(IPAContext &,IPAFrameContext &,double); '
                    'void process(IPAContext &,uint32_t,IPAFrameContext &,'
                    'const SwIspStats *,ControlList &); };\n')
        prelude += body + '\n}\n'
    if definitions['candidate'] == definitions['baseline']:
        raise ValueError('baseline must be the unaccelerated source')
    output.mkdir(parents=True, mode=0o700)
    (output / 'actual-functions.inc').write_text(prelude)
    harness = HERE / 'harness.cpp'
    unit = output / 'harness.cpp'
    unit.write_bytes(harness.read_bytes())
    command = [args.compiler, '-std=c++17', '-g', '-Wall', '-Wextra', '-Werror',
               str(unit), '-o', str(output / 'regression')]
    if args.sanitizers != 'none':
        command += ['-fsanitize=' + args.sanitizers, '-fno-omit-frame-pointer']
    report = {'schema': 'sfp7-simple-agc-startup/v1', 'source_inputs': inputs,
              'extractor_sha256': sha(extractor.read_bytes()),
              'runner_sha256': sha(Path(__file__).read_bytes()),
              'harness_sha256': sha(harness.read_bytes()),
              'prelude_sha256': sha((HERE / 'prelude.h').read_bytes()),
              'translation_sha256': sha(prelude.encode()),
              'compiler': args.compiler, 'sanitizers': args.sanitizers,
              'command': command, 'hardware_tested': False,
              'scope': 'Actual functions; controlled IPA structures/logger. No I2C, delay, thread or image-quality model.'}
    status = 1
    try:
        built = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (output / 'compile.stdout.log').write_text(built.stdout)
        (output / 'compile.stderr.log').write_text(built.stderr)
        report['compile_exit'] = built.returncode
        if built.returncode:
            print(built.stdout + built.stderr, end='')
            report['passed'] = False
        else:
            run = subprocess.run([str(output / 'regression')], capture_output=True,
                                 text=True, timeout=30)
            (output / 'run.stdout.log').write_text(run.stdout)
            (output / 'run.stderr.log').write_text(run.stderr)
            expected = 'PASS 77 actual configure/process/update cases; deficit scaling, threshold exit, 96 valid calls and 300 empty calls included\n'
            passed = run.returncode == 0 and run.stdout == expected
            report.update(passed=passed, exit=run.returncode, stdout=run.stdout,
                          stderr=run.stderr, cases=77 if passed else None)
            print(run.stdout + run.stderr, end='')
            status = 0 if passed else 1
    except (OSError, subprocess.TimeoutExpired) as error:
        report.update(passed=False, error=repr(error))
    finally:
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return status


if __name__ == '__main__':
    raise SystemExit(main())
