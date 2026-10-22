/* screen_frames.c - count distinct frames that actually reach the display.
 *
 * Uses DXGI Desktop Duplication on the primary output, which sees borderless
 * and fullscreen OpenGL windows (GDI capture does not). For SECONDS it waits
 * for desktop updates, copies a centre region (W x H) of each, hashes it and
 * records when the visible content changed. Prints distinct frames per
 * second, the largest gaps between distinct frames and a CSV of change times.
 *
 * Build (MSYS2 mingw64):
 *   gcc -O2 -o screen_frames.exe tools/screen_frames.c -ld3d11 -ldxgi
 * Usage: screen_frames SECONDS OUT.csv [W H]
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

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s SECONDS OUT.csv [W H]\n", argv[0]); return 2; }
    double secs = atof(argv[1]);
    int rw = argc > 4 ? atoi(argv[3]) : 640, rh = argc > 4 ? atoi(argv[4]) : 360;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                                 D3D11_SDK_VERSION, &dev, &fl, &ctx))) return 3;
    IDXGIDevice *xd; IDXGIAdapter *ad; IDXGIOutput *out; IDXGIOutput1 *out1;
    ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&xd);
    IDXGIDevice_GetAdapter(xd, &ad);
    if (FAILED(IDXGIAdapter_EnumOutputs(ad, 0, &out))) return 4;
    IDXGIOutput_QueryInterface(out, &IID_IDXGIOutput1, (void **)&out1);
    IDXGIOutputDuplication *dup;
    if (FAILED(IDXGIOutput1_DuplicateOutput(out1, (IUnknown *)dev, &dup))) return 5;
    DXGI_OUTDUPL_DESC dd; IDXGIOutputDuplication_GetDesc(dup, &dd);
    int sw = (int)dd.ModeDesc.Width, sh = (int)dd.ModeDesc.Height;
    if (rw > sw) rw = sw;
    if (rh > sh) rh = sh;

    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = rw; td.Height = rh; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *stage;
    if (FAILED(ID3D11Device_CreateTexture2D(dev, &td, NULL, &stage))) return 6;
    D3D11_BOX box = { (UINT)(sw - rw) / 2, (UINT)(sh - rh) / 2, 0,
                      (UINT)(sw + rw) / 2, (UINT)(sh + rh) / 2, 1 };

    FILE *csv = fopen(argv[2], "w");
    if (!csv) return 7;
    fprintf(csv, "t_s,gap_ms,accumulated\n");
    double t0 = now_s(), last = -1, end = t0 + secs;
    uint64_t prev = 0; long updates = 0, distinct = 0, accumulated = 0, reconnects = 0;
    static long per_sec[3600];
    double gaps[16] = {0};
    while (now_s() < end) {
        DXGI_OUTDUPL_FRAME_INFO fi; IDXGIResource *res;
        HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(dup, 100, &fi, &res);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) continue;
        if (hr == DXGI_ERROR_ACCESS_LOST) {   /* mode or fullscreen switch */
            IDXGIOutputDuplication_Release(dup);
            dup = NULL;
            while (!dup && now_s() < end)
                if (FAILED(IDXGIOutput1_DuplicateOutput(out1, (IUnknown *)dev, &dup))) { dup = NULL; Sleep(20); }
            reconnects++;
            if (!dup) break;
            continue;
        }
        if (FAILED(hr)) { fprintf(stderr, "acquire failed 0x%08lx\n", hr); break; }
        if (fi.LastPresentTime.QuadPart) {
            updates++;
            accumulated += fi.AccumulatedFrames > 1 ? fi.AccumulatedFrames - 1 : 0;
            ID3D11Texture2D *tex;
            IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&tex);
            ID3D11DeviceContext_CopySubresourceRegion(ctx, (ID3D11Resource *)stage, 0, 0, 0, 0,
                                                      (ID3D11Resource *)tex, 0, &box);
            ID3D11Texture2D_Release(tex);
            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)stage, 0, D3D11_MAP_READ, 0, &m))) {
                uint64_t h = 0xCBF29CE484222325ull;
                for (int y = 0; y < rh; y++) h = fnv((uint8_t *)m.pData + (size_t)y * m.RowPitch, (size_t)rw * 4, h);
                ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)stage, 0);
                double t = now_s() - t0;
                fprintf(csv, "%.4f,-1,%u\n", t, fi.AccumulatedFrames);   /* every update */
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
            }
        }
        IDXGIResource_Release(res);
        IDXGIOutputDuplication_ReleaseFrame(dup);
    }
    fclose(csv);
    printf("screen %dx%d region %dx%d: %ld updates, %ld distinct frames in %.1f s (%.1f/s), %ld coalesced\n",
           sw, sh, rw, rh, updates, distinct, secs, distinct / secs, accumulated);
    printf("distinct per second:");
    for (int s = 0; s < (int)secs && s < 3600; s++) printf(" %ld", per_sec[s]);
    printf("\nlargest gaps ms:");
    for (int i = 0; i < 10; i++) printf(" %.0f", gaps[i]);
    printf("\n");
    return 0;
}
