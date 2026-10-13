#include "eye_compose.h"
#include <d3dcompiler.h>
#include <cstdio>

namespace framevr {
using Microsoft::WRL::ComPtr;

bool EyeCompose::Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat, void (*log)(const char*)) {
    if (pipeline && targetFormat == rtvFormat) return true;
    *this = {};
    targetFormat = rtvFormat;
    auto fail = [&]() { *this = {}; if (log) log("Frame OpenXR: UI overlay unavailable; the center eye image is kept"); return false; };
    struct Library { HMODULE module; ~Library() { if (module) FreeLibrary(module); } } compiler{LoadLibraryW(L"d3dcompiler_47.dll")};
    const auto compile = compiler.module ? reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(compiler.module, "D3DCompile")) : nullptr;
    const auto d12 = GetModuleHandleW(L"d3d12.dll");
    const auto serialize = d12 ? reinterpret_cast<decltype(&D3D12SerializeRootSignature)>(GetProcAddress(d12, "D3D12SerializeRootSignature")) : nullptr;
    if (!compile || !serialize || rtvFormat == DXGI_FORMAT_UNKNOWN) return fail();
    constexpr char shader[] = R"(
Texture2D<float4> uiTex : register(t0);
SamplerState samp : register(s0);
cbuffer Rect : register(b0) {
    uint canvasW, canvasH, eyeW, eyeH, barTop, barBottom, barLift, originX;
    uint placeX, placeY, placeW, placeH, placeDrop;
};
float4 VS(uint id : SV_VertexID) : SV_Position {
    return float4(id == 2 ? 3 : -1, id == 1 ? -3 : 1, 0, 1);
}
float3 ToLinear(float3 encoded) {
    return lerp(pow((encoded + 0.055) / 1.055, 2.4), encoded / 12.92, encoded <= 0.04045);
}
float4 DrawAt(float srcX, float srcY) {
    uint tw, th; uiTex.GetDimensions(tw, th);
    if (srcX < 0 || srcY < 0 || srcX >= float(tw) || srcY >= float(th) || srcY >= float(canvasH)) discard;
    float4 c = uiTex.Load(int3(uint2(srcX, srcY), 0));
    float opacity = saturate(1 - c.a);
    if (opacity < 1.0 / 255.0) discard;
    float3 encoded = max(c.rgb, 0) / max(opacity, 1e-6);
    return float4(ToLinear(encoded) * opacity, c.a);
}
float4 PS(float4 position : SV_Position) : SV_Target {
    float2 eyePx = position.xy;
    if (placeW && placeH) {
        float left = float(placeX) - float(originX);
        float top = float(placeY) + float(placeDrop);
        if (eyePx.x >= left && eyePx.x < left + float(placeW) &&
            eyePx.y >= top && eyePx.y < top + float(placeH))
            return DrawAt(float(placeX) + (eyePx.x - left), float(placeY) + (eyePx.y - top));
    }
    if (!canvasW || !eyeW || barBottom <= barTop) discard;
    float fit = float(eyeW) / float(canvasW);
    float bandH = float(barBottom - barTop) * fit;
    float lift = float(barLift);
    float limit = float(eyeH) - bandH;
    if (limit < 0) limit = 0;
    if (lift > limit) lift = limit;
    float y0 = float(eyeH) - bandH - lift;
    if (eyePx.y < y0 || eyePx.y >= y0 + bandH) discard;
    float srcX = eyePx.x / fit;
    float srcY = float(barTop) + (eyePx.y - y0) / fit;
    if (srcY < float(barTop) || srcY >= float(barBottom)) discard;
    return DrawAt(srcX, srcY);
}
)";
    ComPtr<ID3DBlob> vs, ps, errors, signature;
    const auto v = compile(shader, sizeof(shader) - 1, "eye-ui", nullptr, nullptr, "VS", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs, &errors);
    const auto p = compile(shader, sizeof(shader) - 1, "eye-ui", nullptr, nullptr, "PS", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps, &errors);
    if (FAILED(v) || FAILED(p)) {
        if (log && errors) log(static_cast<const char*>(errors->GetBufferPointer()));
        return fail();
    }
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants = {0, 0, 13};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2;
    desc.pParameters = parameters;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    if (FAILED(serialize(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors)) ||
        FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root))))
        return fail();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
    state.pRootSignature = root.Get();
    state.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    state.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    auto& blend = state.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_ONE;
    blend.DestBlend = D3D12_BLEND_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    state.SampleMask = UINT_MAX;
    state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    state.RasterizerState.DepthClipEnable = TRUE;
    state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    state.NumRenderTargets = 1;
    state.RTVFormats[0] = rtvFormat;
    state.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline)))) return fail();
    D3D12_DESCRIPTOR_HEAP_DESC heap{};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap.NumDescriptors = 1;
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&sourceHeap)))) return fail();
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&targetHeap)))) return fail();
    if (log) log("Frame OpenXR: toolbar is head-locked across the bottom of each eye");
    return true;
}

namespace {
bool Rgba8Family(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_R8G8B8A8_TYPELESS || format == DXGI_FORMAT_R8G8B8A8_UNORM ||
           format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}
// SteamVR's GetDesc on an sRGB swapchain image reports typeless or UNORM.
bool EyeTargetMatches(DXGI_FORMAT wanted, DXGI_FORMAT actual) {
    return actual == wanted || (Rgba8Family(wanted) && Rgba8Family(actual));
}
}

bool EyeCompose::Render(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12Resource* ui, ID3D12Resource* eye, OverlayRect rect) {
    if (!pipeline || !ui || !eye || !rect.canvasW || !rect.eyeW || rect.barBottom <= rect.barTop) return false;
    const auto source = ui->GetDesc(), target = eye->GetDesc();
    if (source.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || !EyeTargetMatches(targetFormat, target.Format)) return false;
    if (target.Width != rect.eyeW || target.Height != rect.eyeH) return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(ui, &srv, sourceHeap->GetCPUDescriptorHandleForHeapStart());
    D3D12_RENDER_TARGET_VIEW_DESC rtv{};
    rtv.Format = targetFormat;
    rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(eye, &rtv, targetHeap->GetCPUDescriptorHandleForHeapStart());
    auto handle = targetHeap->GetCPUDescriptorHandleForHeapStart();
    list->OMSetRenderTargets(1, &handle, FALSE, nullptr);
    const D3D12_VIEWPORT vp{0, 0, float(rect.eyeW), float(rect.eyeH), 0, 1};
    const D3D12_RECT sc{0, 0, LONG(rect.eyeW), LONG(rect.eyeH)};
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);
    list->SetGraphicsRootSignature(root.Get());
    list->SetPipelineState(pipeline.Get());
    ID3D12DescriptorHeap* heaps[] = {sourceHeap.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetGraphicsRootDescriptorTable(0, sourceHeap->GetGPUDescriptorHandleForHeapStart());
    const uint32_t constants[13] = {
        rect.canvasW, rect.canvasH, rect.eyeW, rect.eyeH, rect.barTop, rect.barBottom, rect.barLift, rect.originX,
        rect.placeX, rect.placeY, rect.placeW, rect.placeH, rect.placeDrop};
    list->SetGraphicsRoot32BitConstants(1, 13, constants, 0);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    return true;
}
}
