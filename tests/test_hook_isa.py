"""The hook stubs (src/*_hook.h) run on the host as written: the ISA rewrite only patches the game's
own code, so an instruction a Sandy or Ivy Bridge CPU lacks stops the game there (#818: andn in the
timed QTE stub, exception 0xc000001d on every QTE)."""
from paths import ROOT
import re
import unittest
import isa_rewrite

# Missing on the oldest supported CPUs (Intel Sandy Bridge, AMD FX).
NEWER_SETS = ('BMI1', 'BMI2', 'LZCNT', 'MOVBE', 'F16C', 'AVX2', 'FMA')


def code_arrays(text):
    """The byte lists in braces ({0x48,0xb8, 0,0,...}): stub bodies, moved instructions, cheat bytes."""
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    for group in re.findall(r'\{([^{}]*)\}', text):
        tokens = [t.strip() for t in group.split(',') if t.strip()]
        if tokens and all(re.fullmatch(r'0x[0-9a-fA-F]+|\d+', t) for t in tokens):
            yield bytes(int(t, 0) for t in tokens)


@unittest.skipUnless(isa_rewrite.iced_x86, 'pip install iced-x86')
class HookIsaTests(unittest.TestCase):
    def test_stubs_use_only_instructions_of_the_oldest_supported_cpus(self):
        iced = isa_rewrite.iced_x86
        newer = {getattr(iced.CpuidFeature, name): name for name in NEWER_SETS}
        found = []
        for path in sorted((ROOT / 'src').glob('*_hook.h')):
            for code in code_arrays(path.read_text(encoding='utf-8')):
                for ins in iced.Decoder(64, code):
                    sets = [newer[f] for f in ins.cpuid_features() if f in newer]
                    if sets:
                        found.append(f'{path.name}: {ins} ({", ".join(sets)})')
        self.assertEqual(found, [])


if __name__ == '__main__':
    unittest.main()
