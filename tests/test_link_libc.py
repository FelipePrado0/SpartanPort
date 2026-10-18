"""Cross-module identity and TLS relocation tests without game binaries."""
from paths import ROOT
import json
from pathlib import Path
import struct
import tempfile
import hashlib
import unittest
from unittest.mock import patch
import link_libc
from test_probe import package


def self_fixture():
    """A SELF holding one prx-like module: one code byte, a string table, one exported symbol
    'bzQExy189ZI#C#A' whose library C and module A are both ('libc', 1), and empty relocation
    tables. Enough for link_libc.module without game binaries."""
    data = bytearray(0x3a0)
    data[:4] = b'O='
    struct.pack_into('<H', data, 24, 2)
    struct.pack_into('<QQQQ', data, 32, 0x800, 0x200, 16, 16)
    struct.pack_into('<QQQQ', data, 64, 0x100800, 0x220, 0x160, 0x160)
    struct.pack_into('<16sHHIQQQIHHHHHH', data, 96, b'ELF	', 0xfe18, 62, 1, 0, 64, 0, 0,
                     64, 56, 3, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', data, 160, 1, 5, 0x1000, 0, 0, 16, 16, 0x1000)
    struct.pack_into('<IIQQQQQQ', data, 216, 0x61000000, 4, 0x2000, 0, 0, 0x160, 0, 16)
    struct.pack_into('<IIQQQQQQ', data, 272, 2, 4, 0x20b0, 0, 0, 176, 176, 8)
    data[0x200] = 0xc3
    data[0x220:0x230] = b'bzQExy189ZI#C#A\0'
    data[0x230:0x235] = b'libc\0'
    struct.pack_into('<IBBHQQ', data, 0x260, 0, 0x12, 0, 1, 0, 1)
    libc = (1 << 32) | 16  # version 1, name at string offset 16
    tags = [(0x61000035, 0), (0x61000037, 32), (0x61000039, 64), (0x6100003f, 24),
            (0x61000029, 0), (0x6100002d, 0), (0x6100002f, 0), (0x61000031, 0),
            (0x61000013, (2 << 48) | libc), (0x6100000d, libc), (0, 0)]
    for i, (tag, value) in enumerate(tags):
        struct.pack_into('<QQ', data, 0x2d0 + 16 * i, tag, value)
    return bytes(data)


class ModuleTests(unittest.TestCase):
    def test_module_accepts_unpacked_elf_with_the_same_exports(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'libc.prx'
            source = self_fixture()
            path.write_bytes(source)
            expected = link_libc.module(path)
            elf = link_libc.parse_self(source)[0]
            path.write_bytes(elf)
            actual = link_libc.module(path)
        self.assertEqual(actual['symbols'], expected['symbols'])
        self.assertEqual(actual['ph'], expected['ph'])
        self.assertEqual(actual['relocs'], expected['relocs'])
        self.assertEqual(actual['sha256'], hashlib.sha256(elf).hexdigest())

    def test_module_reads_the_file_once_and_reports_its_hash(self):
        data = self_fixture()
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'libc.prx'
            path.write_bytes(data)
            info = link_libc.module(path)
        self.assertEqual(info['sha256'], hashlib.sha256(data).hexdigest())
        self.assertEqual(info['missing'], [])
        self.assertEqual(info['symbols'][0]['identity'], ('bzQExy189ZI', ('libc', 1), ('libc', 1)))
        self.assertEqual(info['relocs'], [])


class LinkTests(unittest.TestCase):
    def fixture(self, version=1, relocs=()):
        main_key=('fixture',('libc',1),('libc',1))
        lib_key=('fixture',('libc',version),('libc',1))
        main=dict(identity=lambda name:main_key,libraries={'q':('libc',1)},
                  modules={'q':('libc',1)},ph=[dict(type=0x61000001,vaddr=256)])
        symbols=[dict(name='',type=3,binding=0,section=1,value=0,size=0,identity=None),
                 dict(name='fixture#C#A',type=2,binding=1,section=1,value=16,size=1,identity=lib_key)]
        elf=b'\x31\xc0\xc3'+bytes(13)+b'\xc3'+bytes(111)
        libc=dict(elf=elf,ph=[dict(type=1,vaddr=0,memsz=4096,filesz=len(elf),offset=0,flags=5),
                            dict(type=7,vaddr=128,memsz=32,filesz=4)],
                  tags={12:0},symbols=symbols,relocs=list(relocs),sha256='fixture')
        return main,libc

    def run_link(self,main,libc,code=b'\xc3'):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)
            (out/'boot.bin').write_bytes(package(code,names=['fixture#q#q'],capabilities=1))
            with patch.object(link_libc,'module',side_effect=[main,libc]):
                link_libc.link(Path('fixture-game'),out)
            return (out/'boot-libc.bin').read_bytes(),json.loads((out/'libc-link.json').read_text())

    def test_different_local_ids_bind_same_identity(self):
        data,report=self.run_link(*self.fixture())
        self.assertEqual(data[:8],b'G3PROBE4')
        self.assertEqual(report['symbol_bindings'],[
            dict(import_name='fixture#q#q',address='0x10010',kind=1)])

    def test_same_nid_wrong_library_version_does_not_bind(self):
        _,report=self.run_link(*self.fixture(version=2))
        self.assertEqual(report['bindings'],0)

    def test_tls_module_id_is_literal_not_image_pointer(self):
        data,report=self.run_link(*self.fixture(relocs=[(64,16,0,0)]))
        size=struct.unpack_from('<Q',data,8)[0]
        image=data[-size:]
        self.assertEqual(struct.unpack_from('<Q',image,65536+64)[0],2)
        self.assertEqual(report['tls_relocations'],1)

    def test_unsupported_tls_symbol_rejected(self):
        with self.assertRaisesRegex(ValueError,'unsupported external TLS'):
            self.run_link(*self.fixture(relocs=[(64,16,1,0)]))

    def test_relocation_outside_segments_rejected(self):
        with self.assertRaisesRegex(ValueError,'relocation outside image'):
            self.run_link(*self.fixture(relocs=[(4092,8,0,0)]))

    def test_fs_thread_pointer_loads_are_rewritten_to_gs(self):
        main,libc=self.fixture()
        load=bytes.fromhex('64488b042500000000')
        code=load+b'\x90'+load+b'\xc3'
        main['ph']=main['ph']+[dict(type=1,vaddr=0,filesz=64,memsz=64,flags=5),
                               dict(type=7,vaddr=2048,filesz=0,memsz=48,align=16)]
        data,report=self.run_link(main,libc,code)
        size=struct.unpack_from('<Q',data,8)[0]
        image=data[-size:]
        self.assertEqual(image[0],0x65)
        self.assertEqual(image[10],0x65)
        self.assertEqual(report['fs_loads_patched'],2)
        self.assertEqual(report['main_tls'],dict(vaddr=2048,filesz=0,memsz=48,align=16))
        self.assertEqual(struct.unpack_from('<4Q',data,56+64),(2048,0,48,16))

    def test_fs_loads_outside_executable_segments_are_kept(self):
        main,libc=self.fixture()
        load=bytes.fromhex('64488b042500000000')
        main['ph']=main['ph']+[dict(type=1,vaddr=0,filesz=64,memsz=64,flags=6)]
        data,report=self.run_link(main,libc,load)
        image=data[-struct.unpack_from('<Q',data,8)[0]:]
        self.assertEqual(image[0],0x64)
        self.assertEqual(report['fs_loads_patched'],0)
