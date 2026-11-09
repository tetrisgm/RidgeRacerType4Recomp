"""Delivery integrity and no-overwrite guarantees, using temporary local folders."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from deliver_windows import deliver, verify


class DeliveryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / 'version-1'
        self.source.mkdir()
        (self.source / 'r4-runtime.exe').write_bytes(b'fixture')
        (self.source / 'BUILD.json').write_text(json.dumps({'working_tree_dirty': False}))
        (self.source / 'NATIVE-OVERLAYS.json').write_text(json.dumps(
            {'linked_variants': 7459, 'published_variants': 7459}))
        self.manifest()
        self.destination_root = self.root / 'builds'
        self.destination_root.mkdir()

    def manifest(self):
        (self.source / 'SHA256SUMS').write_text(''.join(
            f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n'
            for p in sorted(self.source.iterdir()) if p.name != 'SHA256SUMS'))

    def test_copy_and_reuse_refusal(self):
        target = deliver(self.source, self.destination_root)
        self.assertEqual(verify(target), verify(self.source))
        with self.assertRaises(FileExistsError):
            deliver(self.source, self.destination_root)
        self.assertEqual((target / 'r4-runtime.exe').read_bytes(), b'fixture')

    def test_missing_destination_root_is_not_created(self):
        missing = self.root / 'missing'
        with self.assertRaisesRegex(ValueError, 'unavailable'):
            deliver(self.source, missing)
        self.assertFalse(missing.exists())

    def test_tampered_payload_is_rejected_before_reservation(self):
        (self.source / 'r4-runtime.exe').write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
            deliver(self.source, self.destination_root)
        self.assertFalse((self.destination_root / 'r4').exists())

    def test_unlisted_nested_manifest_is_rejected(self):
        (self.source / 'extra').mkdir()
        (self.source / 'extra/SHA256SUMS').write_text('hidden')
        with self.assertRaisesRegex(ValueError, 'file set differ'):
            deliver(self.source, self.destination_root)

    def test_dirty_build_is_rejected(self):
        (self.source / 'BUILD.json').write_text(json.dumps({'working_tree_dirty': True}))
        self.manifest()
        with self.assertRaisesRegex(ValueError, 'dirty'):
            deliver(self.source, self.destination_root)
        self.assertFalse((self.destination_root / 'r4').exists())


if __name__ == '__main__':
    unittest.main()
