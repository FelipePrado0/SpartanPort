from paths import ROOT
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from patches import (compile_patches, external_patches, external_selection, compile_external,
                     game_profile, selected_patches, patch_requirements)

GOW3 = ROOT / 'patches/God_of_War_III_Remastered.xml'
GOW3_EU = ROOT / 'patches/God_of_War_III_Remastered_EU.xml'
GOW3_IDS = {'CUSA01623'}
SEGMENTS = [(0, 0x6000000)]


EXTERNAL = """<?xml version="1.0"?>
<Patch>
  <TitleID><ID>CUSA01623</ID></TitleID>
  <Metadata Title="God of War III" Name="On" Author="x" PatchVer="1.0" AppVer="01.02" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="bytes" Address="0x00401000" Value="9090"/></PatchList>
  </Metadata>
  <Metadata Title="God of War III" Name="Off" Author="x" PatchVer="1.0" AppVer="01.02" AppElf="eboot.bin">
    <PatchList><Line Type="bytes32" Address="0x00402000" Value="0x12345678"/></PatchList>
  </Metadata>
  <Metadata Title="God of War III" Name="Mask" Author="x" PatchVer="1.0" AppVer="01.02" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="mask" Value="90 ?? 90" Offset="0"/></PatchList>
  </Metadata>
  <Metadata Title="God of War III" Name="Old" Author="x" PatchVer="1.0" AppVer="01.00" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="bytes" Address="0x00403000" Value="90"/></PatchList>
  </Metadata>
</Patch>"""


class ExternalPatchTests(unittest.TestCase):
    def test_selection_and_unsupported_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / 'extra.xml').write_text(EXTERNAL)
            (folder / 'other.xml').write_text(EXTERNAL.replace('CUSA01623', 'CUSA99999'))
            (folder / 'broken.xml').write_text('<Patch>')
            found = external_patches(folder, '01.02', GOW3, GOW3_IDS)
            self.assertEqual([key for key, _, _ in found],
                             ['extra.xml/On', 'extra.xml/Off', 'extra.xml/Mask'])
            # The file's isEnabled; the mask patch is skipped as a whole.
            writes = compile_external(external_selection(found, None), SEGMENTS)
            self.assertEqual(writes, [(0x1000, bytes.fromhex('9090'))])
            config = folder / 'patches.json'
            config.write_text('{"enabled": ["extra.xml/Off"], "disabled": ["extra.xml/On"]}')
            writes = compile_external(external_selection(found, config), SEGMENTS)
            self.assertEqual(writes, [(0x2000, (0x12345678).to_bytes(4, 'little'))])

    def test_built_in_file_is_not_external(self):
        self.assertEqual(external_patches(GOW3.parent, '01.02', GOW3, GOW3_IDS), [])
        # Nor another build's: the US file also names CUSA01715, for its own 01.02 addresses.
        self.assertEqual(external_patches(GOW3.parent, '01.02', GOW3_EU, {'CUSA01715'}), [])


class GameProfileTests(unittest.TestCase):
    def test_profiles_by_title_id(self):
        self.assertEqual(game_profile('CUSA01623')[1:3], (GOW3, '01.02'))
        self.assertEqual(game_profile('CUSA01623', '01.00')[1:3], (GOW3, '01.02'))  # its only profile
        self.assertEqual(game_profile('CUSA01715', '01.00')[1:3], (GOW3_EU, '01.00'))
        self.assertIsNone(game_profile('CUSA99999'))
        self.assertIsNone(game_profile(None))

    def test_gow3_defaults_and_extras(self):
        self.assertEqual(selected_patches(GOW3, '01.02', ''),
                         ['Bug Fix - Texture Corruption Fix', 'Skip Any Video With X Button'])
        self.assertEqual(selected_patches(GOW3, '01.02', 'Resolution Patch - 720p; Skip Any Video With X Button'),
                         ['Skip Any Video With X Button', 'Resolution Patch - 720p'])
        self.assertEqual(selected_patches(GOW3, '01.00', ''), [])
        # The launcher's selection is exact: unchecked defaults stay off.
        self.assertEqual(selected_patches(GOW3, '01.02', 'Skip Any Video With X Button', only=True), ['Skip Any Video With X Button'])
        self.assertEqual(selected_patches(GOW3, '01.02', '', only=True), [])

    def test_gow3_resolution_patch_replaces_texture_fix(self):
        self.assertEqual(selected_patches(GOW3, '01.02', 'Resolution Patch - 4K'),
                         ['Skip Any Video With X Button', 'Resolution Patch - 4K'])
        with self.assertRaises(ValueError):
            selected_patches(GOW3, '01.02', 'Resolution Patch - 4K;Resolution Patch - 720p')

    def test_eboot_build_check(self):
        from patches import eboot_matches
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / 'eboot.bin').write_bytes(b'another build')
            self.assertFalse(eboot_matches(folder, game_profile('CUSA01623')))
            self.assertTrue(eboot_matches(folder, None))  # no profile: nothing to check

    def test_game_patchable(self):
        from patches import game_patchable
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            (folder / 'eboot.bin').write_bytes(b'another build')
            self.assertFalse(game_patchable(folder))  # no param.sfo: unknown game
            self.assertFalse(game_patchable(None))

    def test_gow3_patches_compile(self):
        for xml, version in ((GOW3, '01.02'), (GOW3_EU, '01.00'), (GOW3_EU, '01.02')):
            names = [m.get('Name') for m in ET.parse(xml).getroot().iter('Metadata')
                     if m.get('AppVer') == version]
            self.assertTrue(compile_patches(xml, names, version, SEGMENTS))

    def test_eu_intro_hang_fix(self):
        # The second frame's display buffer index (.bss, image offset 0x14ba600) starts at 1.
        self.assertEqual(selected_patches(GOW3_EU, '01.00', ''),
                         ['Bug Fix - Intro Hang', 'Bug Fix - Texture Corruption Fix'])
        self.assertEqual(compile_patches(GOW3_EU, ['Bug Fix - Intro Hang'], '01.00', SEGMENTS),
                         [(0x14ba600, (1).to_bytes(4, 'little'))])
        self.assertEqual(game_profile('CUSA01715', '01.00')[3],
                         '4e399baa5362f47be0066aaed208d7352f1fb43e590b74be3a639a392120f396')
        # Update 01.02 fixes the hang itself; a selection made for 01.00 drops the fix there.
        self.assertEqual(selected_patches(GOW3_EU, '01.02', ''), ['Bug Fix - Texture Corruption Fix'])
        self.assertEqual(selected_patches(GOW3_EU, '01.02', 'Bug Fix - Intro Hang', only=True), [])
        self.assertEqual(game_profile('CUSA01715', '01.02')[3],
                         '6e72330e9092e5164049b865e5568f2d450904001a73fe0a2f4b39194c195b3c')
        # The texture fix needs the direct memory its note asks for.
        self.assertEqual(patch_requirements(GOW3_EU, ['Bug Fix - Texture Corruption Fix'], '01.02'),
                         (5056 + 2048, None))

    def test_eu_0102_resolution_patches(self):
        # Ported from CUSA01623: code +0x120 (0x5f0000-0x609fff) / -0x10 (0x564000-0x56bfff).
        writes = dict(compile_patches(GOW3_EU, ['Resolution Patch - 720p'], '01.02', SEGMENTS))
        self.assertEqual(writes[0x5f0c5b - 0x400000], (1280).to_bytes(4, 'little'))
        self.assertEqual(writes[0x5643e3 - 0x400000], (720).to_bytes(4, 'little'))
        self.assertEqual(selected_patches(GOW3_EU, '01.02', 'Resolution Patch - 720p'),
                         ['Resolution Patch - 720p'])  # it replaces the texture fix

    def test_patch_selection_key(self):
        from patches import patch_key
        self.assertEqual(patch_key('CUSA01623', game_profile('CUSA01623')), 'CUSA01623')
        self.assertEqual(patch_key('CUSA01715', game_profile('CUSA01715', '01.02')), 'CUSA01715 01.02')
        self.assertEqual(patch_key('CUSA99999', None), 'CUSA99999')

    def test_requirements_from_patch_notes(self):
        # Notes ask for extra direct memory (shadPS4 "DMEM") and a VBlank rate.
        self.assertEqual(patch_requirements(GOW3, ['Resolution Patch - 4K', 'Skip Any Video With X Button'], '01.02'),
                         (5056 + 6144, None))
        self.assertEqual(patch_requirements(GOW3, ['Frame Rate Patch - 120 FPS'], '01.02'), (None, 120))
        self.assertEqual(patch_requirements(GOW3, [], '01.02'), (None, None))


if __name__ == '__main__':
    unittest.main()
