# DelSpace
功能：删除代码里的nullstmt（独立的分号）和注释

## 环境依赖
1. 请在LLVM 18环境下运行。
2. LLVM18 和LLVM 12不完全兼容，如果有需要在LLVM 12下运行工具，请对main函数中的代码做如下修改。
```c++
// 将main函数中以下代码
auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc_f,
        argv,
        MyASTSlicer_category);
// 修改为
auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc_f,
        argv,
        MyASTSlicer_category,
        llvm::cl::NumOccurrencesFlag::ZeroOrMore);
```
3. cmakelist, 不手动静态链接
target_link_libraries(astslicer
    PRIVATE
    clang-cpp  
    LLVM  
)
## 编译
```shell
mkdir build
cd build
cmake ..
make
```

## 运行
```shell
 ./delSpace [compile_commands.json PATH]  [xxx-instsInfo.txt PATH] 
# 示例

./delSpace /home/nishikino/new_version_benchmarks/tests/compile_commands.json /home/nishikino/new_version_benchmarks/tests/test1-instsInfo.txt

# 利用compile_commands.json将源程序编译为AST
```




## Clang编译并打印AST命令
```
clang -Xclang -ast-dump -c example.c
```