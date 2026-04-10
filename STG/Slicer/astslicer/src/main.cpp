// #include "action.h"
#include "inst_action.h"
#include "inst_data.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <llvm/Support/CommandLine.h>
/*
mkdir build && cd build
cmake -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm -DClang_DIR=/usr/lib/llvm-18/lib/cmake/clang ..
make -j2
*/
static llvm::cl::OptionCategory MyASTSlicer_category("myastslicer options");
static llvm::cl::extrahelp
    common_help(clang::tooling::CommonOptionsParser::HelpMessage);

    //commandLine Arguments
static llvm::cl::opt<std::string> input("inputArg", llvm::cl::desc("Insert Argument"),
                                  llvm::cl::cat(MyASTSlicer_category));
static llvm::cl::opt<std::string> wl("wlines", llvm::cl::desc("Warning Lines"),
                                  llvm::cl::cat(MyASTSlicer_category));
static llvm::cl::opt<std::string> inst_info_f("inst", llvm::cl::desc("inst_info_file"),
                                llvm::cl::cat(MyASTSlicer_category));
int main(int argc, const char **argv)
{
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc,
        argv,
        MyASTSlicer_category
        // ,
        // llvm::cl::NumOccurrencesFlag::ZeroOrMore
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
    clang::tooling::ArgumentsAdjuster ardj = clang::tooling::getInsertArgumentAdjuster("-I/usr/local/llvm/lib/clang/12.0.0/include");
    tool.appendArgumentsAdjuster(ardj);

        // clang::tooling::ClangTool tool(options_parser.getCompilations(),
        // options_parser.getSourcePathList());
    for (auto it : tool.getSourcePaths())
    {
        //--std::cout << "** files: " << it << "\n";  //it is "mytest.c"
    }
    // std::cout<<input<<" ++ "<<wl<<" ++ "<<inst_info_f<<" ++\n";
    // std::cout<<"argc = "<<argc;
    std::string slicer_type = inst_info_f!="" ? "inst" : argv[2];
    if (slicer_type == "inst")
    {
        //--std::cout << "[inst slicer]\n";
        std::string inst_info_file = inst_info_f;
        if(inst_info_file=="" ) inst_info_file =  argv[3];
        std::string global_header = "";//argv[4];
        std::string insert_info="",type_header="";
        if(argc>4){
            global_header=argv[4];
        }
        if(argc>5){
            type_header=argv[5];
        }
        std::cout<<"argc = "<<argc<<"\n";
        astslicer::InstInfo inst_info(inst_info_file,global_header,type_header,insert_info);


        std::unique_ptr<astslicer::InstFactory> inst_factory =
            std::make_unique<astslicer::InstFactory>(inst_info);

        tool.run(inst_factory.get());
    }
    //--std::cout << "[slicer exit]\n";
    return 0;
}
