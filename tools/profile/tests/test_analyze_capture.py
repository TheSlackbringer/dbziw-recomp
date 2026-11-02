#!/usr/bin/env python3
"""Fast unit test for analyze_capture.py (issue 010) on a made-up capture: the field rate comes from the FieldStart
markers, slow / normal follow that rate, fields without a marker are left out, and the game thread's waits on the GS
worker (markers 8/9) are counted. Not part of CI (a debugging tool); run with the other profiling tool tests:
python3 -m unittest discover -s tools/profile/tests"""
import contextlib, io, json, os, sys, tempfile, unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import analyze_capture, common

FIELD_NS = 16_683_350   # one NTSC-timing field (59.94 Hz)
GAME_TID = 7


def capture(d, rate_kind, fields, hud, gs_waits=()):
    recs = [common.MARKER_RECORD.pack(1_000_000 + f * FIELD_NS, f, 1, 0, 0, 1, rate_kind) for f in fields]
    for field, ms in gs_waits:
        t0 = 1_000_000 + field * FIELD_NS + 1_000_000
        recs.append(common.MARKER_RECORD.pack(t0, 3, GAME_TID, 0, 0, 8, 0))
        recs.append(common.MARKER_RECORD.pack(t0 + ms * 1_000_000, 0, GAME_TID, 0, 0, 9, 0))
    header = common.MARKER_HEADER.pack(b'DBZIWPM1', 1, common.MARKER_RECORD.size, len(recs), 0, 0, 0, 1, 0, 0, 1)
    with open(os.path.join(d, 'markers.bin'), 'wb') as f:
        f.write(header + b''.join(recs))
    with open(os.path.join(d, 'samples.jsonl'), 'w', encoding='utf-8') as f:
        f.write(json.dumps({'type': 'thread', 'pid': 1, 'tid': GAME_TID, 'name': 'GameThread', 'process': 'dbziw'}) + '\n')
    with open(os.path.join(d, 'target.log'), 'w', encoding='utf-8') as f:
        f.writelines(f'[hud] tick={t} fps={fps:.1f} vsync=59.9 frameMs=16.7 maxFrameMs=17.0\n' for t, fps in hud)


class AnalyzeCapture(unittest.TestCase):
    def run_on(self, *args, **kw):
        with tempfile.TemporaryDirectory() as d:
            capture(d, *args, **kw)
            with contextlib.redirect_stdout(io.StringIO()):   # its readable summary
                analyze_capture.main([d])
            with open(os.path.join(d, 'analysis.json'), encoding='utf-8') as f:
                return json.load(f)

    def test_field_rate_and_classes_follow_the_markers(self):
        # 60 Hz: normal at >= 59.4 fps, slow below 54 (90 %); 57 fps is neither. Field 0 has no marker.
        out = self.run_on(60, range(1, 200), [(60, 60.0), (120, 50.0), (180, 57.0)])
        self.assertEqual(out['field_rate'], 60)
        self.assertEqual(out['fields'], {'normal': 59, 'slow': 60})

    def test_pal_rate(self):
        # 50 Hz: DW5's limits (49.5 normal, 45 slow).
        out = self.run_on(50, range(1, 200), [(50, 49.5), (100, 44.9), (150, 46.0)])
        self.assertEqual(out['field_rate'], 50)
        self.assertEqual(out['fields'], {'normal': 49, 'slow': 50})

    def test_gs_worker_waits(self):
        out = self.run_on(60, range(1, 200), [(60, 60.0)], gs_waits=[(10, 2), (20, 4)])
        self.assertAlmostEqual(out['q3_idle_gap']['normal']['waiting_for_gs_worker_ms'], 6 / 59, places=6)


if __name__ == '__main__':
    unittest.main()
