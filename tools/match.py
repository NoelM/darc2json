# Match DARC (page, slot) boards against IDFM estimated-timetable polls.
# usage: match.py [--freq MHZ] blockreader_output.txt api_dir arrets.csv [paris|all] [tolerance_s] [current_mapping.json]
#   --freq: FM frequency of the recording, stored in each entry's "freqs" list
#   arrets.csv: IDFM stop reference (data.iledefrance-mobilites.fr, dataset "arrets", ';' separated)
#   paris (default): only bus stops whose commune is Paris; all: every bus stop
# Prints the matches and writes them to stops.json in the current directory.
# With a current mapping, also writes audit.json: for each mapped pair, how many snapshots
# its own stop+line agreed with the slot ("hits") out of those where the slot showed two times ("tried").
# Score of a (slot, stop+line) pair = number of distinct vehicle journeys of that stop+line
# whose expected time the slot showed (as next or following) within the tolerance.
import sys, re, gzip, csv, glob, collections, bisect
FREQ = None
if '--freq' in sys.argv:
    i = sys.argv.index('--freq'); FREQ = f'{float(sys.argv[i + 1]):g}'; del sys.argv[i:i + 2]
PARIS = (sys.argv[4] if len(sys.argv) > 4 else 'paris') == 'paris'
TOL = int(sys.argv[5]) if len(sys.argv) > 5 else 30
stops = {r['arrid']: (r['arrname'], r['arrtown'], r['arrpostalregion'])
         for r in csv.DictReader(open(sys.argv[3], encoding='utf-8-sig'), delimiter=';')}
def stop_of(ref):   # 'STIF:StopPoint:Q:25410:' -> reference row
    return stops.get(ref.split(':')[3] if ref.count(':') >= 4 else '', ('?', '?', ''))

sod = lambda s: int(s[0:2])*3600 + int(s[3:5])*60 + int(s[6:8])
iso_sod = lambda s: sod(s[11:19]) if len(s) >= 19 else None   # '2026-11-14T16:48:02.000Z' -> UTC s of day

obs = collections.defaultdict(list)   # slot -> time-ordered (t, eta1, eta2); None when the word is not a time
rx = re.compile(r'^(\d\d:\d\d:\d\d)  p(\w{4}) s(\d+)  (.*)$')
word = re.compile(r'(\d\d:\d\d:\d\d) [+-] *\d+:\d\d /\d+|FF\w\w|-(?= |$)')
for l in open(sys.argv[1]):
    m = rx.match(l)
    if not m: continue
    e = [sod(w.group(1)) if w.group(1) else None for w in word.finditer(m.group(4))]
    assert len(e) == 2, l
    obs[(m.group(2), int(m.group(3)))].append((sod(m.group(1)), e[0], e[1]))

hit = collections.defaultdict(lambda: collections.defaultdict(list))   # slot -> key -> [(T, eta1-u1, eta2-u2)]
tried = collections.Counter()
meta = {}; seen = set(); nsnap = 0
for p in sorted(glob.glob(sys.argv[2] + '/*.csv.gz')):
    rows = csv.reader(gzip.open(p, 'rt'))
    stamp = next(rows)[1]
    if stamp in seen: continue      # the API only refreshes every 2 minutes
    seen.add(stamp); nsnap += 1; T = iso_sod(stamp)
    times = collections.defaultdict(list)
    for r in rows:
        t = iso_sod(r[11]) or iso_sod(r[12])
        if t is None or t < T - 30 or r[7] != 'BUS': continue
        if PARIS and not stop_of(r[10])[2].startswith('75'): continue
        key = (r[10], r[0])
        meta[key] = (r[1], r[5], r[7], r[6])
        times[key].append(t)
    bucket = collections.defaultdict(list)   # next expected time (10 s buckets) -> (next, following, key)
    for key, ts in times.items():
        ts.sort()
        if len(ts) >= 2: bucket[ts[0]//10].append((ts[0], ts[1], key))
    for slot, o in obs.items():
        i = bisect.bisect_right(o, T + 20, key=lambda r: r[0]) - 1
        if i < 0: continue
        _, e1, e2 = o[i]
        if e1 is None or e2 is None or e1 < T - 60: continue
        tried[slot] += 1
        for b in range((e1-TOL)//10, (e1+TOL)//10 + 1):
            for u1, u2, key in bucket.get(b, ()):
                if abs(u1 - e1) <= TOL and abs(u2 - e2) <= TOL: hit[slot][key].append((T, e1-u1, e2-u2))

res = []
for slot in tried:
    top = sorted(((len(h), k) for k, h in hit[slot].items()), reverse=True)[:2]
    if top: res.append((slot, top[0][1], top[0][0], top[1][0] if len(top) > 1 else 0, tried[slot], hit[slot][top[0][1]]))
ok = [r for r in res if r[2] >= 3 and r[2] >= 2*r[3] and r[2] >= 0.5*r[4]]
print(f"{nsnap} API snapshots, {len(obs)} slots seen, {len(tried)} with two times, tolerance {TOL} s")
print("best score distribution (snapshots matched: slots):", sorted(collections.Counter(r[2] for r in res).items()))
print(f"confident matches (>=3 snapshots, >= half of those tried, runner-up at most half): {len(ok)}")
for slot, k, n, n2, tr, h in sorted(ok):
    m = meta[k]
    d1 = sorted(x[1] for x in h)[len(h)//2]; d2 = sorted(x[2] for x in h)[len(h)//2]
    st = stop_of(k[0])
    print(f"p{slot[0]} s{slot[1]:02d}  {n:2d}/{tr:2d} (runner-up {n2})  diff {d1:+3d} s {d2:+3d} s  bus {m[0]:>4} -> {m[1][:30]:30} at {st[0][:32]:32} ({st[1]}, stop {k[0].split(':')[3]})")

# Mapping for the web app: {"PPPP:S": {"stop", "line", "direction", "stop_id"}}
import json
out = {f"{slot[0]}:{slot[1]}": {"stop": stop_of(k[0])[0], "line": meta[k][0], "direction": meta[k][1], "stop_id": k[0].split(':')[3], "line_ref": k[1].split(':')[3],
                                **({"freqs": [FREQ]} if FREQ else {})}
       for slot, k, n, n2, tr, h in sorted(ok)}
json.dump(out, open('stops.json', 'w'), ensure_ascii=False, indent=1)

if len(sys.argv) > 6:
    audit = {}
    for key, e in json.load(open(sys.argv[6])).items():
        page, n = key.split(':'); slot = (page, int(n))
        if 'line_ref' not in e or slot not in tried: continue
        k = (f"STIF:StopPoint:Q:{e['stop_id']}:", f"STIF:Line::{e['line_ref']}:")
        audit[key] = {"hits": len(hit[slot].get(k, ())), "tried": tried[slot]}
    json.dump(audit, open('audit.json', 'w'), indent=1)
