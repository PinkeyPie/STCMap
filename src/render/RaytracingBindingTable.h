#pragma once

template <typename ShaderData>
struct RaytracingBindingTable {
    void Initialize(DxRaytracingPipeline* pipeline);

    // This expects an array of length numRayTypes, i.e. shader data for all hit groups.
    void Push(const ShaderData* sd);

    void Build();

    Ptr<DxBuffer> GetBuffer() { return _bindingTableBuffer; }
    uint32 GetNumberOfHitGroups() { return _currentHitGroup; }

private:

    struct alignas(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) BindingTableEntry {
        uint8 Identifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
        ShaderData ShaderData;
    };

    void Allocate();

    void* _bindingTable = 0;
    uint32 _totalBindingTableSize;

    uint32 _maxNumHitGroups;
    uint32 _currentHitGroup = 0;

    uint32 _numRayTypes;

    BindingTableEntry* _raygen;
    BindingTableEntry* _miss;
    BindingTableEntry* _hit;

    DxRaytracingPipeline* _pipeline;

    Ptr<DxBuffer> _bindingTableBuffer;
};


template<typename ShaderData>
inline void RaytracingBindingTable<ShaderData>::Initialize(DxRaytracingPipeline* pipeline) {
    _pipeline = pipeline;

    assert(pipeline->ShaderBindingTableDesc.EntrySize == sizeof(BindingTableEntry));

    _numRayTypes = (uint32)pipeline->ShaderBindingTableDesc.HitGroups.size();
    _maxNumHitGroups = 1024;
    Allocate();

    memcpy(_raygen->Identifier, pipeline->ShaderBindingTableDesc.RayGen, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    for (uint32 i = 0; i < _numRayTypes; ++i) {
        BindingTableEntry* m = _miss + i;
        memcpy(m->Identifier, pipeline->ShaderBindingTableDesc.Miss[i], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
}

template<typename ShaderData>
inline void RaytracingBindingTable<ShaderData>::Allocate() {
    _totalBindingTableSize = (uint32)
        (AlignTo(sizeof(BindingTableEntry), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT)
            + AlignTo(sizeof(BindingTableEntry) * _numRayTypes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT)
            + AlignTo(sizeof(BindingTableEntry) * _numRayTypes * _maxNumHitGroups, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

    if (!_bindingTable) {
        _bindingTable = _aligned_malloc(_totalBindingTableSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    }
    else {
        _bindingTable = _aligned_realloc(_bindingTable, _totalBindingTableSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    }

    assert(_pipeline->ShaderBindingTableDesc.RayGenOffset == 0);
    assert(_pipeline->ShaderBindingTableDesc.MissOffset == AlignTo(sizeof(BindingTableEntry), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    assert(_pipeline->ShaderBindingTableDesc.HitOffset == AlignTo((1 + _numRayTypes) * sizeof(BindingTableEntry), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

    _raygen = (BindingTableEntry*)_bindingTable;
    _miss = (BindingTableEntry*)AlignTo(_raygen + 1, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    _hit = (BindingTableEntry*)AlignTo(_miss + _numRayTypes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
}

template<typename ShaderData>
inline void RaytracingBindingTable<ShaderData>::Push(const ShaderData* sd)
{
    if (_currentHitGroup >= _maxNumHitGroups)
    {
        _maxNumHitGroups *= 2;
        Allocate();
    }

    BindingTableEntry* base = _hit + (_numRayTypes * _currentHitGroup);
    ++_currentHitGroup;

    for (uint32 i = 0; i < _numRayTypes; ++i)
    {
        BindingTableEntry* h = base + i;

        memcpy(h->Identifier, _pipeline->ShaderBindingTableDesc.HitGroups[i].Mesh, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        h->ShaderData = sd[i];
    }
}

template<typename ShaderData>
inline void RaytracingBindingTable<ShaderData>::Build() {
    _bindingTableBuffer = DxBuffer::Create(1, _totalBindingTableSize, _bindingTable);
}
