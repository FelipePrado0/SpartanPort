import os
import sys
from paths import ROOT
from pathlib import Path
import subprocess
import unittest

EXE = ROOT / ('out/runtime-test.exe' if sys.platform == 'win32' else 'out/runtime-test')


@unittest.skipUnless(EXE.exists(), 'run bash build.sh --test first')
class RuntimeTests(unittest.TestCase):
    def run_case(self, *args, env=None):
        run_env = {**os.environ, **env} if env else None
        return subprocess.run([str(EXE.resolve()), *args], capture_output=True, text=True, timeout=10, env=run_env)

    def test_runtime_contracts(self):
        r=self.run_case()
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('PASS: callback lifecycle',r.stdout)

    def test_recursive_guard_fails_explicitly(self):
        r=self.run_case('--guard-recursion')
        self.assertEqual(r.returncode,21)
        self.assertIn('recursive/concurrent static initialization',r.stderr)

    def test_stack_corruption_is_not_ignored(self):
        r=self.run_case('--stack-failure')
        self.assertEqual(r.returncode,22)
        self.assertIn('stack protector detected corruption',r.stderr)

    def test_tls_bounds_are_checked(self):
        r=self.run_case('--bad-tls')
        self.assertEqual(r.returncode,21)
        self.assertIn('unsupported TLS module/offset',r.stderr)

    def test_rwlock_lifecycle_and_ownership(self):
        r=self.run_case('--rwlock-lifecycle')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)

    def test_rwlock_multiple_readers_exclude_writer(self):
        r=self.run_case('--rwlock-concurrency')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)

    def test_rwlock_timeouts_use_orbis_errors(self):
        r=self.run_case('--rwlock-timeouts')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)

    def test_guest_memory_failure_exits_with_error(self):
        r = self.run_case('--guest-memory-fail', env={'SPARTAN_FAIL_GUEST_MEMORY': '1450'})
        self.assertEqual(r.returncode, 21)
        self.assertIn('cannot create', r.stderr)
        self.assertIn('error 1450', r.stderr)
        if sys.platform == 'win32':
            self.assertIn('is the page file large enough?', r.stderr)
        else:
            self.assertIn('is /dev/shm or RAM large enough?', r.stderr)
