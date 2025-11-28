//
// Created by Chingis on 19.11.2025.
//

#include "DxQuery.h"
#include "DxContext.h"

void DxTimestampQueryHeap::Initialize(uint32 maxCount) {
    D3D12_QUERY_HEAP_DESC desc;
    desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    desc.Count = maxCount;
    desc.NodeMask = 0;

    ThrowIfFailed(DxContext::Instance().GetDevice()->CreateQueryHeap(&desc, IID_PPV_ARGS(&Heap)));
}
