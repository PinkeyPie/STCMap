#ifndef SKINNING_RS_HLSLI
#define SKINNING_RS_HLSLI

#include "../common/common.hlsli"

struct SkinningCb
{
    uint32 FirstJoint;
    uint32 NumJoints;
    uint32 FirstVertex;
    uint32 NumVertices;
    uint32 WriteOffset;
};

#define SKINNING_RS \
"RootConstants(b0, num32BitConstants = 5), " \
"SRV(t0), " \
"SRV(t1), " \
"UAV(u0)"

#define SkinningRsCb			        0
#define SkinningRsInputVertexBuffer     1
#define SkinningRsMatruces              2
#define SkinningRsOutput                3

#endif