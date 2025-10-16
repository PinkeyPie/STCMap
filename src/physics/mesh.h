#pragma once

struct aiScene;

const aiScene* LoadAssimpScene(const char* filepathRaw);
void FreeScene(const aiScene* scene);
void AnalyzeScene(const aiScene* scene);
