#pragma once
#include <windows.h>

struct EngineConfig
{
    static constexpr UINT DefaultWidth = 800;
    static constexpr UINT DefaultHeight = 600;
    static constexpr float DefaultClearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
    static constexpr UINT MsaaSampleCount = 8;
    static constexpr LONGLONG TargetFrameTime = 16666; // 16.666ms
    static constexpr UINT SwapChainBufferCount = 3;
    static inline constexpr const wchar_t* ModelObjFilePath[] = {
        LR"(..\Models\Cube.obj)"
    }; // tmp
    static inline constexpr const wchar_t* DDSFilePath[] = {
        LR"(..\Textures\bricks.dds)",
        LR"(..\Textures\bricks2.dds)",
        LR"(..\Textures\bricks3.dds)",
    }; // tmp
    static inline constexpr const wchar_t* ShaderFilePath = LR"(..\\Shaders\\Default.hlsl)"; // tmp
    //static inline constexpr const wchar_t* ShaderFilePath = LR"(..\\Shaders\\color.hlsl)"; // tmp
};