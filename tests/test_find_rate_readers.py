"""tools/find_rate_readers.py (high FPS roadmap, F4.0) on a synthetic executable: a fake engine rate
setter with the src/probe.c signature, its globals, the float constants and three readers.

Run: python -I tests/test_find_rate_readers.py (no game files; objdump is not needed)."""
import importlib.util
import io
import os
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, "tools", "find_rate_readers.py")

CODE, DATA = 0x1000, 0x3000  # virtual addresses of the two segments
FPS, ROUNDED, NTSC, SIXTY, SIXTIETH, HALF, OTHER = (DATA + x for x in (0, 4, 0x10, 0x14, 0x18, 0x1C, 0x40))


def load_tool():
    spec = importlib.util.spec_from_file_location("find_rate_readers", TOOL)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def rip(at, target, after=0):
    """disp32 at `at` of an instruction that ends `after` bytes after it."""
    return struct.pack("<i", target - (at + 4 + after))


def synthetic(swing=True):
    """(ELF bytes, swing reader address): three functions, the setter, the readers and a decoy."""
    code = bytearray(b"\xcc" * 0x400)

    def put(at, data):
        code[at - CODE:at - CODE + len(data)] = data

    # The setter (src/probe.c engine_fps_setter) inside a function.
    setter = CODE + 4
    put(CODE, b"\x55\x48\x89\xe5")
    put(setter, b"\x48\x8d\x05" + rip(setter + 3, FPS) + b"\xc5\xfa\x11\x00" + b"\xc5\xfa\x58\x0d"
        + rip(setter + 15, HALF) + b"\xc5\xfa\x2c\xc1" + b"\x48\x8d\x0d" + rip(setter + 26, ROUNDED)
        + b"\x89\x01\xc3")
    # The swing energy reader (frame rate patch pattern): lea rax, [rip+fps].
    put(CODE + 0x100, b"\x55\x48\x89\xe5")
    swing_at = CODE + 0x104 + 8
    if swing:
        put(CODE + 0x104, b"\xc5\xfa\x11\x85\x98\xfb\xff\xff" + b"\x48\x8d\x05" + rip(swing_at + 3, FPS)
            + b"\xc5\xfa\x10\x00\xc3")
    # mov eax, [rip+rounded]; vmulss xmm0, xmm0, [rip+1/60]; a decoy lea to other data.
    put(CODE + 0x200, b"\x55\x48\x89\xe5")
    put(CODE + 0x204, b"\x8b\x05" + rip(CODE + 0x206, ROUNDED))
    put(CODE + 0x20A, b"\xc5\xfa\x59\x05" + rip(CODE + 0x20E, SIXTIETH))
    put(CODE + 0x212, b"\x48\x8d\x05" + rip(CODE + 0x215, OTHER) + b"\xc3")

    data = bytearray(0x100)
    struct.pack_into("<f", data, FPS - DATA, 59.94)
    struct.pack_into("<i", data, ROUNDED - DATA, 60)
    for address, raw in ((NTSC, 0x426FC28F), (SIXTY, 0x42700000), (SIXTIETH, 0x3C888889), (HALF, 0x3F000000)):
        struct.pack_into("<I", data, address - DATA, raw)

    code_offset, data_offset = 0x1000, 0x2000
    header = bytearray(64)
    header[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", header, 16, 2, 0x3E, 1, CODE, 64, 0, 0, 64, 56, 2, 64, 0, 0)
    phdrs = struct.pack("<IIQQQQQQ", 1, 5, code_offset, CODE, CODE, len(code), len(code), 0x1000)
    phdrs += struct.pack("<IIQQQQQQ", 1, 6, data_offset, DATA, DATA, len(data), len(data), 0x1000)
    elf = bytearray(data_offset + len(data))
    elf[:64] = header
    elf[64:64 + len(phdrs)] = phdrs
    elf[code_offset:code_offset + len(code)] = code
    elf[data_offset:data_offset + len(data)] = data
    return bytes(elf), swing_at


def wrap_self(elf):
    """Place the ELF's two segments at different SELF file offsets."""
    data = bytearray(0x700)
    data[:4] = b"O\x15=\x1d"
    struct.pack_into("<H", data, 24, 2)
    data[96:96 + 176] = elf[:176]
    for index, source in enumerate((0x200, 0x600)):
        offset, = struct.unpack_from("<Q", elf, 64 + index * 56 + 8)
        size, = struct.unpack_from("<Q", elf, 64 + index * 56 + 32)
        struct.pack_into("<QQQQ", data, 32 + index * 32, 0x800 | (index << 20), source, size, size)
        data[source:source + size] = elf[offset:offset + size]
    return bytes(data)


class FindRateReadersTest(unittest.TestCase):
    def setUp(self):
        self.tool = load_tool()
        self.folder = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.folder.cleanup()

    def write(self, data):
        path = os.path.join(self.folder.name, "eboot.elf")
        with open(path, "wb") as f:
            f.write(data)
        return path

    def test_signature_search_matches_src_signature_h(self):
        image = self.tool.Image(b"\x01\x02\x03\x01\x02\x04\x01\x02\x03")
        self.assertIsNone(self.tool.find_unique(image, "01 02 03"))  # twice: ambiguous
        self.assertEqual(self.tool.find_unique(image, "01 02 04"), 3)
        self.assertEqual(self.tool.find_unique(image, "02 ?? 01 02 03"), 4)

    def test_three_readers_and_the_known_case(self):
        data, swing = synthetic()
        out = io.StringIO()
        readers, ok = self.tool.report(self.write(data), tool=None, out=out)
        text = out.getvalue()
        self.assertEqual(len(readers), 3, text)  # the setter's own writes are not readers
        self.assertTrue(ok, text)
        self.assertEqual([r[0] for r in readers], [swing, CODE + 0x204, CODE + 0x20A])
        self.assertEqual([r[3] for r in readers], ["lea", "mov", "vmulss"])
        self.assertIn("globals: float 0x%x, rounded 0x%x" % (FPS, ROUNDED), text)
        self.assertIn("readers: 3 in 2 functions", text)
        self.assertIn("function 0x%x" % (CODE + 0x200), text)
        self.assertIn("0x%x (file offset 0x%x) lea rate (float global)" % (swing, 0x1000 + swing - CODE), text)
        self.assertIn("vmulss constant 1/60 -> 0x%x" % SIXTIETH, text)
        self.assertIn("known case: the swing energy reader at 0x%x is listed (0x48ff1a in v01.02)" % swing, text)
        self.assertNotIn("0x%x" % OTHER, text.split("readers:")[1])  # the decoy

    def test_missing_known_case_is_a_warning(self):
        data, _ = synthetic(swing=False)
        out = io.StringIO()
        readers, ok = self.tool.report(self.write(data), tool=None, out=out)
        self.assertEqual(len(readers), 2)
        self.assertFalse(ok)
        self.assertIn("WARNING: the swing energy reader", out.getvalue())

    def test_self_maps_readers_and_zero_filled_global(self):
        elf, swing = synthetic()
        elf = bytearray(elf)
        rounded = DATA + 0x200
        struct.pack_into("<Q", elf, 64 + 56 + 40, 0x300)
        elf[0x1000 + 4 + 26:0x1000 + 4 + 30] = rip(CODE + 4 + 26, rounded)
        elf[0x1206:0x120A] = rip(CODE + 0x206, rounded)
        image = self.tool.Image(wrap_self(elf))
        targets, setter, _ = self.tool.find_targets(image)
        readers = self.tool.find_readers(image, targets, setter)
        self.assertEqual([r[0] for r in readers], [swing, CODE + 0x204, CODE + 0x20A])
        self.assertEqual(targets[rounded], "rate (rounded global)")
        self.assertEqual(image.file_offset(swing), 0x200 + swing - CODE)
        self.assertIsNone(image.file_offset(rounded))

    def test_constants_in_executable_segment_are_found(self):
        elf, _ = synthetic()
        elf = bytearray(elf)
        constant = CODE + 0x300
        struct.pack_into("<I", elf, 0x1300, 0x42700000)
        elf[0x120E:0x1212] = rip(CODE + 0x20E, constant)
        image = self.tool.Image(elf)
        targets, setter, _ = self.tool.find_targets(image)
        readers = self.tool.find_readers(image, targets, setter)
        self.assertEqual(targets.get(constant), "constant 60.0")
        self.assertIn((CODE + 0x20A, CODE + 0x20E, constant, "vmulss"), readers)

    def test_runs_isolated_without_objdump(self):
        data, _ = synthetic()
        flat = bytes(self.tool.Image(data).bytes)
        for source, kind in ((data, "ELF"), (wrap_self(data), "SELF"),
                             (flat, "flat file: not an ELF")):
            with self.subTest(kind=kind):
                run = subprocess.run([sys.executable, "-I", TOOL, "--no-objdump", self.write(source)],
                                     capture_output=True, text=True, cwd=self.folder.name, timeout=30)
                self.assertEqual(run.returncode, 0, run.stderr)
                self.assertIn("(%s)" % kind, run.stdout)
                self.assertIn("readers: 3 in 2 functions", run.stdout)
        warned = subprocess.run([sys.executable, "-I", TOOL, "--no-objdump", self.write(synthetic(False)[0])],
                                capture_output=True, text=True)
        self.assertEqual(warned.returncode, 0, warned.stderr)  # a warning, not a failure
        self.assertIn("WARNING", warned.stdout)


if __name__ == "__main__":
    unittest.main()
