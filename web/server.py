#!/usr/bin/env python3
"""Live DARC bus-board viewer: rtl_fm (or a replay file) -> darcrx -> blockreader -> HTTP."""
import unicodedata, argparse, collections, json, os, re, signal, struct, subprocess, sys, threading, time
import urllib.request, urllib.parse
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from zoneinfo import ZoneInfo

import numpy as np
import darcrx

HERE = os.path.dirname(os.path.abspath(__file__))
STOPS = os.path.join(HERE, 'stops.json')
BLOCKREADER = os.path.join(HERE, '..', 'blockreader', 'blockreader')
RTL_FM = 'rtl_fm -M fm -l 0 -A std -p 0 -s 228k -g 19 -F 9 -f 101.5M'.split()
FS = darcrx.FS
SEG, OVL = 4 * FS, 1 * FS      # new samples per pass, extra samples so a block cut at the edge is seen whole
PARIS = ZoneInfo('Europe/Paris')
STARTED = time.time()
FREQ_MHZ = RTL_FM[RTL_FM.index('-f') + 1].rstrip('M')
API_URL = 'https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring?MonitoringRef=STIF:StopPoint:Q:%s:'
API_TTL, API_TIMEOUT = 60, 5
LINE = re.compile(r'(\d\d):(\d\d):(\d\d)\s+p([0-9A-F]{4}) s(\d+)\s+(.{19}) (.{19})\s*$')

lock = threading.Lock()
slots = {}            # "PPPP:S" -> dict(frame_sod, received, words=[word, word])
state = dict(mode='', speed=1.0, blocks=collections.deque(), total=0, last_block=0.0,
             frame_sod=None, frame_wall=0.0, error='', done=False)
procs = []
stop_event = threading.Event()


def parse_word(s):
    s = s.strip()
    if s == '-': return dict(empty=True)
    if s.startswith('FF'): return dict(status=s)
    m = re.match(r'(\d+):(\d+):(\d+) ([+-]) ?(\d+):(\d+) /(\d+)', s)
    h, mi, se, sign, wm, ws, flag = m.groups()
    return dict(sod=int(h) * 3600 + int(mi) * 60 + int(se), flag=int(flag))


def read_blockreader(pipe):
    for raw in pipe:
        m = LINE.match(raw.decode('ascii', 'replace'))
        if not m:
            continue
        h, mi, se, page, slot, a, b = m.groups()
        sod = int(h) * 3600 + int(mi) * 60 + int(se)
        try:
            words = [parse_word(a), parse_word(b)]
        except AttributeError:
            continue
        now = time.time()
        with lock:
            slots[f'{page}:{int(slot)}'] = dict(frame_sod=sod, received=now, words=words)
            state.update(frame_sod=sod, frame_wall=now)


def feed(source, out):
    """Cut the sample stream into overlapping segments, decode, write 44-byte records to blockreader."""
    t0 = time.time()
    buf = np.zeros(0, dtype=np.float32)
    consumed = 0                      # samples already dropped from buf
    last = -1.0
    while not stop_event.is_set():
        data = source.read(2 * FS // 4)
        if not data:
            break
        buf = np.concatenate([buf, np.frombuffer(data[:len(data) // 2 * 2], dtype='<i2').astype(np.float32)])
        while len(buf) >= SEG + OVL:
            for t, bic, by in darcrx.blocks(buf[:SEG + OVL]):
                t += consumed / FS
                if t <= last + 0.009: continue   # already decoded in the previous overlap
                last = t
                out.write(struct.pack('<QH', int((t0 + t) * 1e6), bic) + by)
                with lock:
                    state['total'] += 1
                    state['blocks'].append(time.time())
                    state['last_block'] = time.time()
            out.flush()
            buf = buf[SEG:]
            consumed += SEG


def replay_source(path, speed, start):
    f = open(path, 'rb')
    f.seek(int(start * FS) * 2)
    class R:
        def read(self, n):
            data = f.read(n)
            if data:
                stop_event.wait(len(data) / 2 / FS / speed)
            return data
    return R()


def run_chain(args):
    br = subprocess.Popen([BLOCKREADER, '/dev/stdin'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    procs.append(br)
    threading.Thread(target=read_blockreader, args=(br.stdout,), daemon=True).start()
    if args.replay:
        source = replay_source(args.replay, args.speed, args.start)
        state.update(mode='replay', speed=args.speed)
    else:
        rtl = subprocess.Popen(RTL_FM, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        procs.append(rtl)
        source = rtl.stdout
        state.update(mode='live')
    try:
        feed(source, br.stdin)
        if not args.replay and not stop_event.is_set():
            state['error'] = 'rtl_fm stopped (dongle unplugged or busy?)'
    except Exception as e:
        state['error'] = repr(e)
    state['done'] = True


def stream_now():
    """Seconds of day on the stream's clock, now."""
    if state['mode'] == 'live':
        return (time.time() % 86400)   # UTC; the stream clock is UTC
    if state['frame_sod'] is None:
        return None
    return state['frame_sod'] + (time.time() - state['frame_wall']) * state['speed']


def wrap(d):
    return (d + 43200) % 86400 - 43200


def api_state():
    now = time.time()
    with lock:
        blocks = state['blocks']
        while blocks and blocks[0] < now - 20: blocks.popleft()
        rate = len(blocks) / 20.0 / (state['speed'] if state['mode'] == 'replay' else 1)
        sn = stream_now()
        # wall time of the stream's "now": differs from ours in replay
        stream_wall = now + wrap(sn - now % 86400) if sn is not None else now
        out = []
        for key, s in sorted(slots.items()):
            cols = []
            for w in s['words']:
                w = dict(w)
                if 'sod' in w:
                    ahead = wrap(w['sod'] - sn)
                    w['seconds'] = ahead
                    w['minutes'] = int(ahead // 60)
                    w['local'] = datetime.fromtimestamp(stream_wall + ahead, PARIS).strftime('%H:%M')
                cols.append(w)
            out.append(dict(key=key, page=key[:4], slot=int(key[5:]), updated_ago=round(now - s['received']),
                            columns=cols))
        stops = load_stops()
        recv = dict(mode=state['mode'], blocks_per_s=round(rate, 1), total_blocks=state['total'],
                    last_block_ago=round(now - state['last_block'], 1) if state['last_block'] else None,
                    error=state['error'], finished=state['done'], uptime_s=round(now - STARTED),
                    freq_mhz=FREQ_MHZ if state['mode'] == 'live' else None,
                    slots_received=len(slots),
                    slots_with_times=sum(any('sod' in w for w in s['words']) for s in slots.values()),
                    identified_total=len(stops), identified_received=sum(k in slots for k in stops),
                    api_available=bool(os.environ.get('IDFM_API_KEY')))
        if state['mode'] == 'live' and state['frame_sod'] is not None:
            # stream clock of the last decoded frame minus our clock when it arrived (includes ~5-8 s pipeline lag)
            recv['clock_offset_s'] = wrap(state['frame_sod'] - state['frame_wall'] % 86400)
        if sn is not None:
            recv['stream_time_local'] = datetime.fromtimestamp(stream_wall, PARIS).strftime('%H:%M:%S')
    return dict(receiver=recv, slots=out, stops=stops)


def norm(s):
    s = unicodedata.normalize('NFD', s or '')
    return re.sub(r'[^a-z0-9]+', ' ', ''.join(c for c in s if not unicodedata.combining(c)).lower()).strip()


api_cache = {}        # stop_id -> (fetched_at, visits list or None on failure)
api_lock = threading.Lock()
def stop_visits(stop_id):
    """MonitoredStopVisit list for a stop point, cached API_TTL s (failures too); None if unavailable."""
    with api_lock:
        hit = api_cache.get(stop_id)
        if hit and time.time() - hit[0] < API_TTL: return hit[1]
        print('api fetch', stop_id, flush=True)
        try:
            req = urllib.request.Request(API_URL % urllib.parse.quote(stop_id, safe=''),
                                         headers={'apiKey': os.environ['IDFM_API_KEY']})
            with urllib.request.urlopen(req, timeout=API_TIMEOUT) as r:
                d = json.load(r)
            visits = d['Siri']['ServiceDelivery']['StopMonitoringDelivery'][0]['MonitoredStopVisit']
        except Exception as e:
            print('api error', stop_id, type(e).__name__, flush=True)
            visits = None
        api_cache[stop_id] = (time.time(), visits)
        return visits


def first(lst):
    return lst[0]['value'] if lst else ''


def compare(keys):
    """Per watched slot key with a stop_id: the API's next two times on the same clock as the radio's."""
    stops = load_stops()
    with lock:
        sn = stream_now()
        now = time.time()
        stream_wall = now + wrap(sn - now % 86400) if sn is not None else now
    out = {}
    for k in keys:
        e = stops.get(k) or {}
        if not e.get('stop_id'): continue
        if not os.environ.get('IDFM_API_KEY'): out[k] = dict(status='nokey'); continue
        visits = stop_visits(e['stop_id'])
        if visits is None: out[k] = dict(status='unavailable'); continue
        want = norm(e.get('direction'))
        if not want: out[k] = dict(status='nodirection'); continue
        hits = []
        for v in visits:
            j = v['MonitoredVehicleJourney']
            names = [norm(first(j.get('DirectionName'))), norm(first(j.get('DestinationName')))]
            c = j.get('MonitoredCall', {})
            t = c.get('ExpectedArrivalTime') or c.get('ExpectedDepartureTime')
            if t and want in names:
                ahead = datetime.fromisoformat(t.replace('Z', '+00:00')).timestamp() - stream_wall
                hits.append((ahead, j['LineRef']['value']))
        hits.sort()
        hits = [h for h in hits if h[0] >= 0]
        times = [dict(seconds=a, minutes=int(a // 60), local=datetime.fromtimestamp(stream_wall + a, PARIS).strftime('%H:%M'))
                 for a, _ in hits[:2]]
        # shortcut: the API gives no public line number, so two lines with the same direction name at one stop mix
        out[k] = dict(status='ok' if times else 'none', times=times, ambiguous=len({l for _, l in hits}) > 1)
    return out


_stops_cache = (None, {})
def load_stops():
    global _stops_cache
    try:
        mt = os.stat(STOPS).st_mtime_ns
        if mt != _stops_cache[0]:
            with open(STOPS) as f:
                _stops_cache = (mt, json.load(f))
    except (OSError, ValueError):
        pass   # keep the last good copy if the file is mid-write or invalid
    return _stops_cache[1]


stops_lock = threading.Lock()
def edit_stops(key, entry):
    """Set (entry dict) or delete (None) one key; every other key is kept as read from disk."""
    with stops_lock:
        with open(STOPS) as f:
            data = json.load(f)
        if entry is None: data.pop(key, None)
        else: data[key] = entry
        tmp = STOPS + '.tmp'
        with open(tmp, 'w') as f:
            json.dump(data, f, indent=2, ensure_ascii=False); f.write('\n')
        os.replace(tmp, STOPS)


KEY = re.compile(r'^[0-9A-F]{4}:(0|[1-9]\d*)$')

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def send(self, code, body, ctype='application/json'):
        if not isinstance(body, bytes): body = json.dumps(body, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header('Content-Type', ctype + '; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def key(self):
        k = self.path[len('/api/stops/'):] if self.path.startswith('/api/stops/') else ''
        return k if KEY.match(k) else None

    def do_GET(self):
        if self.path == '/api/state': self.send(200, api_state())
        elif self.path.startswith('/api/compare?'):
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            self.send(200, compare([k for k in ','.join(q.get('keys', [])).split(',') if KEY.match(k)]))
        elif self.path == '/api/stops': self.send(200, load_stops())
        elif self.path in ('/', '/index.html'):
            with open(os.path.join(HERE, 'index.html'), 'rb') as f: self.send(200, f.read(), 'text/html')
        else: self.send(404, {'error': 'not found'})

    def do_PUT(self):
        k = self.key()
        if not k: return self.send(400, {'error': 'key must be PPPP:S (4 uppercase hex digits, slot decimal)'})
        try:
            e = json.loads(self.rfile.read(int(self.headers.get('Content-Length', 0))))
            assert isinstance(e, dict) and isinstance(e.get('stop'), str) and e['stop'].strip()
            assert all(isinstance(v, str) for v in e.values()) and set(e) <= {'stop', 'line', 'direction', 'stop_id'}
        except (ValueError, AssertionError):
            return self.send(400, {'error': 'body must be an object with "stop" and optional line/direction/stop_id strings'})
        edit_stops(k, e)
        self.send(200, {'ok': True})

    def do_DELETE(self):
        k = self.key()
        if not k: return self.send(400, {'error': 'bad key'})
        edit_stops(k, None)
        self.send(200, {'ok': True})


def shutdown(*_):
    stop_event.set()
    for p in procs:
        if p.poll() is None: p.kill()
    for p in procs:
        p.wait()
    os._exit(0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--replay', metavar='FILE', help='int16 LE 228 kHz mono samples to play instead of rtl_fm')
    ap.add_argument('--speed', type=float, default=1.0, help='replay speed factor (default 1 = real time)')
    ap.add_argument('--start', type=float, default=0.0, help='replay start offset in seconds')
    ap.add_argument('--port', type=int, default=8080)
    args = ap.parse_args()
    signal.signal(signal.SIGINT, shutdown); signal.signal(signal.SIGTERM, shutdown)
    threading.Thread(target=run_chain, args=(args,), daemon=True).start()
    srv = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    print(f'http://127.0.0.1:{args.port}/ ({"replay" if args.replay else "live"})', flush=True)
    try:
        srv.serve_forever()
    finally:
        shutdown()


if __name__ == '__main__':
    main()
