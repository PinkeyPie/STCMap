#pragma once

#include "../pch.h"

struct SkinningWeights {
	uint8 SkinIndices[4];
	uint8 SkinWeights[4];
};

struct SkeletonJoint {
	char Name[16];
	uint32 ParentId;
	
};