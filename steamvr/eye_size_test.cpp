#include "eye_size.h"
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
    return 0;
}
