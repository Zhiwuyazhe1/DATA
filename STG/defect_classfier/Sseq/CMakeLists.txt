cmake_minimum_required(VERSION 3.13.4)
project(sseq)
# You should specify the LLVM_DIR and Clang_DIR
# e.g. DLLVM_DIR=<LLVM_BUILD_ROOT>/lib/cmake/llvm DClang_DIR=<LLVM_BUILD_ROOT>/lib/cmake/clang
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.
set(CMAKE_CXX_STANDARD 17)
include_directories(
    ./include
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
add_definitions(${CLANG_DEFINITIONS})

# Now build our tools
aux_source_directory(./src srcs)
add_executable(sseq ${srcs})

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader)

# Link against LLVM libraries  服务器改成${llvm}
target_link_libraries(sseq LLVM clangAST clangASTMatchers clangBasic clangFrontend clangSerialization clangTooling)

target_compile_features(sseq PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(sseq PROPERTIES COMPILE_FLAGS "-fno-rtti")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_BUILD_TYPE Debug)
