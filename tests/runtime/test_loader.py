# SPDX-License-Identifier: MIT
"""Run the loader with a private fake filesystem and command boundaries."""

import json
import os
import re
import shlex
from pathlib import Path
import subprocess
import tempfile
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
IDENTITIES = json.loads((Path(__file__).parent / 'p1-identities.json').read_text())

# This executable substitutes system commands only. Expected identities are the
# fixed original P1 records, never recalculated from the candidate loader.
COMMAND = r'''#!/usr/bin/env python3
import json,os,pathlib,sys
root=pathlib.Path(os.environ['SFP7_TEST_ROOT'])
data=json.loads((root/'identities.json').read_text())
mode=os.environ.get('SFP7_TEST_FAILURE','')
name=pathlib.Path(sys.argv[0]).name
args=sys.argv[1:]
modules={r['module']:r for r in data['modules']}
if name=='uname':
    print('wrong-kernel' if mode=='release' else data['release'])
elif name=='sha256sum':
    relative='/'+str(pathlib.Path(args[0]).relative_to(root))
    sha=data['firmware_sha256'] if relative.endswith('/ipu4p_cpd.bin') else next(r['sha256'] for r in modules.values() if r['path']==relative)
    wrong = mode=='digest' or (mode=='module-digest' and not relative.endswith('/ipu4p_cpd.bin'))
    print(('0'*64 if wrong else sha)+'  '+args[0])
elif name=='modinfo':
    entry=modules[args[-1]]
    if args[0]=='-n': print(root/('wrong-path' if mode=='path' else entry['path'].lstrip('/')))
    elif args[1]=='vermagic': print('wrong-kernel' if mode=='vermagic' else data['release']+' SMP')
    elif args[1]=='signer': print('wrong-signer' if mode=='signer' else data['signer'])
elif name=='modprobe':
    with (root/'loads').open('a') as stream: stream.write(args[0]+'\n')
    if args[0]=='intel_ipu4p_isys':
        pin=root/'sys/bus/intel-ipu4-bus/devices/intel-ipu4-mmu1/power/control'
        assert pin.read_text().strip()=='on', 'MMU pin must precede ISYS load'
    if mode=='load' and args[0]=='intel_ipu4p_psys': sys.exit(1)
elif name in ('journalctl','sleep'): pass
else: raise SystemExit('unexpected mock command: '+name)
'''


@unittest.skipUnless(os.geteuid() == 0 and os.uname().sysname == 'Linux',
                     'run in the isolated Linux build container as root')
class LoaderTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='sfp7-loader-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'identities.json').write_text(json.dumps(IDENTITIES))
        self.write('usr/lib/firmware/ipu4p_cpd.bin', 'fixture firmware')
        for entry in IDENTITIES['modules']:
            self.write(entry['path'].lstrip('/'), 'fixture module')
        self.write('proc/sys/kernel/random/boot_id', 'fixture-boot\n')
        self.pin = self.write('sys/bus/intel-ipu4-bus/devices/intel-ipu4-mmu1/power/control', 'auto\n')
        self.write('dev/media0', '')
        driver = self.root / 'sys/bus/i2c/drivers/dw9719'
        driver.mkdir(parents=True)
        vcm = self.root / 'sys/bus/i2c/devices/i2c-INT347A:00-VCM'
        vcm.mkdir(parents=True)
        (vcm / 'driver').symlink_to(driver)
        (self.root / 'run').mkdir()
        commands = self.root / 'bin'
        commands.mkdir()
        for name in ('uname', 'sha256sum', 'modinfo', 'modprobe', 'journalctl', 'sleep'):
            path = commands / name
            path.write_text(COMMAND)
            path.chmod(0o755)
        # Map filesystem roots in a disposable copy. The actual statement order,
        # error checks, table parsing, trap and one-attempt behavior stay intact.
        text = (REPOSITORY / 'runtime/sfp7-camera-load-p1').read_text()
        roots = r'(?<![A-Za-z0-9_/.])/(usr/lib/firmware|lib/modules|sys|proc|run|dev)(?=/|[\"\s)])'
        text = re.sub(roots, lambda match: str(self.root) + match.group(0), text)
        self.loader = self.write('loader', text)
        self.env = dict(os.environ, PATH=str(commands) + ':' + os.environ['PATH'],
                        SFP7_TEST_ROOT=str(self.root))
        self.marker = self.root / 'run/sfp7-camera-load-fixture-boot'

    def write(self, relative, value):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value)
        return path

    def run_loader(self, failure=''):
        return subprocess.run(['bash', str(self.loader)], capture_output=True, text=True,
                              env=dict(self.env, SFP7_TEST_FAILURE=failure), timeout=20)

    def test_success_preserves_order_pin_and_single_attempt(self):
        result = self.run_loader()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / 'loads').read_text().splitlines(),
                         [entry['module'] for entry in IDENTITIES['modules']])
        self.assertEqual(self.pin.read_text(), 'auto\n')
        self.assertTrue(self.marker.is_dir())
        second = self.run_loader()
        self.assertEqual(second.returncode, 2)
        self.assertIn('second IPU4P load attempt', second.stderr)
        self.assertEqual(len((self.root / 'loads').read_text().splitlines()), 7)

    def test_identity_failures_never_load_or_create_attempt(self):
        for failure in ('release', 'digest', 'module-digest', 'path', 'vermagic', 'signer'):
            with self.subTest(failure=failure):
                result = self.run_loader(failure)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertFalse((self.root / 'loads').exists())
                self.assertFalse(self.marker.exists())

    def test_load_failure_keeps_attempt_marker_and_stops_sequence(self):
        result = self.run_loader('load')
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('failed at module/state: intel_ipu4p_psys', result.stderr)
        self.assertTrue(self.marker.is_dir())
        self.assertEqual(len((self.root / 'loads').read_text().splitlines()), 6)
        self.assertEqual(self.run_loader().returncode, 2)

    def test_service_and_module_records_share_the_original_release(self):
        service = (REPOSITORY / 'runtime/etc/systemd/system/sfp7-camera-load-p1.service').read_text()
        self.assertIn('ConditionKernelVersion==' + IDENTITIES['release'], service)
        self.assertTrue(all('/' + IDENTITIES['release'] + '/' in r['path']
                            for r in IDENTITIES['modules']))

    def test_candidate_checks_every_original_module_identity(self):
        source = (REPOSITORY / 'runtime/sfp7-camera-load-p1').read_text()
        table = re.search(r'(?ms)^readonly -a IPU_MODULE_IDENTITIES=\(\n(.*?)^\)', source)
        self.assertIsNotNone(table, 'candidate must declare the P1 module records')
        actual = []
        for row in shlex.split(table.group(1), comments=True):
            module, relative, sha = row.split('|')
            actual.append({'module': module,
                           'path': '/lib/modules/' + IDENTITIES['release'] + '/' + relative,
                           'sha256': sha})
        self.assertEqual(actual, IDENTITIES['modules'])


if __name__ == '__main__':
    unittest.main()
