#pragma once
#pragma once

#include "SimpleLoader.h"
#include <string>
#include <vector>
#ifdef _ENABLE_FILE_LOAD

#define FileLoad_API __declspec(dllexport)
#else
#define FileLoad_API __declspec(dllimport)
#endif

struct ModelData {
    MeshData mesh;
    std::vector<std::wstring> textures;
};

FileLoad_API ModelData LoadGLTF(const std::wstring& filename);