#pragma once

#include <unordered_map>

#include "../pch.h"
#include "../core/math.h"

#define NO_PARENT 0xffffffff

struct SkinningWeights {
	uint8 SkinIndices[4];
	uint8 SkinWeights[4];
};

struct SkeletonJoint {
	std::string Name;
	uint32 ParentId;
	trs BindTransform; // Position of joint relative to model space.
	mat4 InvBindMatrix; // Transforms from model space to joint space.
};

struct AnimationJoint {
	bool IsAnimated = false;

	uint32 FirstPositionKeyframe;
	uint32 NumPositionKeyframes;

	uint32 FirstRotationKeyframe;
	uint32 NumRotationKeyframes;

	uint32 FirstScaleKeyframe;
	uint32 NumScaleKeyframes;
};

struct AnimationClip {
	std::string Name;

	std::vector<float> PositionTimestamps;
	std::vector<float> RotationTimestamps;
	std::vector<float> ScaleTimestamps;

	std::vector<vec3> PositionKeyframes;
	std::vector<quat> RotationKeyframes;
	std::vector<vec3> ScaleKeyframes;

	std::vector<AnimationJoint> Joints;

	float LengthInSeconds;
};

struct AnimationSkeleton {
	std::vector<SkeletonJoint> Joints;
	std::unordered_map<std::string, uint32> NameToJointId;

	std::vector<AnimationClip> Clips;
	std::unordered_map<std::string, uint32> NameToClipId;

	void LoadFromAssimp(const struct aiScene* scene, float scale = 1.f);
	void PushAssimpAnimation(const char* suffix, const struct aiAnimation* animation, float scale = 1.f);
	void PushAssimpAnimations(const char* sceneFilename, float scale = 1.f);
	void PushAssimpAnimationsInDirectory(const char* directory, float scale = 1.f);

	void SampleAnimation(const std::string& name, float time, trs* outLocalTransforms) const;
	void GetSkinningMatricesFromLocalTransforms(const trs* localTransforms, mat4* outSkinningMatrices, const trs& worldTransform = trs::identity) const;

	void PrettyPrintHierarchy() const;
};