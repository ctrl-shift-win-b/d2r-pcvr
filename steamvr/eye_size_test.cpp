#include "eye_size.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
static void Expect(bool condition, const char* what) {
    if (!condition) { std::fprintf(stderr, "eye size: %s\n", what); std::exit(1); }
}
int main() {
    const auto native = framevr::ChooseEyePixels(2064, 2208, 8192, 8192, 0, 0);
    Expect(native.width == 2064 && native.height == 2208 && !native.clamped, "recommended size");
    const auto over = framevr::ChooseEyePixels(2064, 2208, 8192, 8192, 2400, 2400);
    Expect(over.width == 2400 && over.height == 2400 && !over.clamped, "ini override");
    const auto partial = framevr::ChooseEyePixels(2064, 2208, 8192, 8192, 2400, 0);
    Expect(partial.width == 2064 && partial.height == 2208, "half override ignored");
    const auto capped = framevr::ChooseEyePixels(12000, 12000, 8192, 4096, 0, 0);
    Expect(capped.width == 8192 && capped.height == 4096 && capped.clamped, "runtime max");
    const auto odd = framevr::ChooseEyePixels(2017, 2241, 8192, 8192, 0, 0);
    Expect(odd.width == 2018 && odd.height == 2242 && odd.clamped, "even dimensions");
    const auto none = framevr::ChooseEyePixels(0, 0, 0, 0, 0, 0);
    Expect(none.width == 0 && none.height == 0, "unknown stays off");
    Expect(framevr::IsScreenSwapchain(5120, 1440, true), "desktop swapchain");
    Expect(!framevr::IsScreenSwapchain(64, 64, true), "tiny swapchain");
    Expect(!framevr::IsScreenSwapchain(1920, 1080, false), "offscreen swapchain");

    const auto full = framevr::FitDesktopBox(0, 0, 5120, 1440, 2064, 2208, 5120, 1440, 2064, 2208);
    Expect(full.changed && full.w == 2064.f && full.h == 2208.f, "desktop viewport scaled onto the eye");
    const auto already = framevr::FitDesktopBox(0, 0, 2064, 2208, 2064, 2208, 5120, 1440, 2064, 2208);
    Expect(!already.changed, "eye viewport left alone");
    const auto shadow = framevr::FitDesktopBox(0, 0, 4096, 4096, 2064, 2208, 5120, 1440, 4096, 4096);
    Expect(!shadow.changed, "shadow map left alone");
    const auto dlss = framevr::FitDesktopBox(0, 0, 3414, 960, 2064, 2208, 5120, 1440, 2064, 2208);
    Expect(dlss.changed && dlss.w > 1000.f && dlss.w < 2064.f && dlss.h > 1000.f && dlss.h < 2208.f, "upscaler subrect scaled");
    uint32_t w = 0, h = 0;
    framevr::UnpackSize(framevr::PackSize(2064, 2208), w, h);
    Expect(w == 2064 && h == 2208, "pack round trip");

    const auto canvas = framevr::Canvas16x9(4004, 4004);
    Expect(canvas.width == 7118 && canvas.height == 4004, "16:9 canvas");
    Expect(canvas.originY == 0 && canvas.originX + 4004 <= 7118, "eye rect inside the canvas");
    Expect(canvas.originX == (7118 - 4004) / 2, "eye rect centered");
    const auto tall = framevr::Canvas16x9(2064, 2208);
    Expect(tall.height == 2208 && tall.width > 2208 && (tall.width % 2) == 0, "taller eye still gets a wider canvas");
    Expect(tall.originX + tall.eyeW <= tall.width && tall.originY + tall.eyeH <= tall.height, "tall eye rect fits");
    Expect(framevr::Canvas16x9(0, 0).width == 0, "no canvas without an eye");

    const auto wide = framevr::WidenHorizontal(-0.8f, 0.9f, 4004, 7118);
    const float frac = 4004.f / 7118.f;
    auto ndc = [](float t, float L, float R) { return (2.f * t - R - L) / (R - L); };
    Expect(std::fabs(ndc(-0.8f, wide.left, wide.right) + frac) < 1e-4f, "widened left edge");
    Expect(std::fabs(ndc(0.9f, wide.left, wide.right) - frac) < 1e-4f, "widened right edge");
    const auto same = framevr::WidenHorizontal(-0.5f, 0.5f, 4004, 4004);
    Expect(same.left == -0.5f && same.right == 0.5f, "matching canvas is not widened");

    const auto fitted = framevr::Fit16x9(0, 0, 5120, 1440, 0, 0, 0, 0);
    Expect(fitted.ok && fitted.w == 2560 && fitted.h == 1440 && fitted.x == 1280 && fitted.y == 0, "16:9 window fits the monitor");
    const auto framed = framevr::Fit16x9(0, 0, 5120, 1440, 8, 31, 8, 8);
    Expect(framed.ok && framed.w <= 5104 && framed.h <= 1401 && framed.x >= 8 && framed.y >= 31, "frame stays inside the work area");
    Expect(framed.x + framed.w + 8 <= 5120 && framed.y + framed.h + 8 <= 1440, "outer window inside the work area");

    Expect(framevr::MapSpan(0, 2560, 7118) == 0, "mouse origin");
    Expect(framevr::MapSpan(2559, 2560, 7118) == 7117, "mouse far edge");
    Expect(framevr::MapSpan(framevr::MapSpan(1280, 2560, 7118), 7118, 2560) == 1280, "mouse round trip at the center sample");
    return 0;
}
