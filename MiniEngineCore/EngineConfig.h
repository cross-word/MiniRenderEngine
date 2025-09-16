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
    static constexpr UINT ConstantBufferCount = 1;
    static constexpr UINT NumDefaultObjectSRVSlot = 65536; //16-bit
    static inline constexpr const wchar_t* ModelObjFilePath[] = {
        LR"(..\..\Models\Skull.obj)",
        LR"(..\..\Models\Car.obj)"
    }; // tmp
    static inline constexpr const wchar_t* DDSFilePath[] = {
        LR"(..\..\Textures\bricks.dds)",
        LR"(..\..\Textures\bricks2.dds)",
        LR"(..\..\Textures\bricks3.dds)",
    }; // tmp
    static inline constexpr const wchar_t* ShaderFilePath = LR"(..\..\\Shaders\\tmpfolder\\Default.hlsl)"; // tmp
    static inline constexpr const wchar_t* ShadowShaderFilePath = LR"(..\..\\Shaders\\tmpfolder\\Shadows.hlsl)"; // tmp
    static inline constexpr const wchar_t* SceneFilePath = LR"(D:\MiniEngine\main_sponza\NewSponza_Main_glTF_003.gltf)"; // tmp
    static constexpr UINT NumThreadWorker = 4;
    static constexpr bool UseMultiThread = true;
    static constexpr UINT MaxTextureCount = 80;

};