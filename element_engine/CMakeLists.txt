cmake_minimum_required(VERSION 3.16)
project(ElementEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform sources
set(ENGINE_SRCS
    src/element_engine.cpp
    src/element_cache.cpp
    src/ocr_fallback.cpp
)

if(WIN32)
    list(APPEND ENGINE_SRCS src/windows_impl.cpp)
elseif(APPLE)
    list(APPEND ENGINE_SRCS src/macos_impl.cpp)
elseif(UNIX)
    list(APPEND ENGINE_SRCS src/linux_impl.cpp)
endif()

# Static library
add_library(element_engine_static STATIC ${ENGINE_SRCS})
target_include_directories(element_engine_static PUBLIC include)

if(WIN32)
    target_link_libraries(element_engine_static PRIVATE uiautomationcore ole32 oleaut32)
elseif(APPLE)
    target_link_libraries(element_engine_static PRIVATE
        "-framework ApplicationServices"
        "-framework CoreFoundation")
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ATSPI atspi-2)
    if(ATSPI_FOUND)
        target_include_directories(element_engine_static PRIVATE ${ATSPI_INCLUDE_DIRS})
        target_link_libraries(element_engine_static PRIVATE ${ATSPI_LIBRARIES})
        target_compile_definitions(element_engine_static PRIVATE HAVE_ATSPI=1)
    endif()
endif()

# Python bindings sub-project
add_subdirectory(python_bindings)
