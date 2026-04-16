cmake_minimum_required(VERSION 3.13.4)
project(delSpace)

# 明确指定 LLVM 18
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 包含目录
include_directories(
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

# 添加定义
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
add_definitions(${CLANG_DEFINITIONS})

# 针对 LLVM 18 的特定修复
add_definitions(-DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1)

# 构建可执行文件
aux_source_directory(./src srcs)
add_executable(delSpace ${srcs})

# 使用新的组件映射方式
llvm_map_components_to_libnames(llvm_libs
    Support
    Core
    IRReader
)

# 链接库 - 使用新的推荐方式
target_link_libraries(delSpace
    PRIVATE
    clang-cpp  
    LLVM  
)
# 编译特性
target_compile_features(delSpace PRIVATE cxx_std_17)

# 编译选项
target_compile_options(delSpace PRIVATE
    -fno-rtti
    -Wno-unused-command-line-argument
)

# 针对不同构建类型的设置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(delSpace PRIVATE DEBUG=1)
else()
    target_compile_definitions(delSpace PRIVATE NDEBUG=1)
endif()