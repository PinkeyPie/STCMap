#pragma once

#include <d3d12.h>

#if defined(__cplusplus)

struct CD3DX12_DEFAULT {};
extern const DECLSPEC_SELECTANY CD3DX12_DEFAULT D3D12_DEFAULT;

//---------------------------------------------------------------------------------------------------
inline bool operator==(const D3D12_VIEWPORT& l, const D3D12_VIEWPORT& r) {
    return l.TopLeftX == r.TopLeftX and l.TopLeftY == r.TopLeftY and l.Width == r.Width and l.Height == r.Height and l.MinDepth == r.MinDepth and l.MaxDepth == r.MaxDepth;
}

//---------------------------------------------------------------------------------------------------
inline bool operator!=(const D3D12_VIEWPORT& l, const D3D12_VIEWPORT& r) {
    return !(l == r);
}

struct CD3DX12_RECT : public D3D12_RECT {
    CD3DX12_RECT() = default;
    explicit CD3DX12_RECT(const D3D12_RECT& o) : D3D12_RECT(o) {}
    CD3DX12_RECT(LONG Left, LONG Top, LONG Right, LONG Bottom) {
        left = Left;
        top = Top;
        right = Right;
        bottom = Bottom;
    }
    ~CD3DX12_RECT() = default;
    operator const D3D12_RECT&() const { return *this; }
};

//---------------------------------------------------------------------------------------------------
struct CD3DX12_BOX : public D3D12_BOX {
    CD3DX12_BOX() = default;
    explicit CD3DX12_BOX(const D3D12_BOX& o) : D3D12_BOX(o) {}
    CD3DX12_BOX(LONG Left, LONG Right) {
        left = Left;
        top = 0;
        front = 0;
        right = Right;
        bottom = 1;
        back = 1;
    }
    CD3DX12_BOX(LONG Left, LONG Right, LONG Top, LONG Bottom) {
        left = Left;
        right = Right;
        front = 0;
        top = Top;
        bottom = Bottom;
        back = 1;
    }
    CD3DX12_BOX(LONG Left, LONG Top, LONG Front, LONG Right, LONG Bottom, LONG Back) {
        left = Left;
        top = Top;
        front = Front;
        right = Right;
        bottom = Bottom;
        back = Back;
    }
    ~CD3DX12_BOX() = default;
    operator const D3D12_BOX&() const { return *this; }
};

inline bool operator==(const D3D12_BOX& l, const D3D12_BOX& r) {
    return l.left == r.left and l.right == r.right and l.top == r.top and l.bottom == r.bottom and l.front == r.front and l.back == r.back;
}

inline bool operator!=(const D3D12_BOX& l, const D3D12_BOX&r) {
    return !(l == r);
}

//---------------------------------------------------------------------------------------------------
struct CD3DX12_DEPTH_STENCIL_DESC : public D3D12_DEPTH_STENCIL_DESC {
    CD3DX12_DEPTH_STENCIL_DESC() = default;
    explicit CD3DX12_DEPTH_STENCIL_DESC(const D3D12_DEPTH_STENCIL_DESC& o) : D3D12_DEPTH_STENCIL_DESC(o) {}
    explicit CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT) {
        DepthEnable = TRUE;
        DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        StencilEnable = FALSE;
        StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        constexpr D3D12_DEPTH_STENCILOP_DESC defaultDepthStencilOp = {
            D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS
        };
        FrontFace = defaultDepthStencilOp;
        BackFace = defaultDepthStencilOp;
    }
    explicit CD3DX12_DEPTH_STENCIL_DESC(
        BOOL depthEnable,
        D3D12_DEPTH_WRITE_MASK depthWriteMask,
        D3D12_COMPARISON_FUNC depthFunc,
        BOOL stencilEnable,
        UINT8 stencilReadMask,
        UINT8 stencilWriteMask,
        D3D12_STENCIL_OP frontStencilFailOp,
        D3D12_STENCIL_OP frontStencilDepthFailOp,
        D3D12_STENCIL_OP frontStencilPassOp,
        D3D12_COMPARISON_FUNC frontStencilFunc,
        D3D12_STENCIL_OP backStencilFailOp,
        D3D12_STENCIL_OP backStencilDepthFailOp,
        D3D12_STENCIL_OP backStencilPassOp,
        D3D12_COMPARISON_FUNC backStencilFunc
        ) {
        DepthEnable = depthEnable;
        DepthWriteMask = depthWriteMask;
        DepthFunc = depthFunc;
        StencilEnable = stencilEnable;
        StencilReadMask = stencilReadMask;
        StencilWriteMask = stencilWriteMask;
        FrontFace.StencilFailOp = frontStencilFailOp;
        FrontFace.StencilDepthFailOp = frontStencilDepthFailOp;
        FrontFace.StencilPassOp = frontStencilPassOp;
        FrontFace.StencilFunc = frontStencilFunc;
        BackFace.StencilFailOp = backStencilFailOp;
        BackFace.StencilDepthFailOp = backStencilDepthFailOp;
        BackFace.StencilPassOp = backStencilPassOp;
        BackFace.StencilFunc = backStencilFunc;

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList;

    }
    ~CD3DX12_DEPTH_STENCIL_DESC() = default;
    operator const ::D3D12_DEPTH_STENCIL_DESC&() const { return *this; }
};

//---------------------------------------------------------------------------------------------------
struct CD3DX12_BLEND_DESC : public D3D12_BLEND_DESC {

};

#endif
