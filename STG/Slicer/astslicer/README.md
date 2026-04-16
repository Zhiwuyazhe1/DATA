# ASTSlicer
`ASTSlicer`工具主要通过利用`DG`（一个基于LLVM IR的切片工具）的切片结果，即`DG`切片后指令对应源码中的位置信息或切片后函数的函数名信息，实现基于Clang AST的指令级切片功能。!!给DG的bc不要优化（-O1 -O2 -O3） 

为了获得`DG`的切片信息，我们对`DG`的代码进行了一定的修改，使其输出对应的指令级切片信息文件`xxx-instsInfo.txt`作为`ASTSlicer`的输入。

`ASTSlicer`通过对相同源码编译生成的Clang AST进行分析，利用Clang Tool中的`Rewrite`进行源码的修改，并输出切片后的C程序。

此外，由于IR和C代码之间的差异性，`ASTSlicer`的切片精度有一定损失。

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
./astslicer [compile_commands.json PATH] inst [xxx-instsInfo.txt PATH] [struct_relation.txt PATH] 
# 示例

./astslicer ./test-project2/build/compile_commands.json  inst ./test-project2/src/main-instsInfo.txt ./struct_relation.txt 

# 利用compile_commands.json将源程序编译为AST
# inst参数表示为指令级切片
# inst参数后为源程序对应DG指令级切片输出的切片信息文件的路径
# struct_relation.txt 为输出结构体等级的位置，用于变量名、类型名混淆

```


## Clang编译并打印AST命令
```
clang -Xclang -ast-dump -c example.c
```