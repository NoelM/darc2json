# Matching DARC boards to real stops

Scripts used to tie each (page, slot) of the DARC arrival boards to a bus line and stop.

1. Record the FM multiplex (one frequency at a time), e.g. 22 minutes:

       rtl_fm -M fm -l 0 -A std -p 0 -s 228k -g 19 -F 9 -f 101.5M > mpx.raw

2. While recording, log the IDFM real-time API once a minute (needs `IDFM_API_KEY`):

       mkdir api && python3 poll.py 22

3. Decode the recording (needs numpy and scipy), then the boards:

       python3 darcrx.py mpx.raw blocks.bin
       ../blockreader/blockreader blocks.bin > boards.txt

4. Match. `arrets.csv` is the IDFM stop reference (dataset "arrets" on
   data.iledefrance-mobilites.fr, CSV export with `;`):

       python3 match.py boards.txt api arrets.csv all 15

`stops.json` and `matches_101500k.txt` are the result for 101.5 MHz (370 slots).

`darcrx.py` replaces the demodulator in `src/` for weak signals: coherent MSK
demodulation, and error correction from the block parity using the least
reliable symbols.
