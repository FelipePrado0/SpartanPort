"""The Aspect ratio filter on the Home and Performance pages: the render resolution list shows one
aspect ratio, and choosing another one moves the resolution to its closest size."""
import unittest

from test_launcher_diagnostics_layout import DiagnosticsLayoutTest, tk


@unittest.skipUnless(tk, 'requires Tk')
class AspectFilterTest(unittest.TestCase):
    window = DiagnosticsLayoutTest.window

    def test_aspect_follows_and_filters_the_render_resolution(self):
        with self.window('en', 1.0) as (root, app):
            resolution, aspect = app.var('render_resolution', 'ini'), app.aspect_var()
            self.assertEqual((resolution.get(), aspect.get()), ('native', '16:9'))
            aspect.set('16:10')
            self.assertEqual(resolution.get(), '1920x1200')
            self.assertEqual([v for v, _t in app.aspect_resolution_options()],
                             ['1280x800', '1440x900', '1920x1200', '2560x1600'])
            resolution.set('3440x1440')
            self.assertEqual(aspect.get(), '21:9')
            self.assertIn('4:3', [v for v, _t in app.aspect_options()])
            self.assertEqual(dict(app.aspect_resolution_options())['3440x1440'], ('3440 × 1440',))  # confirmed in game: not beta
