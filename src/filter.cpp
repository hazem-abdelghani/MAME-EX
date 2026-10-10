/* The filter window (experimental): xBRZ, FXAA, dither smoothing and better scalers for MAME.
   MAME runs in a window of the game's own picture size; this window copies the picture of the MAME window every frame, runs the pixel shaders of the
   Direct3D 11 renderer of ZiNc EX over it (the xBRZ, FXAA and dither smoothing HLSL is taken from there) and shows the result on top, over the whole
   screen. The window is click-through and never takes the focus, so the keyboard and the mouse still go to MAME. It is shown only while MAME has the focus. */
#include "common.h"
#include <d3d11.h>
#include <string>
#include <algorithm>

typedef HRESULT (WINAPI *PFN_COMPILE)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);

static const char *kHlslHead = R"HLSL(
Texture2D<float4> srcTex : register(t1);
SamplerState smpPoint : register(s0);
SamplerState smpLinear : register(s1);
#define smp smpLinear   /* the FXAA shader of the original renderer calls its sampler smp */
cbuffer Disp : register(b0) {
    float4 params;   /* x: the scaler of ps_plain; y: dither threshold (0 = off); y, z in the FXAA pass: one pixel in uv units */
    float4 region;   /* x y w h of the picture in texels */
    float4 xs;       /* output pixels per source texel (xBRZ) */
    float4 ss;       /* source size w h and 1/w 1/h (the FXAA pass: the size of the texture it reads) */
    float4 rect;     /* where the quad is: left top right bottom in clip space */
}

struct DSO { float4 p : SV_Position; float2 uv : UV; };

DSO vs_disp(uint id : SV_VertexID) {
    DSO o;
    float2 t = float2(id & 1, id >> 1);
    o.p = float4(lerp(rect.x, rect.z, t.x), lerp(rect.y, rect.w, t.y), 0.0, 1.0);
    o.uv = t;
    return o;
}

/* dither smoothing: a pixel whose four neighbours are alike but which is a little different itself (the checkerboard of a dithered gradient) is blended with them. */
float ditherMax(float3 a, float3 b) { float3 d = abs(a - b); return max(d.r, max(d.g, d.b)); }
float3 ditherDelta(Texture2D<float4> t, int2 q, int st) {
    int2 lo = int2((int)region.x, (int)region.y);
    int2 hi = int2((int)(region.x + region.z) - 1, (int)(region.y + region.w) - 1);
    float3 c = t.Load(int3(q, 0)).rgb;
    float3 n = t.Load(int3(clamp(q + int2(0, -st), lo, hi), 0)).rgb;
    float3 s = t.Load(int3(clamp(q + int2(0, st), lo, hi), 0)).rgb;
    float3 w = t.Load(int3(clamp(q + int2(-st, 0), lo, hi), 0)).rgb;
    float3 e = t.Load(int3(clamp(q + int2(st, 0), lo, hi), 0)).rgb;
    float th = params.y;
    float3 a = (n + s + w + e) * 0.25;
    bool alike = ditherMax(n, s) < th && ditherMax(w, e) < th && ditherMax(n, w) < th;
    bool near = ditherMax(c, a) < th * 2.0 && ditherMax(c, a) > 0.0;
    return (alike && near) ? (a - c) * 0.5 : float3(0.0, 0.0, 0.0);
}

/* one texel of the picture, the edges of the picture repeated */
float3 fetch(int2 p) {
    int2 lo = int2((int)region.x, (int)region.y);
    int2 hi = int2((int)(region.x + region.z) - 1, (int)(region.y + region.w) - 1);
    return srcTex.Load(int3(clamp(p, lo, hi), 0)).rgb;
}

/* the cubic filter of Mitchell and Netravali (B = C = 1/3): smoother than bilinear without the boxy texels, no ringing */
float mitchell(float d) {
    const float B = 1.0 / 3.0, C = 1.0 / 3.0;
    d = abs(d);
    if (d < 1.0) return ((12.0 - 9.0 * B - 6.0 * C) * d * d * d + (-18.0 + 12.0 * B + 6.0 * C) * d * d + (6.0 - 2.0 * B)) / 6.0;
    if (d < 2.0) return ((-B - 6.0 * C) * d * d * d + (6.0 * B + 30.0 * C) * d * d + (-12.0 * B - 48.0 * C) * d + (8.0 * B + 24.0 * C)) / 6.0;
    return 0.0;
}

/* params.x: 0 hard pixels, 1 bilinear, 3 sharp bilinear (blends only along the edges of the texels), 4 bicubic */
float4 ps_plain(DSO v) : SV_Target {
    float2 tc = region.xy + v.uv * region.zw;
    float3 c;
    if (params.x > 3.5) {
        float2 p = tc - 0.5;
        int2 i = int2(floor(p));
        float2 f = p - floor(p);
        float3 acc = float3(0.0, 0.0, 0.0);
        float sw = 0.0;
        [unroll] for (int y = -1; y <= 2; y++) {
            [unroll] for (int x = -1; x <= 2; x++) {
                float w = mitchell((float)x - f.x) * mitchell((float)y - f.y);
                acc += w * fetch(i + int2(x, y));
                sw += w;
            }
        }
        c = acc / sw;
    } else if (params.x > 2.5) {
        float2 p = tc - 0.5;
        int2 i = int2(floor(p));
        float2 f = clamp((p - floor(p) - 0.5) * xs.xy + 0.5, 0.0, 1.0);
        c = lerp(lerp(fetch(i), fetch(i + int2(1, 0)), f.x), lerp(fetch(i + int2(0, 1)), fetch(i + int2(1, 1)), f.x), f.y);
    } else if (params.x > 0.5) {
        c = srcTex.SampleLevel(smpLinear, tc * ss.zw, 0).rgb;
    } else {
        c = srcTex.SampleLevel(smpPoint, tc * ss.zw, 0).rgb;
    }
    if (params.y > 0.0) c += ditherDelta(srcTex, int2(floor(tc)), 1);
    return float4(c, 1.0);
}
)HLSL";

static const char *kHlslXbrz = R"HLSL(
)HLSL"
#include "fx_xbrz.inc"
R"HLSL(
float4 ps_xbrz(DSO v) : SV_Target { return float4(xbrz(v.uv).rgb, 1.0); }
)HLSL";

static const char *kHlslFxaa = R"HLSL(
)HLSL"
#include "fx_fxaa.inc"
;

/* ---------------------------------------------------------------- state */
typedef struct { float params[4], region[4], xs[4], ss[4], rect[4]; } CB;

static HANDLE g_thread;
static volatile LONG g_stop;
static DWORD g_pid;
static int g_running;

typedef struct { DWORD pid; HWND found; } FindCtx;
static BOOL CALLBACK find_proc(HWND h, LPARAM lp) {
    FindCtx *c = (FindCtx *)lp;
    DWORD p = 0;
    RECT r;
    GetWindowThreadProcessId(h, &p);
    if (p == c->pid && IsWindowVisible(h) && !GetWindow(h, GW_OWNER) && GetWindowTextLengthW(h) > 0 && GetClientRect(h, &r) && r.right >= 64 && r.bottom >= 64) { c->found = h; return FALSE; }
    return TRUE;
}

/* the picture window of MAME: the first visible top-level window of the process that has a title and a size */
static HWND find_mame_window(DWORD pid, volatile LONG *stop) {
    DWORD t0 = GetTickCount();
    while (!*stop && GetTickCount() - t0 < 30000) {
        FindCtx c = {pid, NULL};
        EnumWindows(find_proc, (LPARAM)&c);
        if (c.found) return c.found;
        Sleep(200);
    }
    return NULL;
}

static ID3DBlob *compile(PFN_COMPILE fn, const std::string &src, const char *entry, const char *target) {
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr = fn(src.data(), src.size(), "fx.hlsl", NULL, NULL, entry, target, 0, 0, &code, &err);
    if (err) err->Release();
    return SUCCEEDED(hr) ? code : NULL;
}

static LRESULT CALLBACK out_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, w, l);
}

template <class T> static void rel(T *&p) { if (p) { p->Release(); p = NULL; } }

static DWORD WINAPI filter_thread(LPVOID unused) {
    (void)unused;
    const int scaler = g_set.fxScaler, fxaa = g_set.fxFxaa, dither = g_set.fxDither, keepAspect = g_set.keepAspect;
    HWND mame = find_mame_window(g_pid, &g_stop), out = NULL;
    HMODULE comp = LoadLibraryW(L"d3dcompiler_47.dll");
    PFN_COMPILE fn = comp ? (PFN_COMPILE)GetProcAddress(comp, "D3DCompile") : NULL;
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    IDXGISwapChain *sc = NULL;
    ID3D11RenderTargetView *bbRTV = NULL, *fxRTV = NULL;
    ID3D11Texture2D *srcTex = NULL, *fxTex = NULL;
    ID3D11ShaderResourceView *srcSRV = NULL, *fxSRV = NULL;
    ID3D11VertexShader *vs = NULL;
    ID3D11PixelShader *psPlain = NULL, *psXbrz = NULL, *psFxaa = NULL;
    ID3D11SamplerState *smpPoint = NULL, *smpLinear = NULL;
    ID3D11Buffer *cbuf = NULL;
    ID3D11RasterizerState *rs = NULL;
    HDC wdc = NULL, mdc = NULL;
    HBITMAP bmp = NULL, oldBmp = NULL;
    unsigned char *bits = NULL;
    int bmpW = 0, bmpH = 0, srcW = 0, srcH = 0, outW = 0, outH = 0, ok = 0;
    WNDCLASSEXW wc;
    MSG msg;
    if (!mame || !fn) goto done;
    {   /* the output window: the monitor of the MAME window, click-through, never active, always on top */
        MONITORINFO mi;
        HMONITOR mon = MonitorFromWindow(mame, MONITOR_DEFAULTTOPRIMARY);
        DXGI_SWAP_CHAIN_DESC sd;
        D3D11_BUFFER_DESC bd;
        D3D11_SAMPLER_DESC sm;
        D3D11_RASTERIZER_DESC rd;
        ID3D11Texture2D *bb = NULL;
        D3D_FEATURE_LEVEL fl;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        std::string head = kHlslHead, xb = head + kHlslXbrz, fx = head + kHlslFxaa;
        ID3DBlob *b;
        memset(&mi, 0, sizeof mi);
        mi.cbSize = sizeof mi;
        GetMonitorInfoW(mon, &mi);
        outW = mi.rcMonitor.right - mi.rcMonitor.left;
        outH = mi.rcMonitor.bottom - mi.rcMonitor.top;
        memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = out_proc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = L"MameExFilter";
        RegisterClassExW(&wc);
        out = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"MameExFilter", L"", WS_POPUP,
                              mi.rcMonitor.left, mi.rcMonitor.top, outW, outH, NULL, NULL, g_inst, NULL);
        if (!out) goto done;
        SetLayeredWindowAttributes(out, 0, 255, LWA_ALPHA);
        memset(&sd, 0, sizeof sd);
        sd.BufferDesc.Width = outW; sd.BufferDesc.Height = outH;
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;
        sd.OutputWindow = out;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 3, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx))) goto done;
        if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb))) goto done;
        dev->CreateRenderTargetView(bb, NULL, &bbRTV);
        bb->Release();
        if (!bbRTV) goto done;
        if (!(b = compile(fn, xb, "vs_disp", "vs_4_0"))) goto done;
        dev->CreateVertexShader(b->GetBufferPointer(), b->GetBufferSize(), NULL, &vs);
        b->Release();
        if ((b = compile(fn, xb, "ps_plain", "ps_4_0"))) { dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), NULL, &psPlain); b->Release(); }
        if (scaler == 2 && (b = compile(fn, xb, "ps_xbrz", "ps_4_0"))) { dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), NULL, &psXbrz); b->Release(); }
        if (fxaa && (b = compile(fn, fx, "ps_fxaa", "ps_4_0"))) { dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), NULL, &psFxaa); b->Release(); }
        if (!vs || !psPlain || (scaler == 2 && !psXbrz)) goto done;
        memset(&sm, 0, sizeof sm);
        sm.AddressU = sm.AddressV = sm.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sm.MaxLOD = D3D11_FLOAT32_MAX;
        sm.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; dev->CreateSamplerState(&sm, &smpPoint);
        sm.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; dev->CreateSamplerState(&sm, &smpLinear);
        memset(&bd, 0, sizeof bd);
        bd.ByteWidth = sizeof(CB); bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        dev->CreateBuffer(&bd, NULL, &cbuf);
        memset(&rd, 0, sizeof rd);
        rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        dev->CreateRasterizerState(&rd, &rs);
        if (fxaa && psFxaa) {   /* the picture before FXAA: the size of the screen */
            D3D11_TEXTURE2D_DESC td;
            memset(&td, 0, sizeof td);
            td.Width = outW; td.Height = outH; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            if (SUCCEEDED(dev->CreateTexture2D(&td, NULL, &fxTex))) { dev->CreateRenderTargetView(fxTex, NULL, &fxRTV); dev->CreateShaderResourceView(fxTex, NULL, &fxSRV); }
            if (!fxRTV || !fxSRV) { rel(fxTex); rel(fxRTV); rel(fxSRV); psFxaa = NULL; }
        }
        ok = 1;
    }
    wdc = GetDC(NULL);
    mdc = CreateCompatibleDC(wdc);
    while (ok && !g_stop && IsWindow(mame)) {
        RECT wr, cr;
        POINT org = {0, 0};
        int cw, ch, offx, offy, visible;
        DWORD fgpid = 0;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        GetWindowThreadProcessId(GetForegroundWindow(), &fgpid);
        visible = !IsIconic(mame) && fgpid == g_pid;   /* only while MAME has the focus: Alt+Tab shows the desktop */
        if (!visible) { if (IsWindowVisible(out)) ShowWindow(out, SW_HIDE); Sleep(60); continue; }
        if (!IsWindowVisible(out)) ShowWindow(out, SW_SHOWNOACTIVATE);
        GetWindowRect(mame, &wr);
        GetClientRect(mame, &cr);
        ClientToScreen(mame, &org);
        cw = cr.right; ch = cr.bottom;
        if (cw < 16 || ch < 16) { Sleep(30); continue; }
        offx = org.x - wr.left; offy = org.y - wr.top;
        if (wr.right - wr.left != bmpW || wr.bottom - wr.top != bmpH || !bmp) {   /* a bitmap the size of the whole MAME window (PrintWindow draws all of it) */
            if (bmp) { SelectObject(mdc, oldBmp); DeleteObject(bmp); }
            bmpW = wr.right - wr.left; bmpH = wr.bottom - wr.top;
            BITMAPINFO bi;
            memset(&bi, 0, sizeof bi);
            bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = bmpW; bi.bmiHeader.biHeight = -bmpH;
            bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
            bmp = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
            if (!bmp) break;
            oldBmp = (HBITMAP)SelectObject(mdc, bmp);
        }
        if (cw != srcW || ch != srcH || !srcTex) {   /* the texture of the picture: the client size of the MAME window */
            D3D11_TEXTURE2D_DESC td;
            rel(srcSRV); rel(srcTex);
            memset(&td, 0, sizeof td);
            td.Width = cw; td.Height = ch; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DYNAMIC; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(dev->CreateTexture2D(&td, NULL, &srcTex)) || FAILED(dev->CreateShaderResourceView(srcTex, NULL, &srcSRV))) break;
            srcW = cw; srcH = ch;
        }
        if (!PrintWindow(mame, mdc, 2 /* PW_RENDERFULLCONTENT */)) { Sleep(10); continue; }
        {   /* the client part of the window bitmap into the texture */
            D3D11_MAPPED_SUBRESOURCE mp;
            if (SUCCEEDED(ctx->Map(srcTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
                int y;
                for (y = 0; y < ch; y++) memcpy((unsigned char *)mp.pData + (size_t)y * mp.RowPitch, bits + ((size_t)(offy + y) * bmpW + offx) * 4, (size_t)cw * 4);
                ctx->Unmap(srcTex, 0);
            }
        }
        {   /* where the picture goes: the whole screen, or the shape that was asked for */
            int c = 0, rw = cw, rh = ch, dx, dy, dw, dh;
            float ar;
            D3D11_VIEWPORT vp;
            CB cb;
            ID3D11ShaderResourceView *none[2] = {NULL, NULL};
            ID3D11RenderTargetView *tgt;
            const float black[4] = {0, 0, 0, 1};
            if (!keepAspect) { dw = outW; dh = outH; }   /* Keep Aspect Ratio off: the whole screen */
            else {   /* the shape of the arcade monitor, black bars where the screen is wider */
                ar = 4.0f / 3.0f;
                dh = outH; dw = (int)(dh * ar + 0.5f);
                if (dw > outW) { dw = outW; dh = (int)(dw / ar + 0.5f); }
            }
            dx = (outW - dw) / 2; dy = (outH - dh) / 2;
            memset(&cb, 0, sizeof cb);
            cb.params[1] = dither == 1 ? 0.04f : dither == 2 ? 0.08f : dither == 3 ? 0.14f : 0.0f;
            cb.params[0] = scaler == 0 ? 1.0f : scaler == 1 ? 0.0f : scaler == 3 ? 3.0f : scaler == 4 ? 4.0f : 0.0f;   /* 0 smooth, 1 sharp, 2 xBRZ (its own shader), 3 sharp bilinear, 4 bicubic */
            cb.region[0] = (float)c; cb.region[1] = (float)c; cb.region[2] = (float)rw; cb.region[3] = (float)rh;
            cb.xs[0] = (float)dw / rw; cb.xs[1] = (float)dh / rh;
            cb.ss[0] = (float)cw; cb.ss[1] = (float)ch; cb.ss[2] = 1.0f / cw; cb.ss[3] = 1.0f / ch;
            cb.rect[0] = 2.0f * dx / outW - 1.0f; cb.rect[1] = 1.0f - 2.0f * dy / outH;
            cb.rect[2] = 2.0f * (dx + dw) / outW - 1.0f; cb.rect[3] = 1.0f - 2.0f * (dy + dh) / outH;
            tgt = psFxaa ? fxRTV : bbRTV;
            vp.TopLeftX = 0; vp.TopLeftY = 0; vp.Width = (float)outW; vp.Height = (float)outH; vp.MinDepth = 0; vp.MaxDepth = 1;
            ctx->RSSetViewports(1, &vp);
            ctx->RSSetState(rs);
            ctx->OMSetRenderTargets(1, &tgt, NULL);
            ctx->ClearRenderTargetView(tgt, black);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            ctx->IASetInputLayout(NULL);
            ctx->VSSetShader(vs, NULL, 0);
            ctx->PSSetShader(scaler == 2 ? psXbrz : psPlain, NULL, 0);
            ctx->UpdateSubresource(cbuf, 0, NULL, &cb, 0, 0);
            ctx->VSSetConstantBuffers(0, 1, &cbuf);
            ctx->PSSetConstantBuffers(0, 1, &cbuf);
            ctx->PSSetSamplers(0, 1, &smpPoint);
            ctx->PSSetSamplers(1, 1, &smpLinear);
            ctx->PSSetShaderResources(1, 1, &srcSRV);
            ctx->Draw(4, 0);
            ctx->PSSetShaderResources(1, 1, &none[0]);
            if (psFxaa) {   /* FXAA: from the window sized picture to the screen */
                CB f;
                memset(&f, 0, sizeof f);
                f.params[1] = 1.0f / outW; f.params[2] = 1.0f / outH;
                f.region[2] = (float)outW; f.region[3] = (float)outH;
                f.ss[0] = (float)outW; f.ss[1] = (float)outH; f.ss[2] = 1.0f / outW; f.ss[3] = 1.0f / outH;
                f.rect[0] = -1; f.rect[1] = 1; f.rect[2] = 1; f.rect[3] = -1;
                ctx->OMSetRenderTargets(1, &bbRTV, NULL);
                ctx->PSSetShader(psFxaa, NULL, 0);
                ctx->UpdateSubresource(cbuf, 0, NULL, &f, 0, 0);
                ctx->PSSetShaderResources(1, 1, &fxSRV);
                ctx->Draw(4, 0);
                ctx->PSSetShaderResources(1, 1, &none[0]);
            }
        }
        sc->Present(1, 0);
    }
done:
    if (ctx) ctx->ClearState();
    rel(cbuf); rel(rs); rel(smpPoint); rel(smpLinear); rel(psPlain); rel(psXbrz); rel(psFxaa); rel(vs);
    rel(fxSRV); rel(fxRTV); rel(fxTex); rel(srcSRV); rel(srcTex); rel(bbRTV); rel(sc); rel(ctx); rel(dev);
    if (bmp) { SelectObject(mdc, oldBmp); DeleteObject(bmp); }
    if (mdc) DeleteDC(mdc);
    if (wdc) ReleaseDC(NULL, wdc);
    if (out) DestroyWindow(out);
    if (comp) FreeLibrary(comp);
    g_running = 0;
    return 0;
}

/* starts the filter window for a MAME process (it finds the window of the process by itself) */
void filter_start(DWORD pid) {
    filter_stop();
    g_pid = pid;
    g_stop = 0;
    g_running = 1;
    g_thread = CreateThread(NULL, 0, filter_thread, NULL, 0, NULL);
    if (!g_thread) g_running = 0;
}

void filter_stop(void) {
    if (!g_thread) return;
    g_stop = 1;
    WaitForSingleObject(g_thread, 3000);
    CloseHandle(g_thread);
    g_thread = NULL;
    g_running = 0;
}
