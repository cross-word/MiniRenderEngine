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

#include "../RenderDX12/RenderDX12.h"

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

        static UINT frameIndex = 0;
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
            wchar_t title[100];
            swprintf(title, 100, L"%lld", m_timer.GetTime());

            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            HWND hChild = CreateWindowEx(
                0,
                L"STATIC",
                title,
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                x, y, 120, 30,
                m_hwnd,
                NULL,
                GetModuleHandle(NULL),
                NULL
            );
            return 0;
        }
        case WM_DESTROY:
        {
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