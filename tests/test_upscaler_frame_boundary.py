import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RENDERER = ROOT / 'gpu/shadps4/video_core/renderer_vulkan'


class UpscalerFrameBoundaryTests(unittest.TestCase):
    def test_composite_finishes_before_camera_and_frame_state_are_cleared(self):
        source = (RENDERER / 'vk_rasterizer.cpp').read_text(encoding='utf-8')
        start = source.index('void Rasterizer::DrawRecord(')
        boundary = source[start:source.index('bind_prepared = used_prepared;', start)]
        self.assertLess(boundary.index('upscaler->OnFrameStart()'),
                        boundary.index('camera_motion->OnDisplayPass('))

        source = (RENDERER / 'vk_temporal_upscaler.cpp').read_text(encoding='utf-8')
        start = source.index('bool TemporalUpscaler::OnFrameStart()')
        boundary = source[start:source.index('void TemporalUpscaler::OnGraphicsDraw(', start)]
        self.assertLess(boundary.index('OnGraphicsDraw(0, {});'),
                        boundary.index('ldr_pending = false;'))


if __name__ == '__main__':
    unittest.main()
