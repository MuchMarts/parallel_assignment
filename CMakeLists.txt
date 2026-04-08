cmake_minimum_required(VERSION 3.10)
project(histeq)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenCL REQUIRED)

add_executable(histeq main.cpp)
target_link_libraries(histeq OpenCL::OpenCL)

target_compile_definitions(
    histeq PRIVATE CL_TARGET_OPENCL_VERSION=300
)
target_include_directories(histeq PRIVATE ${CMAKE_SOURCE_DIR}/include)

configure_file(
    ${CMAKE_SOURCE_DIR}/kernels/naive.cl
    ${CMAKE_BINARY_DIR}/naive.cl
    COPYONLY
)