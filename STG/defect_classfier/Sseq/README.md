# Sseq
Sseq 是一个基于 Clang 开发的命令行工具，主要依赖 AST 规则匹配实现轻量级的静态分析事实提取。

- 提取的特征列表：特征说明.xlsx
### 快速使用
1. 环境配置  
   `Clang18` ：指定安装版本为clang-18。
   `cmake`：3.13.4+
2. 编译命令  
    ```shell
    mkdir build
    cd build
    cmake .. [-DLLVM_DIR=<LLVM_BUILD_ROOT>/lib/cmake/llvm -DClang_DIR=<LLVM_BUILD_ROOT>/lib/cmake/clang]
    make -j2
    ```
3. 运⾏命令  
    ```shell
    ./sseq  compile_commands.json defect_info.json config.txt output.json
    ```   
    - compile_commands.json 编译命令库，包含需要分析的文件的编译命令。通过bear命令生成后由脚本处理
    - defect_info.json 缺陷信息文件，由脚本生成
    - config.txt 配置文件，由用户编写，规定输出的特征
    - output.json 输出文件位置
     
4. Demo示例  
    demo文件夹包含一个示例，包含编译命令库、缺陷信息文件和配置文件。可以在build目录下使用以下命令运行
    ```shell
    ./sseq  ../demo/01.w_Defects/compile_commands.json ../demo/defect_infos/report-2SpfSy_2.json ../demo/config.txt ../demo/report-2SpfSy/report-2SpfSy_2.json
    ```

    - 输入：
        - 01.w_Defects/ 存储一个null_pointer.c 以及对应的编译命令库。include/ 包含编译该文件需要的头文件
        - report-2SpfSy.plist 是使用Clang Static Analyzer扫描01.w_Defects/null_pointer.c得到的缺陷信息列表
        - config.txt 是配置文件，规定了输出的特征列表
        - defect_infos/ 包含若干个处理后的缺陷信息文件，任意一个都可以作为工具的输入

    - 输出：
        - report-2SpfSy/ 包含每个处理后的缺陷文件作为输入的输出结果
    