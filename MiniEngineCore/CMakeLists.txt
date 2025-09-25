add_executable(MiniEngineCore WIN32
    MiniWindow.cpp
    MiniTimer.cpp
    AgilitySDK.cpp
    stdafx.cpp
)

set_target_properties(MiniEngineCore PROPERTIES
    OUTPUT_NAME MiniEngineCore
)

if(MSVC)
    target_compile_options(MiniEngineCore PRIVATE /W4 /permissive- /wd4100 /wd4201)
else()
    target_compile_options(MiniEngineCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_precompile_headers(MiniEngineCore PRIVATE stdafx.h)

target_include_directories(MiniEngineCore
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(MiniEngineCore
    PRIVATE
        UNICODE
        _UNICODE
)

target_link_libraries(MiniEngineCore
    PRIVATE
        RenderDX12
        d2d1
        shell32
        shcore
)