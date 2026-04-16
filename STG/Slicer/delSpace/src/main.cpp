// #include "action.h"
#include "inst_action.h"
#include "inst_data.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <llvm/Support/CommandLine.h>

static llvm::cl::OptionCategory MydelSpace_category("mydelSpace options");

int main(int argc, const char **argv) {
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc, 
        argv, 
        MydelSpace_category
    );
    if (!expected_parser) {
        llvm::errs() << expected_parser.takeError();
        return 1;
    }
    
    clang::tooling::CommonOptionsParser &options_parser = expected_parser.get();
    clang::tooling::ClangTool tool(
        options_parser.getCompilations(),
        options_parser.getCompilations().getAllFiles());

    // for (auto it : tool.getSourcePaths()) {
    //     //--std::cout<< "** files: " << it << "\n";
    // }

    //--std::cout<< "[del space]\n";
    std::string inst_info_file = argv[2];
 
    //--std::cout<< inst_info_file <<"\n";
    delSpace::InstInfo inst_info(inst_info_file);
    std::unique_ptr<delSpace::InstFactory> inst_factory =
        std::make_unique<delSpace::InstFactory>(inst_info);

    tool.run(inst_factory.get());
    
    //--std::cout<< "[tool exit]\n";
    return 0;
}