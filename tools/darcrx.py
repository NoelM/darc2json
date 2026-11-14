# Prototype DARC receiver: FM-demodulated MPX (int16, 228 kHz) -> 44-byte records (same as darc2json -o)
import sys, struct, itertools
import numpy as np
from scipy import signal

FS = 228000; SPS = 16
BICS = [0x135E, 0x74A6, 0xA791, 0xC875]
SEQ = [0xafaa,0x814a,0xf2ee,0x073a,0x4f5d,0x4486,0x70bd,0xb343,0xbc3f,0xe0f7,0xc5cc,0x8253,0xb479,0xf362,0xa471,0xb571,0x3110,0x0846,0x1390]
SCR = np.array([(w >> (15 - b)) & 1 for w in SEQ for b in range(16)][:272], dtype=np.uint8)
def poly(c): return sum(1 << i for i in c)
G82 = poly([82,77,76,71,67,66,56,52,48,40,36,34,24,22,18,10,4,0])
G14 = poly([14,11,2,0])
def xmod(deg_list_len, g, gdeg):
    # r[i] = x^(n-1-i) mod g  (bit i is the coefficient of x^(n-1-i))
    n = deg_list_len; r = [0]*n; v = 1
    for i in range(n-1, -1, -1):
        r[i] = v; v <<= 1
        if v >> gdeg & 1: v ^= g
    return r
SYN = xmod(272, G82, 82)
CRC = xmod(190, G14, 14)

def demod(x):
    n = np.arange(len(x))
    z = x * np.exp(-2j*np.pi*76000/FS*n)
    z = signal.resample_poly(z, 64, 57)                      # 256 kHz = 16 samples/bit
    y = np.convolve(z, np.sin(np.pi*np.arange(1, 2*SPS)/(2*SPS)), 'same')   # half-sine matched filter
    w = y*y; m = np.arange(len(w))
    hop = 512; L = 4096
    def box(u):  # centred moving sum of length L, every hop samples
        cs = np.concatenate(([0], np.cumsum(u)))
        i = np.arange(0, len(u), hop)
        return cs[np.minimum(i + L//2, len(u))] - cs[np.maximum(i - L//2, 0)]
    Wp = box(w*np.exp(-1j*np.pi*m/SPS))
    Wm = box(w*np.exp(+1j*np.pi*m/SPS))
    tau = np.unwrap(np.angle(Wm*np.conj(Wp)))*SPS/(2*np.pi)  # timing, in samples, continuous
    th2 = np.unwrap(np.angle(Wp*np.exp(1j*np.pi*tau/SPS)))   # 2*carrier phase
    tw = m[::hop]
    k = np.arange(int((len(y) - 64)/SPS) - 2)
    t = SPS*k + np.interp(SPS*k, tw, tau)
    for _ in range(2): t = SPS*k + np.interp(t, tw, tau)
    t = t[(t > 1) & (t < len(y) - 2)]
    k = np.round((t - np.interp(t, tw, tau))/SPS).astype(int)
    i0 = np.floor(t).astype(int); f = t - i0
    c = y[i0]*(1-f) + y[i0+1]*f
    r = c*np.exp(-0.5j*np.interp(t, tw, th2))
    a = np.where(k % 2 == 0, r.real, r.imag)
    a /= np.median(np.abs(a))
    return a, t*57/64/FS                                     # soft symbols, time in seconds

def bits_from(a, variant):
    par = (np.arange(len(a)) + (variant & 1)) % 2
    d = (a[1:]*a[:-1] > 0) ^ (par[1:] == 1) ^ bool(variant & 2)
    return d.astype(np.uint8)

def syndrome(bits):
    s = 0
    for i in np.flatnonzero(bits): s ^= SYN[i]
    return s

def crc_ok(bits):
    s = 0
    for i in np.flatnonzero(bits[:190]): s ^= CRC[i]
    return s == 0

def decode_block(d, rel, nflip=10):
    """d: 272 descrambled bits; rel: reliability of the 273 symbols they are differences of."""
    s = syndrome(d)
    if s == 0: return (d, 0) if crc_ok(d) else (None, 0)
    # a wrong symbol j flips bits j-1 and j (bit i = symbol i * symbol i+1)
    cand = np.argsort(rel)[:nflip]
    es = []
    for j in cand:
        e = 0
        if j >= 1: e ^= SYN[j-1]
        if j < 272: e ^= SYN[j]
        es.append(e)
    for nf in range(1, 7):
        for combo in itertools.combinations(range(nflip), nf):
            x = s
            for q in combo: x ^= es[q]
            if x == 0:
                d = d.copy()
                for q in combo:
                    j = cand[q]
                    if j >= 1: d[j-1] ^= 1
                    if j < 272: d[j] ^= 1
                return (d, nf) if crc_ok(d) else (None, nf)
    return None, -1

NEAR = {}
for _b in BICS:
    NEAR[_b] = _b
    for _i in range(16):
        NEAR[_b ^ 1 << _i] = _b
        for _j in range(_i): NEAR[_b ^ 1 << _i ^ 1 << _j] = _b   # BIC with <= 2 bit errors

def blocks(x):
    """Yield (time in s, bic, 34 bytes) for every block decoded in this stretch of MPX."""
    a, ts = demod(x)
    best = None
    for v in (0, 1):  # the two symbol-parity alignments; the other two variants are equivalent
        d = bits_from(a, v)
        wv = np.zeros(len(d) - 15, dtype=np.uint32)
        for b in range(16): wv = (wv << 1) | d[b:len(d)-15+b]   # 16-bit sliding word, MSB first
        hits = np.flatnonzero(np.isin(wv, BICS)); hs = set(hits.tolist())
        good = sum(1 for h in hits if h + 288 in hs)
        if best is None or good > best[0]: best = (good, d, wv)
    _, d, wv = best
    pos = 0; N = len(wv)
    while pos + 288 + 1 < N:
        w = int(wv[pos])
        if w not in NEAR: pos += 1; continue
        res, nf = decode_block(d[pos+16:pos+288] ^ SCR, np.abs(a[pos+16:pos+289]))
        if res is None: pos += 1; continue
        yield ts[pos+288], NEAR[w], np.packbits(res, bitorder='little').tobytes()
        pos += 288

def main():
    # usage: darcrx.py mpx.raw out.bin [unix start time] [start s] [end s]
    out = open(sys.argv[2], 'wb')
    t0 = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
    seg, ovl = 30*FS, 1*FS
    f = np.memmap(sys.argv[1], dtype='<i2', mode='r')
    lo = int(float(sys.argv[4])*FS) if len(sys.argv) > 4 else 0
    hi = min(len(f), int(float(sys.argv[5])*FS)) if len(sys.argv) > 5 else len(f)
    last = -1.0; n = 0
    for start in range(lo, hi, seg):
        x = np.asarray(f[start:min(start + seg + ovl, len(f))], dtype=np.float32)
        if len(x) < FS: break
        for t, bic, by in blocks(x):
            t += start/FS
            if t <= last + 0.009: continue   # block already decoded in the previous segment's overlap (a block lasts 18 ms)
            last = t; n += 1
            out.write(struct.pack('<QH', int((t0 + t)*1e6), bic) + by)
    print(f"{n} blocks in {(hi-lo)/FS:.0f} s = {n/((hi-lo)/FS):.1f}/s (max 55.6)", file=sys.stderr)

if __name__ == '__main__': main()
