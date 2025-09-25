add_library(FileLoader SHARED
    GLTFLoader.cpp
    SimpleLoader.cpp
)

set_target_properties(FileLoader PROPERTIES
    OUTPUT_NAME FileLoader
)

if(MSVC)
    target_compile_options(FileLoader PRIVATE /W4 /permissive- /wd4100 /wd4201)
else()
    target_compile_options(FileLoader PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(FileLoader
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/external/tinygltf
)

target_compile_definitions(FileLoader
    PRIVATE
        _ENABLE_FILE_LOAD
        UNICODE
        _UNICODE
)