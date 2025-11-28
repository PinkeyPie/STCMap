#pragma once

#include "DxCommandList.h"
#include "../core/threading.h"
#include "DxContext.h"

extern bool ProfilerWindowOpen;
#if ENABLE_DX_PROFILING

#define COMPOSITE_VARNAME_(a, b) a##b
#define COMPOSITE_VARNAME(a, b) COMPOSITE_VARNAME_(a, b)

#define DX_PROFILE_BLOCK_(counter, cl, name) DxProfileBlockRecorder COMPOSITE_VARNAME(__DX_PROFILE_BLOCK, counter)(cl, name)
#define DX_PROFILE_BLOCK(cl, name) DX_PROFILE_BLOCK_(__COUNTER__, cl, name)

#define MAX_NUM_DX_PROFILE_BLOCKS 2048
#define MAX_NUM_DX_PROFILE_EVENTS (MAX_NUM_DX_PROFILE_BLOCKS * 2) // One for start and end.
#define MAX_NUM_DX_PROFILE_FRAMES 1024

enum EProfileEventType {
    EProfileEventFrameMarker,
    EProfileEventBeginBlock,
    EProfileEventEndBlock,
};

enum EProfileClType {
    EProfileClGraphics,
    EProfileClCompute,

    EProfileClCount,
};

struct DxProfileEvent {
    EProfileEventType Type;
    EProfileClType ClType;
    const char* Name;
    uint64 Timestamp;
};

extern DxProfileEvent ProfileEvents[NUM_BUFFERED_FRAMES][MAX_NUM_DX_PROFILE_EVENTS];

struct DxProfileBlockRecorder {
    DxCommandList* CommandList;
    const char* Name;

    DxProfileBlockRecorder(DxCommandList* commandList, const char* name) : CommandList(commandList), Name(name) {
        DxContext& context = DxContext::Instance();
        uint32 queryIndex = context.IncrementQueryIndex();
        commandList->QueryTimestamp(queryIndex);

        ProfileEvents[context.FrameId()][queryIndex] = { EProfileEventBeginBlock, commandList->Type() == D3D12_COMMAND_LIST_TYPE_DIRECT ? EProfileClGraphics : EProfileClCompute, Name};
    }

    ~DxProfileBlockRecorder() {
        DxContext& context = DxContext::Instance();
        uint32 queryIndex = context.IncrementQueryIndex();
        CommandList->QueryTimestamp(queryIndex);

        ProfileEvents[context.FrameId()][queryIndex] = { EProfileEventEndBlock, CommandList->Type() == D3D12_COMMAND_LIST_TYPE_DIRECT ? EProfileClGraphics : EProfileClCompute, Name};
    }
};

#else

#define DX_PROFILE_BLOCK(cl, name)

#endif