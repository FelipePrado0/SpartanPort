# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("build_streamline", Path(__file__).resolve().parents[1] /
                                              "packaging/windows/build_streamline.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class StreamlinePackageTests(unittest.TestCase):
    def test_archive_cannot_write_outside_staging(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "sdk.zip"
            with zipfile.ZipFile(archive, "w") as target:
                target.writestr("include/sl.h", "header")
                target.writestr("../foreign.dll", "untrusted")
            with zipfile.ZipFile(archive) as source:
                with self.assertRaises(ValueError):
                    sdk.checked_members(source, Path(directory) / "stage")
            self.assertFalse((Path(directory) / "foreign.dll").exists())

    def test_package_uses_only_production_fg_and_reflex_libraries(self):
        self.assertIn("nvngx_dlssg.dll", sdk.DLLS)
        self.assertIn("NvLowLatencyVk.dll", sdk.DLLS)
        self.assertNotIn("nvngx_dlss.dll", sdk.DLLS) # The existing SR bridge retains its SDK.
        self.assertEqual(sdk.VERSION, "2.14.16")


if __name__ == "__main__":
    unittest.main()
