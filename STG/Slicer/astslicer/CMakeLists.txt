cmake_minimum_required(VERSION 3.13.4)
project(ASTSlicer)

# 1. 强制指定 LLVM/Clang 18 版本，避免版本模糊匹配
find_package(Clang 18.1.8 REQUIRED CONFIG)
find_package(LLVM 18.1.8 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

# 2. 统一 C++ 标准（删除冗余的 -std=c++17 手动添加）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 3. 禁用 LLVM 内部冗余选项注册（关键：解决重复注册错误）
add_definitions(
  -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1  # 关闭 ABI 兼容性检查冗余
  -DLLVM_NO_DEAD_STRIP                             # 避免符号重复导出
  -DCLANG_NO_DEAD_STRIP                            # 避免 Clang 符号重复导出
  -DLLVM_OPTION_REGISTRY_DISABLE_DUPLICATE_CHECKS=0 # 强制检测重复选项（可选，用于调试）
  -DBUILD_SHARED_LIBS=ON
)

# 4. 目标专属配置（替换全局 include_directories/add_definitions，避免污染）
file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ./src/*.cpp ./src/*.h) # 显式匹配源文件，避免冗余
add_executable(astslicer ${srcs})

# 仅给目标添加 LLVM/Clang 头文件路径
target_include_directories(astslicer PRIVATE
  ${CLANG_INCLUDE_DIRS}
  ${LLVM_INCLUDE_DIRS}
)

# 仅给目标添加 LLVM 编译定义
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(astslicer PRIVATE
  ${CLANG_DEFINITIONS}
  ${LLVM_DEFINITIONS_LIST}
)

# 5. 强制动态链接 LLVM（避免静态库与动态库混合导致的选项重复）
set(LLVM_LINK_LLVM_DYLIB ON) # 关键：使用动态库 libLLVM-18.so，而非静态库
llvm_map_components_to_libnames(llvm_libs support core irreader) # 仅保留必要组件，避免冗余

# 6. 链接时优先使用 Clang 18 库（避免隐式链接旧版本）
link_directories(BEFORE ${LLVM_LIBRARY_DIRS} ${CLANG_LIBRARY_DIRS}) # BEFORE 确保路径优先级
target_link_libraries(astslicer
    PRIVATE
    clang-cpp  
    LLVM  
)
# 7. 精简编译选项（删除冗余的 compile_features，C++17 已包含）
target_compile_options(astslicer PRIVATE
  -fno-rtti  # LLVM/Clang 不支持 RTTI，必须禁用
  #-Wall      # 开启警告，便于排查隐藏问题（可选）
)
# 8. 构建类型通过命令行指定（不硬编码 Debug，增加灵活性）
# set(CMAKE_BUILD_TYPE Debug) # 注释掉，编译时用 -DCMAKE_BUILD_TYPE=Debug 指定