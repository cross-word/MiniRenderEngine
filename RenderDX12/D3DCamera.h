#pragma once
#include "stdafx.h"
#ifdef _BUILDING_RENDERDX12
#define RenderDX12_API __declspec(dllexport)
#else
#define RenderDX12_API __declspec(dllimport)
#endif

using namespace DirectX;

class RenderDX12_API D3DCamera
{
public:
    XMMATRIX const GetViewMatrix();
    XMMATRIX const GetProjectionMatrix(float aspect);
    void Move(const XMFLOAT3& delta);
    void Rotate(float dx, float dy);

private:
    XMFLOAT3 m_position{ -1.5f, 2.5f, -4.0f };
    float m_yaw{ 0.3f };
    float m_pitch{ 0.45f };
};