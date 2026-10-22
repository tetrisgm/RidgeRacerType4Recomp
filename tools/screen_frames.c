/* screen_frames.c - count distinct frames that actually reach the display.
 *
 * Uses DXGI Desktop Duplication on the primary output, which sees borderless
 * and fullscreen OpenGL windows (GDI capture does not). For SECONDS it waits
 * for desktop updates, copies NREG patches (W x H) spread across the screen,
 * hashes them and records when the visible content changed. Prints distinct
 * frames per second, the largest gaps between distinct frames and a CSV of
 * updates (gap -1) and changes. A mode switch or lost access rebuilds the
 * capture.
 *
 * Build (MSYS2 mingw64):
 *   gcc -O2 -o screen_frames.exe tools/screen_frames.c -ld3d11 -ldxgi -luuid
 * Usage: screen_frames SECONDS OUT.csv [W H [DUMP_PREFIX]]
 * With DUMP_PREFIX, the centre patch of 48 consecutive distinct frames from
 * t = 10 s is written as PREFIX_NN.ppm (motion-order checks).
 */
#define _WIN32_WINNT 0x0A00
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NREG 5   /* hashed patches per frame */

static double now_s(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

static uint64_t fnv(const uint8_t *p, size_t n, uint64_t h)
{
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

typedef struct Capture {
    IDXGIOutputDuplication *dup;
    ID3D11Texture2D *stage;
    D3D11_BOX box[NREG];
    int sw, sh, rw, rh;
} Capture;

static void capture_release(Capture *c)
{
    if (c->dup) IDXGIOutputDuplication_Release(c->dup);
    if (c->stage) ID3D11Texture2D_Release(c->stage);
    c->dup = NULL; c->stage = NULL;
}

/* Patches across the screen: one patch can sit on sky, a wall or flat
 * asphalt that barely changes between a frame and its neighbour. */
static int capture_open(Capture *c, ID3D11Device *dev, IDXGIOutput1 *out, int rw, int rh)
{
    static const double reg[NREG][2] = {{.5, .5}, {.25, .35}, {.75, .35}, {.25, .7}, {.75, .7}};
    capture_release(c);
    if (FAILED(IDXGIOutput1_DuplicateOutput(out, (IUnknown *)dev, &c->dup))) { c->dup = NULL; return 0; }
    DXGI_OUTDUPL_DESC dd;
    IDXGIOutputDuplication_GetDesc(c->dup, &dd);
    c->sw = (int)dd.ModeDesc.Width; c->sh = (int)dd.ModeDesc.Height;
    c->rw = rw * 4 > c->sw ? c->sw / 4 : rw;
    c->rh = rh * 4 > c->sh ? c->sh / 4 : rh;
    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = (UINT)(c->rw * NREG); td.Height = (UINT)c->rh; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(ID3D11Device_CreateTexture2D(dev, &td, NULL, &c->stage))) { capture_release(c); return 0; }
    for (int i = 0; i < NREG; i++) {
        UINT cx = (UINT)(reg[i][0] * c->sw), cy = (UINT)(reg[i][1] * c->sh);
        c->box[i] = (D3D11_BOX){cx - c->rw / 2, cy - c->rh / 2, 0,
                                cx - c->rw / 2 + c->rw, cy - c->rh / 2 + c->rh, 1};
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s SECONDS OUT.csv [W H]\n", argv[0]); return 2; }
    double secs = atof(argv[1]);
    int rw = argc > 4 ? atoi(argv[3]) : 480, rh = argc > 4 ? atoi(argv[4]) : 270;
    const char *dump = argc > 5 ? argv[5] : NULL;
    int dumped = 0;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                                 D3D11_SDK_VERSION, &dev, &fl, &ctx))) return 3;
    IDXGIDevice *xd; IDXGIAdapter *ad; IDXGIOutput *out; IDXGIOutput1 *out1;
    ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&xd);
    IDXGIDevice_GetAdapter(xd, &ad);
    if (FAILED(IDXGIAdapter_EnumOutputs(ad, 0, &out))) return 4;
    IDXGIOutput_QueryInterface(out, &IID_IDXGIOutput1, (void **)&out1);

    FILE *csv = fopen(argv[2], "w");
    if (!csv) return 7;
    fprintf(csv, "t_s,gap_ms,accumulated\n");
    double t0 = now_s(), last = -1, end = t0 + secs;
    uint64_t prev = 0;
    long updates = 0, distinct = 0, accumulated = 0, opens = 0;
    static long per_sec[3600];
    double gaps[16] = {0};
    Capture cap = {0};
    while (now_s() < end) {
        if (!cap.dup) {
            if (!capture_open(&cap, dev, out1, rw, rh)) { Sleep(50); continue; }
            opens++;
        }
        DXGI_OUTDUPL_FRAME_INFO fi; IDXGIResource *res;
        HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(cap.dup, 100, &fi, &res);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) continue;
        if (FAILED(hr)) { capture_release(&cap); continue; }   /* mode switch, access lost */
        if (fi.LastPresentTime.QuadPart) {
            updates++;
            accumulated += fi.AccumulatedFrames > 1 ? fi.AccumulatedFrames - 1 : 0;
            ID3D11Texture2D *tex;
            IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&tex);
            for (int i = 0; i < NREG; i++)
                ID3D11DeviceContext_CopySubresourceRegion(ctx, (ID3D11Resource *)cap.stage, 0,
                                                          (UINT)(i * cap.rw), 0, 0,
                                                          (ID3D11Resource *)tex, 0, &cap.box[i]);
            ID3D11Texture2D_Release(tex);
            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)cap.stage, 0, D3D11_MAP_READ, 0, &m))) {
                uint64_t h = 0xCBF29CE484222325ull;
                for (int y = 0; y < cap.rh; y++)
                    h = fnv((uint8_t *)m.pData + (size_t)y * m.RowPitch, (size_t)cap.rw * NREG * 4, h);
                double t = now_s() - t0;
                fprintf(csv, "%.4f,-1,%u\n", t, fi.AccumulatedFrames);   /* every update */
                if (h != prev && dump && t >= 10.0 && dumped < 48) {
                    char name[512];
                    snprintf(name, sizeof name, "%s_%02d.ppm", dump, dumped++);
                    FILE *f = fopen(name, "wb");
                    if (f) {
                        fprintf(f, "P6 %d %d 255\n", cap.rw, cap.rh);
                        for (int y = 0; y < cap.rh; y++) {
                            const uint8_t *row = (const uint8_t *)m.pData + (size_t)y * m.RowPitch;
                            for (int x = 0; x < cap.rw; x++) { fputc(row[x*4+2], f); fputc(row[x*4+1], f); fputc(row[x*4], f); }
                        }
                        fclose(f);
                    }
                }
                if (h != prev) {
                    prev = h; distinct++;
                    if ((int)t < 3600) per_sec[(int)t]++;
                    double gap = last < 0 ? 0 : (t - last) * 1000.0;
                    for (int i = 0; i < 16; i++) if (gap > gaps[i]) {
                        memmove(&gaps[i + 1], &gaps[i], sizeof(double) * (15 - i)); gaps[i] = gap; break;
                    }
                    fprintf(csv, "%.4f,%.2f,%u\n", t, gap, fi.AccumulatedFrames);
                    last = t;
                }
                ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)cap.stage, 0);
            }
        }
        IDXGIResource_Release(res);
        IDXGIOutputDuplication_ReleaseFrame(cap.dup);
    }
    fclose(csv);
    printf("screen %dx%d %d regions %dx%d: %ld updates, %ld distinct frames in %.1f s (%.1f/s), %ld coalesced, %ld captures opened\n",
           cap.sw, cap.sh, NREG, cap.rw, cap.rh, updates, distinct, secs, distinct / secs, accumulated, opens);
    printf("distinct per second:");
    for (int s = 0; s < (int)secs && s < 3600; s++) printf(" %ld", per_sec[s]);
    printf("\nlargest gaps ms:");
    for (int i = 0; i < 10; i++) printf(" %.0f", gaps[i]);
    printf("\n");
    return 0;
}
