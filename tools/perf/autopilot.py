import time, math, sys
from d import c

def words(a, n):
    out = []
    while n > 0:
        k = min(n, 256)
        out += [int(x, 16) for x in c(cmd='mem_words', addr=hex(a), count=k)['words']]
        a += 4 * k; n -= k
    return out

def s32(v): return v - (1 << 32) if v & 0x80000000 else v
def wrap(a): return (a + math.pi) % (2 * math.pi) - math.pi

class AP:
    """Closed-loop driver: centreline from RAM, car heading from camera yaw
    (0x1F80001C, 4096/turn) calibrated online against motion; reverses out
    when pinned against a wall."""
    def __init__(s):
        s.stop = False; s.load()
        s.cal = {1: [0.0, 0.0], -1: [0.0, 0.0]}   # circular sums of offset
        s.stuck = 0; s.reverse = 0
    def load(s):
        n, = words(0x800AC084, 1); tab, = words(0x800AC05C, 1)
        raw = words(tab, n * 15); s.n = n
        s.pts = [(s32(raw[i * 15 + 2]), s32(raw[i * 15 + 3])) for i in range(n)]
        s.seg = -1; s.prev = None
    def heading(s, yaw, hx, hz, moving):
        y = yaw * 2 * math.pi / 4096
        if moving:
            h = math.atan2(hx, hz)
            for sg in (1, -1):
                o = wrap(h - sg * y)
                s.cal[sg][0] += math.cos(o); s.cal[sg][1] += math.sin(o)
        best = max((1, -1), key=lambda sg: math.hypot(*s.cal[sg]))
        cx, cy = s.cal[best]
        if math.hypot(cx, cy) < 3:   # not calibrated yet: use motion/track
            return None
        return wrap(best * y + math.atan2(cy, cx))
    def step(s):
        w = [s32(v) for v in words(0x1F800008, 6)]
        cx, cz, yaw = w[0] / 4, w[2] / 4, w[5]
        rng = range(s.n) if s.seg < 0 else range(s.seg - 20, s.seg + 21)
        best = min(rng, key=lambda k: (s.pts[k % s.n][0] - cx) ** 2 + (s.pts[k % s.n][1] - cz) ** 2) % s.n
        s.seg = best; i = (best - 6) % s.n
        a = s.pts[i]; b = s.pts[(i - 1) % s.n]
        dx, dz = b[0] - a[0], b[1] - a[1]; L = math.hypot(dx, dz) or 1
        off = float(__import__('os').environ.get('AP_OFFSET', '0'))
        # right-hand normal of the travel direction, scaled by segment spacing
        t = (a[0] + dz / L * off * L, a[1] - dx / L * off * L)
        hx = hz = 0.0; moving = False
        if s.prev:
            hx, hz = cx - s.prev[0], cz - s.prev[1]; moving = hx * hx + hz * hz > 4
        s.prev = (cx, cz)
        h = s.heading(yaw, hx, hz, moving)
        if h is None:
            if moving: h = math.atan2(hx, hz)
            else:
                a = s.pts[(best - 1) % s.n]; b = s.pts[best]; h = math.atan2(a[0] - b[0], a[1] - b[1])
        err = wrap(math.atan2(t[0] - cx, t[1] - cz) - h)
        # stuck: not moving for ~1 s while throttling -> reverse with opposite lock
        s.stuck = 0 if moving else s.stuck + 1
        if s.reverse > 0:
            s.reverse -= 1
            b = 0xFFFF & ~(1 << 15)            # square = brake/reverse
            b &= ~(1 << 7) if err > 0 else ~(1 << 5)   # opposite lock while reversing
        else:
            if s.stuck > 30: s.reverse = 25; s.stuck = 0
            b = 0xBFFF
            if err > 0.05: b = 0xBFDF
            elif err < -0.05: b = 0xBF7F
            if abs(err) > 0.5 and moving: b |= 0x4000
        c(cmd='set_input', buttons='0x%04X' % b)
        return best, err
    def run(s, secs):
        end = time.time() + secs; log = []
        while time.time() < end and not s.stop:
            log.append(s.step()); time.sleep(0.03)
        c(cmd='clear_input')
        return log

if __name__ == '__main__':
    ap = AP(); print('segments', ap.n)
    log = ap.run(float(sys.argv[1]))
    print([x[0] for x in log][::30])
