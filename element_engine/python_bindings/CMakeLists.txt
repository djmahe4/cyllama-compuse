cmake_minimum_required(VERSION 3.16)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG QUIET)

if(NOT pybind11_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)
endif()

pybind11_add_module(element_engine_cpp py_bindings.cpp)
target_link_libraries(element_engine_cpp PRIVATE element_engine_static)
target_include_directories(element_engine_cpp PRIVATE ${CMAKE_SOURCE_DIR}/include)
