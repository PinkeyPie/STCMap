#pragma once

#include "../pch.h"
#include "dx.h"

class DxTimestampQueryHeap {
public:
    void Initialize(uint32 maxCount);

    DxQueryHeap Heap;
};
