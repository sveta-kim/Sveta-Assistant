#include "rendering/SpriteOverlay.h"

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>

#include "core/Logger.h"

namespace sveta::rendering {

namespace {
const Gdiplus::Color kBarColor(230, 90, 170, 235); // soft blue-violet, mostly opaque

// GDI+ needs exactly one Startup/Shutdown pair per registration; a second,
// independent registration (window/ChatBubble.cpp also has one) is valid
// and keeps this file from silently depending on ChatBubble::Create()
// having already run first to start GDI+ for it.
class GdiplusScope {
public:
    GdiplusScope() {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&token_, &input, nullptr);
    }
    ~GdiplusScope() { Gdiplus::GdiplusShutdown(token_); }
    GdiplusScope(const GdiplusScope&) = delete;
    GdiplusScope& operator=(const GdiplusScope&) = delete;

private:
    ULONG_PTR token_ = 0;
};

void EnsureGdiplusStarted() {
    static GdiplusScope scope;
    (void)scope;
}

} // namespace

std::vector<uint8_t> WithTalkingIndicator(const Sprite& sprite, bool frameA) {
    EnsureGdiplusStarted();

    std::vector<uint8_t> pixels = sprite.PremultipliedBgra(); // mutable copy
    const int width = static_cast<int>(sprite.Width());
    const int height = static_cast<int>(sprite.Height());
    const int destStride = width * 4;

    // Draw into a small, normally-allocated GDI+ bitmap rather than
    // directly onto `pixels`: anti-aliased FillRectangle onto a Bitmap
    // that wraps external premultiplied memory silently draws nothing.
    // Composite the result onto `pixels` by hand afterward instead.
    // Created and locked in the same PixelFormat32bppPARGB (matching
    // window/ChatBubble.cpp's working pattern) so LockBits doesn't need to
    // perform a format conversion.
    const int boxW = std::max(20, static_cast<int>(width * 0.16f));
    const int boxH = std::max(16, static_cast<int>(height * 0.10f));

    Gdiplus::Bitmap overlay(boxW, boxH, PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&overlay);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    // Roughly where the mouth sits on the current artwork, calibrated the
    // same way as the emotion overlays (tools/generate_emotion_sprites.py).
    const float cx = boxW * 0.5f;
    const float baseY = boxH * 0.5f;
    const float barWidth = std::max(2.0f, boxW * 0.12f);
    const float gap = barWidth * 1.8f;
    const float tallBar = boxH * 0.62f;
    const float shortBar = boxH * 0.30f;

    const float heights[3] = {
        frameA ? shortBar : tallBar,
        frameA ? tallBar : shortBar,
        frameA ? shortBar : tallBar,
    };
    const float offsets[3] = {-gap, 0.0f, gap};

    Gdiplus::SolidBrush brush(kBarColor);
    for (int i = 0; i < 3; ++i) {
        Gdiplus::RectF rect(cx + offsets[i] - barWidth / 2, baseY - heights[i] / 2, barWidth, heights[i]);
        graphics.FillRectangle(&brush, rect);
    }

    Gdiplus::BitmapData overlayData{};
    Gdiplus::Rect lockRect(0, 0, boxW, boxH);
    const Gdiplus::Status lockStatus =
        overlay.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &overlayData);
    if (lockStatus != Gdiplus::Ok || !overlayData.Scan0) {
        core::Logger::Warn("WithTalkingIndicator: LockBits failed; showing sprite without the indicator");
        return pixels;
    }

    const int destX = width / 2 - boxW / 2;
    const int destY = static_cast<int>(height * 0.50f) - boxH / 2;

    for (int y = 0; y < boxH; ++y) {
        const int py = destY + y;
        if (py < 0 || py >= height) {
            continue;
        }
        const auto* srcRow = static_cast<const uint8_t*>(overlayData.Scan0) + y * overlayData.Stride;
        uint8_t* dstRow = pixels.data() + py * destStride;
        for (int x = 0; x < boxW; ++x) {
            const int px = destX + x;
            if (px < 0 || px >= width) {
                continue;
            }
            const uint8_t* s = srcRow + x * 4; // premultiplied BGRA
            uint8_t* d = dstRow + px * 4;
            const uint8_t invA = 255 - s[3];
            // "over" compositing of two premultiplied-alpha pixels.
            d[0] = static_cast<uint8_t>(s[0] + (d[0] * invA) / 255);
            d[1] = static_cast<uint8_t>(s[1] + (d[1] * invA) / 255);
            d[2] = static_cast<uint8_t>(s[2] + (d[2] * invA) / 255);
            d[3] = static_cast<uint8_t>(s[3] + (d[3] * invA) / 255);
        }
    }

    overlay.UnlockBits(&overlayData);
    return pixels;
}

} // namespace sveta::rendering
