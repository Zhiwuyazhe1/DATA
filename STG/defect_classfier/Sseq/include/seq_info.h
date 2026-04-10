#ifndef SEQ_INFO_H
#define SEQ_INFO_H

#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "tool.h"
#include "Config.h"
#include "PDG.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
namespace sseq
{
    std::string normalize_path(const std::string& input_path);
    std::string make_absolute_path(const std::string& relative_path);
    const char kPathSeparator =
    #ifdef _WIN32
                                '\\';
    #else
                                '/';
    #endif
    //警报类型-目前参考CSA
    //未来是不是可以弄成配置文件？不同的静态分析工具不一致-但是action的一些策略是否需要和警报类型相关呢
    enum Wtype{
        WDefault = 0,
        NullPointerDereference = 1,//Dereference of null pointer
        DivisionByZero=2,//Division by zero
        DeadAssignment = 3,
        DeadInit = 4,// Dead initialization
        DeadNestedAssignment = 5,
        GarbageAssigned = 6, //Assigned value is garbage or undefined
        UninitializedArgument = 7, //Uninitialized argument value
        GarbageOperationResult = 8,//Result of operation is garbage or undefined
        ReturnStackMemory = 9, //Return of address to stack-allocated memory
    };

    //位置类型
    enum Postype{
        PDefault = 0,
        Function = 1,//函数内语句
        IfBranch=2,//True分支
        ElseBranch=3,//False分支
        SwitchBranch = 4,
        ForBody = 5,
        WhileBody = 6,
        DoBody = 7,
        Parma = 8, // 函数参数
    };
    
// 初始状态根据静态分析工具的输出，能得到警报类型和每个点的位置，其余信息在分析过程中进行补充
struct Point{
    int id;//
    std::string filepath;//文件路径
    std::string filename;//文件名
    int line;//行号
    int col;
    int line_end;//行号
    int col_end;
    int node_col; //匹配到的节点的开头
    int node_cole; //匹配到的节点的end
    Wtype w=WDefault;//警报类型
    Postype pos = PDefault;
    bool isWarning =false;//是否告警点
    bool isInMacro = false;//这个位置是否属于宏
    int isInBranch = 0; // 0:不在， 1：不确定是否执行 ，2：一定不执行的分支， 3：一定执行的分支
    bool isInLoop = false;
    bool isFPCall = false; //函数指针调用
    clang::Stmt *loopStmt=nullptr;
    clang::Stmt *branchStmt=nullptr;
    
    std::unordered_set<int> conditions_related;
    std::vector<std::string> variables; //变量名数组
    std::string func_name;
    std::string func_sign; // 函数签名
    //变量类型 是否const 指针 引用 数组
    //后续需要输入机器学习算法的话，应该存储原始格式还是直接在此处进行转换

    /* 记录当前认为的最接近该point的节点 */
    int line_diff=INT8_MAX,col_diff = -1;
    clang::Stmt *node = nullptr;
    clang::Decl *decl_node = nullptr;
    PDGNode *pnode=nullptr;
    std::string pnode_str;
    Point(int id, std::string fp,int line,int col, int l2, int c2):id{id},filepath{fp},\
    line{line},col{col},line_end{l2},col_end{c2}
    {
        func_name = "";
        func_sign = "";
        pnode_str = "";
        std::string::size_type iPos = filepath.find_last_of(kPathSeparator) + 1;
        filename =  filepath.substr(iPos, filepath.length() - iPos); 
    }
    void add_variable(std::string v){variables.push_back(v);}
    void set_func(std::string f){func_name = f;}
    void set_func_sign(std::string functionSignature){func_sign = functionSignature;}
    void set_warning(){isWarning = true;}
    bool get_warning(){return isWarning;}
    friend std::ostream &operator<<(std::ostream &os, Point &p){
        if(!p.node && !p.decl_node){
            os << "pointer[" << p.id << "] :<Empty Statement>\n";
            return os;
        }
        
        os << "pointer[" << p.id << "] in "<< p.func_sign << " <" << p.pnode_str << "> " \
       << "\t";//<< " (" << p.line << ", " << p.col <<")\t";// ") variables:[";
        //for(auto &s:p.variables) os << s << ",";
        //os << "]\t 
        os<<"isInBranch:" << p.isInBranch<<" isInLoop:"<<p.isInLoop<<" isInMacro:"<<p.isInMacro<<" isFPCall:"<<p.isFPCall<<" isWarning:"<<p.isWarning<<"\n";
        return os;
    }
  
};

struct TypeInfo{
    int return_types_info = 0;
    // bool return_sign_match;// 返回类型有符号无符号 1
    // bool return_types_equal;//返回类型是否都相等 2
    // bool return_types_compatible;//返回类型是否都兼容 4 --暂无作用

    bool used_CScast= false;//使用强制类型转换
    
    int BO_types_info= 0; 
    // bool BO_sign_match;// BO有符号无符号 1
    // bool BO_types_equal;//BO类型是否都相等 2
    // bool BO_types_compatible;//BO类型是否都兼容 4

    int structure_info = 0;
    //嵌套定义 1
    //匿名结构体/联合体 2
    //typedef匿名结构体/联合体 4
    //联合体使用 8
    int used_MD_Array = 0;// 多维数组 1 ，二级指针 2
    int used_bitfield = 0;// 位域 1 填充位 2

    TypeInfo& operator|=(const TypeInfo& rhs) {
        return_types_info |= rhs.return_types_info;
        BO_types_info |= rhs.BO_types_info;
        used_CScast |= rhs.used_CScast;
        structure_info |=rhs.structure_info;
        used_MD_Array |=rhs.used_MD_Array;
        used_bitfield |= rhs.used_bitfield;
        return *this;
    }
    friend std::ostream &operator<<(std::ostream &os, TypeInfo &t){
        os << "TypeInfo[ return_types_info :" << t.return_types_info << ", BO_types_info:" \
        << t.BO_types_info<<", used_CScast:" << t.used_CScast<<", structure_info:"<< t.structure_info\
        << ", used_MD_Array:"<< t.used_MD_Array << ", used_bitfield:"<< t.used_bitfield<< "]";
        return os;
    }
};
struct SpecialInfo{
    int compute_values = 0;// 特殊值 1:宏. 2:全局变量

    /* 位置 */
    int bit = 3;// 设置以下每个条件占的位数，默认3位（每种三个情况）
    int compute_goto_label = 0;//goto 目标 1：IndirectGotoStmt不存在固定目标 ， 2：使用了，但是它存在固定目标
    int compute_switch= 0; // switch表达式 1 大于两个变量
    int compute_case= 0; // case   1 非标准扩展

    int assign_scenario = 0;// 赋值场景
    /*  为每个变量记录
        1. 普通赋值 |1
        2. 自增自减 |2
        3. 声明同时初始化 |4
        4. 声明和初始化分离 |8 
            1. 变量声明时没有初始化 - 8
            2. 变量经历普通赋值 -> 检查是否有1，没有的话不考虑该变量
        5. 条件内赋值 |16
    */
    SpecialInfo& operator|=(const SpecialInfo& rhs) {
        compute_goto_label |= rhs.compute_goto_label;
        compute_switch |= rhs.compute_switch;
        compute_case |= rhs.compute_case;
        compute_values |= rhs.compute_values;
        assign_scenario |= rhs.assign_scenario;
        return *this;
    }
    friend std::ostream &operator<<(std::ostream &os, SpecialInfo &s){
        os << "SpecialInfo[ compute_values:"<< s.compute_values<<", compute_goto_label :" << s.compute_goto_label << ", compute_switch:" \
        << s.compute_switch<<", compute_case:" << s.compute_case <<", assign_scenario:" << s.assign_scenario << "]";
        return os;
    }
};

struct ControlFlowInfo {
    int if_count = 0;
    int switch_count = 0;
    int case_count = 0;
    int for_count = 0;
    int while_count = 0;
    int do_while_count = 0;

    friend std::ostream& operator<<(std::ostream& os, const ControlFlowInfo& info) {
        os << "if: " << info.if_count
           << ", switch: " << info.switch_count
           << ", case: " << info.case_count
           << ", for: " << info.for_count
           << ", while: " << info.while_count
           << ", do: " << info.do_while_count;
        return os;
    }

    ControlFlowInfo& operator|=(const ControlFlowInfo& rhs) {
        if_count += rhs.if_count;
        switch_count += rhs.switch_count;
        case_count += rhs.case_count;
        for_count += rhs.for_count;
        while_count += rhs.while_count;
        do_while_count += rhs.do_while_count;
        return *this;
    }
};

struct ResourceUsageInfo {
    int file_open = 0;
    int file_close = 0;
    int memory_alloc = 0;
    int memory_free = 0;
    int net_socket = 0;
    int net_connect = 0;
    int db_access = 0;

    // 用于合并多个函数的资源使用情况
    ResourceUsageInfo& operator|=(const ResourceUsageInfo& other) {
        file_open += other.file_open;
        file_close += other.file_close;
        memory_alloc += other.memory_alloc;
        memory_free += other.memory_free;
        net_socket += other.net_socket;
        net_connect += other.net_connect;
        db_access += other.db_access;
        return *this;
    }
    friend std::ostream &operator<<(std::ostream &os, const ResourceUsageInfo &info) {
        os << "file_open=" << info.file_open
           << ", file_close=" << info.file_close
           << ", memory_alloc=" << info.memory_alloc
           << ", memory_free=" << info.memory_free
           << ", net_socket=" << info.net_socket
           << ", net_connect=" << info.net_connect
           << ", db_access=" << info.db_access;
        return os;
    }
};

struct ExceptionHandlingInfo {
    int try_count = 0;              // try 块数量
    int catch_count = 0;            // catch 块数量
    int throw_count = 0;            // throw 表达式数量
    int setjmp_count = 0;           // setjmp 调用数量
    int longjmp_count = 0;          // longjmp 调用数量
    int max_nesting_depth = 0;      // 异常嵌套最大深度

    friend std::ostream& operator<<(std::ostream& os, const ExceptionHandlingInfo& info) {
        os << "try: " << info.try_count
           << ", catch: " << info.catch_count
           << ", throw: " << info.throw_count
           << ", setjmp: " << info.setjmp_count
           << ", longjmp: " << info.longjmp_count
           << ", max depth: " << info.max_nesting_depth;
        return os;
    }

    ExceptionHandlingInfo& operator|=(const ExceptionHandlingInfo& rhs) {
        try_count += rhs.try_count;
        catch_count += rhs.catch_count;
        throw_count += rhs.throw_count;
        setjmp_count += rhs.setjmp_count;
        longjmp_count += rhs.longjmp_count;
        if (rhs.max_nesting_depth > max_nesting_depth)
            max_nesting_depth = rhs.max_nesting_depth;
        return *this;
    }
};

struct NestedBlockInfo {
    int if_if_count = 0;      // if-嵌套if
    int if_loop_count = 0;    // if-嵌套loop
    int loop_loop_count = 0;  // loop-嵌套loop
    int max_nesting_depth = 0; // 最大嵌套深度

    friend std::ostream& operator<<(std::ostream& os, const NestedBlockInfo& info) {
        os << "if-if: " << info.if_if_count
           << ", if-loop: " << info.if_loop_count
           << ", loop-loop: " << info.loop_loop_count
           << ", max depth: " << info.max_nesting_depth;
        return os;
    }

    NestedBlockInfo& operator|=(const NestedBlockInfo& rhs) {
        
        if_if_count += rhs.if_if_count;
        if_loop_count += rhs.if_loop_count;
        loop_loop_count += rhs.loop_loop_count;
        std::cout<<rhs.loop_loop_count<<loop_loop_count<<"\n";
        if (rhs.max_nesting_depth > max_nesting_depth)
            max_nesting_depth = rhs.max_nesting_depth;
        return *this;
    }
};

struct ControlInfo{
    //针对整个程序
    int switch_default = 0;// 1:switch-case 语句不包含 default 分支
    int loop_state = 0;// 1:while 或 for 语句中的终止条件可能导致死循环 
    int dead_code = 0;// 1:if(false) 2:if(false){} else{}  4:if(true){} else{} 8:while(false)
    ControlInfo& operator|=(const ControlInfo& rhs) {
        switch_default |= rhs.switch_default;
        loop_state |= rhs.loop_state;
        dead_code |= rhs.dead_code;
        return *this;
    }
    friend std::ostream &operator<<(std::ostream &os, ControlInfo &c){
        os << "ControlInfo[ switch_default:"<< c.switch_default<<", loop_state :" << c.loop_state << ", dead_code:" \
        << c.dead_code<<"]";
        return os;
    }
};
class SeqInfo
{
public:
    //接收路径信息
    SeqInfo(std::string fn,std::string fn2) :file_name{ fn },config_file{ fn2 }  \
     { 
        parse_file();parse_config();
        root_path =  "~/benchmarks/defect_scenario_c"; //unused
        program_name = "test_defect";
     }
    ~SeqInfo();
    void print_functions(){
        for(auto &x:functions) std::cout<<x<<"\t";
        std::cout<<"\n";
    }
    // 解析json
    void parse_file()
    {
        json j;
        std::ifstream jfile(file_name);
        if (!jfile.is_open())
        {
            std::cout << "[error]open file:" << file_name << " failed!\n";
            exit(-1);
        }
        std::cout << "open file:" << file_name << "!\n";
        jfile >> j;
        bug_type = j.at("bug_type");
        checker = j.at("checker");
        is_single_file = j.at("is_single_file");
        is_single_function = j.at("is_single_function");
        if (j.contains("line_number")) {
            warning_line = j.at("line_number");
        } else {
            warning_line = -1; // 字段不存在时设为 -1
        }
        std::string warning_function = "";
        if (j.contains("warning_func")){
            warning_function = j.at("warning_func");
        }
        else if (j.contains("entry_function")) {
            warning_function = j.at("entry_function");
        }
        
        if (warning_function.empty()) {
            warning_line = -1;  //必须同时出现
        }
        std::string temp_func, temp_fp;
        int temp_line,temp_col,line_end,col_end;
        int len = j["trace"].size();
        for(int i=0; i<len; i++){
            temp_func = j["trace"][i].at("function");
            temp_fp = j["trace"][i].at("file_path");
            temp_fp = normalize_path(make_absolute_path(temp_fp));
            temp_line = j["trace"][i].at("line");
            temp_col = j["trace"][i].at("col");
            line_end = j["trace"][i].at("line2");
            col_end = j["trace"][i].at("col2");
            //int id, std::string fp,int line
            Point p = Point(i,temp_fp,temp_line,temp_col,line_end,col_end);
            p.set_func(temp_func);
            for(auto &s: j["trace"][i].at("var"))
                p.add_variable(s);
            if(warning_line != -1){
                std::string filename1 = temp_fp.substr(temp_fp.find_last_of('/') + 1); // basename 最后一个斜杠之后的部分
                std::string warning_function_name = warning_function.substr(warning_function.find_last_of('#') + 1);
                std::string warning_function_file = warning_function.substr(0, warning_function.find_last_of('#'));
                if((filename1 == warning_function_file) && (temp_func == warning_function_name) &&
                    (warning_line >= temp_line && warning_line<=line_end)) p.set_warning();
            }
            else if(i==len-1) p.set_warning();
            trace.push_back(p);
            functions.insert(temp_func);
        }
        jfile.close();
        print_functions();
    }

 // 需要处理的语义信息
    void parse_config()
    {
        Config config = Config(config_file);
        config_vec = config.getOptions();
        std::cout<<"--------Config-----------\n";
        for(auto [a,b] : config_vec){ // config_vec: true/false
            std::cout<<a<<" :"<<b<<"\n";
            info_vec[a] = 0;
        }
        std::cout<<"------------------------\n";
    }

    std::string get_trace_str() {
        std::ostringstream oss;
        for (auto& p : trace) {
            oss << p;
        }
        return oss.str();
    }
    void print_trace(){
        std::cout<<"Trace: \n";
        for(auto &p:trace){
            std::cout << p ;// << p.pnode->getID() <<"\n";
        }
    }
    //FIX:跨函数使用Tool::get_decl_string系列可能导致段错误，若需要输出则需要提前保存字符串
    void print_rela_variables(){
        if(rela_variables.size()==0) return;
        std::cout<<"Related variables: \n";
        for(auto it = rela_variables.begin();it!=rela_variables.end();it++){
            if(*it != nullptr) 
                std::cout << Tool::get_decl_string(*it)<<" "<< (*it)->getNameAsString()<<"\n";
        }
    }
    void print_assign(){
        std::cout<<"=========================\nAll Variables in AST, assign_times & scen : \n";
        for (auto& [k, v] : assign_times) {
            if(k==nullptr) continue;
            std::cout<< Tool::get_decl_string(k) <<" : " <<v<<" & ";
            if(varible_assign_scen.count(k)){
                std::cout<<varible_assign_scen[k]<<"\n";
            }else{
                std::cout<<0<<"\n";
            }
        }
        std::cout<<"=========================\n";
    }
    void print_seq(const std::string& filePath = "")
    {
        output_seq(filePath);
        std::cout<<"-------END OF SSEQ----------\n";
        for(auto [a,b] : info_vec){
            info_vec[a]=0;
            std::cout<<a<<" :"<<b<<"\n";
        }
        std::cout<<"-----------------\n";
    }

    // 提取目录，拼接 /output/ 并加上原文件名
    std::string getNewFilePath(std::string& originalPath) {
        std::filesystem::path path(originalPath);
        std::string directory = path.parent_path().string();
        std::string filename = path.filename().string();
        originalPath = directory + "/output/" + filename;
        return directory;
    }
    // 输出序列到json
    void output_seq(const std::string& filePath = "")
    {
        std::string fullFilePath = file_name;//defect_info.json的路径
        std::string outputDirectory = getNewFilePath(fullFilePath);
        // 如果传入文件路径，使用
        if (!filePath.empty()) {
            std::filesystem::path path(filePath);
            outputDirectory = path.parent_path().string();
            fullFilePath = filePath;
        }
        // 创建 output 目录
        std::filesystem::create_directories(outputDirectory);
        // std::cout<<"写入："<<fullFilePath<<"\n";

        nlohmann::json jsonData;
        jsonData["bug_type"] = bug_type;
        jsonData["checker"] = checker;
        jsonData["is_single_file"] = is_single_file;
        jsonData["is_single_function"] = is_single_function;
        jsonData["trace"] = get_trace_str();
        for (const auto& [x, y] : info_vec) {
            std::istringstream iss(x);//A_b(_c)
            std::string a, b, c;
            char delimiter;
            // std::cout<<"info_vec["<<x<<"] : "<<y<<"\n";
            std::getline(iss, a, '_');
            std::getline(iss, b, '_');
            if (!iss.eof()) { // A_b_C
                std::getline(iss, c);
                if(c=="ALL") continue;
                if (!jsonData.contains(a)) {
                    jsonData[a] = nlohmann::json::object();
                }
                if (!jsonData[a].contains(b)) {
                    jsonData[a][b] = nlohmann::json::object();
                }
                jsonData[a][b][c] = y;
            } else { // A_B
                if (!jsonData.contains(a)) {
                    jsonData[a] = nlohmann::json::object();
                }
                jsonData[a][b] = y;
            }
        }

        std::ofstream outputFile(fullFilePath);
        if (!outputFile.is_open()) {
            std::cerr << "无法打开文件: " << fullFilePath << std::endl;
            return;
        }

        // 输出 JSON 数据到文件
        outputFile << jsonData.dump(4);
        outputFile.close();
    }

    bool find_function(std::string &func_name){
        return functions.find(func_name) != functions.end();
    }
    /* get set function*/
    void add_rela_variables(const clang::VarDecl * VD) { rela_variables.insert(VD);}
    void add_assign_times(const clang::VarDecl *VD, int n) { assign_times[VD]+=n;}
    void add_loop_assign_times(const clang::VarDecl *VD, int n) { loop_assign_times[VD]+=n;}
    void add_varible_assign_scen(const clang::VarDecl *VD, int n)  { varible_assign_scen[VD]|=n;}
    void set_pdg(const clang::FunctionDecl * FD, PDG* pdg) { pdgs[FD] = pdg;}
    void setCtx(clang::ASTContext *ctx){ _ctx = ctx; }

    void add_used_array(const clang::VarDecl * VD, int index){ used_array[VD].insert(index);}
    void add_used_record(const clang::RecordDecl * RD, const clang::FieldDecl  *index){ if(index==nullptr) return; used_record[RD].insert(index);}
    void add_record_vars(const clang::RecordDecl * RD, const clang::VarDecl *index){ if(index==nullptr) return; record_vars[RD].insert(index);}
    void add_macro(std::string &&mm,int value){if(is_new_macro(mm)) macro_map[mm]=value; else macro_map[mm]|=value;}
    bool is_new_macro(std::string &mm){ if(macro_map.count(mm)!=0) return false; return true;}
    // bool is_special_integerLiteral(int mm){if(special_integerLiteral_map.count(mm)!=0) return true; return false;}
    // void add_integerLiteral(int mm,int value){if(is_special_integerLiteral(mm)) special_integerLiteral_map[mm]|=value; }

    void update_type_info(const clang::FunctionDecl * FD, TypeInfo &TI){func_types_info[FD] = TI; std::cout<<func_types_info[FD]<<"\n";}
    void update_all_types_info();
    
    void update_special_info(const clang::FunctionDecl * FD, SpecialInfo &SI){func_special_info[FD] = SI; std::cout<<func_special_info[FD]<<"\n";}
    void update_all_special_info();

    void update_controlflow_info(const clang::FunctionDecl * FD, ControlFlowInfo &CI){func_controlflow_info[FD] = CI; std::cout<<func_controlflow_info[FD]<<"\n";}
    void update_all_controlflow_info();

    void update_resource_info(const clang::FunctionDecl * FD, ResourceUsageInfo &RI){func_resource_info[FD] = RI; std::cout<<func_resource_info[FD]<<"\n";}
    void update_all_resource_info();

    void update_exception_info(const clang::FunctionDecl *FD, ExceptionHandlingInfo &EI) {func_exception_info[FD] = EI;std::cout << func_exception_info[FD] << "\n";}
    void update_all_exception_info() ;

    void update_nestedblock_info(const clang::FunctionDecl *FD, NestedBlockInfo &NI) {func_nestedblock_info[FD] = NI;std::cout << func_nestedblock_info[FD] << "\n";}
    void update_all_nestedblock_info() ;

    void update_control_info(const clang::FunctionDecl * FD, ControlInfo &CI){func_control_info[FD] = CI; std::cout<<func_control_info[FD]<<"\n";}
    void update_all_control_info();

    void print_used_array();
    void print_used_record();
    void print_record_vars();
    void print_used_macro();

    void add_point_to_trace(Point &p) { trace.push_back(p);}
    std::vector<Point>& get_trace() { return trace; }
    PDG* get_pdg(clang::FunctionDecl * FD) { return pdgs[FD];}
    std::string get_bug_type() { return bug_type;}
    bool get_config(std::string str) { if(config_vec.count(str)) return config_vec[str];  return false;}
    /* flow sensitive */
    void compute_flowS();
    /* path sensitive */
    void compute_pathS();
    /* field sensitive */
    void compute_fieldS();
    /* context sensitive */
    void compute_contextS();

    void compute_macro();

    void compute_typeSafe();
    
    void compute_special();

    void compute_controlFlow();

    void compute_resource();

    void compute_exception();

    void compute_nestedblock();


    void compute_control();

    std::string root_path,program_name;
    // seq_info.h
    void set_controlflow_info(const FunctionDecl* FD, const ControlFlowInfo& info) {
        func_controlflow_info[FD] = info;
    }
    void set_resource_info(const FunctionDecl* FD, const ResourceUsageInfo& info) {
        func_resource_info[FD] = info;
    }
    void set_exception_info(const clang::FunctionDecl *FD, const ExceptionHandlingInfo &info) {
        func_exception_info[FD] = info;
    }
    void set_nestedblock_info(const clang::FunctionDecl *FD, const NestedBlockInfo &info) {
        func_nestedblock_info[FD] = info;
    }
    
    void set_current_file_name(std::string str){
        current_file_name = str;
    }
    std::string get_current_file_name(){
        return current_file_name;
    }
    void refrash_all_info(){
        record_vars.clear();
        used_array.clear();
        used_record.clear();
        macro_map.clear();
    }
    
private:

    std::string file_name,config_file;
    std::string current_file_name;
    /* 缺陷类型和扫描的checker */
    std::string bug_type,checker;
    int warning_line;

    bool is_single_file, is_single_function;

    /* 经过的程序点、函数名*/
    std::vector<Point> trace;
    std::set<std::string> functions;

    /* 和缺陷产生相关的变量*/
    std::unordered_set<const clang::VarDecl *> rela_variables;
    std::unordered_map<const clang::VarDecl *,int> assign_times;
    std::unordered_map<const clang::VarDecl *,int> loop_assign_times;
    std::unordered_map<const clang::VarDecl *,int> varible_assign_scen;//为每个变量记录赋值场景
    /*  
        1. 普通赋值 |1
        2. 自增自减 |2
        3. 声明同时初始化 |4
        4. 声明和初始化分离 |8 
            1. 变量声明时没有初始化 - 8
            2. 变量经历普通赋值 -> 检查是否有1，没有的话不考虑该变量
        5. 条件内赋值 |16
    */

    /* 使用的数组/自定义类型 */
    std::unordered_map<const clang::VarDecl *,std::unordered_set<int>> used_array; //数组->使用下标
    std::unordered_map<const clang::RecordDecl *,std::unordered_set<const clang::FieldDecl *>> used_record; //结构体->使用了的成员 ? clang::FieldDecl *提供getParent方法获取结构体定义
    std::unordered_map<const clang::RecordDecl *,std::unordered_set<const clang::VarDecl *>> record_vars; //结构体->该类型变量
    std::unordered_map<const clang::VarDecl *,std::unordered_set<const clang::CallExpr* >> fp_used; //函数指针->使用的Call

    /* 使用的宏 */
    std::unordered_map<std::string, int> macro_map; // 0未出现，1使用，2运算
    // std::unordered_map<int, int> special_integerLiteral_map={ 2147483647 };//特殊字面量


    /* FD -> type info*/
    std::unordered_map<const clang::FunctionDecl*, TypeInfo> func_types_info;
    TypeInfo all_types_info;
    std::unordered_map<const clang::FunctionDecl*, SpecialInfo> func_special_info;
    SpecialInfo all_special_info;
    std::map<const FunctionDecl*, ControlFlowInfo> func_controlflow_info;
    ControlFlowInfo all_controlflow_info;
    std::map<const FunctionDecl *, ResourceUsageInfo> func_resource_info;
    ResourceUsageInfo all_resource_info;
    std::map<const FunctionDecl *, ExceptionHandlingInfo> func_exception_info;
    ExceptionHandlingInfo all_exception_info;
    std::map<const FunctionDecl *, NestedBlockInfo> func_nestedblock_info;
    NestedBlockInfo all_nestedblock_info;
    std::unordered_map<const clang::FunctionDecl*, ControlInfo> func_control_info;
    ControlInfo all_control_info;

    /* 配置项，关于需要处理的敏感/语义信息维度 */
    std::unordered_map<std::string, bool>  config_vec;
    std::unordered_map<std::string, int>  info_vec;

    /* FD -> PDG*/
    std::unordered_map<const clang::FunctionDecl*, PDG*> pdgs;

    clang::ASTContext *_ctx;

}; // SeqInfo



}  // namespace sseq

#endif