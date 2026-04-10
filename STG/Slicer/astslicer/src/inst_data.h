#ifndef Inst_DATA_H
#define Inst_DATA_H
#include <clang/AST/Decl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
// Use C++ filesystem to get canonical paths where available
#include <filesystem>

namespace astslicer
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
    InstInfo(std::string fn,std::string fn3,std::string fn4,std::string fn5) :\
     file_name{ fn }, func_file{fn3} ,insert_info{fn5}, typedef_file{fn4} \
     { parse_file();record_num=0;typedef_num=0;}

    void parse_file()
    {
        std::ifstream ofs(file_name);
        if (!ofs.is_open())
        {
            std::cout << "[error]open file:" << file_name << " failed!\n";
            exit(-1);
        }
        std::cout << "open file:" << file_name << "!\n";
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

                funcs_locs.emplace(
                    std::pair<std::string, DefFunc>(func_name, def_func));
            }
            else if (func_type == "Declare")
            {
                ofs >> func_name;
                declare_funcs.insert(func_name);
            }
            else
            {
                if (func_type.empty()) continue;
                std::cout << "[error]wrong in instsInfo.txt:" << file_name
                          << " failed!\n";
                std::cout << "*** " << func_type << "\n";
                // exit(-1);
            }
        }
        ofs.close();
    }

    std::string SKIPMACRO = "SlIcEr_SKIPMACRO";
    std::string file_name,func_file,typedef_file,insert_info,insert_fn,insert_f,insert_d;
    std::set<std::string> define_funcs, declare_funcs;
    std::set<std::string> define_files; // 需要修改的文件列表
    std::map<std::string, DefFunc> funcs_locs;

    std::vector<std::string> _visit;
    // std::vector<const clang::RecordDecl *> _visit_RD;

    std::unordered_set<clang::FunctionDecl *> insert_funcdec; //插入函数声明的
    std::unordered_set<const clang::VarDecl *> insert_gvar; //插入全局变量声明的
    std::unordered_set<std::string> insert_RD,handle_RD,forward_declare_RD;
    std::unordered_set<const clang::RecordDecl *> out_record;
    
    std::unordered_set<std::string> insert_typedef; // 插入了typedef A B的
    std::unordered_set<std::string> processed_typedef;//处理了其他的
    std::unordered_set<clang::TypedefDecl *> out_typedef;// after_insert处理完毕 完全结束的
    std::unordered_set<const clang::EnumDecl *> out_enum;
    std::unordered_set<const clang::EnumDecl *> ano_enum;
    std::unordered_set<const clang::Decl *> processed_decl;
    
    int record_num,typedef_num;
    std::unordered_set<std::string> out_obs;
    std::unordered_map<const clang::RecordDecl *,std::string> reocrd_map;
    
};

struct Position{
    int line;
    int col;
};
struct PreMessage
{
    PreMessage() {}
    std::unordered_set<const clang::ValueDecl *> global_used;
    std::unordered_map<const clang::ValueDecl *, Position> decl_pos;
    std::unordered_map<const clang::RecordDecl *, std::unordered_set<std::string> >used_structvar;//struct-varName->使用的field名称
    std::unordered_map<std::string, std::unordered_set<int> >used_fields;//struct-field_index->初始化没使用到的
    std::unordered_map<std::string, std::unordered_map<int, std::string> >idx_f;//index和field对应的
    std::unordered_set<std::string> nd_funcs;//保留函数声明的

    std::unordered_map<const clang::RecordDecl *,std::unordered_set< std::string>> tydef_level;
    std::unordered_map<const clang::VarDecl *,std::unordered_set<const clang::VarDecl *>> global_var_init;//全局变量初始化用到的
    std::unordered_map<const clang::VarDecl *,std::unordered_set<const clang::FunctionDecl *>> global_func_init;//全局变量初始化用到的


    std::unordered_map<clang::FieldDecl *, int> field_idx;//某个结构体的某个field对应的id;替换或输出时再加上Field的类型
    std::unordered_map<const clang::RecordDecl *,std::string> field_anonymous;//匿名结构体成员所属的
    
    int first_func_flag;
    std::unordered_map<int, std::string> macro_visit;
    std::unordered_map<int, std::string> macro_origin;
    std::unordered_map<std::string,clang::SourceLocation> first_func;
};

}  // namespace astslicer

#endif