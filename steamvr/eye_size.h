#pragma once
#include <cstdint>

// The world is copied 1:1 from the center of one 16:9 backbuffer into the
// square OpenXR eye. Menus, name tags and the rest of the interface stay on
// that wider canvas. The physical window is only a 16:9 view of it.
namespace framevr {
inline uint64_t PackSize(uint32_t width, uint32_t height) {
    return uint64_t(width) | (uint64_t(height) << 32);
}
inline void UnpackSize(uint64_t packed, uint32_t& width, uint32_t& height) {
    width = uint32_t(packed);
    height = uint32_t(packed >> 32);
}

struct EyePixels {
    uint32_t width = 0;
    uint32_t height = 0;
    bool clamped = false;
};

// override is an ini pair; 0,0 keeps the runtime recommendation.
// max is the runtime's maxImageRect (0 = unknown). Dimensions are even.
inline EyePixels ChooseEyePixels(uint32_t recommendedW, uint32_t recommendedH,
                                 uint32_t maxW, uint32_t maxH,
                                 uint32_t overrideW, uint32_t overrideH) {
    uint32_t w = 0, h = 0;
    if (overrideW >= 640 && overrideH >= 480) { w = overrideW; h = overrideH; }
    else { w = recommendedW; h = recommendedH; }
    if (w < 640 || h < 480) return {};
    bool clamped = false;
    auto cap = [&](uint32_t& dim, uint32_t limit) {
        if (!limit || dim <= limit) return;
        uint32_t next = limit & ~1u;
        if (next < 640) next = limit;
        dim = next;
        clamped = true;
    };
    if (w & 1u) { ++w; clamped = true; }
    if (h & 1u) { ++h; clamped = true; }
    cap(w, maxW);
    cap(h, maxH);
    cap(w, 16384);
    cap(h, 16384);
    if (w < 640 || h < 480) return {};
    return {w, h, clamped};
}

inline bool IsScreenSwapchain(uint32_t width, uint32_t height, bool hasWindow) {
    return hasWindow && width >= 640 && height >= 480 && width <= 16384 && height <= 16384;
}

// A viewport written in desktop pixels, drawing into an eye-sized target.
// Shadow maps and viewports that already fit the target are left alone.
struct BoxFix {
    float x = 0, y = 0, w = 0, h = 0;
    bool changed = false;
};
inline BoxFix FitDesktopBox(float x, float y, float w, float h,
                            uint32_t eyeW, uint32_t eyeH,
                            uint32_t deskW, uint32_t deskH,
                            uint32_t targetW, uint32_t targetH) {
    BoxFix out{x, y, w, h, false};
    if (!(w > 0.f) || !(h > 0.f)) return out;
    if (!eyeW || !eyeH || !deskW || !deskH || !targetW || !targetH) return out;
    if (targetW != eyeW || targetH != eyeH) return out;
    if (deskW == eyeW && deskH == eyeH) return out;
    if (w <= float(targetW) + 1.f && h <= float(targetH) + 1.f) return out;
    if (w > float(deskW) * 1.02f || h > float(deskH) * 1.02f) return out;
    const float sx = float(eyeW) / float(deskW);
    const float sy = float(eyeH) / float(deskH);
    out.x = x * sx;
    out.y = y * sy;
    out.w = w * sx;
    out.h = h * sy;
    out.changed = true;
    return out;
}

// 16:9 canvas tall enough to hold the eye at 1:1. The eye rect sits in the
// center, so a center copy is the runtime fov with no resampling.
struct UiCanvas {
    uint32_t width = 0, height = 0;
    uint32_t eyeW = 0, eyeH = 0;
    uint32_t originX = 0, originY = 0;
};
inline UiCanvas Canvas16x9(uint32_t eyeW, uint32_t eyeH) {
    UiCanvas c{};
    if (eyeW < 640 || eyeH < 480 || eyeW > 16384 || eyeH > 16384) return c;
    uint32_t w = uint32_t((uint64_t(eyeH) * 16u + 4u) / 9u);
    if (w & 1u) ++w;
    if (w < eyeW) w = eyeW + (eyeW & 1u);
    if (w > 16384) w = 16384;
    const uint32_t h = eyeH;
    if (w < eyeW || h > 16384) return c;
    c.width = w;
    c.height = h;
    c.eyeW = eyeW;
    c.eyeH = eyeH;
    c.originX = (w - eyeW) / 2u;
    c.originY = (h - eyeH) / 2u;
    return c;
}

// Horizontal tangents whose center eye-width crop matches the original fov.
struct TanSpan { float left = 0, right = 0; };
inline TanSpan WidenHorizontal(float left, float right, uint32_t eyeW, uint32_t canvasW) {
    TanSpan s{left, right};
    if (!eyeW || canvasW <= eyeW || !(right > left)) return s;
    const float frac = float(eyeW) / float(canvasW);
    const float sum = left + right;
    const float diff = (right - left) / frac;
    s.left = (sum - diff) * 0.5f;
    s.right = (sum + diff) * 0.5f;
    return s;
}

// Largest even 16:9 client that fits in the work area inside the window frame.
// x,y is the client origin on the screen.
struct WindowFit { int x = 0, y = 0, w = 0, h = 0; bool ok = false; };
inline WindowFit Fit16x9(int workX, int workY, int workW, int workH,
                         int frameLeft, int frameTop, int frameRight, int frameBottom) {
    WindowFit f{};
    if (workW <= 0 || workH <= 0 || frameLeft < 0 || frameTop < 0 || frameRight < 0 || frameBottom < 0) return f;
    const int availW = workW - frameLeft - frameRight;
    const int availH = workH - frameTop - frameBottom;
    if (availW < 640 || availH < 360) return f;
    int cw = 0, ch = 0;
    if (int64_t(availW) * 9 >= int64_t(availH) * 16) {
        ch = availH & ~1;
        cw = int(int64_t(ch) * 16 / 9) & ~1;
    } else {
        cw = availW & ~1;
        ch = int(int64_t(cw) * 9 / 16) & ~1;
    }
    if (cw < 640 || ch < 360 || cw > availW || ch > availH) return f;
    f.w = cw;
    f.h = ch;
    f.x = workX + frameLeft + (availW - cw) / 2;
    f.y = workY + frameTop + (availH - ch) / 2;
    f.ok = true;
    return f;
}

// Map a coordinate across spans so both endpoints land on both endpoints.
inline int MapSpan(int v, int fromN, int toN) {
    if (fromN <= 1 || toN <= 1) return 0;
    if (v < 0) v = 0;
    if (v >= fromN) v = fromN - 1;
    return int(((int64_t)v * (toN - 1) + (fromN - 1) / 2) / (fromN - 1));
}
}
