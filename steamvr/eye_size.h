#pragma once
#include <cstdint>

// One backbuffer is drawn twice (left eye, then right) and copied 1:1 into the
// OpenXR eye textures. The game sizes that buffer from the window, so the
// window and the swapchain have to be the headset's per-eye rect.
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
}
