#include "rendering/Sprite.h"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <format>

#include <algorithm>

#include "core/Logger.h"

using Microsoft::WRL::ComPtr;

namespace sveta::rendering {

namespace {

// CoInitializeEx must be paired with CoUninitialize for every successful
// return (S_OK or S_FALSE) — only a failed call (e.g. RPC_E_CHANGED_MODE)
// must not be uninitialized.
class ComScope {
public:
    ComScope() : initialized_(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {}
    ~ComScope() {
        if (initialized_) {
            CoUninitialize();
        }
    }
    ComScope(const ComScope&) = delete;
    ComScope& operator=(const ComScope&) = delete;

private:
    bool initialized_;
};

} // namespace

Sprite::Sprite(uint32_t width, uint32_t height, std::vector<uint8_t> pixels)
    : width_(width), height_(height), pixels_(std::move(pixels)) {}

std::optional<Sprite> Sprite::LoadFromFile(const std::filesystem::path& path, uint32_t maxDimension) {
    ComScope comScope;

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        core::Logger::Error(std::format("Failed to create WIC factory (hr=0x{:08X})", static_cast<unsigned>(hr)));
        return std::nullopt;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        core::Logger::Error(std::format("Failed to decode sprite '{}' (hr=0x{:08X})", path.string(), static_cast<unsigned>(hr)));
        return std::nullopt;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        core::Logger::Error(std::format("Failed to get sprite frame '{}' (hr=0x{:08X})", path.string(), static_cast<unsigned>(hr)));
        return std::nullopt;
    }

    ComPtr<IWICBitmapSource> source = frame;

    UINT sourceWidth = 0;
    UINT sourceHeight = 0;
    hr = frame->GetSize(&sourceWidth, &sourceHeight);
    if (FAILED(hr) || sourceWidth == 0 || sourceHeight == 0) {
        core::Logger::Error(std::format("Sprite '{}' has invalid size", path.string()));
        return std::nullopt;
    }

    const UINT longerSide = std::max(sourceWidth, sourceHeight);
    if (maxDimension > 0 && longerSide > maxDimension) {
        const double scale = static_cast<double>(maxDimension) / static_cast<double>(longerSide);
        const UINT scaledWidth = std::max(1u, static_cast<UINT>(sourceWidth * scale + 0.5));
        const UINT scaledHeight = std::max(1u, static_cast<UINT>(sourceHeight * scale + 0.5));

        ComPtr<IWICBitmapScaler> scaler;
        hr = factory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) {
            hr = scaler->Initialize(frame.Get(), scaledWidth, scaledHeight, WICBitmapInterpolationModeFant);
        }
        if (SUCCEEDED(hr)) {
            source = scaler;
        } else {
            core::Logger::Warn(std::format("Failed to downscale sprite '{}'; using native size (hr=0x{:08X})", path.string(), static_cast<unsigned>(hr)));
        }
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        core::Logger::Error("Failed to create WIC format converter");
        return std::nullopt;
    }

    // 32bppPBGRA == premultiplied alpha, exactly what UpdateLayeredWindow needs.
    hr = converter->Initialize(
        source.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
        nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        core::Logger::Error(std::format("Failed to convert sprite '{}' to BGRA (hr=0x{:08X})", path.string(), static_cast<unsigned>(hr)));
        return std::nullopt;
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        core::Logger::Error(std::format("Sprite '{}' has invalid size", path.string()));
        return std::nullopt;
    }

    const UINT stride = width * 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(stride) * height);
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) {
        core::Logger::Error(std::format("Failed to copy sprite pixels '{}' (hr=0x{:08X})", path.string(), static_cast<unsigned>(hr)));
        return std::nullopt;
    }

    core::Logger::Info(std::format("Loaded sprite '{}' ({}x{})", path.string(), width, height));
    return Sprite(width, height, std::move(pixels));
}

} // namespace sveta::rendering
