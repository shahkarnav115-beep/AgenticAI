cmake_minimum_required(VERSION 3.16)
project(AgenticAI VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/Zc:__cplusplus /permissive-)
endif()

# Configure llama.cpp options to only build server & backend dependencies
set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Include thirdparty llama.cpp
add_subdirectory(thirdparty/llama.cpp EXCLUDE_FROM_ALL)

# Find Qt (supports both Qt6 and Qt5 for local & GitHub CI runners)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Main Executable
add_executable(AgenticAI
    src/main.cpp
    src/ui/ChatWidget.h
    src/ui/ChatWidget.cpp
    src/ui/IdeWidget.h
    src/ui/IdeWidget.cpp
    src/continuous_thinking/ContinuousThinkingEngine.h
    src/continuous_thinking/ContinuousThinkingEngine.cpp
    src/agent/LlamaClient.h
    src/agent/LlamaClient.cpp
    src/agent/LlamaManager.h
    src/agent/LlamaManager.cpp
    src/agent/ToolRegistry.h
    src/agent/ToolRegistry.cpp
    src/agent/tools/DocumentTools.h
    src/agent/tools/DocumentTools.cpp
    src/agent/tools/ImageTools.h
    src/agent/tools/ImageTools.cpp
)

target_link_libraries(AgenticAI PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    llama-server-impl
)

add_dependencies(AgenticAI llama-server)
