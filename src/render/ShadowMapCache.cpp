// #include "ShadowMapCache.h"
// #include "../directx/DxRenderer.h"
//
//
// struct TreeNode {
// 	uint32 X, Y;
// };
//
// static constexpr uint32 maximumSize = 2048;
// static constexpr uint32 minimumSize = 128;
// static_assert(SHADOW_MAP_WIDTH % maximumSize == 0, "");
// static_assert(SHADOW_MAP_HEIGHT % maximumSize == 0, "");
// static_assert(isPowerOfTwo(maximumSize), "");
// static_assert(isPowerOfTwo(minimumSize), "");
//
// static constexpr uint32 numBuckets = IndexOfLeastSignificantSetBit(maximumSize) - IndexOfLeastSignificantSetBit(minimumSize) + 1;
// static std::vector<TreeNode> nodes[numBuckets];
//
// static void Initialize() {
// 	for (uint32 y = 0; y < SHADOW_MAP_HEIGHT / maximumSize; ++y) {
// 		for (uint32 x = 0; x < SHADOW_MAP_WIDTH / maximumSize; ++x) {
// 			nodes[0].push_back({ x * maximumSize, y * maximumSize });
// 		}
// 	}
// }
//
// static uint32 GetIndex(uint32 size) {
// 	uint32 index = IndexOfLeastSignificantSetBit(size) - IndexOfLeastSignificantSetBit(minimumSize);
// 	index = numBuckets - index - 1;
// 	return index;
// }
//
// static TreeNode Insert(uint32 size) {
// 	assert(size <= maximumSize);
// 	assert(size >= minimumSize);
// 	assert(isPowerOfTwo(size));
//
// 	uint32 index = GetIndex(size);
//
// 	uint32 insertIndex = index;
// 	for (; insertIndex != -1 && nodes[insertIndex].size() == 0; --insertIndex);
//
// 	assert(insertIndex != -1);
// 	assert(nodes[insertIndex].size() > 0);
//
//
// 	for (uint32 i = insertIndex; i < index; ++i) {
// 		uint32 nodeSize = 1 << (IndexOfLeastSignificantSetBit(maximumSize) - i);
// 		uint32 halfSize = nodeSize / 2;
//
// 		TreeNode n = nodes[i].back();
// 		nodes[i].pop_back();
//
// 		nodes[i + 1].push_back({ n.X, n.Y });
// 		nodes[i + 1].push_back({ n.X + halfSize, n.Y });
// 		nodes[i + 1].push_back({ n.X, n.Y + halfSize });
// 		nodes[i + 1].push_back({ n.X + halfSize, n.Y + halfSize });
// 	}
//
// 	TreeNode result = nodes[index].back();
// 	nodes[index].pop_back();
// 	return result;
// }
//
// static void Free(TreeNode remove, uint32 size) {
// 	if (size == maximumSize) {
// 		nodes[0].push_back(remove);
// 		return;
// 	}
//
//
// 	uint32 index = GetIndex(size);
// 	std::vector<TreeNode>& list = nodes[index];
// 	if (list.size() == 0) {
// 		list.push_back(remove);
// 		return;
// 	}
//
//
// 	uint32 doubleSize = size * 2;
// 	uint32 shift = IndexOfLeastSignificantSetBit(doubleSize);
//
// 	uint32 parentX = remove.X >> shift;
// 	uint32 parentY = remove.Y >> shift;
// 	uint32 parentWidth = SHADOW_MAP_WIDTH >> shift;
// 	uint32 parentIndex = parentY * parentWidth + parentX;
//
//
// 	uint32 nodesWithThisParent = 0;
// 	auto insertIt = list.end();
//
// 	for (auto it = list.begin(); it != list.end(); ++it) {
// 		TreeNode node = *it;
//
// 		uint32 pX = node.X >> shift;
// 		uint32 pY = node.Y >> shift;
// 		uint32 pi = pY * parentWidth + pX;
//
// 		if (pi == parentIndex)
// 		{
// 			++nodesWithThisParent;
// 		}
// 		else if (pi > parentIndex)
// 		{
// 			insertIt = it;
// 			break;
// 		}
// 	}
//
// 	assert(nodesWithThisParent < 4);
//
// 	if (nodesWithThisParent == 3) {
// 		auto eraseIt = insertIt - 3;
// 		list.erase(eraseIt, insertIt);
// 		Free(TreeNode{ parentX * doubleSize, parentY * doubleSize }, doubleSize);
// 	}
// 	else {
// 		list.insert(insertIt, remove);
// 	}
// }
//
// void ShadowMapLightInfo::TestShadowMapCache(ShadowMapLightInfo* infos, uint32 numInfos) {
// 	Initialize();
// 	auto r = Insert(512);
// 	auto r2 = Insert(1024);
// 	Free(r, 512);
//
// 	for (uint32 i = 0; i < numInfos; ++i) {
// 		ShadowMapLightInfo& info = infos[i];
//
// 		if (info.LightMovedOrAppeared || info.Viewport.CpuVP[3] < 0.f) {
// 			// Render static geometry.
// 			// Cache result.
// 			// Render dynamic geometry.
// 		}
// 		else {
// 			if (info.GeometryInRangeMoved) {
// 				// Copy static cache to active.
// 				// Render dynamic geometry.
// 			}
// 			else {
// 				// Shadow map exists.
// 				// Copy it from back to front buffer?
// 			}
// 		}
// 	}
//
// 	for (uint32 i = 0; i < numInfos; ++i)
// 	{
// 		ShadowMapLightInfo& info = infos[i];
//
// 		// info.Viewport.CpuVP[0] = rect.x;
// 		// info.Viewport.CpuVP[1] = rect.y;
// 		// info.Viewport.CpuVP[2] = rect.w;
// 		// info.Viewport.CpuVP[3] = rect.h;
// 		// info.Viewport.ShaderVP.x = (float)rect.x / SHADOW_MAP_WIDTH;
// 		// info.Viewport.ShaderVP.y = (float)rect.y / SHADOW_MAP_WIDTH;
// 		// info.Viewport.ShaderVP.z = (float)rect.w / SHADOW_MAP_WIDTH;
// 		// info.Viewport.ShaderVP.w = (float)rect.h / SHADOW_MAP_WIDTH;
// 	}
// }
