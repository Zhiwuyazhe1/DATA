#include "../include/action.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <llvm/Support/CommandLine.h>


static llvm::cl::OptionCategory sseq_category("sseq options");
static llvm::cl::extrahelp
    common_help(clang::tooling::CommonOptionsParser::HelpMessage);
//Arg: cc.json diag.json config.txt
//commandLine Arguments

/*
mkdir build && cd build
cmake -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm -DClang_DIR=/usr/lib/llvm-18/lib/cmake/clang ..
make -j2
*/
static llvm::cl::opt<std::string> compile_info_f("cc", llvm::cl::desc("a json file, compile commands , you can get it by used 'bear make' when compile test project."),
                                llvm::cl::cat(sseq_category));
static llvm::cl::opt<std::string> path_info_f("diag", llvm::cl::desc("a json file, include Diagnostic info, like checker and trace."),
                                llvm::cl::cat(sseq_category));

static llvm::cl::opt<std::string> config_f("conf", llvm::cl::desc("a config file, control which features would be used."),
                                llvm::cl::cat(sseq_category));

int main(int argc, const char **argv)
{
    int argc_f           = argc - 2;
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc,
        argv,
        sseq_category
        );
    if (!expected_parser)
    {
        // Fail gracefully for unsupported options.
        llvm::errs() << expected_parser.takeError();
        return 1;
    }
    clang::tooling::CommonOptionsParser &options_parser = expected_parser.get();
    
    // clang::tooling::ClangTool tool(options_parser.getCompilations(),
    //                                options_parser.getSourcePathList());
    clang::tooling::ClangTool tool(
        options_parser.getCompilations(),
        options_parser.getCompilations().getAllFiles());

//add
    // clang::tooling::ArgumentsAdjuster ardj = clang::tooling::getInsertArgumentAdjuster("-I/usr/local/llvm/lib/clang/12.0.0/include");
    // tool.appendArgumentsAdjuster(ardj);

    std::cout << "[get semantic sequence]\n";
    std::string path_info_file = path_info_f;
    //./sseq compile_commands.json test_info_1.json config.txt
    if(path_info_file=="" ) path_info_file =  argv[2];//待修改
    std::string config_file =  argv[3];//配置项
    sseq::SeqInfo seq_info(path_info_file, config_file);
    std::unique_ptr<sseq::SeqFactory> sseq_factory =
        std::make_unique<sseq::SeqFactory>(seq_info);

    tool.run(sseq_factory.get());
    // seq_info.print_trace();
    std::cout << "[exit]\n";
    // seq_info.print_rela_variables();
    // seq_info.print_assign();
    std::string output_file =  argv[4];//输出
    seq_info.print_seq(output_file);// call output_seq();
    return 0;
}
/* test command
./sseq /home/nishikino/benchmark/Defect_Scenario_C/compile_commands.json  /home/nishikino/benchmark/Defect_Scenario_C/defect_info.json  /home/nishikino/benchmark/Defect_Scenario_C/config.txt 
*/
