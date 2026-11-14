# DARC bus arrivals – live viewer

Runs `rtl_fm` → `darcrx.py` (demodulator, copied unchanged from the prototype) → `../blockreader/blockreader`
and serves a page showing next arrivals for chosen (page, slot) pairs.

## Install
Needs `rtl_fm` (Homebrew `rtl-sdr`), a C compiler and Python 3.9+.

    make -C blockreader                 # from the repo root
    python3 -m venv web/venv && web/venv/bin/pip install -r web/requirements.txt

## Run
Live (dongle must be free; the server starts and stops `rtl_fm` itself, 101.5 MHz):

    web/venv/bin/python web/server.py            # then open http://127.0.0.1:8080/

Replay a raw capture at real-time speed, no dongle. The file is any capture you made yourself with

    rtl_fm -M fm -l 0 -A std -p 0 -s 228k -g 19 -F 9 -f 101.5M > capture.raw

(int16 LE, 228 kHz, mono):

    web/venv/bin/python web/server.py --replay capture.raw [--start SECONDS] [--speed 1] [--port 8080]

In replay, "now" is the stream's own clock, so minutes-to-arrival make sense against the recording.
Display lag is about 5–8 s (4 s segments). Stop with Ctrl-C.

## API
`GET /api/state` (receiver status, all slots, stops), `GET /api/stops`, `PUT /api/stops/PPPP:S` (JSON body),
`DELETE /api/stops/PPPP:S`. Watched slots are kept in the browser (localStorage).

"Last change" on a slot is when its content last changed: blockreader prints a slot only when it changes,
so an unchanged slot that is rebroadcast every ~30 s does not refresh it. Arrival minutes stay correct regardless.

## Mapping: `web/stops.json`
Object keyed `"PPPP:S"` (page = 4 uppercase hex digits, slot = decimal, no padding):

    { "0015:0": { "stop": "Gare de Lyon", "line": "20", "direction": "Gare Saint-Lazare", "stop_id": "123" } }

`stop` is required; `line`, `direction`, `stop_id` optional. The file is reloaded when it changes on disk;
edits through the page or API change only the key edited.
