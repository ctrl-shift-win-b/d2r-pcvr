#pragma once
#include <d3d12.h>
#include <cstdint>
#include <wrl/client.h>

namespace framevr {
struct OverlayRect {
    uint32_t canvasW = 0, canvasH = 0;
    uint32_t eyeW = 0, eyeH = 0;
    // Toolbar strip in canvas pixels, the same rect the interface clear uses.
    uint32_t barTop = 0, barBottom = 0;
};

// Draw the toolbar (orbs, belt, skills) across the bottom of a square eye.
// The band is the full canvas width, including the center, width-fitted and
// bottom-aligned. Every other interface pixel stays out, so area-name text
// and name tags are not drawn a second time.
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
