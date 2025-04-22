//
// Created by Chingis on 18.04.2025.
//

#include "MathHelper.h"

using namespace DirectX;

const float MathHelper::Infinity    = FLT_MAX;
const float MathHelper::Pi          = 3.1415926535f;

float MathHelper::AngleFromXY(float x, float y) {
    float theta = 0.f;

    if(x >= 0.f) {
        theta = atanf(y / x);
        if(theta < 0.f) {
            theta += 2.f * Pi;
        }
    }
    else {
        theta = atanf(y / x) + Pi;
    }

    return theta;
}

DirectX::XMVECTOR MathHelper::RandUnitVec3() {
    XMVECTOR One = XMVectorSet(1.f, 1.f, 1.f, 1.f);
    XMVECTOR Zero = XMVectorZero();

    while (true) {
        XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.f, 1.f), MathHelper::RandF(-1.f, 1.f), MathHelper::RandF(-1.f, 1.f), MathHelper::RandF(-1.f, 1.f));
        if(XMVector3Greater(XMVector3LengthSq(v), One)) {
            continue;
        }

        return XMVector3Normalize(v);
    }
}

DirectX::XMVECTOR MathHelper::RandHemisphereUnitVec3(DirectX::XMVECTOR n) {
    XMVECTOR One = XMVectorSet(1.f, 1.f, 1.f, 1.f);
    XMVECTOR Zero = XMVectorZero();

    while (true) {
        XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.f, 1.f), MathHelper::RandF(-1.f, 1.f), MathHelper::RandF(-1.f, 1.f),MathHelper::RandF(-1.f, 1.f));
        if(XMVector3Greater(XMVector3LengthSq(v), One)) {
            continue;
        }
        if(XMVector3Less(XMVector3Dot(n, v), Zero)) {
            continue;
        }

        return XMVector3Normalize(v);
    }
}

