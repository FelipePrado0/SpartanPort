import json
import os
import sys
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'scripts'))
import discord_presence as dp  # noqa: E402


class FakeTransport:
    def __init__(self, replies):
        self.incoming = b''.join(dp.pack(dp.FRAME, reply) for reply in replies)
        self.written = []
        self.closed = False

    def write(self, data):
        self.written.append(data)

    def read(self, size):
        chunk, self.incoming = self.incoming[:size], self.incoming[size:]
        return chunk

    def close(self):
        self.closed = True


def decode(data):
    opcode, length = dp.HEADER.unpack(data[:dp.HEADER.size])
    return opcode, json.loads(data[dp.HEADER.size:dp.HEADER.size + length])


class Framing(unittest.TestCase):
    def test_pack_has_opcode_length_and_utf8_body(self):
        data = dp.pack(dp.FRAME, {'a': 'ñ'})
        opcode, length = dp.HEADER.unpack(data[:8])
        self.assertEqual(opcode, dp.FRAME)
        self.assertEqual(length, len(data) - 8)
        self.assertEqual(json.loads(data[8:]), {'a': 'ñ'})

    def test_read_frame_reassembles_split_reads(self):
        class Slow(FakeTransport):
            def read(self, size):
                return super().read(min(size, 3))
        transport = Slow([{'evt': 'READY'}])
        self.assertEqual(dp.read_frame(transport), (dp.FRAME, {'evt': 'READY'}))

    def test_read_frame_raises_when_connection_ends(self):
        with self.assertRaises(ConnectionError):
            dp.read_frame(FakeTransport([]))


class Activity(unittest.TestCase):
    def test_only_the_start_time_is_shown(self):
        self.assertEqual(dp.activity(100.9), {'timestamps': {'start': 100}})

    def test_clearing_omits_the_activity(self):
        _, body = decode(dp.set_activity(7))
        self.assertEqual(body['args'], {'pid': 7})
        self.assertEqual(body['cmd'], 'SET_ACTIVITY')


class PresenceLifecycle(unittest.TestCase):
    def setUp(self):
        self.env = mock.patch.dict(os.environ, {}, clear=False)
        self.env.start()
        os.environ.pop('GOW3_DISCORD', None)
        os.environ.pop('GOW3_DISCORD_ID', None)

    def tearDown(self):
        self.env.stop()

    def test_disabled_without_client_id(self):
        with mock.patch.object(dp, 'CLIENT_ID', ''), mock.patch.object(dp, 'open_transport') as opened:
            presence = dp.Presence()
            presence.close()
        opened.assert_not_called()
        self.assertIsNone(presence.thread)

    def test_disabled_by_switch(self):
        os.environ['GOW3_DISCORD'] = '0'
        with mock.patch.object(dp, 'open_transport') as opened:
            presence = dp.Presence(client_id='123')
            presence.close()
        opened.assert_not_called()

    def test_handshake_activity_and_clean_close(self):
        transport = FakeTransport([{'evt': 'READY'}, {'evt': None}])
        with mock.patch.object(dp, 'open_transport', return_value=transport):
            presence = dp.Presence(client_id='123')
            presence.thread.join(5)
            presence.close()
        frames = [decode(data) for data in transport.written]
        self.assertEqual(frames[0], (dp.HANDSHAKE, {'v': 1, 'client_id': '123'}))
        opcode, body = frames[1]
        self.assertEqual(opcode, dp.FRAME)
        self.assertEqual(body['args']['pid'], os.getpid())
        self.assertEqual(body['args']['activity'], {'timestamps': {'start': int(presence.started)}})
        self.assertNotIn('activity', frames[2][1]['args'])
        self.assertEqual(frames[3][0], dp.CLOSE)
        self.assertTrue(transport.closed)

    def test_rejected_handshake_closes_and_retries_later(self):
        transport = FakeTransport([{'evt': 'ERROR'}])
        with mock.patch.object(dp, 'open_transport', return_value=transport), \
                mock.patch.object(dp, 'RETRY_SECONDS', 60):
            presence = dp.Presence(client_id='123')
            for _ in range(100):
                if transport.closed:
                    break
                presence.thread.join(0.05)
            presence.close()
        self.assertTrue(transport.closed)
        self.assertIsNone(presence.transport)

    def test_discord_not_running_does_not_raise(self):
        with mock.patch.object(dp, 'open_transport', return_value=None), \
                mock.patch.object(dp, 'RETRY_SECONDS', 60):
            presence = dp.Presence(client_id='123')
            presence.close()
        self.assertFalse(presence.thread.is_alive())

    def test_close_without_connection_is_harmless(self):
        presence = dp.Presence(client_id='')
        presence.close()
        presence.close()


class LauncherSwitch(unittest.TestCase):
    def setUp(self):
        sys.path.insert(0, str(ROOT / 'launcher'))
        import gow3_launcher_win as launcher
        self.launcher = launcher

    def environment(self, **settings):
        values = {**self.launcher.APP_DEFAULTS, **settings}
        with mock.patch.object(self.launcher, 'game_profile_of', return_value=(None, None)):
            return self.launcher.game_environment(values)

    def test_off_by_default(self):
        self.assertFalse(self.launcher.APP_DEFAULTS['discord_rpc'])
        self.assertEqual(self.environment()['GOW3_DISCORD'], '0')

    def test_checked_enables_it(self):
        self.assertEqual(self.environment(discord_rpc=True)['GOW3_DISCORD'], '1')

    def test_label_is_translated(self):
        import gow3_lang
        self.assertEqual(gow3_lang.table('es')['Show Discord Rich Presence'], 'Mostrar Discord Rich Presence')
        self.assertIn('Discord', gow3_lang.table('pt')['Show Discord Rich Presence'])


if __name__ == '__main__':
    unittest.main()
