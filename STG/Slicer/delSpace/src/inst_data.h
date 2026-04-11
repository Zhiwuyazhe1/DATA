#ifndef Inst_DATA_H
#define Inst_DATA_H
#include <clang/AST/Decl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
// Use C++ filesystem to get canonical paths where available
#include <filesystem>
namespace delSpace
{
struct DefFunc
{
    DefFunc() {}
    DefFunc(std::string fn) : func_name{ fn } {}
    std::map<int, std::vector<std::pair<int, int>>> loc_info;
    std::set<int> lines;
    std::string func_name;
};
namespace fs = std::filesystem;
// 路径标准化函数
std::string normalize_path(const std::string& input_path) {
    // 1. 构造 path 对象（自动处理分隔符，兼容 \ 和 /）
    fs::path path_obj(input_path);
    // 2. 标准化路径（处理 ./、../、连续分隔符）
    fs::path normalized_path = path_obj.lexically_normal();
    // 3. 转为字符串返回（输出分隔符自动适配当前系统，也可强制用 /）
    return normalized_path.string();  // 或 u8string()（UTF-8）
}

std::string make_absolute_path(const std::string& relative_path) {
    try {
        std::filesystem::path abs_path = std::filesystem::absolute(relative_path);
        return abs_path.string();
    } catch (const std::filesystem::filesystem_error& e) {
        llvm::errs() << "错误：无法转换路径: " << e.what() << "\n";
        return relative_path;
    }
}
struct InstInfo
{
    InstInfo(std::string fn) : file_name{ fn } { parse_file();}

    void parse_file()
    {
        std::ifstream ofs(file_name);
        if (!ofs.is_open())
        {
            //--std::cout<< "[error]open file:" << file_name << " failed!\n";
            exit(-1);
        }
        //--std::cout<< "open file:" << file_name << "!\n";
        while (!ofs.eof())
        {
            std::string func_type, func_name, func_file;
            int count;
            ofs >> func_type;
            if (func_type == "Define")
            {
                ofs >> func_name >> func_file >> count;
                define_funcs.insert(func_name);
                define_files.insert(normalize_path(make_absolute_path(func_file)));
                std::cout << "define:"<<func_name << " " << func_file << " " 
                              << normalize_path(make_absolute_path(func_file)) <<"\n";
                DefFunc def_func(func_name);
                while (count--)
                {
                    int line, col_start, col_end;
                    ofs >> line >> col_start >> col_end;
                    def_func.loc_info[line].emplace_back(
                        std::pair<int, int>(col_start, col_end));
                    def_func.lines.insert(line);
                }
            }
        }
        ofs.close();
    }

    std::set<std::string> define_funcs;
    std::string file_name;
    std::set<std::string> define_files; // 需要修改的文件列表
    
};
}  // namespace delSpace

#endif