#pragma once

#include "window/d3dApp.h"

using namespace DirectX;

struct Vertex1 {
    XMFLOAT4 Pos;
    XMFLOAT4 Color;
};

struct Vertex2 {
    XMFLOAT3 Pos;
    XMFLOAT3 Norm;
    XMFLOAT2 Tex0;
    XMFLOAT2 Tex1;
};

class InitAppWindow : public D3DApp {
public:
    InitAppWindow();
    ~InitAppWindow() override;

    bool Initialize() override;
private:
    void OnResize() override;
    void Update(const GameTimer &gt) override;
    void Draw(const GameTimer &gt) override;
};