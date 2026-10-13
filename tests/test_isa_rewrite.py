"""Site discovery and classification for the Haswell instruction rewrite."""
from paths import ROOT
import contextlib
import hashlib
import io
import struct
import tempfile
import unittest
from pathlib import Path
import isa_rewrite

BLSR_ECX = bytes.fromhex('c4e270f3c9')          # blsr ecx, ecx            (5)
MOVBE_STORE = bytes.fromhex('0f38f108')          # movbe [rax], ecx         (4)
BEXTR_RIP = bytes.fromhex('c4e270f7050c000000')  # bextr eax, [rip+0xc], ecx (9)
LZCNT_AX = bytes.fromhex('66f30fbdc1')           # lzcnt ax, cx             (5)
TZCNT_RAX = bytes.fromhex('f3480fbcc1')          # tzcnt rax, rcx           (5)
NOP, RET, JMP_SELF = b'\x90', b'\xc3', bytes.fromhex('ebfe')
MOV_RIP = bytes.fromhex('8b0500000000')          # mov eax, [rip+0]         (6)
MOV_FS = bytes.fromhex('648b042500000000')       # mov eax, fs:[0]          (8): not a site


@unittest.skipUnless(isa_rewrite.iced_x86, 'pip install iced-x86')
class ScanSegmentTests(unittest.TestCase):
    def test_blsr_is_a_jmp_site(self):
        s, = isa_rewrite.scan_segment(BLSR_ECX, 0x1000)
        self.assertEqual((s.offset, s.length, s.kind, s.width, s.mode), (0x1000, 5, isa_rewrite.KIND_BLSR, 32, isa_rewrite.MODE_JMP))
        self.assertEqual((s.dst, s.src1, s.src2, s.mem_slot), (1, 1, isa_rewrite.NO_REG, 0))
        self.assertEqual((s.original, s.relocated), (BLSR_ECX, b''))

    def test_bextr_rip_relative_memory_source(self):
        s, = isa_rewrite.scan_segment(BEXTR_RIP, 0x1000)
        self.assertEqual((s.kind, s.width, s.dst, s.src1, s.src2, s.mem_slot), (isa_rewrite.KIND_BEXTR, 32, 0, isa_rewrite.NO_REG, 1, 2))
        self.assertEqual((s.base, s.index, s.mem_flags, s.disp), (isa_rewrite.NO_REG, isa_rewrite.NO_REG, 1, 0x1000 + 9 + 0xc))

    def test_sixteen_bit_lzcnt_and_sixty_four_bit_tzcnt(self):
        a, b = isa_rewrite.scan_segment(LZCNT_AX + TZCNT_RAX, 0)
        self.assertEqual((a.kind, a.width, a.length), (isa_rewrite.KIND_LZCNT, 16, 5))
        self.assertEqual((b.kind, b.width, b.dst, b.src1), (isa_rewrite.KIND_TZCNT, 64, 0, 1))

    def test_four_byte_store_relocates_a_plain_next_instruction(self):
        s, = isa_rewrite.scan_segment(MOVBE_STORE + NOP + RET, 0)
        self.assertEqual((s.kind, s.mode, s.mem_slot, s.base, s.dst, s.src1), (isa_rewrite.KIND_MOVBE_STORE, isa_rewrite.MODE_JMP_RELOCATE, 1, 0, isa_rewrite.NO_REG, 1))
        self.assertEqual(s.relocated, NOP)

    def test_four_byte_site_falls_back_to_int3(self):
        for tail in (JMP_SELF, MOV_RIP, BLSR_ECX, b''):       # branch, RIP-relative, another site, end of segment
            s = isa_rewrite.scan_segment(MOVBE_STORE + tail, 0)[0]
            self.assertEqual((s.mode, s.relocated), (isa_rewrite.MODE_INT3, b''), tail.hex())

    def test_next_instruction_that_is_a_branch_target_blocks_relocation(self):
        code = bytes.fromhex('eb04') + MOVBE_STORE + NOP + RET   # jmp +4 lands on the nop
        s, = isa_rewrite.scan_segment(code, 0)
        self.assertEqual(s.mode, isa_rewrite.MODE_INT3)

    def test_unsupported_encodings(self):
        for code, why in ((b'\x64' + BEXTR_RIP, 'segment prefix'), (b'\x67' + MOVBE_STORE, '32-bit address'),
                          (bytes.fromhex('c4e2791310'), 'vcvtph2ps xmm2, [rax]')):
            s, = isa_rewrite.scan_segment(code + RET, 0)
            self.assertEqual(s.mode, isa_rewrite.MODE_UNSUPPORTED, why)
        self.assertEqual(isa_rewrite.scan_segment(bytes.fromhex('c4e2791310') + RET, 0)[0].kind, isa_rewrite.KIND_VCVTPH2PS)

    def test_negative_displacement_is_signed(self):
        s, = isa_rewrite.scan_segment(bytes.fromhex('0f38f048f8'), 0)   # movbe ecx, [rax-8]
        self.assertEqual((s.kind, s.disp, s.base, s.mem_slot), (isa_rewrite.KIND_MOVBE_LOAD, -8, 0, 2))

    def test_sib_memory_operand(self):
        s, = isa_rewrite.scan_segment(bytes.fromhex('c4e2e0f2449108'), 0)   # andn rax, rbx, [rcx+rdx*4+8]
        self.assertEqual((s.kind, s.width, s.dst, s.src1, s.src2, s.mem_slot), (isa_rewrite.KIND_ANDN, 64, 0, 3, isa_rewrite.NO_REG, 2))
        self.assertEqual((s.base, s.index, s.scale, s.mem_flags, s.disp), (1, 2, 4, 0, 8))

    def test_non_sites_are_ignored(self):
        self.assertEqual(isa_rewrite.scan_segment(MOV_FS + RET, 0), [])


def boot_linked(segments):
    """Fake G3PROBE5 as link_modules.py writes it: (code, size, flags) segments laid out from address 0,
    no modules, bindings, names or relocations."""
    image = b''.join(code.ljust(size, b'\0') for code, size, flags in segments)
    table, address = [], 0
    for code, size, flags in segments:
        table.append((address, size, flags))
        address += size
    return (struct.pack('<8s6Q', b'G3PROBE5', len(image), 0, len(table), 0, 0, 1) + struct.pack('<Q', 0)
            + struct.pack('<4Q', 0, 0, 0, 0) + struct.pack('<Q', 0) + struct.pack('<Q', 0)
            + b''.join(struct.pack('<3Q', *t) for t in table) + image)


def run_main(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = isa_rewrite.main(argv)
    return code, out.getvalue()


class ReadBootLinkedTests(unittest.TestCase):
    def test_read_boot_linked_returns_segments_and_image(self):
        data = boot_linked([(BLSR_ECX + RET, 0x1000, 5), (b'data', 0x2000, 6)])
        segments, image = isa_rewrite.read_boot_linked(data)
        self.assertEqual(segments, [(0, 0x1000, 5), (0x1000, 0x2000, 6)])
        self.assertEqual(len(image), 0x3000)
        self.assertEqual((image[:6], image[0x1000:0x1004]), (BLSR_ECX + RET, b'data'))

    def test_read_boot_linked_rejects_bad_magic_and_truncation(self):
        data = boot_linked([(RET, 0x1000, 5)])
        with self.assertRaisesRegex(ValueError, 'not a G3PROBE5 image'):
            isa_rewrite.read_boot_linked(b'G3PROBE4' + data[8:])
        with self.assertRaisesRegex(ValueError, 'out of bounds'):
            isa_rewrite.read_boot_linked(data[:-1])

    def test_records_are_sixty_four_bytes(self):
        self.assertEqual((isa_rewrite.HEADER.size, isa_rewrite.RECORD.size), (64, 64))

    def test_main_without_iced_x86(self):
        saved, isa_rewrite.iced_x86 = isa_rewrite.iced_x86, None
        try:
            code, out = run_main(['missing.bin', '--out', '.'])
        finally:
            isa_rewrite.iced_x86 = saved
        self.assertEqual((code, out), (1, 'ISA: rewrite table unavailable (iced_x86 is not installed)\n'))


@unittest.skipUnless(isa_rewrite.iced_x86, 'pip install iced-x86')
class TableTests(unittest.TestCase):
    def test_encode_table_header_and_record(self):
        sites = isa_rewrite.scan_segment(BLSR_ECX + MOVBE_STORE + NOP + RET, 0x2000)
        image = bytes(0x2000) + BLSR_ECX + MOVBE_STORE + NOP + RET
        table = isa_rewrite.encode_table(image, sites)
        magic, size, sha, count, unsupported = isa_rewrite.HEADER.unpack_from(table, 0)
        self.assertEqual((magic, size, sha, count, unsupported), (b'G3ISA1\0\0', len(image), hashlib.sha256(image).digest(), 2, 0))
        self.assertEqual(len(table), 64 + 2 * 64)
        offset, length, kind, width, mode, dst, src1, src2, mem_slot, base, index, scale, flags, disp, original, rl, relocated = \
            isa_rewrite.RECORD.unpack_from(table, 64 + 64)
        self.assertEqual((offset, length, kind, mode, mem_slot, base, rl, relocated[:rl]),
                         (0x2005, 4, isa_rewrite.KIND_MOVBE_STORE, isa_rewrite.MODE_JMP_RELOCATE, 1, 0, 1, NOP))
        self.assertEqual(original[:length], MOVBE_STORE)

    def test_encode_table_keeps_negative_displacement(self):
        sites = isa_rewrite.scan_segment(bytes.fromhex('0f38f048f8') + RET, 0)   # movbe ecx, [rax-8]
        record = isa_rewrite.RECORD.unpack_from(isa_rewrite.encode_table(b'', sites), 64)
        self.assertEqual(record[13], -8)

    def test_encode_table_sorts_records_by_offset(self):
        late, early = isa_rewrite.scan_segment(BLSR_ECX, 0x2000) + isa_rewrite.scan_segment(BLSR_ECX, 0x1000)
        table = isa_rewrite.encode_table(bytes(0x3000), [late, early])
        offsets = [isa_rewrite.RECORD.unpack_from(table, 64 + 64 * i)[0] for i in range(2)]
        self.assertEqual(offsets, [0x1000, 0x2000])

    def test_summary_line(self):
        sites = isa_rewrite.scan_segment(BLSR_ECX + MOVBE_STORE + NOP + bytes.fromhex('c4e2791310') + RET, 0)
        self.assertEqual(isa_rewrite.summary(sites),
                         'ISA: 3 sites (MOVBE 1, BMI1 1, LZCNT 0, F16C 1); 1 relocated, 0 int3, 1 unsupported')

    def test_main_writes_table_for_executable_segments_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            boot = Path(tmp) / 'boot-linked.bin'
            boot.write_bytes(boot_linked([(BLSR_ECX + RET, 0x1000, 5), (b'', 0x1000, 5), (BLSR_ECX, 0x1000, 6)]))
            code, out = run_main([str(boot), '--out', tmp])
            self.assertEqual(code, 0)
            self.assertEqual(out, 'ISA: 1 sites (MOVBE 0, BMI1 1, LZCNT 0, F16C 0); 0 relocated, 0 int3, 0 unsupported\n')
            table = (Path(tmp) / 'isa-rewrite.bin').read_bytes()
            image = isa_rewrite.read_boot_linked(boot.read_bytes())[1]
            self.assertEqual(table, isa_rewrite.encode_table(image, isa_rewrite.scan_segment(BLSR_ECX + RET, 0)))

    def test_main_reports_unavailable_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad, short = Path(tmp) / 'bad.bin', Path(tmp) / 'short.bin'
            bad.write_bytes(b'G3PROBE4' + bytes(64))
            short.write_bytes(boot_linked([(RET, 0x1000, 5)])[:-1])
            for path, reason in ((Path(tmp) / 'missing.bin', ''), (bad, 'not a G3PROBE5 image'), (short, 'out of bounds')):
                code, out = run_main([str(path), '--out', tmp])
                self.assertEqual(code, 1, path.name)
                self.assertTrue(out.startswith('ISA: rewrite table unavailable (' + reason), out)
            self.assertFalse((Path(tmp) / 'isa-rewrite.bin').exists())

    def test_fixture_file_is_current(self):
        code = isa_rewrite.fixture_code()
        expected = isa_rewrite.encode_table(code, isa_rewrite.scan_segment(code, 0))
        self.assertEqual((ROOT / 'tests/data/isa_fixture.bin').read_bytes(), expected)
        sites = isa_rewrite.scan_segment(code, 0)
        self.assertEqual(len(sites), 6)
        self.assertEqual((sites[0].kind, sites[1].mode, sites[5].mode),
                         (isa_rewrite.KIND_BLSR, isa_rewrite.MODE_JMP_RELOCATE, isa_rewrite.MODE_INT3))
        self.assertNotIn(isa_rewrite.MODE_UNSUPPORTED, [s.mode for s in sites])


if __name__ == '__main__':
    unittest.main()
