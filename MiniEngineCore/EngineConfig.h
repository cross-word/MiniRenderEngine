#pragma once
#include <windows.h>

struct EngineConfig
{
    static constexpr UINT DefaultWidth = 800;
    static constexpr UINT DefaultHeight = 600;
    static constexpr float DefaultClearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
    static constexpr UINT MsaaSampleCount = 8;
    static constexpr LONGLONG TargetFrameTime = 16666; // 16.666ms
    static constexpr UINT SwapChainBufferCount = 2;
};