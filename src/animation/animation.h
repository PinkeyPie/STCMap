#pragma once

#include "../pch.h"
#include "../core/math.h"

struct SkinningWeights {
	uint8 SkinIndices[4];
	uint8 SkinWeights[4];
};

struct SkeletonJoint {
	char Name[16];
	uint32 ParentId;
	trs BindTransform; // Position of joint relative to model space.
	mat4 InvBindMatrix; // Transforms from model space to joint space.
};

struct AnimationSkeleton {
	SkeletonJoint* Joints;
	uint32 NumJoints;
};

struct AnimationPositionKeyframe {
	vec3 Position;
	float Time;
};

struct AnimationRotationKeyframe {
	quat Rotation;
	float Time;
};

struct AnimationScaleKeyframe {
	float Scale;
	float Time;
};

struct AnimationJoint {
	AnimationPositionKeyframe* Positions;
	AnimationRotationKeyframe* Rotations;
	AnimationScaleKeyframe* Scales;

	uint32 NumPositions;
	uint32 NumRotations;
	uint32 NumScales;
};

struct AnimationClip {
	char Name[16];
	float Length;
	bool Looping;

	AnimationJoint* Joints;
	uint32 NumJoints;

	AnimationPositionKeyframe* AllPositionsKeyframes;
	AnimationRotationKeyframe* AllRotationKeyframes;
	AnimationScaleKeyframe* AllScaleKeyframes;

	uint32 TotalNumberOfPositionKeyframes;
	uint32 TotalNumberOfRotationKeyframes;
	uint32 TotalNumberOfScaleKeyframes;
};