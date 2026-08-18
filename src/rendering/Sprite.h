#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace sveta::rendering {

// A decoded PNG held as top-down, premultiplied 32bpp BGRA pixels
// (stride == width * 4) — the exact format UpdateLayeredWindow expects
// when blending with AC_SRC_ALPHA.
class Sprite {
public:
    // maxDimension caps the longer side, preserving aspect ratio; 0 means
    // "use the source image's native size". Never upscales.
    static std::optional<Sprite> LoadFromFile(const std::filesystem::path& path, uint32_t maxDimension = 0);

    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }
    const std::vector<uint8_t>& PremultipliedBgra() const { return pixels_; }

private:
    Sprite(uint32_t width, uint32_t height, std::vector<uint8_t> pixels);

    uint32_t width_;
    uint32_t height_;
    std::vector<uint8_t> pixels_;
};

} // namespace sveta::rendering
