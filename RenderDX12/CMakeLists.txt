set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/external/imgui)
set(IMGUI_BACKENDS_DIR ${IMGUI_DIR}/backends)

if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR "ImGui sources were not found. Clone https://github.com/ocornut/imgui (commit 87d7f7744efe63e77f4d0e00ccb5f6affd12aca7) into ${IMGUI_DIR}.")
endif()

add_library(RenderDX12 SHARED
    D3DCamera.cpp
    D3DTimer.cpp
    D3DUtil.cpp
    DX12CommandList.cpp
    DX12ConstantManager.cpp
    DX12DDSManager.cpp
    DX12DescriptorHeap.cpp
    DX12Device.cpp
    DX12FrameBuffer.cpp
    DX12FrameResource.cpp
    DX12PSO.cpp
    DX12RenderGeometry.cpp
    DX12Resource.cpp
    DX12RootSignature.cpp
    DX12ShadowManager.cpp
    DX12SwapChain.cpp
    DX12TextureManager.cpp
    DX12View.cpp
    RenderDX12.cpp
    stdafx.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_dx12.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_win32.cpp
)

set_target_properties(RenderDX12 PROPERTIES
    OUTPUT_NAME RenderDX12
)

find_package(DirectXTex CONFIG REQUIRED)
find_package(DirectXTK12 CONFIG QUIET)
find_package(winpixevent CONFIG QUIET)

if(TARGET Microsoft::WinPixEventRuntime)
  target_link_libraries(RenderDX12 PUBLIC Microsoft::WinPixEventRuntime)
  target_compile_definitions(RenderDX12 PUBLIC USE_PIX)
else()
  message(WARNING "PIX not found via vcpkg. Building without PIX markers.")
endif()

if(MSVC)
    target_compile_options(RenderDX12 PRIVATE /W4 /permissive- /wd4100 /wd4201 /wd4324)
else()
    target_compile_options(RenderDX12 PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(RenderDX12
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/FileLoader
        ${IMGUI_DIR}
        ${IMGUI_BACKENDS_DIR}
        ${RenderDX12_PIX_INCLUDE}
)

target_link_libraries(RenderDX12
    PRIVATE
        FileLoader
        Microsoft::DirectXTex
        $<$<TARGET_EXISTS:Microsoft::DirectXTK12::DirectXTK12>:Microsoft::DirectXTK12::DirectXTK12>
        d3d12
        dxgi
        d3dcompiler
        dxguid
        user32
        gdi32
        shell32
        imm32
        shcore
)

target_compile_definitions(RenderDX12
    PRIVATE
        _BUILDING_RENDERDX12
        UNICODE
        _UNICODE
)

target_precompile_headers(RenderDX12 PRIVATE stdafx.h)