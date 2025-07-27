#include "SimpleLoader.h"
#include <DirectXColors.h>
#include <fstream>
#include <sstream>

MeshData LoadOBJ(const std::wstring& filename) {
    MeshData mesh;
    std::wifstream file(filename);
    if (!file.is_open()) {
        return mesh;
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT4> colors;

    std::wstring line;
    while (std::getline(file, line)) {
        std::wistringstream iss(line);
        std::wstring prefix;
        iss >> prefix;
        if (prefix == L"v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
            float b, g, r, a;
            iss >> b >> g >> r >> a;
            colors.emplace_back(b, g, r, a);
        }
        else if (prefix == L"f") {
            std::wstring v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            auto parseIndex = [](const std::wstring& token) {
                size_t pos = token.find(L"/");
                return static_cast<uint32_t>(std::stoi(token.substr(0, pos))) - 1;
                };
            uint32_t i1 = parseIndex(v1);
            uint32_t i2 = parseIndex(v2);
            uint32_t i3 = parseIndex(v3);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }

    for (int i = 0; i < positions.size(); i++) {
        Vertex v;
        v.pos = positions[i];
        v.color = colors[i];
        mesh.vertices.push_back(v);
    }
    return mesh;
}