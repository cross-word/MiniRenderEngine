#pragma once

#include "SimpleLoader.h"
#include <string>
#include <vector>
#include <DirectXMath.h>
#include <dxgiformat.h>

using namespace DirectX;

#ifdef _ENABLE_FILE_LOAD
#  define FileLoad_API __declspec(dllexport)
#else
#  define FileLoad_API __declspec(dllimport)
#endif

struct MaterialTex {
    int baseColor = -1;
    int normal = -1;
    int metallicRoughness = -1;
    int occlusion = -1;
    int emissive = -1;

    XMFLOAT4 baseColorFactor = { 1,1,1,1 };
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;

    int alphaMode = 0;
    float alphaCutoff = 0.5f;
    int doubleSided = 0;
};

struct PrimitiveMeshEx {
    MeshData mesh;
    int material = -1;
};

struct NodeInstance {
    int primitive;
    XMFLOAT4X4 world;
};

struct SceneData {
    std::vector<PrimitiveMeshEx> primitives;
    std::vector<NodeInstance>    instances;
    std::vector<MaterialTex>     materials;
    std::vector<std::wstring>    textures;
};

FileLoad_API SceneData LoadGLTFScene(const std::wstring& filename);