#!/usr/bin/env python3
"""Analysis of one profiled game capture (issue 010, from DW5 025's p4/analyze_025.py): answers questions 1-5 from a
capture dir that holds samples.jsonl, markers.bin and target.log (report.json for the waits). A debugging aid:
nothing here is a pass or fail, and profiled runs are never timing evidence.

The field rate (50 PAL, 60 NTSC timing / PAL60) comes from the FieldStart markers. Slow / normal seconds come from
the game's own [hud] readings (PS2X_HUD_LOG=1): fps below --slow-below (default 90 % of the field rate; DW5: 45 of
50) is slow, fps at or above 99 % of it (DW5: 49.5) is normal; each reading covers the `rate` fields (one second)
before its tick. Field times come from the FieldStart markers (steady_clock, the same clock as the recorder samples
after conversion).

Device-job questions (Q1, Q1b, Q3c) need the async device worker (PS2X_VU1_ASYNC=1); with it off (DBZIW's default
since issue 009) the VU1 work runs on the game thread and shows up in Q2's samples instead. Q3 also reports the
game thread's waits on the GS worker (markers 8/9, DBZIW).

Usage: analyze_capture.py <capture dir> [--from TICK] [--to TICK] [--slow-below FPS]
  ->  <dir>/analysis.json, <dir>/analysis.txt"""
import argparse, bisect, collections, json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common, report

FIELD, SUBMIT, JSTART, JEND, WSTART, WEND, FRAME, GSTART, GEND = 1, 2, 3, 4, 5, 6, 7, 8, 9
WORKER, GAME = 'ps2-device', 'GameThread'


def load(cap):
    _, marks = common.read_markers(os.path.join(cap, 'markers.bin'))
    records = [json.loads(l) for l in open(os.path.join(cap, 'samples.jsonl'), encoding='utf-8') if l.strip()]
    text = open(os.path.join(cap, 'target.log'), encoding='utf-8', errors='replace').read()
    hud = [(int(t), float(f)) for t, f in re.findall(r'\[hud\] tick=(\d+) fps=([\d.]+)', text)]
    return marks, records, hud


def classify_fields(hud, lo, hi, rate, slow_below=None):
    """field -> 'slow' | 'normal' (fields covered by a HUD reading in the window)."""
    slow_below = 0.9 * rate if slow_below is None else slow_below
    cls = {}
    for tick, fps in hud:
        if tick < lo or tick > hi:
            continue
        c = 'slow' if fps < slow_below else ('normal' if fps >= 0.99 * rate else None)
        if c:
            for f in range(max(lo, tick - rate), tick):
                cls.setdefault(f, c)
    return cls


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('cap')
    ap.add_argument('--from', dest='lo', type=int, default=0)
    ap.add_argument('--to', dest='hi', type=int, default=1 << 62)
    ap.add_argument('--slow-below', type=float, default=None,
                    help='a HUD reading below this is slow (default 90 %% of the field rate)')
    a = ap.parse_args(argv)
    marks, records, hud = load(a.cap)
    out = {'capture': os.path.abspath(a.cap), 'markers': len(marks)}

    # ---- field timeline ----
    fstart = {}
    for m in marks:
        if m['event'] == FIELD:
            fstart.setdefault(m['seq'], m['t_ns'])
    fields = sorted(fstart)
    ftimes = [fstart[f] for f in fields]
    rates = collections.Counter(m['kind'] for m in marks if m['event'] == FIELD and m.get('kind') in (50, 60))
    rate = rates.most_common(1)[0][0] if rates else 50
    budget_ms = 1000.0 / rate
    out['field_rate'] = rate

    def field_at(t):
        i = bisect.bisect_right(ftimes, t) - 1
        return fields[i] if i >= 0 else None

    # Only fields with a FieldStart marker (DW5's window began at tick 3000; here it may start at 0).
    cls = {f: c for f, c in classify_fields(hud, a.lo, a.hi, rate, a.slow_below).items() if f in fstart}
    out['fields'] = {'slow': sum(1 for c in cls.values() if c == 'slow'), 'normal': sum(1 for c in cls.values() if c == 'normal')}

    # ---- Q1: work vs speed (device jobs) ----
    starts = {}
    per_field = collections.defaultdict(lambda: {'busy_ns': 0, 'vu1_cycles': 0, 'vu1_busy_ns': 0, 'jobs': 0})
    programs = {'slow': collections.Counter(), 'normal': collections.Counter()}
    for m in marks:
        if m['event'] == JSTART:
            starts[m['seq']] = m['t_ns']
        elif m['event'] == JEND and m['seq'] in starts:
            d = m['t_ns'] - starts.pop(m['seq'])
            f = field_at(m['t_ns'])
            if f is None:
                continue
            pf = per_field[f]
            pf['busy_ns'] += d
            pf['jobs'] += 1
            if m['vu1_cycles']:
                pf['vu1_cycles'] += m['vu1_cycles']
                pf['vu1_busy_ns'] += d
            c = cls.get(f)
            if c and m['vu1_cycles']:
                programs[c][f'0x{m["program"]:04x}'] += m['vu1_cycles']
    q1 = {}
    for c in ('slow', 'normal'):
        fs = [per_field[f] for f, k in cls.items() if k == c]
        n = len(fs) or 1
        cyc = sum(p['vu1_cycles'] for p in fs)
        q1[c] = {'fields': len(fs), 'vu1_cycles_per_field': cyc / n,
                 'device_busy_ms_per_field': sum(p['busy_ns'] for p in fs) / n / 1e6,
                 'ns_per_vu1_cycle': (sum(p['vu1_busy_ns'] for p in fs) / cyc) if cyc else None,
                 'jobs_per_field': sum(p['jobs'] for p in fs) / n,
                 'top_programs_by_cycles': programs[c].most_common(8)}
    out['q1_work_vs_speed'] = q1

    # ---- Q1b: how full each field is for the device (job time split over the fields it spans) ----
    span_busy = collections.defaultdict(float)
    open_jobs = {}
    for m in marks:
        if m['event'] == JSTART:
            open_jobs[m['seq']] = m['t_ns']
        elif m['event'] == JEND and m['seq'] in open_jobs:
            t0, t1 = open_jobs.pop(m['seq']), m['t_ns']
            i = bisect.bisect_right(ftimes, t0) - 1
            while 0 <= i < len(fields) and t0 < t1:
                edge = min(t1, ftimes[i + 1]) if i + 1 < len(ftimes) else t1
                span_busy[fields[i]] += (edge - t0) / 1e6
                t0, i = edge, i + 1
    q1b = {}
    for c in ('slow', 'normal'):
        v = sorted(span_busy[f] for f, k in cls.items() if k == c and f + 1 in fstart)
        if v:
            q1b[c] = {'fields': len(v), 'median_ms': v[len(v) // 2], 'p90_ms': v[int(len(v) * .9)],
                      **{f'share_at_least_{pct}pct_of_field': sum(1 for b in v if b >= budget_ms * pct / 100) / len(v)
                         for pct in (80, 90, 97.5)}}
    out['q1b_device_fill'] = q1b

    # ---- thread identities ----
    names = {}
    for r in records:
        if r['type'] == 'thread' and r.get('name'):
            names[(r['pid'], r['tid'])] = r['name']
    worker = {k for k, v in names.items() if v == WORKER}
    game = {k for k, v in names.items() if v == GAME}
    out['threads_found'] = {WORKER: len(worker), GAME: len(game)}

    # ---- Q2: functions per thread, slow vs normal ----
    base = [r for r in records if r['type'] in ('thread', 'module', 'process')]
    q2 = {}
    for c in ('slow', 'normal'):
        subset = [r for r in records if r['type'] == 'sample' and cls.get(field_at(r['t_ns'])) == c]
        rep = report.build(base + subset, [], set(common.ALLOWED_PROGRAMS))
        q2[c] = {}
        for t in rep['threads']:
            if t['name'] in (WORKER, GAME, 'gs-worker', 'GSRaster') and t['samples']:
                key = t['name']
                q2[c].setdefault(key, {'samples': 0, 'self': collections.Counter(), 'total': collections.Counter()})
                q2[c][key]['samples'] += t['samples']
                q2[c][key]['self'].update(dict(t['self']))
                q2[c][key]['total'].update(dict(t['total']))
        for key, v in q2[c].items():
            n = sum(1 for f, k in cls.items() if k == c) or 1
            v['ms_on_cpu_per_field'] = v['samples'] / n          # 1 sample = 1 ms
            v['self'] = v['self'].most_common(15)
            v['total'] = v['total'].most_common(15)
    out['q2_functions'] = q2

    # ---- Q3: idle gap: worker busy vs game thread waiting for it (and for the GS worker, DBZIW) ----
    waits = collections.defaultdict(int)
    wstart = {}
    for m in marks:
        if m['event'] == WSTART:
            wstart[(m['tid'], m['seq'])] = m['t_ns']
        elif m['event'] == WEND and (m['tid'], m['seq']) in wstart:
            t0 = wstart.pop((m['tid'], m['seq']))
            f = field_at(t0)
            if f is not None:
                waits[f] += m['t_ns'] - t0
    gs_waits = collections.defaultdict(int)
    gstart = {}
    for m in marks:
        if m['event'] == GSTART:
            gstart[m['tid']] = m['t_ns']
        elif m['event'] == GEND and m['tid'] in gstart:
            t0 = gstart.pop(m['tid'])
            f = field_at(t0)
            if f is not None:
                gs_waits[f] += m['t_ns'] - t0
    game_samples = collections.Counter(field_at(r['t_ns']) for r in records
                                       if r['type'] == 'sample' and (r['pid'], r['tid']) in game)
    q3 = {}
    for c in ('slow', 'normal'):
        fl = [f for f, k in cls.items() if k == c]
        n = len(fl) or 1
        lengths = [(fstart[f + 1] - fstart[f]) / 1e6 for f in fl if f + 1 in fstart]
        q3[c] = {'field_ms_mean': sum(lengths) / len(lengths) if lengths else None,
                 'device_busy_ms': sum(per_field[f]['busy_ns'] for f in fl) / n / 1e6,
                 'game_thread_waiting_for_device_ms': sum(waits[f] for f in fl) / n / 1e6,
                 'waiting_for_gs_worker_ms': sum(gs_waits[f] for f in fl) / n / 1e6,
                 'game_thread_on_cpu_ms': sum(game_samples[f] for f in fl) / n}
    out['q3_idle_gap'] = q3

    # ---- Q3c: device idle gaps (queue empty -> next submit), kick phase, game thread's work inside the gaps ----
    last_sub, gaps, gap_start = 0, [], None
    for m in sorted((m for m in marks if m['event'] in (SUBMIT, JEND)), key=lambda m: m['t_ns']):
        if m['event'] == SUBMIT:
            if gap_start is not None:
                gaps.append((gap_start, m['t_ns']))
                gap_start = None
            last_sub = max(last_sub, m['seq'])
        elif m['seq'] >= last_sub:
            gap_start = m['t_ns']
    gstarts = [g[0] for g in gaps]

    def gap_at(t):
        i = bisect.bisect_right(gstarts, t) - 1
        return i if i >= 0 and t < gaps[i][1] else None

    first_submit = {}
    for m in marks:
        if m['event'] == SUBMIT:
            f = field_at(m['t_ns'])
            if f is not None and f not in first_submit:
                first_submit[f] = (m['t_ns'] - fstart[f]) / 1e6
    in_gap = {'slow': [], 'normal': []}
    for r in records:
        if r['type'] == 'sample' and (r['pid'], r['tid']) in game:
            i = gap_at(r['t_ns'])
            c = cls.get(field_at(gaps[i][0])) if i is not None else None
            if c:
                in_gap[c].append(r)
    q3c = {}
    for c in ('slow', 'normal'):
        n = sum(1 for k in cls.values() if k == c) or 1
        gl = sorted((b - e) / 1e6 for e, b in gaps if cls.get(field_at(e)) == c)
        fsub = sorted(v for f, v in first_submit.items() if cls.get(f) == c)
        rep = report.build(base + in_gap[c], [], set(common.ALLOWED_PROGRAMS))
        gt = next((t for t in rep['threads'] if t['name'] == GAME and t['samples']), None)
        q3c[c] = {'gaps_per_field': len(gl) / n, 'gap_ms_per_field': sum(gl) / n,
                  'gap_ms_median': gl[len(gl) // 2] if gl else None, 'gap_ms_p90': gl[int(len(gl) * .9)] if gl else None,
                  'first_submit_after_field_start_ms_median': fsub[len(fsub) // 2] if fsub else None,
                  'game_thread_cpu_ms_per_field_in_gaps': (gt['samples'] / n) if gt else 0.0,
                  'game_thread_in_gaps_self': gt['self'][:10] if gt else [],
                  'game_thread_in_gaps_total': gt['total'][:20] if gt else []}
    out['q3c_device_gaps'] = q3c

    # ---- Q3b: what each thread waits for (the full report's waits: switched out Waiting -> woken) ----
    q3b = {}
    rep_path = os.path.join(a.cap, 'report.json')
    if os.path.exists(rep_path):
        full = json.load(open(rep_path, encoding='utf-8'))
        for th in full['threads']:
            if th['name'] not in (GAME, WORKER, 'gs-worker'):
                continue
            agg = {c: collections.defaultdict(lambda: [0, 0.0]) for c in ('slow', 'normal')}
            for w in th['waits']:
                c = cls.get(field_at(w['start_ns']))
                if not c:
                    continue
                frames = [f for f in (w.get('waker_stack') or []) if f != '[kernel]' and not f.startswith('<')][:3]
                e = agg[c][(w['waker_thread'] or w['waker_process'] or '?', ' < '.join(frames))]
                e[0] += 1
                e[1] += (w['end_ns'] - w['start_ns']) / 1e6
            q3b[th['name']] = {}
            for c in ('slow', 'normal'):
                n = sum(1 for k in cls.values() if k == c) or 1
                top = sorted(agg[c].items(), key=lambda kv: -kv[1][1])[:6]
                q3b[th['name']][c] = {'waiting_ms_per_field': sum(v[1] for v in agg[c].values()) / n,
                                      'by_waker': [{'waker': k[0], 'waker_frames': k[1], 'ms_per_field': v[1] / n,
                                                    'per_field': v[0] / n} for k, v in top]}
        del full
    out['q3b_waits_by_waker'] = q3b

    # ---- Q4: interruptions (switched out while still runnable) ----
    sw = sorted((r for r in records if r['type'] == 'switch'), key=lambda r: r['t_ns'])
    out_since = {}
    inter = {c: collections.defaultdict(lambda: {'count': 0, 'ms': 0.0, 'by': collections.Counter()}) for c in ('slow', 'normal')}
    for s in sw:
        old, new = (s['old_pid'], s['old_tid']), (s['new_pid'], s['new_tid'])
        if (old in worker or old in game) and s['old_state'] != 'Waiting':
            out_since[old] = (s['t_ns'], s['new_process'])
        if new in out_since:
            t0, by = out_since.pop(new)
            c = cls.get(field_at(t0))
            if c:
                e = inter[c][WORKER if new in worker else GAME]
                e['count'] += 1
                e['ms'] += (s['t_ns'] - t0) / 1e6
                e['by'][by] += 1
    q4 = {}
    for c, d in inter.items():
        n = sum(1 for f, k in cls.items() if k == c) or 1
        q4[c] = {k: {'per_field': v['count'] / n, 'ms_per_field': v['ms'] / n, 'by': v['by'].most_common(5)}
                 for k, v in d.items()}
    out['q4_interruptions'] = q4

    # ---- Q5: longest fields ----
    longest = sorted(((fstart[f + 1] - fstart[f]) / 1e6, f) for f in fields if f + 1 in fstart)[-10:][::-1]
    out['q5_longest_fields'] = [{'field': f, 'ms': ms, 'class': cls.get(f)} for ms, f in longest]

    common.write_json(os.path.join(a.cap, 'analysis.json'), out)
    lines = [f'analysis of {a.cap}', f'field rate {rate}; fields: {out["fields"]}; markers {len(marks)}; '
             f'threads {out["threads_found"]}', '',
             'Q1 work vs speed (per field):']
    for c, v in q1.items():
        npc = f'{v["ns_per_vu1_cycle"]:.2f}' if v['ns_per_vu1_cycle'] else '-'
        lines.append(f'  {c:6}: VU1 cycles {v["vu1_cycles_per_field"]:,.0f}, device busy {v["device_busy_ms_per_field"]:.2f} ms, '
                     f'{npc} ns/VU1 cycle, {v["jobs_per_field"]:.1f} jobs; top programs {v["top_programs_by_cycles"][:4]}')
    lines.append('Q1b device busy per field (job time split over fields):')
    for c, v in q1b.items():
        lines.append(f'  {c:6}: {v["fields"]} fields, median {v["median_ms"]:.1f} ms, p90 {v["p90_ms"]:.1f}; ' +
                     ', '.join(f'>={budget_ms * pct / 100:.1f} ms {v[f"share_at_least_{pct}pct_of_field"] * 100:.1f} %'
                               for pct in (80, 90, 97.5)))
    lines.append('Q2 functions (own time, samples):')
    for c in ('slow', 'normal'):
        for key, v in q2[c].items():
            lines.append(f'  {c:6} {key}: {v["ms_on_cpu_per_field"]:.1f} ms CPU/field; ' +
                         ', '.join(f'{fn} {n}' for fn, n in v['self'][:6]))
    lines.append('Q3 idle gap (per field):')
    for c, v in q3.items():
        lines.append(f'  {c:6}: field {v["field_ms_mean"] or 0:.2f} ms, device busy {v["device_busy_ms"]:.2f}, '
                     f'game thread waits for device {v["game_thread_waiting_for_device_ms"]:.2f}, '
                     f'waits for GS worker {v["waiting_for_gs_worker_ms"]:.2f}, game thread on CPU {v["game_thread_on_cpu_ms"]:.1f}')
    lines.append('Q3c device idle gaps (queue empty -> next submit), per field:')
    for c, v in q3c.items():
        lines.append(f'  {c:6}: {v["gaps_per_field"]:.2f} gaps, {v["gap_ms_per_field"]:.2f} ms (median {v["gap_ms_median"] or 0:.2f}, '
                     f'p90 {v["gap_ms_p90"] or 0:.2f}); first submit {v["first_submit_after_field_start_ms_median"] or 0:.2f} ms '
                     f'after field start (median); game thread on CPU in gaps {v["game_thread_cpu_ms_per_field_in_gaps"]:.2f} ms')
        lines.append('      with callers: ' + ', '.join(f'{fn} {k}' for fn, k in v['game_thread_in_gaps_total'][:12]
                                                    if fn != '[kernel]' and not fn.startswith('<') and 'thread_proxy' not in fn))
    lines.append('Q3b waits by waker (per field; waker thread: its top frames):')
    for name, d in q3b.items():
        for c, v in d.items():
            lines.append(f'  {c:6} {name}: waiting {v["waiting_ms_per_field"]:.2f} ms')
            for b in v['by_waker'][:4]:
                lines.append(f'      {b["ms_per_field"]:6.2f} ms {b["per_field"]:5.2f}x  {b["waker"]}: {b["waker_frames"][:150]}')
    lines.append('Q4 interruptions (switched out while runnable, per field):')
    for c, d in q4.items():
        for key, v in d.items():
            lines.append(f'  {c:6} {key}: {v["per_field"]:.2f} times, {v["ms_per_field"]:.3f} ms; by {v["by"]}')
    lines.append('Q5 longest fields: ' + ', '.join(f'{x["field"]} {x["ms"]:.1f} ms ({x["class"]})' for x in out['q5_longest_fields']))
    open(os.path.join(a.cap, 'analysis.txt'), 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
