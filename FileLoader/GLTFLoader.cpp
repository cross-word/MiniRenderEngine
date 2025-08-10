#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
#include "GLTFLoader.h"
#include <filesystem>

ModelData LoadGLTF(const std::wstring& filename) {
    ModelData data;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;
    
    std::filesystem::path path(filename);
    std::string input = path.string();
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, input);
    if (!ret) {
        return data;
    }
    if (model.meshes.empty()) return data;
    const tinygltf::Primitive& primitive = model.meshes[0].primitives[0];

    auto getAccessorData = [&](const tinygltf::Accessor* accessor) -> const unsigned char* {
        if (!accessor) return nullptr;
        const tinygltf::BufferView& view = model.bufferViews[accessor->bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];
        return &buffer.data[view.byteOffset + accessor->byteOffset];
        };

    auto attrPos = primitive.attributes.find("POSITION");
    auto attrNorm = primitive.attributes.find("NORMAL");
    auto attrTex = primitive.attributes.find("TEXCOORD_0");
    auto attrTan = primitive.attributes.find("TANGENT");

    const tinygltf::Accessor* posAccessor = attrPos != primitive.attributes.end() ? &model.accessors[attrPos->second] : nullptr;
    const tinygltf::Accessor* normAccessor = attrNorm != primitive.attributes.end() ? &model.accessors[attrNorm->second] : nullptr;
    const tinygltf::Accessor* texAccessor = attrTex != primitive.attributes.end() ? &model.accessors[attrTex->second] : nullptr;
    const tinygltf::Accessor* tanAccessor = attrTan != primitive.attributes.end() ? &model.accessors[attrTan->second] : nullptr;

    const float* pos = reinterpret_cast<const float*>(getAccessorData(posAccessor));
    const float* norm = reinterpret_cast<const float*>(getAccessorData(normAccessor));
    const float* tex = reinterpret_cast<const float*>(getAccessorData(texAccessor));
    const float* tan = reinterpret_cast<const float*>(getAccessorData(tanAccessor));

    size_t vertexCount = posAccessor ? posAccessor->count : 0;
    data.mesh.vertices.reserve(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        Vertex v{};
        if (pos) v.position = XMFLOAT3(pos[i * 3 + 0], pos[i * 3 + 1], pos[i * 3 + 2]);
        if (norm) v.normal = XMFLOAT3(norm[i * 3 + 0], norm[i * 3 + 1], norm[i * 3 + 2]);
        if (tex) v.texC = XMFLOAT2(tex[i * 2 + 0], tex[i * 2 + 1]);
        if (tan) v.tangentU = XMFLOAT3(tan[i * 3 + 0], tan[i * 3 + 1], tan[i * 3 + 2]);
        data.mesh.vertices.push_back(v);
    }

    const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
    const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];
    const unsigned char* indexData = &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];

    for (size_t i = 0; i < indexAccessor.count; ++i) {
        uint32_t index = 0;
        switch (indexAccessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            index = reinterpret_cast<const uint16_t*>(indexData)[i];
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            index = reinterpret_cast<const uint32_t*>(indexData)[i];
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            index = reinterpret_cast<const uint8_t*>(indexData)[i];
            break;
        default:
            break;
        }
        data.mesh.indices.push_back(index);
    }

    std::filesystem::path base = path.parent_path();
    for (const auto& image : model.images) {
        std::filesystem::path p(image.uri);
        data.textures.push_back((base / p).wstring());
    }

    return data;
}