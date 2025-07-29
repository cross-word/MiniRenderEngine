#pragma once
#include "stdafx.h"
#include <windows.h>
#include <Windowsx.h>
#include <d2d1.h>
#include <pix3.h>

#include <list>
#include <memory>

#pragma comment(lib, "d2d1")

#include "MiniWindow.h"
#include "resource.h"
#include "MiniTimer.h"
#include "EngineConfig.h"
#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_win32.h"

using namespace std;

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    MainWindow win;

    if (!win.Create(L"Draw Circles", WS_OVERLAPPEDWINDOW))
        return 0;
    RenderDX12 MainRenderer;
    MainRenderer.InitializeDX12(win.GetMainHWND());
    win.SetRenderer(&MainRenderer);

    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL1));
    ShowWindow(win.Window(), nCmdShow);

    MSG msg = {};
    bool running = true;
    LONGLONG lastTime = win.m_timer.GetTime();

    while (running)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }
            if (!TranslateAccelerator(win.Window(), hAccel, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        UINT frameIndex = 0;
        PIXBeginEvent(0, L"Frame %u", frameIndex++);

        win.UpdateFPS();
        win.DrawFPS();

        PIXEndEvent();

        LONGLONG now = win.m_timer.GetTime();
        LONGLONG elapsed = now - lastTime;

        while (elapsed < EngineConfig::TargetFrameTime)
        {
            LONGLONG remain = EngineConfig::TargetFrameTime - elapsed;
            if (remain > 2000)
            {
                Sleep(1);
            }
            else
            {
                // busy-wait
            }
            now = win.m_timer.GetTime();
            elapsed = now - lastTime;
        }
        lastTime = now;
        MainRenderer.Draw();
    }
    return 0;
}

PCWSTR MainWindow::ClassName() const
{
    return L"MainWindow";
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(m_hwnd, uMsg, wParam, lParam))
        return true;
    switch (uMsg)
    {
        case WM_CREATE:
        {
            m_fpsLabel = CreateWindowEx(
                0,
                L"STATIC",
                L"FPS: 0.0",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 10, 150, 20,
                m_hwnd,
                NULL,
                GetModuleHandle(NULL),
                NULL
            );
            
            return 0;
        }
        
        case WM_LBUTTONDOWN:
        {
            m_mouseDown = true;
            m_lastMousePos.x = GET_X_LPARAM(lParam);
            m_lastMousePos.y = GET_Y_LPARAM(lParam);
            SetCapture(m_hwnd);
            return 0;
        }
        case WM_LBUTTONUP:
        {
            m_mouseDown = false;
            ReleaseCapture();
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            if (m_mouseDown)
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                float dx = float(x - m_lastMousePos.x) * 0.005f;
                float dy = float(y - m_lastMousePos.y) * 0.005f;
                MainRenderer->GetD3DCamera()->Rotate(dx, dy);
                m_lastMousePos.x = x;
                m_lastMousePos.y = y;
            }
            return 0;
        }
        case WM_KEYDOWN:
        {
            const float step = 0.1f;
            switch (wParam)
            {
            case 'W': MainRenderer->GetD3DCamera()->Move(XMFLOAT3{ 0.f, 0.f, step }); break;
            case 'S': MainRenderer->GetD3DCamera()->Move(XMFLOAT3{ 0.f, 0.f, -step }); break;
            case 'A': MainRenderer->GetD3DCamera()->Move(XMFLOAT3{ -step, 0.f, 0.f }); break;
            case 'D': MainRenderer->GetD3DCamera()->Move(XMFLOAT3{ step, 0.f, 0.f }); break;
            case 'Q': MainRenderer->GetD3DCamera()->Move(XMFLOAT3{ 0.f, step, 0.f }); break;
            case 'E': MainRenderer->GetD3DCamera()->Move(XMFLOAT3{ 0.f, -step, 0.f }); break;
            }
            return 0;
        }
        case WM_DESTROY:
        {
            MainRenderer->ShutDown();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void MainWindow::UpdateFPS()
{
    m_timer.Update();
}

void MainWindow::DrawFPS()
{
    float fps = m_timer.GetFPS();
    swprintf(m_fpsText, 32, L"FPS: %.1f", fps);
    SetWindowText(m_fpsLabel, m_fpsText);
}

HWND MainWindow::GetMainHWND()
{
    return m_hwnd;
}