#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
#include "GLTFLoader.h"
#include <filesystem>
#include <unordered_map>

const uint8_t* GetDataPtr(const tinygltf::Model& model, const tinygltf::Accessor& acc)
{
    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];
    return buf.data.data() + bv.byteOffset + acc.byteOffset;
}

size_t CompSize(int componentType)
{
    switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_FLOAT:         return 4;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:return 2;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return 1;
    default: return 4;
    }
}

size_t ElemCount(int type)
{
    switch (type) {
    case TINYGLTF_TYPE_SCALAR: return 1;
    case TINYGLTF_TYPE_VEC2:   return 2;
    case TINYGLTF_TYPE_VEC3:   return 3;
    case TINYGLTF_TYPE_VEC4:   return 4;
    default: return 0;
    }
}

size_t GetStride(const tinygltf::Model& m, const tinygltf::Accessor& acc)
{
    const auto& bv = m.bufferViews[acc.bufferView];
    size_t implicit = CompSize(acc.componentType) * ElemCount(acc.type);
    return bv.byteStride ? (size_t)bv.byteStride : implicit;
}

DXGI_FORMAT ToIndexFormat(const tinygltf::Accessor& acc)
{
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) return DXGI_FORMAT_R16_UINT;
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   return DXGI_FORMAT_R32_UINT;

    return DXGI_FORMAT_R32_UINT;
}

XMMATRIX NodeLocal(const tinygltf::Node& n)
{
    if (n.matrix.size() == 16)
    {
        XMMATRIX M = XMMatrixSet(
            (float)n.matrix[0], (float)n.matrix[1], (float)n.matrix[2], (float)n.matrix[3],
            (float)n.matrix[4], (float)n.matrix[5], (float)n.matrix[6], (float)n.matrix[7],
            (float)n.matrix[8], (float)n.matrix[9], (float)n.matrix[10], (float)n.matrix[11],
            (float)n.matrix[12], (float)n.matrix[13], (float)n.matrix[14], (float)n.matrix[15]);
        return XMMatrixTranspose(M);
    }
    else
    {
        XMVECTOR S = XMVectorSet(n.scale.size() ? (float)n.scale[0] : 1.f,
            n.scale.size() ? (float)n.scale[1] : 1.f,
            n.scale.size() ? (float)n.scale[2] : 1.f, 0);
        XMVECTOR R = XMQuaternionIdentity();
        if (n.rotation.size() == 4) R = XMVectorSet((float)n.rotation[0], (float)n.rotation[1], (float)n.rotation[2], (float)n.rotation[3]);
        XMVECTOR T = XMVectorSet(n.translation.size() ? (float)n.translation[0] : 0.f,
            n.translation.size() ? (float)n.translation[1] : 0.f,
            n.translation.size() ? (float)n.translation[2] : 0.f, 0);
        return XMMatrixAffineTransformation(S, XMVectorZero(), R, T);
    }
}

// gltf primitive → meshData
MeshData BuildMeshFromPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& prim)
{
    MeshData md;

    auto findAttr = [&](const char* key)->const tinygltf::Accessor*
    {
        auto it = prim.attributes.find(key);
        if (it == prim.attributes.end()) return nullptr;
        return &model.accessors[it->second];
    };

    const tinygltf::Accessor* posAcc = findAttr("POSITION");
    const tinygltf::Accessor* nrmAcc = findAttr("NORMAL");
    const tinygltf::Accessor* texAcc = findAttr("TEXCOORD_0");
    const tinygltf::Accessor* tanAcc = findAttr("TANGENT");

    const size_t vcount = posAcc ? posAcc->count : 0;
    md.vertices.reserve(vcount);

    auto readVec = [&](const tinygltf::Accessor* acc, size_t i, float* dst, int wantN)->bool
    {
        if (!acc) return false;
        const uint8_t* base = GetDataPtr(model, *acc);
        const size_t   stride = GetStride(model, *acc);
        const uint8_t* ptr = base + i * stride;

        switch (acc->componentType)
        {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
            {
                const float* f = reinterpret_cast<const float*>(ptr);
                for (int k = 0; k < wantN; k++) dst[k] = f[k];
                return true;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            {
                const uint16_t* s = reinterpret_cast<const uint16_t*>(ptr);
                const float sc = acc->normalized ? (1.0f / 65535.0f) : 1.0f;
                for (int k = 0; k < wantN; k++) dst[k] = s[k] * sc;
                return true;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            {
                const uint8_t* b = reinterpret_cast<const uint8_t*>(ptr);
                const float sc = acc->normalized ? (1.0f / 255.0f) : 1.0f;
                for (int k = 0; k < wantN; k++) dst[k] = b[k] * sc;
                return true;
            }
            default:
                return false;
        }
    };

    for (size_t i = 0; i < vcount; ++i)
    {
        Vertex v{};
        float tmp[4] = { 0,0,0,0 };

        if (posAcc) { readVec(posAcc, i, tmp, 3); v.position = { tmp[0], tmp[1], tmp[2] }; }
        if (nrmAcc) { readVec(nrmAcc, i, tmp, 3); v.normal = { tmp[0], tmp[1], tmp[2] }; }
        if (texAcc) { readVec(texAcc, i, tmp, 2); v.texC = { tmp[0], tmp[1] }; } // 필요 시 v = 1 - v[1]
        if (tanAcc) { readVec(tanAcc, i, tmp, 4); v.tangentU = { tmp[0], tmp[1], tmp[2] }; }

        md.vertices.push_back(v);
    }

    // 인덱스
    const auto& idxAcc = model.accessors[prim.indices];
    const uint8_t* idxBase = GetDataPtr(model, idxAcc);
    size_t step = GetStride(model, idxAcc);
    if (step == 0) step = CompSize(idxAcc.componentType);

    for (size_t i = 0; i < idxAcc.count; ++i)
    {
        const uint8_t* p = idxBase + i * step;
        if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            md.indices.push_back(*(const uint16_t*)p);
        else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            md.indices.push_back(*(const uint32_t*)p);
        else
            md.indices.push_back(*(const uint8_t*)p);
    }

    return md;
}

void TraverseNode(const tinygltf::Model& model, int nodeIdx, const XMMATRIX& parent, std::vector<std::pair<int, XMFLOAT4X4>>& out)
{
    const auto& n = model.nodes[nodeIdx];
    XMFLOAT4X4 localF;
    XMStoreFloat4x4(&localF, NodeLocal(n));
    XMMATRIX local = XMLoadFloat4x4(&localF);
    XMMATRIX world = XMMatrixMultiply(parent, local);
    XMFLOAT4X4 W; XMStoreFloat4x4(&W, world);

    if (n.mesh >= 0)
    {
        const auto& mesh = model.meshes[n.mesh];
        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi)
        {
            out.emplace_back((int)&mesh.primitives[pi] - (int)&mesh.primitives[0], W); // temp, 실제 매핑은 아래에서
        }
    }
    for (int c : n.children) TraverseNode(model, c, world, out);
}

SceneData LoadGLTFScene(const std::wstring& filename)
{
    SceneData scene;

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    std::filesystem::path path(filename);
    std::string input = path.string();

    bool ok = false;
    if (path.extension() == L".gltf") ok = loader.LoadASCIIFromFile(&model, &err, &warn, input);
    else                              ok = loader.LoadBinaryFromFile(&model, &err, &warn, input);
    if (!ok) return scene;

    auto base = std::filesystem::weakly_canonical(std::filesystem::absolute(path.parent_path()));
    scene.textures.reserve(model.images.size());
    for (const auto& img : model.images)
    {
        std::filesystem::path p = img.uri;
        auto abs = std::filesystem::weakly_canonical(base / p);
        scene.textures.push_back(abs.wstring());
    }

    scene.materials.reserve(model.materials.size());
    for (const auto& m : model.materials)
    {
        MaterialTex mt{};
        if (m.pbrMetallicRoughness.baseColorTexture.index >= 0) mt.baseColor = m.pbrMetallicRoughness.baseColorTexture.index;
        if (m.normalTexture.index >= 0)                         mt.normal = m.normalTexture.index;
        if (m.occlusionTexture.index >= 0)                      mt.occlusion = m.occlusionTexture.index;
        if (m.emissiveTexture.index >= 0)                       mt.emissive = m.emissiveTexture.index;
        if (m.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) mt.metallicRoughness = m.pbrMetallicRoughness.metallicRoughnessTexture.index;

        if (m.pbrMetallicRoughness.baseColorFactor.size() == 4)
        {
            mt.baseColorFactor = XMFLOAT4(
                (float)m.pbrMetallicRoughness.baseColorFactor[0],
                (float)m.pbrMetallicRoughness.baseColorFactor[1],
                (float)m.pbrMetallicRoughness.baseColorFactor[2],
                (float)m.pbrMetallicRoughness.baseColorFactor[3]);
        }
        if (m.pbrMetallicRoughness.metallicFactor > 0)  mt.metallicFactor = (float)m.pbrMetallicRoughness.metallicFactor;
        if (m.pbrMetallicRoughness.roughnessFactor > 0) mt.roughnessFactor = (float)m.pbrMetallicRoughness.roughnessFactor;

        // alpha/doublesided
        if (m.alphaMode == "BLEND") mt.alphaMode = 2;
        else if (m.alphaMode == "MASK")  mt.alphaMode = 1;
        else                             mt.alphaMode = 0;
        mt.alphaCutoff = (float)m.alphaCutoff;
        mt.doubleSided = m.doubleSided ? 1 : 0;

        scene.materials.push_back(mt);
    }

    std::vector<std::pair<int, int>> primMap;
    for (size_t mi = 0; mi < model.meshes.size(); ++mi)
    {
        const auto& mesh = model.meshes[mi];
        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi)
        {
            const auto& prim = mesh.primitives[pi];
            PrimitiveMeshEx pm{};
            pm.mesh = BuildMeshFromPrimitive(model, prim);
            pm.material = prim.material;
            scene.primitives.push_back(std::move(pm));
            primMap.emplace_back((int)mi, (int)pi);
        }
    }

    // 인스턴스(노드 트리 순회)
    // 노드의 mesh가 k번째 mesh이고 primitive pi라면, "mesh 앞의 프리미티브 개수 합 + pi" = scene.primitives 인덱스
    std::vector<int> meshPrimOffset(model.meshes.size(), 0);
    {
        int acc = 0;
        for (size_t mi = 0; mi < model.meshes.size(); ++mi)
        {
            meshPrimOffset[mi] = acc;
            acc += (int)model.meshes[mi].primitives.size();
        }
    }

    std::vector<std::pair<int, XMFLOAT4X4>> instRaw; // (primitiveIdx, world)
    XMMATRIX I = XMMatrixIdentity();
    if (model.scenes.empty())
    {
        // default scene = 0
        if (!model.nodes.empty())
        {
            for (size_t i = 0; i < model.nodes.size(); ++i)
            {
                std::function<void(int, const XMMATRIX&)> dfs = [&](int nodeIdx, const XMMATRIX& parent)
                {
                    const auto& n = model.nodes[nodeIdx];
                    XMFLOAT4X4 lf;
                    XMStoreFloat4x4(&lf, NodeLocal(n));
                    XMMATRIX lw = XMLoadFloat4x4(&lf);
                    XMMATRIX ww = XMMatrixMultiply(parent, lw);
                    if (n.mesh >= 0)
                    {
                        const auto& mesh = model.meshes[n.mesh];
                        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi)
                        {
                            int primIdx = meshPrimOffset[n.mesh] + (int)pi;
                            XMFLOAT4X4 W; XMStoreFloat4x4(&W, ww);
                            instRaw.emplace_back(primIdx, W);
                        }
                    }
                    for (int c : n.children) dfs(c, ww);
                };
                dfs((int)i, I);
            }
        }
    }
    else
    {
        int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
        const auto& sc = model.scenes[sceneIdx];
        for (int root : sc.nodes)
        {
            std::function<void(int, const XMMATRIX&)> dfs = [&](int nodeIdx, const XMMATRIX& parent)
            {
                const auto& n = model.nodes[nodeIdx];
                XMFLOAT4X4 lf;
                XMStoreFloat4x4(&lf, NodeLocal(n));
                XMMATRIX lw = XMLoadFloat4x4(&lf);
                XMMATRIX ww = XMMatrixMultiply(parent, lw);
                if (n.mesh >= 0)
                {
                    const auto& mesh = model.meshes[n.mesh];
                    for (size_t pi = 0; pi < mesh.primitives.size(); ++pi)
                    {
                        int primIdx = meshPrimOffset[n.mesh] + (int)pi;
                        XMFLOAT4X4 W; XMStoreFloat4x4(&W, ww);
                        instRaw.emplace_back(primIdx, W);
                    }
                }
                for (int c : n.children) dfs(c, ww);
            };
            dfs(root, I);
        }
    }

    scene.instances.reserve(instRaw.size());
    for (auto& pr : instRaw)
    {
        NodeInstance ni{};
        ni.primitive = pr.first;
        ni.world = pr.second;
        scene.instances.push_back(ni);
    }

    return scene;
}