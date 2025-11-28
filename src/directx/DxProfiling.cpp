//
// Created by Chingis on 19.11.2025.
//

#include "DxProfiling.h"

#if ENABLE_DX_PROFILING

#include <algorithm>

DxProfileEvent ProfileEvents[NUM_BUFFERED_FRAMES][MAX_NUM_DX_PROFILE_EVENTS];

struct DxProfileBlock {
    DxProfileBlock* FirstChild;
    DxProfileBlock* LastChild;
    DxProfileBlock* NextSibling;
    DxProfileBlock* Parent;

    uint64 StartClock;
    uint64 EndClock;

    float RelStart;
    float Duration;

    const char* Name;
};

struct DxProfileFrame {
    DxProfileBlock Blocks[EProfileClCount][MAX_NUM_DX_PROFILE_BLOCKS];

    uint64 StartClock;
    uint64 EndClock;
    uint64 GlobalFrameId;

    float Duration;

    uint32 Count[EProfileClCount];
};

static DxProfileFrame ProfileFrames[MAX_NUM_DX_PROFILE_FRAMES];
static uint32 ProfileFrameWriteIndex;
static bool PauseRecording;

void ProfileFrameMarker(DxCommandList* commandList) {
    assert(commandList->Type() == D3D12_COMMAND_LIST_TYPE_DIRECT);

    uint32 queryIndex = DxContext::Instance().IncrementQueryIndex();
    commandList->QueryTimestamp(queryIndex);

    ProfileEvents[DxContext::Instance().BufferedFrameId()][queryIndex] = { EProfileEventFrameMarker, EProfileClGraphics, "Frame end" };
}

void ResolveTimeStampQueries(uint64* timestamps) {
    uint32 currentFrame = ProfileFrameWriteIndex;
    DxContext& context = DxContext::Instance();

    if (not PauseRecording) {
        uint32 numQueries = context.TimestampQueryIndex();
        if (numQueries == 0) {
            return;
        }

        DxProfileEvent* events = ProfileEvents[context.BufferedFrameId()];
        for (uint32 i = 0; i < numQueries; i++) {
            events[i].Timestamp = timestamps[i];
        }

        std::sort(events, events + numQueries, [](const DxProfileEvent& a, const DxProfileEvent& b) {
            return a.Timestamp < b.Timestamp;
        });

        DxProfileBlock* stack[EProfileClCount][1024];
        uint32 depth[EProfileClCount] = {};
        uint32 count[EProfileClCount] = {};

        for (uint32 cl = 0; cl < EProfileClCount; cl++) {
            stack[cl][0] = 0;
        }

        DxProfileFrame& frame = ProfileFrames[ProfileFrameWriteIndex];

        uint64 frameEndTimestamp = 0;

        for (uint32 i = 0; i < numQueries; i++) {
            DxProfileEvent* e = events + i;
            EProfileClType clType = e->ClType;
            uint32& d = depth[clType];

            switch (e->Type) {
                case EProfileEventBeginBlock: {
                    uint32 index = count[clType]++;
                    DxProfileBlock& block = frame.Blocks[clType][index];

                    block.StartClock = e->Timestamp;
                    block.Parent = (d == 0) ? nullptr : stack[clType][d - 1];
                    block.Name = e->Name;
                    block.FirstChild = nullptr;
                    block.LastChild = nullptr;
                    block.NextSibling = nullptr;

                    if (block.Parent) {
                        if (not block.Parent->FirstChild) {
                            block.Parent->FirstChild = &block;
                        }
                        if (block.Parent->LastChild) {
                            block.Parent->LastChild->NextSibling = &block;
                        }
                        block.Parent->LastChild = &block;
                    }
                    else if (stack[clType][d]) {
                        stack[clType][d]->NextSibling = &block;
                    }

                    stack[clType][d] = &block;
                    d++;
                    break;
                }
                case EProfileEventEndBlock: {
                    d--;

                    DxProfileBlock* block = stack[clType][d];
                    assert(block->Name == e->Name);

                    block->EndClock = e->Timestamp;

                    break;
                }
                case EProfileEventFrameMarker: {
                    frameEndTimestamp = e->Timestamp;
                    break;
                }
            }
        }

        uint32 previousFrameIndex = (ProfileFrameWriteIndex == 0) ? (MAX_NUM_DX_PROFILE_FRAMES - 1) : (ProfileFrameWriteIndex - 1);
        DxProfileFrame& previousFrame = ProfileFrames[previousFrameIndex];

        frame.StartClock = (previousFrame.EndClock == 0) ? frameEndTimestamp : previousFrame.EndClock;
        frame.EndClock = frameEndTimestamp;
        frame.GlobalFrameId = context.FrameId();

        frame.Duration = (float)(frame.EndClock - frame.StartClock) / context.RenderQueue.TimestampFrequency() * 1000.f;

        for (uint32 cl = 0; cl < EProfileClCount; cl++) {
            frame.Count[cl] = count[cl];

            uint64 freq = (cl == EProfileClGraphics) ? context.RenderQueue.TimestampFrequency() : context.ComputeQueue.TimestampFrequency();

            for (uint32 i = 0; i < frame.Count[cl]; i++) {
                DxProfileBlock& block = frame.Blocks[cl][i];
                block.RelStart = float(block.StartClock - frame.StartClock) / freq * 1000.f;
                block.Duration = float(block.EndClock - block.StartClock) / freq * 1000.f;
            }
        }

        ProfileFrameWriteIndex++;
        if (ProfileFrameWriteIndex >= MAX_NUM_DX_PROFILE_FRAMES) {
            ProfileFrameWriteIndex = 0;
        }
    }

    static uint32 highlightFrameIndex = -1;
}

#endif
