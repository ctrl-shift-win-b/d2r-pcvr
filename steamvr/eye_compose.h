#pragma once
#include <d3d12.h>
#include <cstdint>
#include <wrl/client.h>

namespace framevr {
struct OverlayRect {
    uint32_t canvasW = 0, canvasH = 0;
    uint32_t eyeW = 0, eyeH = 0;
    // Toolbar strip in canvas pixels. barLift is how many eye pixels it sits above the bottom.
    uint32_t barTop = 0, barBottom = 0, barLift = 0, originX = 0;
    // Location-name rect in canvas pixels. placeDrop moves it down the eye. Zero width skips it.
    uint32_t placeX = 0, placeY = 0, placeW = 0, placeH = 0, placeDrop = 0;
};

// Draw the toolbar across the eye, raised off the bottom, and the location
// name lower than the game draws it. Every other interface pixel stays out.
class EyeCompose {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sourceHeap, targetHeap;
    DXGI_FORMAT targetFormat = DXGI_FORMAT_UNKNOWN;
public:
    bool Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat, void (*log)(const char*));
    bool Render(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12Resource* ui, ID3D12Resource* eye, OverlayRect rect);
};
}
