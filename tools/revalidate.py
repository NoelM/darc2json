# Compare a fresh match (stops.json from match.py) with the current mapping and merge.
# usage: revalidate.py current.json fresh.json merged.json [audit.json]
# Rules: a fresh confident match wins. With audit.json (match.py's 6th argument), a current pair
# whose own stop+line agreed in under 20% of at least 5 snapshots is evicted unless re-matched.
# Pairs with no evidence either way are kept.
import json, sys
cur, new = json.load(open(sys.argv[1])), json.load(open(sys.argv[2]))
audit = json.load(open(sys.argv[4])) if len(sys.argv) > 4 else {}
# same stop point and line = same pair; the direction name follows each bus's destination and varies
ident = lambda e: (e.get('stop_id'), e.get('line_ref')) if e.get('line_ref') else (e['stop'], e['line'], e['direction'])
same = lambda a, b: ident(a) == ident(b)
ok = [k for k in cur if k in new and same(cur[k], new[k])]
changed = [k for k in cur if k in new and not same(cur[k], new[k])]
evicted = [k for k in cur if k not in new and k in audit and audit[k]['tried'] >= 5 and audit[k]['hits'] < 0.2 * audit[k]['tried']]
kept = [k for k in cur if k not in new and k not in evicted]
added = [k for k in new if k not in cur]
print(f"confirmed {len(ok)}, changed {len(changed)}, evicted {len(evicted)}, kept without new evidence {len(kept)}, new {len(added)}")
d = lambda e: f"{e['line']:>4} -> {e['direction'][:28]:28} at {e['stop']}"
for k in changed: print(f"  changed {k:8} was {d(cur[k])}\n  {'':16} now {d(new[k])}")
for k in evicted: print(f"  evicted {k:8} {d(cur[k])}  (agreed {audit[k]['hits']}/{audit[k]['tried']})")
for k in added: print(f"  new     {k:8} {d(new[k])}")
merged = {k: v for k, v in {**cur, **new}.items() if k not in evicted}
# a pair confirmed on another frequency keeps every frequency it was found on
for k in ok:
    fs = sorted(set(cur[k].get('freqs', [])) | set(new[k].get('freqs', [])), key=float)
    if fs: merged[k] = {**merged[k], 'freqs': fs}
json.dump(dict(sorted(merged.items())), open(sys.argv[3], 'w'), ensure_ascii=False, indent=1)
