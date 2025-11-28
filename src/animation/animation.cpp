#include "animation.h"

#include "../physics/assimp.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

void AnimationSkeleton::PushAssimpAnimation(const char* suffix, const aiAnimation* animation, float scale)
{
	AnimationClip& clip = Clips.emplace_back();

	clip.Name = std::string(animation->mName.C_Str()) + std::string("(") + suffix + ")";
	clip.LengthInSeconds = (float)animation->mDuration * 0.001f;

	float timeNormalization = 1.f / (float)animation->mTicksPerSecond;

	clip.Joints.resize(Joints.size());
	clip.PositionKeyframes.clear();
	clip.RotationKeyframes.clear();
	clip.ScaleKeyframes.clear();

	for (uint32 channelID = 0; channelID < animation->mNumChannels; ++channelID)
	{
		const aiNodeAnim* channel = animation->mChannels[channelID];
		std::string jointName = channel->mNodeName.C_Str();

		auto it = NameToJointId.find(jointName);
		if (it != NameToJointId.end())
		{
			AnimationJoint& joint = clip.Joints[it->second];

			joint.FirstPositionKeyframe = (uint32)clip.PositionKeyframes.size();
			joint.FirstRotationKeyframe = (uint32)clip.RotationKeyframes.size();
			joint.FirstScaleKeyframe = (uint32)clip.ScaleKeyframes.size();

			joint.NumPositionKeyframes = channel->mNumPositionKeys;
			joint.NumRotationKeyframes = channel->mNumRotationKeys;
			joint.NumScaleKeyframes = channel->mNumScalingKeys;


			for (uint32 keyID = 0; keyID < channel->mNumPositionKeys; ++keyID)
			{
				clip.PositionKeyframes.push_back(ReadAssimpVector(channel->mPositionKeys[keyID].mValue));
				clip.PositionTimestamps.push_back((float)channel->mPositionKeys[keyID].mTime * 0.001f);
			}

			for (uint32 keyID = 0; keyID < channel->mNumRotationKeys; ++keyID)
			{
				clip.RotationKeyframes.push_back(ReadAssimpQuaternion(channel->mRotationKeys[keyID].mValue));
				clip.RotationTimestamps.push_back((float)channel->mRotationKeys[keyID].mTime * 0.001f);
			}

			for (uint32 keyID = 0; keyID < channel->mNumScalingKeys; ++keyID)
			{
				clip.ScaleKeyframes.push_back(ReadAssimpVector(channel->mScalingKeys[keyID].mValue));
				clip.ScaleTimestamps.push_back((float)channel->mScalingKeys[keyID].mTime * 0.001f);
			}

			joint.IsAnimated = true;
		}
	}

	for (uint32 i = 0; i < (uint32)Joints.size(); ++i)
	{
		if (Joints[i].ParentId == NO_PARENT)
		{
			for (uint32 keyID = 0; keyID < clip.Joints[i].NumPositionKeyframes; ++keyID)
			{
				clip.PositionKeyframes[clip.Joints[i].FirstPositionKeyframe + keyID] *= scale;
			}
			for (uint32 keyID = 0; keyID < clip.Joints[i].NumScaleKeyframes; ++keyID)
			{
				clip.ScaleKeyframes[clip.Joints[i].FirstScaleKeyframe + keyID] *= scale;
			}
		}
	}

	NameToClipId[clip.Name] = (uint32)Clips.size() - 1;
}

void AnimationSkeleton::PushAssimpAnimations(const char* sceneFilename, float scale)
{
	Assimp::Importer importer;

	const aiScene* scene = LoadAssimpSceneFile(sceneFilename, importer);

	if (scene)
	{
		for (uint32 i = 0; i < scene->mNumAnimations; ++i)
		{
			PushAssimpAnimation(sceneFilename, scene->mAnimations[i], scale);
		}
	}
}

void AnimationSkeleton::PushAssimpAnimationsInDirectory(const char* directory, float scale)
{
	for (auto& p : fs::directory_iterator(directory))
	{
		PushAssimpAnimations(p.path().string().c_str());
	}
}

static void readAssimpSkeletonHierarchy(const aiNode* node, AnimationSkeleton& skeleton, uint32& insertIndex, uint32 parentID = NO_PARENT)
{
	std::string name = node->mName.C_Str();

	if (name == "Animation") // TODO: Temporary fix for the pilot.fbx mesh.
	{
		return;
	}

	auto it = skeleton.NameToJointId.find(name);
	if (it != skeleton.NameToJointId.end())
	{
		uint32 jointID = it->second;

		skeleton.Joints[jointID].ParentId = parentID;

		// This sorts the joints, such that parents are before their children.
		skeleton.NameToJointId[name] = insertIndex;
		skeleton.NameToJointId[skeleton.Joints[insertIndex].Name] = jointID;
		std::swap(skeleton.Joints[jointID], skeleton.Joints[insertIndex]);

		parentID = insertIndex;

		++insertIndex;
	}

	for (uint32 i = 0; i < node->mNumChildren; ++i)
	{
		readAssimpSkeletonHierarchy(node->mChildren[i], skeleton, insertIndex, parentID);
	}
}

void AnimationSkeleton::LoadFromAssimp(const aiScene* scene, float scale)
{
	mat4 scaleMatrix = mat4::identity * (1.f / scale);
	scaleMatrix.m33 = 1.f;

	for (uint32 meshID = 0; meshID < scene->mNumMeshes; ++meshID)
	{
		const aiMesh* mesh = scene->mMeshes[meshID];

		for (uint32 boneID = 0; boneID < mesh->mNumBones; ++boneID)
		{
			const aiBone* bone = mesh->mBones[boneID];
			std::string name = bone->mName.C_Str();

			auto it = NameToJointId.find(name);
			if (it == NameToJointId.end())
			{
				NameToJointId[name] = (uint32)Joints.size();

				SkeletonJoint& joint = Joints.emplace_back();
				joint.Name = std::move(name);
				joint.InvBindMatrix = ReadAssimpMatrix(bone->mOffsetMatrix) * scaleMatrix;
				joint.BindTransform = trs(invert(joint.InvBindMatrix));
			}
#if 0
			else
			{
				mat4 invBind = readAssimpMatrix(bone->mOffsetMatrix) * scaleMatrix;
				assert(invBind == joints[it->second].invBindMatrix);
			}
#endif
		}
	}

	uint32 insertIndex = 0;
	readAssimpSkeletonHierarchy(scene->mRootNode, *this, insertIndex);
}

static vec3 samplePosition(const AnimationClip& clip, const AnimationJoint& animJoint, float time)
{
	if (animJoint.NumPositionKeyframes == 1)
	{
		return clip.PositionKeyframes[animJoint.FirstPositionKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.NumPositionKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.FirstPositionKeyframe;
		if (time < clip.PositionTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.PositionTimestamps[firstKeyframeIndex], clip.PositionTimestamps[secondKeyframeIndex], time);

	vec3 a = clip.PositionKeyframes[firstKeyframeIndex];
	vec3 b = clip.PositionKeyframes[secondKeyframeIndex];

	return lerp(a, b, t);
}

static quat sampleRotation(const AnimationClip& clip, const AnimationJoint& animJoint, float time)
{
	if (animJoint.NumRotationKeyframes == 1)
	{
		return clip.RotationKeyframes[animJoint.FirstRotationKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.NumRotationKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.FirstRotationKeyframe;
		if (time < clip.RotationTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.RotationTimestamps[firstKeyframeIndex], clip.RotationTimestamps[secondKeyframeIndex], time);

	quat a = clip.RotationKeyframes[firstKeyframeIndex];
	quat b = clip.RotationKeyframes[secondKeyframeIndex];

	if (dot(a.v4, b.v4) < 0.f)
	{
		b.v4 *= -1.f;
	}

	return lerp(a, b, t);
}

static vec3 sampleScale(const AnimationClip& clip, const AnimationJoint& animJoint, float time)
{
	if (animJoint.NumScaleKeyframes == 1)
	{
		return clip.ScaleKeyframes[animJoint.FirstScaleKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.NumScaleKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.FirstScaleKeyframe;
		if (time < clip.ScaleTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.ScaleTimestamps[firstKeyframeIndex], clip.ScaleTimestamps[secondKeyframeIndex], time);

	vec3 a = clip.ScaleKeyframes[firstKeyframeIndex];
	vec3 b = clip.ScaleKeyframes[secondKeyframeIndex];

	return lerp(a, b, t);
}

void AnimationSkeleton::SampleAnimation(const std::string& name, float time, trs* outLocalTransforms) const
{
	auto clipIndexIt = NameToClipId.find(name);
	assert(clipIndexIt != NameToClipId.end());

	const AnimationClip& clip = Clips[clipIndexIt->second];
	assert(clip.Joints.size() == Joints.size());

	time = fmod(time, clip.LengthInSeconds);

	uint32 numJoints = (uint32)Joints.size();
	for (uint32 i = 0; i < numJoints; ++i)
	{
		const AnimationJoint& animJoint = clip.Joints[i];

		if (animJoint.IsAnimated)
		{
			outLocalTransforms[i].position = samplePosition(clip, animJoint, time);
			outLocalTransforms[i].rotation = sampleRotation(clip, animJoint, time);
			outLocalTransforms[i].scale = sampleScale(clip, animJoint, time);
		}
		else
		{
			outLocalTransforms[i] = trs::identity;
		}
	}
}

void AnimationSkeleton::GetSkinningMatricesFromLocalTransforms(const trs* localTransforms, mat4* outSkinningMatrices, const trs& worldTransform) const
{
	uint32 numJoints = (uint32)Joints.size();
	trs* globalTransforms = (trs*)alloca(sizeof(trs) * numJoints);

	for (uint32 i = 0; i < numJoints; ++i)
	{
		const SkeletonJoint& skelJoint = Joints[i];
		if (skelJoint.ParentId != NO_PARENT)
		{
			assert(i > skelJoint.ParentId); // Parent already processed.
			globalTransforms[i] = globalTransforms[skelJoint.ParentId] * localTransforms[i];
		}
		else
		{
			globalTransforms[i] = worldTransform * localTransforms[i];
		}

		outSkinningMatrices[i] = trsToMat4(globalTransforms[i]) * Joints[i].InvBindMatrix;
	}
}

static void prettyPrint(const AnimationSkeleton& skeleton, uint32 parent, uint32 indent)
{
	for (uint32 i = 0; i < (uint32)skeleton.Joints.size(); ++i)
	{
		if (skeleton.Joints[i].ParentId == parent)
		{
			std::cout << std::string(indent, ' ') << skeleton.Joints[i].Name << std::endl;
			prettyPrint(skeleton, i, indent + 1);
		}
	}
}

void AnimationSkeleton::PrettyPrintHierarchy() const
{
	prettyPrint(*this, NO_PARENT, 0);
}
