"""Base-game artwork remains visible when a separate update omits the images (#543)."""
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from test_launcher_controls import launcher, sfo_bytes


class LauncherArtworkTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.base, self.update = self.root / 'base', self.root / 'update'
        for folder, version in ((self.base, '01.00'), (self.update, '01.02')):
            (folder / 'sce_sys').mkdir(parents=True)
            (folder / 'eboot.bin').write_bytes(b'fixture')
            (folder / 'sce_sys/param.sfo').write_bytes(
                sfo_bytes(TITLE_ID='CUSA01623', APP_VER=version, TITLE='SpartanPort'))
        for name in ('pic1.png', 'icon0.png'):
            (self.base / 'sce_sys' / name).write_bytes(b'base artwork')

    def app(self, update=None):
        app = launcher.Launcher.__new__(launcher.Launcher)
        values = {'game_dir': str(self.base), 'update_dir': str(self.update) if update is None else update}
        app.var = lambda key, kind: mock.Mock(get=lambda: values[key])
        app.root, app.tk = mock.Mock(), mock.Mock()
        app.tk.TclError = RuntimeError
        app.banner = mock.MagicMock()
        app.banner.winfo_width.return_value = 640
        app.banner.__getitem__.return_value = 240
        app.px = lambda value: value
        app.font = lambda family, points, *style: (family, points, *style)
        app.banner_source = app.banner_logo = None
        app.logo_image = mock.Mock(return_value=None)
        app.scaled_art = mock.Mock()
        return app

    def test_cover_falls_back_to_base_but_version_stays_from_update(self):
        app = self.app()
        app.draw_banner()
        app.tk.PhotoImage.assert_called_once_with(file=str(self.base / 'sce_sys/pic1.png'))
        labels = [call.kwargs.get('text', '') for call in app.banner.create_text.call_args_list]
        self.assertTrue(any('01.02' in label for label in labels), labels)

    def test_window_icon_is_the_ports_own_whatever_the_dump(self):
        (self.update / 'sce_sys/icon0.png').write_bytes(b'updated icon')
        app = self.app()
        with mock.patch.object(launcher, 'window_icon') as icon:
            app.set_icon()
        self.assertEqual(icon.call_args.args[2:], (launcher.PORT_DIR / 'launcher' / 'spartanport.ico',
                                                   launcher.PORT_DIR / 'launcher' / 'spartanport.png'))

    def test_update_artwork_overrides_base_when_present(self):
        for name in ('pic1.png', 'icon0.png'):
            (self.update / 'sce_sys' / name).write_bytes(b'update artwork')
        app = self.app()
        app.draw_banner()
        app.tk.PhotoImage.assert_called_once_with(file=str(self.update / 'sce_sys/pic1.png'))

    def test_cover_falls_back_to_base_when_only_the_icon_was_updated(self):
        (self.update / 'sce_sys/icon0.png').write_bytes(b'updated icon')
        app = self.app()
        app.draw_banner()
        app.tk.PhotoImage.assert_called_once_with(file=str(self.base / 'sce_sys/pic1.png'))

    def test_without_an_update_the_base_cover_is_used(self):
        app = self.app(update='')
        app.draw_banner()
        app.tk.PhotoImage.assert_called_once_with(file=str(self.base / 'sce_sys/pic1.png'))

    def test_missing_artwork_still_uses_the_existing_placeholder(self):
        (self.base / 'sce_sys/pic1.png').unlink()
        app = self.app()
        app.tk.PhotoImage.side_effect = OSError('missing image')
        with mock.patch.object(launcher, 'heading_font', return_value='fixture font'):
            app.draw_banner()
        app.banner.create_image.assert_not_called()
        self.assertTrue(app.banner.create_text.called)

    def test_missing_artwork_shows_the_ports_logo(self):
        (self.base / 'sce_sys/pic1.png').unlink()
        app = self.app()
        app.tk.PhotoImage.side_effect = OSError('missing image')
        app.logo_image.return_value = 'logo'
        app.draw_banner()
        self.assertEqual(app.banner.create_image.call_args.kwargs['image'], 'logo')


if __name__ == '__main__':
    unittest.main()
