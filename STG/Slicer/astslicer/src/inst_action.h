#ifndef INST_ACTION_H
#define INST_ACTION_H

#include "inst_data.h"
#include "tool.h"
#include <iostream>
#include <unistd.h>
#include <regex>
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include <clang/AST/Decl.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Basic/Builtins.h>
#include "clang/AST/ParentMapContext.h"
namespace astslicer
{
    
class InstASTVisitor : public clang::RecursiveASTVisitor<InstASTVisitor>
{
    private:
        std::ofstream ofs2;
  public:
    explicit InstASTVisitor(clang::ASTContext *ctx,
                            clang::Rewriter &R,
                            InstInfo &inst_i)
        : _ctx(ctx), _rewriter(R), inst_info{ inst_i }
    {
        pre_msg.first_func_flag=0;
    }
    ~InstASTVisitor(){
    }
    
    static std::string get_stmt_string(const clang::Stmt *S){
        if(S==nullptr){
            return "0";
        }
        clang::LangOptions LO;
        // LO.CPlusPlus = 1;
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        clang::PrintingPolicy Policy(LO);
        Policy.PolishForDeclaration = true;
        S->printPretty(strout, nullptr, Policy);
        return strout.str(); 
    }
    std::string get_decl_string(const clang::Decl *S){
        if(S==nullptr){
            return "0";
        }
        clang::LangOptions LO=S->getASTContext().getLangOpts();
        // LO.CPlusPlus = 1;
        clang::PrintingPolicy Policy(LO);
        Policy.PolishForDeclaration = true;
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        S->print(strout,Policy);
        std::string ret=strout.str();
        return ret;
    }
    
    std::string get_type_string(const clang::Type *underType){
        clang::LangOptions lo;
        clang::PrintingPolicy pp(lo);
        std::string s;
        llvm::raw_string_ostream rso(s);
        if(underType->isArrayType()){
            underType = underType->getArrayElementTypeNoTypeQual();
        }
        clang::QualType::print(underType, clang::Qualifiers(), rso, lo, llvm::Twine());
        return rso.str();
    }
    std::string get_qualType_string(const clang::QualType T){
        if(T.isNull()){
            return "0";
        }
        clang::LangOptions LO;
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        T.getLocalUnqualifiedType().getNonReferenceType().print(strout, clang::PrintingPolicy(LO));
        return strout.str();
    }

    std::string get_loc_file(const clang::SourceLocation &loc)
    {
        auto &SM = _ctx->getSourceManager();
        clang::PresumedLoc PLoc = SM.getPresumedLoc(loc); // 获取推测位置
        std::string filename = SM.getFilename(loc).str();
        unsigned line = 0;

        if (filename == "" && PLoc.isValid()) {  // 检查位置是否有效
            std::string pfilename = PLoc.getFilename();  // 真实文件名
            if(pfilename.find(".h")!=std::string::npos){
                filename = pfilename;
            }
        }
        return filename;
    }

    /**
     * @brief 判断给定的源代码位置是否在系统头文件中
     *
     * 根据给定的源代码位置（clang::SourceLocation），判断该位置是否位于系统头文件中。
     *
     * @param loc 源代码位置
     *
     * @return 如果源代码位置在系统头文件中，则返回 true；否则返回 false
     */
    bool isInSystemHeader(const clang::SourceLocation &loc) {
        auto &SM = _ctx->getSourceManager();
        std::string filename = get_loc_file(loc);
        if(filename=="") return true;
        if(filename.find("/usr/include/")!=std::string::npos) return true;
        if(filename.find("/llvm")!=std::string::npos) return true;
        return false;
    }

    /**
     * @brief 判断指定行号范围是否可以被切片
     *
     * 根据给定的函数名、起始行号和结束行号，判断该范围内的代码是否可以被切片。
     *
     * @param func_name 函数名
     * @param line_start 起始行号
     * @param line_end 结束行号
     *
     * @return 如果指定行号范围内的代码可以被切片，则返回1；否则返回0。
     */
    int isSlice(std::string func_name, int line_start, int line_end){
        int to_slice=1;
        DefFunc def_func = inst_info.funcs_locs[func_name];
        for (int i = line_start; i <= line_end; i++)
        {
            if (def_func.lines.find(i) != def_func.lines.end())
            {
                to_slice = 0;//保留
            }
        }
        return to_slice;
    }

    /**
     * @brief 获取声明语句的源代码范围
     *
     * 通过给定的声明语句，获取其源代码范围。如果声明语句在宏定义中，则通过展开宏定义来获取实际的源代码范围。
     *
     * @param stmt 声明语句指针
     *
     * @return 声明语句的源代码范围
     */
    clang::SourceRange get_decl_sourcerange(clang::Decl*stmt){
        clang::SourceRange SR = stmt->getSourceRange();
        auto &SM = _ctx->getSourceManager();
        auto &LO = _ctx->getLangOpts();
        clang::SourceLocation  loc = SR.getBegin();
        clang::SourceLocation  end = SR.getEnd();
        if(loc.isMacroID()){
            clang::FileID FID = SM.getFileID(loc);
            const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
            while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                loc = Entry->getExpansion().getExpansionLocStart();
                FID = SM.getFileID(loc);
                Entry = &SM.getSLocEntry(FID);
            }
            loc = SM.getExpansionLoc(loc);
            if(end.isMacroID()){
                FID = SM.getFileID(end);
                const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
                while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                    end = Entry->getExpansion().getExpansionLocStart();
                    FID = SM.getFileID(end);
                    Entry = &SM.getSLocEntry(FID);
                }
                end = Entry->getExpansion().getExpansionLocEnd();
            }
            else end = SM.getExpansionLoc(end);
        }
        else if(end.isMacroID()){
            clang::FileID FID = SM.getFileID(end);
            const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
            while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                end = Entry->getExpansion().getExpansionLocStart();
                FID = SM.getFileID(end);
                Entry = &SM.getSLocEntry(FID);
            }
            end = Entry->getExpansion().getExpansionLocEnd();
        }
        
        
        return clang::SourceRange(loc,end);
    }
    //获得stmt的range
    clang::SourceRange get_sourcerange(clang::Stmt *stmt){
        clang::SourceRange SR = stmt->getSourceRange();
        auto &SM = _ctx->getSourceManager();
        auto &LO = _ctx->getLangOpts();
        clang::SourceLocation  begin = SR.getBegin();
        clang::SourceLocation  loc = SR.getBegin();
        clang::SourceLocation  end = SR.getEnd();
        if(loc.isMacroID()){
            clang::FileID FID = SM.getFileID(loc);
            const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
            while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                loc = Entry->getExpansion().getExpansionLocStart();
                FID = SM.getFileID(loc);
                Entry = &SM.getSLocEntry(FID);
            }
            loc = SM.getExpansionLoc(loc);
            if(end.isMacroID()){
                FID = SM.getFileID(end);
                const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
                while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                    end = Entry->getExpansion().getExpansionLocStart();
                    FID = SM.getFileID(end);
                    Entry = &SM.getSLocEntry(FID);
                }
                end = Entry->getExpansion().getExpansionLocEnd();
            }
            else end = SM.getExpansionLoc(end);
        }
        else if(end.isMacroID()){
            clang::FileID FID = SM.getFileID(end);
            const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
            while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                end = Entry->getExpansion().getExpansionLocStart();
                FID = SM.getFileID(end);
                Entry = &SM.getSLocEntry(FID);
            }
            end = Entry->getExpansion().getExpansionLocEnd();
        }
        
        return clang::SourceRange(loc,end);
    }
 
    //获取普通类型，也就是最前端的类型，屏蔽数组和指针,不处理typedef
    clang::QualType get_normal_type_decl(clang::QualType QT){
        QT = QT.getLocalUnqualifiedType().getNonReferenceType();
        while(QT.getTypePtr()->isArrayType()){
            QT = clang::QualType::getFromOpaquePtr(QT.getLocalUnqualifiedType().getNonReferenceType().getTypePtr()->getArrayElementTypeNoTypeQual());
        }
        if(const clang::TypedefType* typedefType = QT->getAs<clang::TypedefType>()){return QT;}
        while(const clang::PointerType* pointerType = QT->getAs<clang::PointerType>()) {
            QT = pointerType->getPointeeType();
        }
        return QT;
    }

    void writeAnyInType(std::string str){
        std::ofstream ofs3(inst_info.typedef_file,std::ios::out|std::ios::app);
        if(!ofs3.is_open()){
            std::cout <<"Error: open file "<<inst_info.typedef_file<<" failed!\n";
            exit(1);
        }
        ofs3 << str << "\n";
        ofs3.close();
    }
    void writeAnyInFunc(std::string str){
        std::ofstream ofs3(inst_info.func_file,std::ios::out|std::ios::app);
        if(!ofs3.is_open()){
            std::cout <<"Error: open file "<<inst_info.func_file<<" failed!\n";
            exit(1);
        }
        ofs3 << str << "\n";
        ofs3.close();
    }
    void insertRecordDeclare(const clang::RecordDecl* RD,std::string type="struct"){
        std::string record_name = RD->getNameAsString();
        if(RD->isUnion()) type="union";
        if(!record_name.empty() && inst_info.forward_declare_RD.count(record_name)==0){
            inst_info.forward_declare_RD.insert(record_name);
            writeAnyInType(type+" "+record_name+";//insertRecordDeclare \n");
        }
    }

    void processEnumConstant(const clang::EnumConstantDecl *ECD) {
        const clang::Expr *InitExpr = ECD->getInitExpr();
        if (!InitExpr) return;
        // std::cout <<"processEnumConstant:"<<get_stmt_string(InitExpr)<<"\n";
        process_Stmt(InitExpr);
    }
    void get_enum(const clang::EnumDecl* ED, bool after_insert = false){
        std::string enum_name = ED->getNameAsString();
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(!ED || isInSystemHeader(ED->getLocation()) || ED->getSourceRange().isInvalid())
            return;
        if(inst_info.out_enum.count(ED)!=0) return;
        if(!enum_name.empty() && inst_info.handle_RD.count(enum_name) != 0) return;
        std::string str = "\n"+get_decl_string(ED)+";\n";
        if(!enum_name.empty()){
            inst_info.handle_RD.insert(enum_name);
        }
        if(!enum_name.empty() && inst_info.insert_RD.count(enum_name)!=0) return;
        if(!enum_name.empty()){
            inst_info.insert_RD.insert(enum_name);
            // 处理枚举常量不是字面量的情况（可能涉及其他结构体）
            for (const clang::EnumConstantDecl *ECD : ED->enumerators()) {
                processEnumConstant(ECD);
            }
            writeAnyInType("/*ENUM*/"+str);
        }else{
            if (after_insert){
                writeAnyInType("/*ANo ENUM*/"+str);
            }else if(inst_info.ano_enum.count(ED)==0){
                inst_info.ano_enum.insert(ED);
                for (const clang::EnumConstantDecl *ECD : ED->enumerators()) {
                    processEnumConstant(ECD);
                }
            }
        }
    }
    
    std::string get_record_string(const clang::RecordDecl* RD, bool inner=false){
        std::string record_name = RD->getNameAsString();
        clang::SourceManager &SM = _ctx->getSourceManager();
        // std::cout <<0;
        if(!RD ||RD->getSourceRange().isInvalid()|| isInSystemHeader(get_decl_sourcerange(const_cast<clang::RecordDecl*>(RD)).getBegin()))
            return "";
        bool isFirstTry = false;
        std::string str = "\n"+get_decl_string(RD)+";\n",type="struct";
        if(inst_info.reocrd_map.count(RD)!=0) return inst_info.reocrd_map[RD];
        if(!record_name.empty() && inst_info.handle_RD.count(record_name) == 0){
            std::cout <<"First get_record: " <<record_name<<"\n";
            isFirstTry = true;
            inst_info.handle_RD.insert(record_name);
        }
        // std::cout <<1;
        
        if(RD->isUnion()) type="union";
        std::unordered_set<const clang::RecordDecl *> after_insert;
        if(record_name.empty() || isFirstTry){
            for(auto d:RD->decls()){
                //处理在结构体内定义的结构体和联合体
                if(clang::RecordDecl* rd = clang::dyn_cast<clang::RecordDecl>(d)){
                    std::string strr = get_decl_string(rd);
                    if(strr.find("{")!=std::string::npos && \
                            strr.find("}")!=std::string::npos){
                        strr = get_record_string(rd,true);
                        std::cout <<"inner struct:"<<strr<<"\n";
                    }
                }
                //处理在结构体内定义的枚举
                else if(clang::EnumDecl* ed = clang::dyn_cast<clang::EnumDecl>(d)){
                    std::string strr = get_decl_string(ed);
                    if(strr.find("{")!=std::string::npos && \
                            strr.find("}")!=std::string::npos){
                            inst_info.out_enum.insert(ed);
                    }
                }

                //处理结构体成员
                else if(clang::FieldDecl* field = clang::dyn_cast<clang::FieldDecl>(d)){
                    std::string strf = get_decl_string(field);
                    // std::cout <<"FieldDecl "<<strf<<"\n";
                    int isPointer=0;
                    clang::QualType fieldType= field->getType(); // 成员类型
                    std::cout <<fieldType.getAsString()<<"\t";
                    if(fieldType.getTypePtr()->isArrayType()){ // 数组，确认实际类型 如是int*[] 还是 int[]
                        auto element_type = clang::QualType::getFromOpaquePtr(fieldType.getLocalUnqualifiedType().getNonReferenceType()\
                                        .getTypePtr()->getArrayElementTypeNoTypeQual());
                        // std::cout <<"element:"<<element_type.getAsString()<<"\t";
                        isPointer |= element_type.getTypePtr()->isPointerType();   
                        fieldType = element_type;
                    }
                    
                    isPointer |= fieldType.getTypePtr()->isPointerType();    
                    auto FT = get_normal_type_decl(fieldType);
                    // std::cout <<"isPointer? "<<isPointer<<"\n";
                    // std::cout <<FT.getAsString()<<"\t";
                    if(const clang::TypedefType *TT = FT->getAs<clang::TypedefType>()) { // 检查是否typedef
                        clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                        if (Typedef->getUnderlyingType()->isFunctionPointerType()){
                            isPointer = false;
                        }
                        get_typedef(Typedef, isPointer);
                    }
                    else if (fieldType->isFunctionPointerType()) {
                        // std::cout <<"function-pointer.\n";
                        if (const auto *pointerType = fieldType->getAs<clang::PointerType>()) {
                            process_functionProtoType(pointerType->getPointeeType());
                        }
                    }
                    else if (FT->isFunctionPointerType()) {
                        // std::cout <<"FT function-pointer.\n";
                        if (const auto *pointerType = fieldType->getAs<clang::PointerType>()) {
                            process_functionProtoType(pointerType->getPointeeType());
                        }
                    }
                    else{
                        const clang::RecordType* recordType = FT->getAsStructureType();
                        int isUnion=0;
                        if(recordType==nullptr){
                            recordType = FT->getAsUnionType();
                            isUnion=1;
                        }
                        if(recordType){
                            // std::cout <<"is reocrd,";
                            const clang::RecordDecl* recordDecl = recordType->getDecl();
                            if(recordDecl!=RD && !isPointer && !recordDecl->getNameAsString().empty()){//需要在使用之前有完整定义
                                // std::cout <<"need compelete_define;";

                                process_QualType(FT,!isPointer);
                                // process_QualType(fieldType);
                            }else if(recordDecl!=RD){//不是自嵌套
                                // if(const clang::TypedefType* TT = FT->getAs<clang::TypedefType>()){
                                //     clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                                //     get_typedef(Typedef);
                                // }
                                //插入前向声明
                                if(!isUnion) insertRecordDeclare(recordDecl,"struct");
                                else  insertRecordDeclare(recordDecl,"union");
                                
                                if(!isPointer) after_insert.insert(recordDecl);
                            }
                        }else if(const clang::EnumType *EType = FT->getAs<clang::EnumType>()){//枚举
                            const clang::EnumDecl *EDecl = EType->getDecl();
                            get_enum(EDecl);
                        }
                        else{//不是结构体 
                            process_QualType(fieldType);
                            // process_QualType(FT);
                        }
                    }
                }
            }
        }

        // std::cout <<2;
        if(!record_name.empty() && inst_info.insert_RD.count(record_name)!=0) {
            // -- std::cout <<"----------------return str--------------------\n";
            return str;
        }
        
        if(record_name.empty() || isFirstTry){
            for(auto &rd : after_insert){
                std::cout <<"after_insert record in get_reocrd_string\n";
                get_record_string(rd);
            }
        }
        // std::cout <<3;
        if(inst_info.out_record.count(RD)==0 && !record_name.empty()){
            inst_info.insert_RD.insert(record_name);
            inst_info.forward_declare_RD.insert(record_name);
            inst_info.out_record.insert(RD);
            inst_info.reocrd_map[RD]=str;
            std::cout <<"write record:"<<record_name<<"\n";
            if(inner == false)
                writeAnyInType(str);
        }
        std::cout <<"----------------return--------------------\n";
        return str;
    }
    
    bool process_QualType(clang::QualType QT, bool isRef = false){
        clang::QualType varType = get_normal_type_decl(QT);
        QT = QT.getLocalUnqualifiedType();
        //如果是指针 就只插入前向声明
        bool isPointer = QT.getTypePtr()->isPointerType();
        std::cout <<"in process_QualType : "<<QT.getAsString()<<" "<<QT->isFunctionReferenceType()<<QT->isFunctionType()<<"\n";
        //typedef
        if(const clang::TypedefType *TT = QT->getAs<clang::TypedefType>()){
            clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
            get_typedef(Typedef, isPointer, isRef);
        }
        else if(const clang::TypedefType *TT = varType->getAs<clang::TypedefType>()){
            clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
            get_typedef(Typedef, isPointer, isRef);
        }
        std::cout <<" varType "<<varType.getAsString()<<"\n";
        if(const clang::RecordType *recordType = varType->getAsStructureType()){
            const clang::RecordDecl *recordDecl = recordType->getDecl();
            std::string recordName = recordDecl->getNameAsString();
            if(recordName!="" && inst_info.forward_declare_RD.count(recordName)==0){
                insertRecordDeclare(recordDecl,"struct");
            }
            if(!isPointer || isRef) get_record_string(recordDecl);
        }
        else if(const clang::RecordType *recordType = varType->getAsUnionType()){
            const clang::RecordDecl *recordDecl = recordType->getDecl();
            std::string recordName = recordDecl->getNameAsString();
            if(recordName!="" && inst_info.forward_declare_RD.count(recordName)==0){
                insertRecordDeclare(recordDecl,"union");
            }
            if(!isPointer|| isRef) get_record_string(recordDecl);
        }
        else if(const clang::EnumType *EType = varType->getAs<clang::EnumType>()){
            const clang::EnumDecl *EDecl = EType->getDecl();
            get_enum(EDecl);
        }
        else if (QT->isFunctionPointerType()) {
            if (const auto *pointerType = QT->getAs<clang::PointerType>()) {
                // delegate handling of function (proto/no-proto) types to helper
                process_functionProtoType(pointerType->getPointeeType());
            }
        }
        return true;
    }
    /**
    * @brief 递归获取 MemberExpr 的每一级 Base，并对每一级类型执行 process_QualType
    * @param BaseExpr 当前级别的 Base 表达式（可能是 MemberExpr、DeclRefExpr、UnaryOperator 等）
    * @note 处理场景：
    * 1. 直接成员访问：a.b.c → 解析 a → a.b → a.b.c（的 Base 是 a.b）
    * 2. 指针成员访问：p->x.y → 解析 p（解引用后）→ p->x（解引用后）
    * 3. 隐式转换：如数组→指针、左值→右值等隐式转换节点
    */
    void traverseMemberExprBases(const clang::Expr* BaseExpr) {
        if (!BaseExpr) return;
        // 步骤1：去除表达式的“糖衣类型”（如 Typedef、Auto 等，获取原始类型）
        BaseExpr = BaseExpr->IgnoreImpCasts();  // 忽略隐式转换（如数组→指针、左值→右值）
        BaseExpr = BaseExpr->IgnoreParens();    // 忽略括号（如 (a.b).c → a.b.c）

        // 步骤2：根据 Base 表达式的类型，递归解析每一级
        if (auto* MemExpr = clang::dyn_cast<clang::MemberExpr>(BaseExpr)) {
            // 场景1：当前 Base 是嵌套的 MemberExpr（如 a.b.c 中的 a.b）
            // 先递归处理上一级 Base（a.b 的 Base 是 a）
            traverseMemberExprBases(MemExpr->getBase());
            // 再处理当前级 Base 的类型（a.b 的类型）
            clang::QualType BaseQT = MemExpr->getBase()->getType().getUnqualifiedType();
            process_QualType(BaseQT, true);
        } 
        else if (auto* UnaryOp = clang::dyn_cast<clang::UnaryOperator>(BaseExpr)) {
            // 场景2：当前 Base 是解引用表达式（如 p->x 中的 *p）
            if (UnaryOp->getOpcode() == clang::UO_Deref) {
                // 递归处理指针本身（p 的类型）
                traverseMemberExprBases(UnaryOp->getSubExpr());
                // 处理解引用后的对象类型（*p 的类型）
                clang::QualType DerefQT = UnaryOp->getType().getUnqualifiedType();
                process_QualType(DerefQT, true);
            }
        }
        else {
            clang::QualType FallbackQT = BaseExpr->getType().getUnqualifiedType();
            process_QualType(FallbackQT, true);
        }
    }
    void processMemberExprAllBases(const clang::MemberExpr* MemExpr) {
        if(!MemExpr) return;
        // std::cout <<"processMemberExprAllBases\t";
        clang::ValueDecl* MemberDecl = MemExpr->getMemberDecl();  // 获取被访问成员的声明
        if (MemberDecl) {
            clang::QualType memberQT;
            if (clang::dyn_cast<clang::FieldDecl>(MemberDecl) ||        // 字段
                clang::dyn_cast<clang::VarDecl>(MemberDecl) ||         // 静态成员变量
                clang::dyn_cast<clang::EnumConstantDecl>(MemberDecl)) { // 枚举成员
                memberQT = MemberDecl->getType();
            } 
            if (!memberQT.isNull()) {
                process_QualType(memberQT); 
            }
        }
        traverseMemberExprBases(MemExpr);
    }
    //
    bool process_Decl(const clang::Decl *decl){
        if(!decl || inst_info.processed_decl.count(decl)!=0) return false;
        std::cout <<"in process_Decl:"<<get_decl_string(decl)<<" Kind:"<<decl->getDeclKindName()<<"\n";
        if(const clang::EnumConstantDecl *EnumConst = clang::dyn_cast<clang::EnumConstantDecl>(decl)) {
            const clang::Decl& currentStmt = *decl;
            const auto& parents  =_ctx->getParents(currentStmt);
            if (!parents.empty()){
                const clang::Decl* d =  parents[0].get<clang::Decl>();
                if(const clang::EnumDecl* ED =  clang::dyn_cast<clang::EnumDecl>(d)){
                    get_enum(ED);
                }
            }
        }
        else if(const clang::VarDecl *var_decl = clang::dyn_cast<clang::VarDecl>(decl)){
            //尝试提取出变量类型
            clang::QualType QT = var_decl->getType();
            // std::cout <<"-- VarDecl:process_QualType::\n";
            process_QualType(QT);
        }
        else if(const clang::FunctionDecl *func_decl = clang::dyn_cast<clang::FunctionDecl>(decl)){
            const auto& params = func_decl->parameters();
            for(auto param : params){
                process_Decl(param);
            }
            get_declare_func(const_cast<clang::FunctionDecl*>(func_decl));
        }
        inst_info.processed_decl.insert(decl);
        return true;
    } 
    void process_arg(const clang::Expr *arg){
        if(!arg) return;
        // std::cout <<"arg type is:"<<arg->getStmtClassName()<<"\t";
        while(const clang::ImplicitCastExpr *argE = clang::dyn_cast<clang::ImplicitCastExpr>(arg)){
            arg = argE->getSubExpr(); findFunctionUsedInStmt(argE);
        }
        std::cout <<"process arg type is:"<<arg->getStmtClassName()<<"\n";
        if(const clang::ParenExpr *parE = llvm::dyn_cast<clang::ParenExpr>(arg)){//圆括号包裹的
            if(parE->getSubExpr()){
                process_arg(parE->getSubExpr());
            }
        }
        else if(const clang::DeclRefExpr *declRefExpr = clang::dyn_cast<clang::DeclRefExpr>(arg)){
            process_Decl(declRefExpr->getDecl());
        }
        else if(const clang::CStyleCastExpr *castExpr=llvm::dyn_cast<clang::CStyleCastExpr>(arg)){
            process_QualType(castExpr->getTypeAsWritten());
            process_arg(castExpr->getSubExpr());
        }
        else if(const clang::CallExpr *callExpr = llvm::dyn_cast<clang::CallExpr>(arg)){
            process_callExpr(callExpr);
        }
        else if(const clang::MemberExpr *memExpr = llvm::dyn_cast<clang::MemberExpr>(arg)){
            processMemberExprAllBases(memExpr);
        }
        else if(const clang::ConditionalOperator *condOp = llvm::dyn_cast<clang::ConditionalOperator>(arg)){
            // 获取三元表达式的三个子表达式
            const clang::Expr *condExpr = condOp->getCond();
            const clang::Expr *trueExpr = condOp->getTrueExpr();
            const clang::Expr *falseExpr = condOp->getFalseExpr();
            
            // 递归处理每一个子表达式
            if (condExpr) process_arg(condExpr);
            if (trueExpr) process_arg(trueExpr);
            if (falseExpr) process_arg(falseExpr);
        }
    }
    void process_callExpr(const clang::CallExpr *callExpr){
        if(!callExpr) return;
        std::cout <<"process_callexpr>>";
        const clang::FunctionDecl *FD = callExpr->getDirectCallee();
        if (FD) {
            get_declare_func(const_cast<clang::FunctionDecl*>(FD));
            pre_msg.nd_funcs.insert(FD->getNameAsString());
            // 获取参数列表
            unsigned numArgs = callExpr->getNumArgs();
            for (unsigned i = 0; i < numArgs; ++i) {
                const clang::Expr *arg = callExpr->getArg(i);
                process_arg(arg);
            }
        }else{
            std::cout <<"Indirect function call encountered.\n";
        }
    }
    bool process_Stmt(const clang::Stmt* stmt) { 
        if (!stmt)
            return false;

        std::string stmt_class = stmt->getStmtClassName();
        // std::cout <<"process_Stmt>>"<<stmt_class<<"\n";
        if (const clang::DeclRefExpr *declRefExpr = clang::dyn_cast<clang::DeclRefExpr>(stmt)){
            process_Decl(declRefExpr->getDecl());
            return true;
        }
        else if (const clang::UnaryExprOrTypeTraitExpr *UExpr = clang::dyn_cast<clang::UnaryExprOrTypeTraitExpr>(stmt)) {
            // std::cout <<"Process_stmt -UnaryExprOrTypeTraitExpr";
            process_QualType(UExpr->getTypeOfArgument());
        }
        else if (const clang::OffsetOfExpr *offsetOfExpr = clang::dyn_cast<clang::OffsetOfExpr>(stmt)) {
            clang::TypeSourceInfo* typeSrcInfo = offsetOfExpr->getTypeSourceInfo();
            if (typeSrcInfo) {
                process_QualType(typeSrcInfo->getType());
            }
            return true;
        }
        else if (const clang::CStyleCastExpr *CExpr = clang::dyn_cast<clang::CStyleCastExpr>(stmt)) {
            process_QualType(CExpr->getTypeAsWritten());
        }
        else if(const clang::CallExpr *callExpr = llvm::dyn_cast<clang::CallExpr>(stmt)){
            process_callExpr(callExpr);
            return true;
        }
        else if(const clang::DeclStmt *declStmt = clang::dyn_cast<clang::DeclStmt>(stmt)){
            for (auto decl : declStmt->decls()) {
                if (decl)
                    process_Decl(decl);
            }
            return true;
        }
        else if(const clang::MemberExpr *memExpr = llvm::dyn_cast<clang::MemberExpr>(stmt)){
            processMemberExprAllBases(memExpr);
            return true;
        }
        
        // 遍历子节点
        for (const clang::Stmt* child : stmt->children()) {
            process_Stmt(child);
        }

        return false;
    }
    bool process_global_VarDecl(clang::VarDecl *var_decl){
        if(inst_info.insert_gvar.count(var_decl)!=0) return true;
        clang::VarDecl* definition = var_decl->getDefinition();
        if (definition) {
            var_decl = definition;
        }
        bool isGlobal = (var_decl->getParentFunctionOrMethod() == nullptr && !var_decl->isLocalVarDeclOrParm());
        if(isGlobal){//global var
            auto &SM = _ctx->getSourceManager();
            auto &LO = _ctx->getLangOpts();
            auto stmt = var_decl;
            std::string str1 = get_decl_string(stmt)+";\n";
            auto SR = get_decl_sourcerange(stmt);
            if(!SM.isInMainFile(SR.getBegin())){
                if(str1.substr(0,7)!="extern " && str1.substr(0,7)!="static ") str1 = "extern "+str1;
            }
            const clang::RecordType* recordType = get_normal_type_decl(stmt->getType())->getAsStructureType();
            if(recordType==nullptr){
                recordType = get_normal_type_decl(stmt->getType())->getAsUnionType();
            }
            bool isAnonymous = isAnonymousRecord(var_decl);
            if(isAnonymous){//  特殊处理匿名union/struct的情况
                int index = str1.find_first_of(')')+1;
                if(recordType){
                    const clang::RecordDecl* recordDecl = recordType->getDecl();
                    str1=get_decl_string(recordDecl)+" "+str1.substr(index);
                    // _rewriter.ReplaceText(SR,str1);
                    // inst_info.insert_gvar.insert(var_decl);
                }
                std::cout<<"Anonymous VarDecl:"<<str1<<"\n";
            }

            process_Decl(var_decl);
            if(var_decl->hasInit()){
                macro_call(var_decl->getInit());
                findFunctionUsedInStmt(var_decl->getInit());
                findGlobalInit(var_decl->getInit());
                findGLobalVarUsedInStmt(var_decl->getInit());
            }
            if(inst_info.insert_gvar.count(var_decl)==0){
                std::cout <<"OUT TO HEADER22:"<<str1<<"\n";
                writeAnyInFunc(str1+";");
                inst_info.insert_gvar.insert(var_decl);
            }
        }
        return true;
    }

    // 递归染色标记
    void color(const clang::VarDecl *VD){
        if(pre_msg.global_used.count(VD)>0) return;
        pre_msg.global_used.insert(VD);
        if(pre_msg.global_var_init.count(VD)!=0){
            std::unordered_set<const clang::VarDecl *> &iset = pre_msg.global_var_init[VD];
            for(auto &i : iset){
                color(i);
            }
        }
        if(pre_msg.global_func_init.count(VD)!=0){
            std::unordered_set<const clang::FunctionDecl *> &iset = pre_msg.global_func_init[VD];
            for(auto &i : iset){
                get_declare_func(const_cast<clang::FunctionDecl*>(i));
            }
        }
    }

    void findGLobalVarUsedInStmt(const clang::Stmt *S, bool flag=false, clang::VarDecl *_VD=nullptr) {
        if(const clang::MemberExpr *memExpr = llvm::dyn_cast<clang::MemberExpr>(S)){
            processMemberExprAllBases(memExpr);
        }
        if (const clang::UnaryExprOrTypeTraitExpr *UExpr = clang::dyn_cast<clang::UnaryExprOrTypeTraitExpr>(S)) {
            process_QualType(UExpr->getTypeOfArgument());
        }
        if (const clang::DeclRefExpr *DRE = llvm::dyn_cast<clang::DeclRefExpr>(S)) {
            if(const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl())){
                bool isGlobal = (VD->getParentFunctionOrMethod() == nullptr && !VD->isLocalVarDeclOrParm());
                if(isGlobal){
                    if(flag){
                        if(pre_msg.global_var_init.count(_VD)==0){
                            std::unordered_set<const clang::VarDecl *> iset ;
                            iset.insert(VD);
                            pre_msg.global_var_init[_VD] = iset;
                        }
                        else{
                            std::unordered_set<const clang::VarDecl *> &iset = pre_msg.global_var_init[_VD];
                            iset.insert(VD);
                            pre_msg.global_var_init[_VD] = iset;
                        }
                    }
                    else{
                        process_global_VarDecl(const_cast<clang::VarDecl*>(VD));
                        if(VD->getNameAsString() != get_stmt_string(S)){
                            pre_msg.global_used.insert(VD);
                            color(VD);
                        }
                    }
                        
                }
            }
        }
        for (const clang::Stmt *Child : S->children()) {
            if (Child) {
                if (const clang::DeclRefExpr *DRE = llvm::dyn_cast<clang::DeclRefExpr>(Child)) {
                    if(const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl())){
                        bool isGlobal = (VD->getParentFunctionOrMethod() == nullptr && !VD->isLocalVarDeclOrParm());
                        if(isGlobal){
                            if(flag){
                                if(pre_msg.global_var_init.count(_VD)==0){
                                    std::unordered_set<const clang::VarDecl *> iset ;
                                    iset.insert(VD);
                                    pre_msg.global_var_init[_VD] = iset;
                                }
                                else{
                                    std::unordered_set<const clang::VarDecl *> &iset = pre_msg.global_var_init[_VD];
                                    iset.insert(VD);
                                    pre_msg.global_var_init[_VD] = iset;
                                }
                            }
                            else{
                                // std::cout <<"Not Flag GLobalVarUsedInStmt-> child process_global_VarDecl"<<get_stmt_string(S)<<"\n";
                                process_global_VarDecl(const_cast<clang::VarDecl*>(VD));
                                if(VD->getNameAsString() != get_stmt_string(S)) {
                                    pre_msg.global_used.insert(VD);
                                    color(VD);
                                }
                            }
                        }
                    }
                }else{
                    findGLobalVarUsedInStmt(Child,flag,_VD);
                }
            }
        }
    }
    void findFunctionUsedInStmt(const clang::Stmt *S, bool flag=false, clang::VarDecl *_VD=nullptr) {
        if (!S) return;
        
        // 首先检查是否是 InitListExpr（数组或结构体初始化）
        if (const clang::InitListExpr *initList = llvm::dyn_cast<clang::InitListExpr>(S)) {
            // 遍历初始化列表中的所有初始化表达式
            for (unsigned i = 0; i < initList->getNumInits(); ++i) {
                const clang::Expr *initExpr = initList->getInit(i);
                if (initExpr) {
                    // std::cout<<"Processing InitListExpr element: " << get_stmt_string(initExpr) << "\n";
                    findFunctionUsedInStmt(initExpr, flag, _VD);
                }
            }
            return;
        }
        
        // 处理类型转换表达式
        if (const clang::ImplicitCastExpr *imExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(S)) {
            const clang::Expr *subExpr = imExpr->getSubExpr();
            if (!subExpr) return;
            
            clang::CastKind castKind = imExpr->getCastKind();
             // 检查是否是 FunctionToPointerDecay
            if (castKind == clang::CK_FunctionToPointerDecay || castKind == clang::CK_LValueToRValue) {
                // 剥去可能的嵌套转换
                subExpr = subExpr->IgnoreImpCasts();
                
                // 获取底层的 DeclRefExpr
                if (const clang::DeclRefExpr *declRefExpr = llvm::dyn_cast<clang::DeclRefExpr>(subExpr)) {
                    const clang::ValueDecl *valueDecl = declRefExpr->getDecl();
                    if (!valueDecl) return;
                    
                    if (const clang::FunctionDecl *FD = llvm::dyn_cast<clang::FunctionDecl>(valueDecl)) {
                        // 处理函数声明
                        if (flag && _VD) {
                            if (pre_msg.global_func_init.count(_VD) == 0) {
                                std::unordered_set<const clang::FunctionDecl *> iset;
                                iset.insert(FD);
                                pre_msg.global_func_init[_VD] = iset;
                            } else {
                                pre_msg.global_func_init[_VD].insert(FD);
                            }
                        }
                        // pre_msg.nd_funcs.insert(FD->getNameAsString());
                        get_declare_func(const_cast<clang::FunctionDecl*>(FD));
                    }
                } else {
                    // 如果不是DeclRefExpr，递归处理子表达式
                    findFunctionUsedInStmt(subExpr, flag, _VD);
                }
            } else {
                // 对于其他类型的转换，递归处理子表达式
                findFunctionUsedInStmt(subExpr, flag, _VD);
            }
            return;
        }
        
        // 处理 CStyleCastExpr
        if (const clang::CStyleCastExpr *csExpr = llvm::dyn_cast<clang::CStyleCastExpr>(S)) {
            const clang::Expr *subExpr = csExpr->getSubExpr();
            if (subExpr) {
                findFunctionUsedInStmt(subExpr, flag, _VD);
            }
            return;
        }
        
        // 处理 DeclRefExpr（直接引用，没有经过类型转换的情况）
        if (const clang::DeclRefExpr *declRefExpr = llvm::dyn_cast<clang::DeclRefExpr>(S)) {
            const clang::ValueDecl *valueDecl = declRefExpr->getDecl();
            if (!valueDecl) return;
            
            if (const clang::FunctionDecl *FD = llvm::dyn_cast<clang::FunctionDecl>(valueDecl)) {
                if (flag && _VD) {
                    if (pre_msg.global_func_init.count(_VD) == 0) {
                        std::unordered_set<const clang::FunctionDecl *> iset;
                        iset.insert(FD);
                        pre_msg.global_func_init[_VD] = iset;
                    } else {
                        pre_msg.global_func_init[_VD].insert(FD);
                    }
                }
                pre_msg.nd_funcs.insert(FD->getNameAsString());
                
                // 打印调试信息
                std::cout << "Found function (direct): " << FD->getNameAsString() << "\n";
            }
            return;
        }
        
        // 对于其他类型的语句，递归处理所有子节点
        for (auto child : S->children()) {
            if (child) {
                findFunctionUsedInStmt(child, flag, _VD);
            }
        }
    }
    //只管数组和类型展开 + 类型
    std::string get_decl_before_equal(const clang::VarDecl *D){
        if(!D)return "";

        auto &SM = _ctx->getSourceManager();
        clang::QualType varType = get_normal_type_decl(D->getType());
        if (const clang::RecordType* recordType = varType->getAsStructureType()) {
            const clang::RecordDecl* recordDecl = recordType->getDecl();
            get_record_string(recordDecl);
        }else if (const clang::RecordType* recordType = varType->getAsUnionType()) {
            const clang::RecordDecl* recordDecl = recordType->getDecl();
            get_record_string(recordDecl);
        }
        auto sloc = get_decl_sourcerange(const_cast<clang::VarDecl*>(D)).getBegin();
        std::string ret = get_decl_string(D); // int a[x]=123
        int index=0,line = SM.getPresumedLineNumber(sloc),col = SM.getPresumedColumnNumber(sloc);
        for(int i=0;i<ret.size();i++){
            if(ret[i]=='=') break;
            if(ret[i]==']'){
                index=i+1;
                // // std::cout <<ret[i]<<"  "<<ret.substr(0,index)<<"\n";
                break;
            }
        }
        if(index){//Array
            auto p = _ctx->getSourceManager().getCharacterData(sloc);
            //不至于数组定义都换行！！
            std::string str1 = ret.substr(0,index);//int a[x]
            std::string str(p);
            ret = str;
            int offset=0;
            for(int i=0;i<ret.size();i++){
                if(ret[i]==']'){
                    offset = i;//int a[x]的长度
                    break;
                }
            }
            auto SR_cast = clang::SourceRange(sloc,sloc.getLocWithOffset(offset));
            
            // std::cout <<"DIreplace "<<str1<<"\n";
            _rewriter.ReplaceText(SR_cast,str1);
        }else{
            if(_ctx->getSourceManager().isMacroBodyExpansion(D->getBeginLoc())){
                //Decl 不包括分号
                std::string str1 = get_decl_string(D);// int x=123
                int l=1;
                for(l=1;l<str1.size() && str1[l]!='=' ;l++);
                if(l == str1.size()) str1 = str1+";";// int x
                else str1 = str1.substr(0,l+1); //取出int x
                // std::cout <<"Dreplace "<<str1.substr(0,l+2)<<"|\n";
                auto p = _ctx->getSourceManager().getCharacterData(sloc);
                for(l=1;p[l]!='=' && p[l]!=';';l++);//直到等号或分号
                std::string strp(p, l);
                auto SR_decl = clang::SourceRange(sloc,sloc.getLocWithOffset(l));
                _rewriter.ReplaceText(SR_decl,str1);
            }
        }
        return ret;
    }
   
    //获取普通类型的字符串，也就是声明语句最前端的类型，同样屏蔽数组和指针
    std::string get_normal_type(const clang::DeclStmt *DS){
        std::string typeName="";
        
        for(auto d : DS->decls()){
            if (const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(d)) {
                clang::QualType QT = VD->getType();
                if (QT.getTypePtr()->isArrayType()){
                    QT = clang::QualType::getFromOpaquePtr(QT.getLocalUnqualifiedType().getNonReferenceType().getTypePtr()->getArrayElementTypeNoTypeQual());
                }
                if (const clang::TypedefType* typedefType = QT->getAs<clang::TypedefType>()){typeName = QT.getAsString(); break;}
                if (const clang::PointerType* pointerType = QT->getAs<clang::PointerType>()) {
                    QT = pointerType->getPointeeType();
                }
                typeName = QT.getAsString();
            }
            break;
        }
        
        return typeName;
    }

    bool isMacro(clang::Stmt *stmt){//识别不出cast-因为有括号包着-特判
        bool f=false;
        if (!stmt) return f;
        if (_ctx->getSourceManager().isMacroBodyExpansion(stmt->getBeginLoc()))f=true;
        if (_ctx->getSourceManager().isMacroBodyExpansion(stmt->getEndLoc()))f=true;
        return f;
    }
    bool hasMacro(clang::Stmt *stmt){
        bool f=false;
        if (!stmt) return f;
        if (_ctx->getSourceManager().isMacroBodyExpansion(stmt->getBeginLoc()))f=true;
        for(auto child : stmt->children()){
            if(f) break;
            if(child){
                f|=hasMacro(child);
            }
        }
        return f;
    }
    void macro_call(clang::Stmt *stmt){
        if (!stmt) return;
        if (clang::CallExpr *callExpr = llvm::dyn_cast<clang::CallExpr>(stmt)){//函数调用
            process_callExpr(callExpr);
            return;
        }
        for(auto child : stmt->children()){
            if(child){
                macro_call(child);
            }
        }
    }
    char getCharInLoc(clang::SourceLocation loc){
        auto p = _ctx->getSourceManager().getCharacterData(loc);
        return p[0];
    }

    void printLoc(clang::SourceLocation loc){
        auto &SM = _ctx->getSourceManager();
        std::cout <<"printLoc:[ "<<SM.getFilename(loc).str()<<" :"<<SM.getPresumedLineNumber(loc)<<","<<SM.getPresumedColumnNumber(loc)<<"]\n";
    }

    //宏全部展开，call提取声明
    bool macro_slice(clang::Stmt *stmt,int cond=0){
        auto &SM = _ctx->getSourceManager();
        auto &LO = _ctx->getLangOpts();
        bool ret=false;


        // 创建工具实例
        MacroTools macroTools(_ctx, _rewriter);
        StmtSourceInfo sourceInfo = macroTools.get_stmt_source_info(stmt);
        // auto SR = sourceInfo.sourceRange;
        auto SR = get_sourcerange(stmt);
        int hashvalue = SR.getBegin().getHashValue();

        std::string str1 = get_stmt_string(stmt);
        
        std::string rewrittenText = _rewriter.getRewrittenText(SR);
        std::cout << "[macro--" << str1 << " -- " << rewrittenText << "--]\n";
        if(pre_msg.macro_origin.count(hashvalue)==0){
            pre_msg.macro_origin[hashvalue] = rewrittenText;
        }

        // // ========== 新增：特判 DeclStmt，提取并保留完整类型名+声明文本 ==========
        // std::string decl_full_text;  // 存储 DeclStmt 的完整声明文本（含类型名）
        // clang::DeclStmt *decl_stmt = clang::dyn_cast<clang::DeclStmt>(stmt);
        // if (decl_stmt) {
        //     // 遍历 DeclStmt 中的所有声明（可能多个变量同声明：int a, b;）
        //     for (clang::Decl *D : decl_stmt->decls()) {
        //         // 提取单个声明的完整源文本（从声明开始到结束）
        //         clang::SourceRange decl_SR = get_decl_sourcerange(D);
        //         if (decl_SR.isValid() && !isInSystemHeader(decl_SR.getBegin())) {
        //             std::string single_decl = _rewriter.getRewrittenText(decl_SR);
        //             if (!single_decl.empty()) {
        //                 decl_full_text = single_decl;
        //             }
        //         }
        //     }
        //     // 如果是 DeclStmt 且提取到完整文本，直接覆盖 str1（避免后续替换丢失类型）
        //     if (!decl_full_text.empty()) {
        //         str1 = decl_full_text;
        //         std::cout << "[DeclStmt 识别] 保留类型名：" << str1 << "\n";
        //     }
        // }
        // // 
        
        
        bool skip_replace = false;
        // 输出检测到的宏信息
        if (!sourceInfo.macroExpansions.empty()) {
            std::cout << "Found " << sourceInfo.macroExpansions.size() << " macro expansions:\n";
            for (const auto &macro : sourceInfo.macroExpansions) {
                std::cout << "  Macro: " << macro.macroName << " -> " << macro.expandedText << "\t";
                if(macro.isMacroNameInExpansion()){
                    std::cout <<"SKIP\n";
                    skip_replace = true;
                    break;
                }else{
                    std::cout <<"\n";
                }
            }
        }
        // printLoc(SR.getBegin());printLoc(SR.getEnd());

        bool sys_f=false;
        if(isInSystemHeader(stmt->getBeginLoc()) || isInSystemHeader(stmt->getEndLoc())){
            sys_f=true;
        } 
        if(!sys_f && str1!=rewrittenText){
            std::cout <<"hashvalue is "<<hashvalue<<" cond is "<<cond<<"\n";
            if(skip_replace){
                _rewriter.ReplaceText(SR, pre_msg.macro_origin[hashvalue]);
            }
            else if(pre_msg.macro_visit.count(hashvalue)==0){
                auto tloc = clang::Lexer::getLocForEndOfToken(SR.getEnd(),1,SM,LO);
                auto nloc = SR.getEnd().getLocWithOffset(1);
                if(!tloc.isValid() || isInSystemHeader(tloc)) {tloc = nloc;}
                if(cond!=4 && (getCharInLoc(tloc)=='\n')) {str1+=";";}
                else if(cond!=4 && (getCharInLoc(tloc)==';')) {str1+=";";}
                else {
                    std::cout <<"normal macro["<<getCharInLoc(tloc)<<" "<<getCharInLoc(nloc)<<"]\n";
                }
                //针对展开后为func1(p1)(p1)的情况 后一个p1是原有的，前一个是展开的stmt内容
                if(getCharInLoc(tloc.getLocWithOffset(1))=='(' && str1.at(str1.size()-1)==')'){
                    tloc = tloc.getLocWithOffset(1);
                    const char *p = SM.getCharacterData(tloc);
                    int left=1;
                    clang::SourceLocation eloc=tloc;
                    for(int i=1;i<100;i++){
                        eloc = tloc.getLocWithOffset(i);
                        if(eloc.isInvalid() || isInSystemHeader(eloc)) break;
                        if(p[i]=='(') left++;
                        if(p[i]==')') left--;
                        if(left==0){
                            if(eloc.isValid()) SR.setEnd(eloc);
                            break;
                        }
                    }
                }
                std::cout <<"11Macro Replace Stmt:"<<str1<<" to "<<_rewriter.getRewrittenText(SR)<<"\n";
                _rewriter.ReplaceText(SR,str1);
                pre_msg.macro_visit[hashvalue] = str1;
            }
            else if(pre_msg.macro_visit[hashvalue] != inst_info.SKIPMACRO){
                // 这个宏展开成很多个stmt，现在处理后续的
                std::cout <<"22next macro add "<<str1<<"\n";
                std::string existing = pre_msg.macro_visit[hashvalue];
                auto eloc = clang::Lexer::getLocForEndOfToken(SR.getEnd(),1,SM,LO);
                if(SR.isValid()){
                    if(existing.back()!=';') existing+=";";
                    str1 = existing + str1;
                    std::cout <<"Macro EXpand Stmt:"<<str1<<" replace2 "<<_rewriter.getRewrittenText(SR)<<"|\n";
                    pre_msg.macro_visit[hashvalue] = str1;// update macro expand text
                    if(cond == 2){
                        // if内部
                        std::string replaced = "{"+str1+";}";
                        _rewriter.ReplaceText(SR,replaced);
                    }
                    else _rewriter.ReplaceText(SR,str1);
                }else{
                    // std::cout <<"Macro EXpand Stmt Invalid SourceRange\n";
                }
                
            }
        }
        return ret;
    }
    
    //对stmt进行切片-false还有剩，true全部切
     //stmtTp用于特殊情况的处理//stmt=4 条件,stmt=6:return 一定保留
    // param1 
    bool instSlice(clang::SourceManager &SM,clang::Stmt *stmt, std::string func_name,std::string filePath, 
                    int & param1,int func_end,bool &cutall, int stmtTp)
    {
        if(!stmt) return false;
        std::string stmt_class = stmt->getStmtClassName();
        std::cout <<"stmt>>"<<get_stmt_string(stmt)<<"  [Stmt-class]"<<stmt_class<<" stmtTp="<<stmtTp<<"\n";
        clang::SourceRange SR = stmt->getSourceRange();
        if (SR.isValid()) {
            // llvm::outs()<< "Valid SourceRange: " << SR.getBegin().printToString(SM) << " - " << SR.getEnd().printToString(SM) << "\n";
            // 打印有效的 SourceRange 信息
            // // std::cout << "Valid SourceRange: " << SR.getBegin().printToString(SM) << " - " << SR.getEnd().printToString(SM) << "\n";
        }else {
            return false;
        //    llvm::outs()<< "Invalid SourceRange\n";
        }
        
        auto &LO = _ctx->getLangOpts();
        int line_start = SM.getPresumedLineNumber(SR.getBegin());
        int line_end   = SM.getPresumedLineNumber(SR.getEnd());
        int col_start, col_end;
        col_start = SM.getPresumedColumnNumber(SR.getBegin());
        col_end   = SM.getPresumedColumnNumber(SR.getEnd());
        int is_slice =isSlice(func_name,line_start,line_end);
        if(isMacro(stmt)){
            std::cout <<"Macro";//<<line_start<<" "<<line_end<<"\n";
            if(clang::LabelStmt *lStmt= llvm::dyn_cast<clang::LabelStmt>(stmt)){//Label:xx 保留label
                stmt = lStmt->getSubStmt();
            }
            if(stmt_class!="ReturnStmt"&&is_slice==1&&stmtTp<=2&&stmt_class!="DeclStmt"){
                // // std::cout <<"remove-\n";
                SR = get_sourcerange(stmt);
                if(SR.isValid()){
                    if(SR.getBegin() != SR.getEnd()){
                        // std::cout <<"---remove Macro---\n";
                        _rewriter.RemoveText(SR);
                    }
                }
                return true;
            }else{
                cutall=false;
                if(stmt_class!="DeclStmt" && stmtTp!=3 && stmtTp!=6 && stmtTp!=8){
                    macro_slice(stmt ,stmtTp);
                }
                findGLobalVarUsedInStmt(stmt);
                macro_call(stmt);
                process_Stmt(stmt);
                return false;
            }
        } 
        if(stmtTp<=2 && is_slice==1){ //不是宏的子结点、需要被删除
            std::cout <<"isSlice-True\n";
            if(clang::LabelStmt *lStmt= llvm::dyn_cast<clang::LabelStmt>(stmt)){//Label:xx 保留label
                stmt = lStmt->getSubStmt();
            }
            if (clang::GotoStmt *gStmt= llvm::dyn_cast<clang::GotoStmt>(stmt)){
                cutall=false;return false; 
            }
            if(clang::ReturnStmt *retStmt= llvm::dyn_cast<clang::ReturnStmt>(stmt)){//return
                findGLobalVarUsedInStmt(stmt);
                if(retStmt->getRetValue()){
                    instSlice(SM,retStmt->getRetValue(),func_name,filePath, param1,func_end,cutall,6);
                    std::string str1 = get_stmt_string(retStmt->getRetValue());
                    auto SR = get_sourcerange(retStmt->getRetValue());
                    _rewriter.ReplaceText(SR,str1);
                }
                cutall=false;
                return false;
            }
            if(stmt_class=="NullStmt"){
                cutall=false;
                return false;
            }
            if(stmtTp==1 &&stmt_class == "BreakStmt"){//switch展开的break语句
                cutall=false;return false;
            }
            if(stmtTp==2 &&(stmt_class=="ContinueStmt" ||stmt_class=="BreakStmt")){//if里的continue/break语句
                cutall=false;return false;
            }

            if (stmt_class == "DeclStmt"){
                cutall=false;
                std::string str_stmt = get_stmt_string(stmt);
                clang::DeclStmt * sd=llvm::dyn_cast<clang::DeclStmt>(stmt);
                bool flag = false;
                int flag_1=0;
                for(auto ss : sd->getDeclGroup()){
                    if(llvm::isa<clang::VarDecl>(ss)){ // int x;
                        const clang::VarDecl *vardecl=llvm::dyn_cast<clang::VarDecl>(ss);
                        if(!flag_1){
                            get_decl_before_equal(vardecl);
                            process_QualType(vardecl->getType());
                            flag_1=1;
                        }
                        if(vardecl->hasInit()){
                            clang::QualType declType=vardecl->getType();
                            while(const clang::PointerType* pointerType = declType->getAs<clang::PointerType>()) {
                                declType = pointerType->getPointeeType();
                            }
                            if(declType.isConstQualified() || declType.getTypePtr()->isArrayType()){
                                // std::cout <<"[var] is const";
                                instSlice(SM,const_cast<clang::Expr *>(vardecl->getAnyInitializer()),func_name,filePath, param1,func_end,cutall,5);
                            }else{
                                remove_with_left(const_cast<clang::Expr*>(vardecl->getAnyInitializer()));
                            }
                        }
                        
                    }
                }
                return false;//保留声明、删去初始化
            }
            
            SR = get_sourcerange(stmt);
            std::cout <<"---remove---\n";
            _rewriter.RemoveText(SR);
            return true;
        }
        else{
            std::cout <<"isSlice-False\n";
            cutall=false;
            bool stmt_cut=false;
            process_Stmt(stmt);
            if(clang::LabelStmt *labelStmt= llvm::dyn_cast<clang::LabelStmt>(stmt)){//Label:xx
                if(labelStmt->getSubStmt()){
                    // std::cout <<get_decl_string(labelStmt->getDecl())<<"\n";
                    instSlice(SM,labelStmt->getSubStmt(),func_name,filePath, param1,func_end,cutall,stmtTp);
                }
            }
            else if(clang::ImplicitCastExpr * iExpr=llvm::dyn_cast<clang::ImplicitCastExpr>(stmt)){
                if(iExpr->getSubExpr())
                    instSlice(SM,iExpr->getSubExpr(),func_name,filePath, param1,func_end,cutall,stmtTp);
            }
            else if (clang::DeclStmt * sd=llvm::dyn_cast<clang::DeclStmt>(stmt)){//定义语句
                findGLobalVarUsedInStmt(sd);
                int flag_1=0;
                for(auto ss : sd->getDeclGroup()){
                    DefFunc def_func = inst_info.funcs_locs[func_name]; 
                    if(llvm::isa<clang::VarDecl>(ss)){ // int x;
                        clang::VarDecl *vardecl=llvm::dyn_cast<clang::VarDecl>(ss);
                        if(!flag_1){
                            clang::QualType type = vardecl->getType();
                            process_QualType(type);
                            get_decl_before_equal(vardecl);
                            flag_1=1;
                        }
                        if(vardecl->hasInit()){
                            if(instSlice(SM,const_cast<clang::Expr *>(vardecl->getAnyInitializer()),func_name,filePath, param1,func_end,cutall,stmtTp)){
                                remove_with_left(const_cast<clang::Expr *>(vardecl->getAnyInitializer()));
                            }
                        }
                    }
                }
            }else if(clang::ForStmt *forStmt = llvm::dyn_cast<clang::ForStmt>(stmt)){//复合语句
                clang::Stmt *sbody = forStmt->getBody();
                if(forStmt->getInit()!=nullptr){
                    findGLobalVarUsedInStmt(forStmt->getInit());
                    instSlice(SM,forStmt->getInit(),func_name,filePath, param1,func_end,cutall,4);
                }
                if(forStmt->getCond()!=nullptr){
                    findGLobalVarUsedInStmt(forStmt->getCond());
                    instSlice(SM,forStmt->getCond(),func_name,filePath, param1,func_end,cutall,4);
                }
                if(forStmt->getInc()!=nullptr){
                    findGLobalVarUsedInStmt(forStmt->getInc());
                    instSlice(SM,forStmt->getInc(),func_name,filePath, param1,func_end,cutall,4);
                }
                std::string sbody_class = sbody->getStmtClassName();
                bool isCutAllFor=true;//for内部是否全部切掉
                if(!(sbody_class=="CompoundStmt")){
                    isCutAllFor&=instSlice(SM,sbody,func_name,filePath, param1,func_end,cutall,stmtTp);
                }
                else{
                    for(auto ss : sbody->children()){
                        if(ss)
                            isCutAllFor&=instSlice(SM,ss,func_name,filePath, param1,func_end,cutall,stmtTp);
                    }
                }
                // if(isCutAllFor){
                //     _rewriter.RemoveText(stmt->getSourceRange());
                // }
            }else if(clang::IfStmt * si=llvm::dyn_cast<clang::IfStmt>(stmt)){
               if(si->getCond()!=nullptr){
                    findGLobalVarUsedInStmt(si->getCond());
                    instSlice(SM,si->getCond(),func_name,filePath, param1,func_end,cutall,4);
                }
                    
                clang::Stmt *sthen = si->getThen();
                bool then_left = false;
                if(sthen){
                    std::string sthen_class = sthen->getStmtClassName();
                    clang::SourceLocation last_stmt=si->getRParenLoc().getLocWithOffset(1);
                    if(!(sthen_class=="CompoundStmt")){
                        std::cout <<"add {}";
                        if(instSlice(SM,sthen,func_name,filePath, param1,func_end,cutall,2)){
                            if(si->hasElseStorage()){
                                // printLoc(si->getRParenLoc().getLocWithOffset(1));
                                // // std::cout <<"[[["<<_rewriter.getRewrittenText(clang::SourceRange(si->getRParenLoc().getLocWithOffset(1),si->getElseLoc().getLocWithOffset(-1)))<<"]]]\n";
                                // _rewriter.ReplaceText(clang::SourceRange(si->getRParenLoc().getLocWithOffset(1),si->getElseLoc().getLocWithOffset(-1)), " {;}");
                                
                                auto tloc = clang::Lexer::getLocForEndOfToken(get_sourcerange(sthen).getEnd(),1,SM,LO);
                                auto nloc = get_sourcerange(sthen).getEnd().getLocWithOffset(1);
                                // printLoc(tloc);printLoc(nloc);
                                if(!tloc.isValid() || isInSystemHeader(tloc)) {tloc = nloc;}
                                //  std::cout <<"if endloc:[[["<<_rewriter.getRewrittenText(clang::SourceRange(si->getRParenLoc().getLocWithOffset(1), tloc.getLocWithOffset(1)))<<"]]]\n";
                                
                                _rewriter.ReplaceText(clang::SourceRange(si->getRParenLoc().getLocWithOffset(1), tloc.getLocWithOffset(1)), " {;}");
                            }

                            else _rewriter.ReplaceText(sthen->getSourceRange(), "{}");
                        }else{
                            if(si->hasElseStorage()){
                                // printLoc(last_stmt);
                                // std::cout <<_rewriter.getRewrittenText(clang::SourceRange(si->getElseLoc(),si->getElse()->getBeginLoc().getLocWithOffset(-1)))<<"]\n";
                                _rewriter.ReplaceText(clang::SourceRange(last_stmt,get_sourcerange(sthen).getBegin().getLocWithOffset(-1)),"\n\t{");
                                then_left =true;
                                // _rewriter.InsertTextAfter(si->getRParenLoc().getLocWithOffset(1), "{ ");
                                // printLoc(si->getElse()->getBeginLoc().getLocWithOffset(-1));
                                _rewriter.ReplaceText(clang::SourceRange(si->getElseLoc(),si->getElseLoc().getLocWithOffset(4)), "}else");
                            }
                        }
                    }
                    else{
                        bool cut_all=true;
                        last_stmt=si->getRParenLoc().getLocWithOffset(1);
                        for(auto ss : sthen->children()){
                            // std::cout <<"if-then:";
                            cut_all&=instSlice(SM,ss,func_name,filePath, param1,func_end,cutall,2);
                        }
                        // if(si->hasElseStorage()){
                        //     _rewriter.ReplaceText(clang::SourceRange(si->getElseLoc(),si->getElse()->getBeginLoc().getLocWithOffset(-1)), "else ");
                        // }
                        // if(cut_all){
                        //     _rewriter.ReplaceText(get_sourcerange(si->getCond()),"0");
                        // }
                    }
                } 
                if(si->hasElseStorage()){
                    clang::Stmt *selse = si->getElse();
                    if(selse){
                        std::string selse_class = selse->getStmtClassName();
                        SR=selse->getSourceRange();
                        if(!isSlice(func_name,SM.getPresumedLineNumber(SR.getBegin()), SM.getPresumedLineNumber(SR.getEnd()))){
                            if(!(selse_class=="CompoundStmt")){
                                instSlice(SM,selse,func_name,filePath, param1,func_end,cutall,2);
                            }
                            else{
                                for(auto ss : selse->children()){
                                    instSlice(SM,ss,func_name,filePath, param1,func_end,cutall,2);
                                }
                            }
                        }
                        else{
                            clang:: SourceLocation startLoc = si->getElseLoc();
                            line_end=SM.getPresumedLineNumber(SR.getEnd()),col_end=SM.getPresumedColumnNumber(SR.getEnd());
                            clang:: SourceLocation endLoc = SR.getEnd().getLocWithOffset(1);
                            if(then_left)
                                _rewriter.ReplaceText(clang::SourceRange(startLoc,endLoc),"/*insert*/}else{;}");
                            else 
                            _rewriter.ReplaceText(clang::SourceRange(startLoc,endLoc),"else{;}");
                        }   
                    }
                }
                
            }else if(clang::DoStmt *doStmt =llvm::dyn_cast<clang::DoStmt>(stmt)){
                clang::Stmt *sbody = doStmt->getBody();
                if(doStmt->getCond()!=nullptr){
                    findGLobalVarUsedInStmt(doStmt->getCond());
                    instSlice(SM,doStmt->getCond(),func_name,filePath, param1,func_end,cutall,4);
                }
                if(sbody){
                    std::string sbody_class = sbody->getStmtClassName();
                    if(!(sbody_class=="CompoundStmt")){
                        instSlice(SM,sbody,func_name,filePath, param1,func_end,cutall,stmtTp);
                    }
                    else{
                        for(auto ss : sbody->children()){ 
                            if(ss) instSlice(SM,ss,func_name,filePath, param1,func_end,cutall,stmtTp);
                        }
                    }
                }
                
            }else if(clang::WhileStmt *whileStmt =llvm::dyn_cast<clang::WhileStmt>(stmt)){
                clang::Stmt *sbody = whileStmt->getBody();
                if(whileStmt->getCond()!=nullptr){
                    findGLobalVarUsedInStmt(whileStmt->getCond());
                    instSlice(SM,whileStmt->getCond(),func_name,filePath, param1,func_end,cutall,4);
                }
                std::string sbody_class = sbody->getStmtClassName();
                if(!(sbody_class=="CompoundStmt")){
                    instSlice(SM,sbody,func_name,filePath, param1,func_end,cutall,stmtTp);
                }
                else{
                    for(auto ss : sbody->children()){ 
                        if(ss) instSlice(SM,ss,func_name,filePath, param1,func_end,cutall,stmtTp);
                    }
                }
            }else if(clang::SwitchStmt *switchStmt = llvm::dyn_cast<clang::SwitchStmt>(stmt)){//FIX 
                clang::Stmt *sbody = switchStmt->getBody();
                if(switchStmt->getCond()!=nullptr){
                    findGLobalVarUsedInStmt(switchStmt->getCond()) ;
                    instSlice(SM,switchStmt->getCond(),func_name,filePath, param1,func_end,cutall,4);
                }
                clang::SwitchCase *currentCase = nullptr;//case暂存
                bool first=false;
                bool keep=false;
                std::string stmtclass;
                for(auto ss : sbody->children()){
                    if(!ss) continue;
                    stmtclass = ss->getStmtClassName();
                    if(stmtclass=="CaseStmt"||stmtclass=="DefaultStmt"){
                        if(!first){
                            first=true;
                            currentCase=llvm::dyn_cast<clang::SwitchCase>(ss);

                            // if(currentCase!=nullptr) std::cout <<"First CASE:"<<get_stmt_string(currentCase)<<"\n";
                        }
                        else{
                            if(keep) 
                                instSlice(SM,currentCase,func_name,filePath, param1,func_end,cutall,3);//保留case x:
                            else   instSlice(SM,currentCase,func_name,filePath, param1,func_end,cutall,1);
                            currentCase=llvm::dyn_cast<clang::SwitchCase>(ss);
                            // if(currentCase!=nullptr) std::cout <<"Change CASE:"<<get_stmt_string(currentCase)<<"\n";
                            keep=false;
                        }
                    }else{//套在case里的其他语句
                        if(clang::isa<clang::BreakStmt>(ss)){
                            if(keep==true) continue;
                            else{//删除break
                                keep|= !instSlice(SM,ss,func_name,filePath, param1,func_end,cutall,1);
                            }
                        }else{
                            keep|= !instSlice(SM,ss,func_name,filePath, param1,func_end,cutall,1);
                        }
                    }
                    
                }
                //最后一个case'
                if(keep) 
                    instSlice(SM,currentCase,func_name,filePath, param1,func_end,cutall,3);//保留case x:
                else   instSlice(SM,currentCase,func_name,filePath, param1,func_end,cutall,1);
                
            }else if(clang::SwitchCase *caseStmt = llvm::dyn_cast<clang::SwitchCase>(stmt)){//
                clang::Stmt *sbody = caseStmt->getSubStmt();
                if(clang::CaseStmt *cStmt = llvm::dyn_cast<clang::CaseStmt>(stmt)){
                    if(cStmt->getLHS()!=nullptr){
                        instSlice(SM,cStmt->getLHS(),func_name,filePath, param1,func_end,cutall,4);
                    }
                }
                findGLobalVarUsedInStmt(stmt);
                if(sbody){
                    std::string sbody_class = sbody->getStmtClassName();
                    if(!(sbody_class=="CompoundStmt")){
                        instSlice(SM,sbody,func_name,filePath, param1,func_end,cutall,1);
                    }
                    else{
                        for(auto ss : sbody->children()){ 
                            if(ss) instSlice(SM,ss,func_name,filePath, param1,func_end,cutall,1);
                        }
                    }
                }
                
            }else if(const clang::BinaryOperator *bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)){//可能是逗号 用于连接多个语句 或是+ - * / 关注其中的逗号和等号
                clang::Expr *lhs =  bop->getLHS(),*rhs = bop->getRHS();
                clang::BinaryOperator::Opcode op = bop->getOpcode();
                if(lhs==nullptr || rhs==nullptr) return false;//防止空指针
                std::string lhs_class = lhs->getStmtClassName(),rhs_class = rhs->getStmtClassName();
                // x = y, x+=1, y = a+b == c+d , x=call(), call()
                if(op==clang::BinaryOperator::Opcode::BO_Comma){// , 连接 左右应当都是单独的表达式 
                    bool tmp1 = instSlice(SM,lhs,func_name,filePath, param1,func_end,cutall,stmtTp),
                    tmp2=instSlice(SM,rhs,func_name,filePath, param1,func_end,cutall,stmtTp);
                    if(tmp1 || tmp2){//cut comma
                        auto opLoc = bop->getOperatorLoc();
                        auto opSR = clang::SourceRange(opLoc,opLoc.getLocWithOffset(1));
                        _rewriter.ReplaceText(opSR,"/* comma */");
                    }else{
                        findGLobalVarUsedInStmt(bop);
                    }
                }else{
                    std::cout <<"--BO--"<<lhs_class<<"  "<<rhs_class<<"\n";
                    bool isCut=false,l=0,r=0,lc=0;
                    bool tmp=0,left_n=0;;
                    int tmp1=2;//tmp 是否插入过/删除;tmp1 是否有剩余 0没有
                    //cast肯定用在declstmt\二元表达式 或者 函数调用的参数？ 
                    if(clang::CallExpr * callExpr=llvm::dyn_cast<clang::CallExpr>(lhs)){//call() +1
                        tmp=1;
                        findGLobalVarUsedInStmt(callExpr);
                        std::string str1=get_stmt_string(callExpr);
                        auto SR=get_sourcerange(callExpr);
                        _rewriter.ReplaceText(SR,str1);
                        process_callExpr(const_cast<clang::CallExpr*>(callExpr));
                    }else if(clang::CStyleCastExpr * castExpr=llvm::dyn_cast<clang::CStyleCastExpr>(lhs)){
                        tmp=1;
                        if(castExpr->getSubExpr())
                            instSlice(SM,castExpr->getSubExpr(),func_name,filePath, param1,func_end,cutall,8);
                    }else if(clang::BinaryOperator *bo=llvm::dyn_cast<clang::BinaryOperator>(lhs)){
                        if(stmtTp!=4)
                            instSlice(SM,lhs,func_name,filePath, param1,func_end,cutall,3);
                    }
                    
                    if(clang::CallExpr * callExpr=llvm::dyn_cast<clang::CallExpr>(rhs)){//x = call()
                        tmp=1;
                        // findGLobalVarUsedInStmt(callExpr);
                        std::string str1 = get_stmt_string(callExpr);
                        auto SR=get_sourcerange(callExpr);
                        _rewriter.ReplaceText(SR,str1);
                        process_callExpr(const_cast<clang::CallExpr*>(callExpr));
                    }else if(clang::CStyleCastExpr * castExpr=llvm::dyn_cast<clang::CStyleCastExpr>(rhs)){
                        tmp=1;
                        if(castExpr->getSubExpr())
                            instSlice(SM,castExpr->getSubExpr(),func_name,filePath, param1,func_end,cutall,8);
                    }else if(clang::BinaryOperator *bo=llvm::dyn_cast<clang::BinaryOperator>(rhs)){
                        if(stmtTp!=4)
                            instSlice(SM,rhs,func_name,filePath, param1,func_end,cutall,3);
                    }
                    findGLobalVarUsedInStmt(stmt);
                    findFunctionUsedInStmt(stmt);
                    if(stmtTp!=3) macro_slice(stmt,stmtTp);
                }

                if(const clang::ImplicitCastExpr *argE = clang::dyn_cast<clang::ImplicitCastExpr>(lhs)){
                    lhs=const_cast<clang::Expr*>(argE->getSubExpr()); findFunctionUsedInStmt(lhs);
                }
                if(const clang::DeclRefExpr *declRefExpr = clang::dyn_cast<clang::DeclRefExpr>(lhs)){
                    process_Decl(declRefExpr->getDecl());
                }
                if(const clang::ImplicitCastExpr *argE = clang::dyn_cast<clang::ImplicitCastExpr>(rhs)){
                    rhs=const_cast<clang::Expr*>(argE->getSubExpr()); findFunctionUsedInStmt(rhs);
                }
                if(const clang::DeclRefExpr *declRefExpr = clang::dyn_cast<clang::DeclRefExpr>(rhs)){
                    process_Decl(declRefExpr->getDecl());
                }

            }
            else if(clang::ParenExpr *parE = llvm::dyn_cast<clang::ParenExpr>(stmt)){//圆括号包裹的
                if(parE->getSubExpr()){
                    if(instSlice(SM,parE->getSubExpr(),func_name,filePath, param1,func_end,cutall,stmtTp)){
                        _rewriter.InsertTextBefore(parE->getEndLoc(),";");
                    }
                }
                else{
                    // std::cout <<"ParenExpr do not have subExpr,"<<get_stmt_string(parE)<<"\n";
                }
            }
            else if(clang::CallExpr *callExpr = llvm::dyn_cast<clang::CallExpr>(stmt)){//函数调用
                std::cout <<"callexpr";
                findGLobalVarUsedInStmt(callExpr);
                std::string str1=get_stmt_string(callExpr);
                auto SR=get_sourcerange(const_cast<clang::CallExpr*>(callExpr));
                if(stmtTp!=8)
                    _rewriter.ReplaceText(SR,str1);

                process_callExpr(const_cast<clang::CallExpr*>(callExpr));
            }
            else if(clang::CStyleCastExpr * castExpr=llvm::dyn_cast<clang::CStyleCastExpr>(stmt)){
                if(stmtTp!=6 && stmtTp!=8) stmtTp=5;
                if(castExpr->getSubExpr()){
                    if(instSlice(SM,castExpr->getSubExpr(),func_name,filePath, param1,func_end,cutall,stmtTp)){
                        if(stmtTp!=8)remove_with_left(castExpr);
                    }else{
                        process_QualType(castExpr->getTypeAsWritten());
                        findGLobalVarUsedInStmt(castExpr);
                        std::string str1 = get_stmt_string(castExpr);
                        int l=1;
                        for(l=1;str1[l]!=')';l++);
                        str1 = str1.substr(0,l+1);
                        auto p = _ctx->getSourceManager().getCharacterData(castExpr->getBeginLoc());
                        l=1;
                        for(l=1;p[l]!=')';l++);
                        auto SR_cast = clang::SourceRange(castExpr->getBeginLoc(),castExpr->getBeginLoc().getLocWithOffset(l));
                        if(stmtTp!=8 && SR_cast.isValid()) _rewriter.ReplaceText(SR_cast,str1);
                    }
                }
            }
            else if(clang::MemberExpr *memExpr=llvm::dyn_cast<clang::MemberExpr>(stmt)){
                // 结构体成员访问需要完整定义
                processMemberExprAllBases(const_cast<clang::MemberExpr*>(memExpr));
                findGLobalVarUsedInStmt(stmt);
                macro_slice(stmt,stmtTp);
            }
            else if(clang::ReturnStmt *retStmt= llvm::dyn_cast<clang::ReturnStmt>(stmt)){//return
                findGLobalVarUsedInStmt(retStmt);
                if(retStmt->getRetValue()){
                    instSlice(SM,retStmt->getRetValue(),func_name,filePath, param1,func_end,cutall,6);
                    std::string str1 = get_stmt_string(retStmt->getRetValue());
                    auto SR = get_sourcerange(retStmt->getRetValue());
                    _rewriter.ReplaceText(SR,str1);
                }
            }
            else{
                findGLobalVarUsedInStmt(stmt);
                findFunctionUsedInStmt(stmt);
                macro_slice(stmt,stmtTp);
            }
            // //for all stmt
            // if(stmtTp==6){//ret
            //     findGLobalVarUsedInStmt(stmt);
            //     std::string str1=get_stmt_string(stmt);
            //     auto SR=get_sourcerange(stmt);
            //     _rewriter.ReplaceText(SR,str1);
            // }
            if(stmtTp==4 && stmt_cut &&is_slice==0){//条件，应该要留下但一点不剩
                std::string str1="1";
                auto SR = stmt->getSourceRange();
                // llvm::outs()<< "Cond get SourceRange: " << SR.getBegin().printToString(SM) << " - " << SR.getEnd().printToString(SM) << "\n";
    
                int tmp_col = SM.getPresumedColumnNumber(SR.getBegin());
                if(isMacro(stmt)) SR=get_sourcerange(stmt);
                
                // // std::cout <<"Cond "<<str1<<"\n";
                // llvm::outs()<< "Cond SourceRange: " << SR.getBegin().printToString(SM) << " - " << SR.getEnd().printToString(SM) << "\n";
                // // std::cout <<"--test--"<<getCharInLoc(SR.getBegin())<<" "<<getCharInLoc(SR.getBegin().getLocWithOffset(-1))<<" "<<get_stmt_string(stmt)[0]<<"\n";
                _rewriter.ReplaceText(SR,str1);
                stmt_cut=false;
            }
            
            std::cout <<"slice finish -> "<<stmt_cut<<"\n";
            return stmt_cut;//是否删除
        }
    }
    void get_declare_func(clang::FunctionDecl *FD){
        if(!FD) return;
        if (FD->getBuiltinID() != clang::Builtin::NotBuiltin) {
            return;
        }
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation loc = get_decl_sourcerange(FD).getBegin();
        std::cout <<"get_declare_func"<<isInSystemHeader(loc)<<"\n";
        if(isInSystemHeader(loc)) return;
        clang::FunctionDecl * funcDef = FD->getDefinition();
        if(funcDef==nullptr) funcDef=FD;
        if(funcDef->isInlineBuiltinDeclaration()) return;
        if(inst_info.insert_funcdec.count(funcDef)!=0){
            return;
        }
        std::cout <<">>"<<funcDef->getNameAsString()<<inst_info.insert_funcdec.count(funcDef)<<"\n";
        std::string str = "\n"+get_decl_string(funcDef)+";\n";
        if(FD->hasBody()){
            auto FB=FD->getBody();
            if(!FB ||isInSystemHeader(get_sourcerange(FB).getEnd())) return;
            std::string f=get_decl_string(FD);
            str=f.substr(0,f.find_first_of('{'));
            str+=";\n";
        }
        std::cout <<">>"<<str<<"\n";
        clang::QualType retType = FD->getReturnType();
        // if (const clang::RecordType* recordType = get_normal_type_decl(retType)->getAs<clang::RecordType>()) {
        //     const clang::RecordDecl* recordDecl = recordType->getDecl();
        //     get_record_string(recordDecl);
        // }
        process_QualType(retType);

        for(auto param : FD->parameters()){
            clang::QualType pType=param->getType();
            std::cout <<"Parma:"<<pType.getAsString()<<"\t";
            // if (const clang::RecordType* recordType = get_normal_type_decl(pType)->getAs<clang::RecordType>()) {
            //     const clang::RecordDecl* recordDecl = recordType->getDecl();
            //     get_record_string(recordDecl);
            // }
            process_QualType(pType);
        }
        if(inst_info.insert_funcdec.count(funcDef)==0){
            std::cout <<"Insert Function Declare "<<str<<'\n';
            std::cout <<"[] "<<get_decl_string(FD)<<'\n';
            // _rewriter.InsertText(it->second,str);
            writeAnyInFunc(str);
            inst_info.insert_funcdec.insert(funcDef);
        }
    }
    
    void get_declare(clang::CallExpr *callExpr){
        auto FD = callExpr->getDirectCallee();
        get_declare_func(FD);
    }
    
    // 普通遇到：处理返回类型和参数类型
    void process_functionProtoType(clang::QualType FT){
        if (const auto *FunctionType = FT->getAs<clang::FunctionProtoType>()){
            clang::QualType ReturnType = FunctionType->getReturnType();
            clang::QualType rType = get_normal_type_decl(ReturnType);//去掉const、指针等修饰符
            int isPointer = ReturnType.getTypePtr()->isPointerType();
            if(const clang::TypedefType *TT = get_normal_type_decl(ReturnType)->getAs<clang::TypedefType>()) {
                clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                get_typedef(Typedef, isPointer);
            }
            else if(const clang::RecordType* recordType = rType->getAs<clang::RecordType>()) {
                const clang::RecordDecl* recordDecl = recordType->getDecl();
                if(isPointer){
                    if(recordDecl->isUnion()){
                        insertRecordDeclare(recordDecl,"union");
                    }else{
                        insertRecordDeclare(recordDecl,"struct");
                    }
                }else{
                    process_QualType(rType);
                    get_record_string(recordDecl);
                }
            }else{
                process_QualType(rType);
            }

            //参数只需要类型
            for (const auto &Param : FunctionType->param_types()) {
                clang::QualType pType = get_normal_type_decl(Param);
                isPointer = Param.getTypePtr()->isPointerType();
                if(const clang::TypedefType *TT = get_normal_type_decl(Param)->getAs<clang::TypedefType>()) {
                    clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                    get_typedef(Typedef, isPointer);
                }
                else if(const clang::RecordType* recordType = pType->getAs<clang::RecordType>()) {
                    const clang::RecordDecl* recordDecl = recordType->getDecl();
                    if(isPointer){
                        if(recordDecl->isUnion()){
                            insertRecordDeclare(recordDecl,"union");
                        }else{
                            insertRecordDeclare(recordDecl,"struct");
                        }
                    }else{
                        process_QualType(pType);
                        get_record_string(recordDecl);
                    }
                }else{
                    process_QualType(pType);
                }
            }
        } else if (const auto *FunctionType2 = FT->getAs<clang::FunctionNoProtoType>()){
            clang::QualType ReturnType = FunctionType2->getReturnType();
            clang::QualType rType = get_normal_type_decl(ReturnType);//去掉const、指针等修饰符 不被typedef
            int isPointer = ReturnType.getTypePtr()->isPointerType();
            if(const clang::TypedefType *TT = get_normal_type_decl(ReturnType)->getAs<clang::TypedefType>()) {
                clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                get_typedef(Typedef, isPointer);
            }
            else if(const clang::RecordType* recordType = rType->getAs<clang::RecordType>()) {
                const clang::RecordDecl* recordDecl = recordType->getDecl();
                if(isPointer){
                    if(recordDecl->isUnion()){
                        insertRecordDeclare(recordDecl,"union");
                    }else{
                        insertRecordDeclare(recordDecl,"struct");
                    }
                }else{
                    process_QualType(rType);
                    get_record_string(recordDecl);
                }
            }else{
                process_QualType(rType);
            }
        }
    }

    // 处理函数类型（包括有参数、无参数）
    void process_functionType(clang::QualType FT, std::unordered_set<const clang::RecordDecl *> &after_insert,
                                bool pointerUsed, bool isRefTD){
        if (const auto *FunctionType = FT->getAs<clang::FunctionProtoType>()){
            clang::QualType ReturnType = FunctionType->getReturnType();
            clang::QualType rType = get_normal_type_decl(ReturnType);//去掉const、指针等修饰符
            int isPointer = ReturnType.getTypePtr()->isPointerType();
            if(const clang::TypedefType *TT = get_normal_type_decl(ReturnType)->getAs<clang::TypedefType>()) {
                clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                get_typedef(Typedef,isPointer||pointerUsed);
            }
            else if(const clang::RecordType* recordType = rType->getAs<clang::RecordType>()) {
                const clang::RecordDecl* recordDecl = recordType->getDecl();
                if(isPointer){
                    if(recordDecl->isUnion()){
                        insertRecordDeclare(recordDecl,"union");
                    }else{
                        insertRecordDeclare(recordDecl,"struct");
                    }
                    if(isRefTD)
                        get_record_string(recordDecl);
                }else{
                    get_record_string(recordDecl);
                }
            }else{
                process_QualType(rType);
            }

            //参数只需要类型
            for (const auto &Param : FunctionType->param_types()) {
                clang::QualType pType = get_normal_type_decl(Param);
                isPointer = Param.getTypePtr()->isPointerType();
                if(const clang::TypedefType *TT = pType->getAs<clang::TypedefType>()) {
                    clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                    get_typedef(Typedef,isPointer||pointerUsed);
                }
                else if(const clang::RecordType* recordType = pType->getAs<clang::RecordType>()) {
                    const clang::RecordDecl* recordDecl = recordType->getDecl();
                    if(isPointer){
                        if(recordDecl->isUnion()){
                            insertRecordDeclare(recordDecl,"union");
                        }else{
                            insertRecordDeclare(recordDecl,"struct");
                        }
                        if(isRefTD)
                            get_record_string(recordDecl);
                    }else{
                        get_record_string(recordDecl);
                    }
                }else{
                    process_QualType(pType);
                }
            }
        } else if (const auto *FunctionType2 = FT->getAs<clang::FunctionNoProtoType>()){
            clang::QualType ReturnType = FunctionType2->getReturnType();
            clang::QualType rType = get_normal_type_decl(ReturnType);//去掉const、指针等修饰符 不被typedef
            int isPointer = ReturnType.getTypePtr()->isPointerType();
            if(const clang::TypedefType *TT = rType->getAs<clang::TypedefType>()) {
                clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                get_typedef(Typedef,isPointer||pointerUsed);
            }
            else if(const clang::RecordType* recordType = rType->getAs<clang::RecordType>()) {
                const clang::RecordDecl* recordDecl = recordType->getDecl();
                if(isPointer){
                    if(recordDecl->isUnion()){
                        insertRecordDeclare(recordDecl,"union");
                    }else{
                        insertRecordDeclare(recordDecl,"struct");
                    }
                    if(isRefTD)
                        get_record_string(recordDecl);
                }else{
                    get_record_string(recordDecl);
                }
            }else{
                process_QualType(rType);
            }
        }
    }
    // 处理函数指针遇到的类型
    void process_functionProtoType_typedef(clang::QualType pointeeType, clang::TypedefDecl *TD,
                                  std::unordered_set<const clang::RecordDecl *> &after_insert,
                                  std::string &pointer, bool &flag_fp, bool pointerUsed, bool isRefTD){
        if(!pointeeType.getTypePtr()) return;
        std::cout <<"functionProtoType_typedef->0";
        // 带参函数指针
        if (const auto *FunctionType = pointeeType->getAs<clang::FunctionProtoType>()){
            flag_fp=1;
            clang::QualType ReturnType = FunctionType->getReturnType();
            clang::QualType rType = get_normal_type_decl(ReturnType);//去掉const、指针等修饰符
            int isPointer = ReturnType.getTypePtr()->isPointerType();
            if(isPointer) pointer="*";
            if(const clang::TypedefType *TT = rType->getAs<clang::TypedefType>()) {
                clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                get_typedef(Typedef, isPointer||pointerUsed);
            }
            else if(const clang::RecordType* recordType = rType->getAs<clang::RecordType>()) {
                const clang::RecordDecl* recordDecl = recordType->getDecl();
                if(isPointer){
                    if(recordDecl->isUnion()){
                        insertRecordDeclare(recordDecl,"union");
                    }else{
                        insertRecordDeclare(recordDecl,"struct");
                    }
                    if(isRefTD)
                        get_record_string(recordDecl);
                }else{
                    // process_QualType(rType);
                    get_record_string(recordDecl);
                }
            }else{
                process_QualType(rType);
            }

            bool notFirstParam=false;// 除了第一个参数都要加逗号分隔
            //参数只需要类型
            for (const auto &Param : FunctionType->param_types()) {
                clang::QualType pType = get_normal_type_decl(Param);
                isPointer = Param.getTypePtr()->isPointerType();
                pointer = isPointer ? "*" : "";
                if(const clang::TypedefType *TT = pType->getAs<clang::TypedefType>()) {
                    clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                    get_typedef(Typedef, isPointer||pointerUsed);
                }
                else if(const clang::RecordType* recordType = pType->getAs<clang::RecordType>()) {
                    const clang::RecordDecl* recordDecl = recordType->getDecl();
                    if(isPointer){
                        if(recordDecl->isUnion()){
                            insertRecordDeclare(recordDecl,"union");
                        }else{
                            insertRecordDeclare(recordDecl,"struct");
                        }
                        if(isRefTD)
                            get_record_string(recordDecl);
                    }else{
                        // process_QualType(pType);
                        get_record_string(recordDecl);
                    }
                }else{
                    process_QualType(pType);
                }
            }
        } else if (const auto *FunctionType2 = pointeeType->getAs<clang::FunctionNoProtoType>()){
            flag_fp=1;
            clang::QualType ReturnType = FunctionType2->getReturnType();
            clang::QualType rType = get_normal_type_decl(ReturnType);//去掉const、指针等修饰符 不被typedef
            int isPointer = ReturnType.getTypePtr()->isPointerType();
            if(isPointer) pointer="*";

            if(const clang::TypedefType *TT = rType->getAs<clang::TypedefType>()) {
                clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                get_typedef(Typedef, isPointer||pointerUsed);
            }
            else if(const clang::RecordType* recordType = rType->getAs<clang::RecordType>()) {
                const clang::RecordDecl* recordDecl = recordType->getDecl();
                if(isPointer){
                    if(recordDecl->isUnion()){
                        insertRecordDeclare(recordDecl,"union");
                    }else{
                        insertRecordDeclare(recordDecl,"struct");
                    }
                    if(isRefTD)
                        get_record_string(recordDecl);
                }else{
                    // process_QualType(rType);
                    get_record_string(recordDecl);
                }
            }else{
                process_QualType(rType);
            }
        }
    }

    //pointerUsed 作为指针使用，可以没有结构体完整定义
    void get_typedef(clang::TypedefDecl *TD, bool pointerUsed = false, bool isRefTD = false) {
        if(!TD) return;
        // if(inst_info.insert_typedef.count(TD->getNameAsString())!=0) return;
        if(!isRefTD && pointerUsed && inst_info.processed_typedef.count(TD->getNameAsString())!=0) return;
        if(isInSystemHeader(get_decl_sourcerange(TD).getBegin())) return;
        inst_info.processed_typedef.insert(TD->getNameAsString());

        clang::SourceManager &SM = _ctx->getSourceManager();
        std::cout <<"typedef "<<TD->getNameAsString()<<" in file["<<SM.getFilename(get_decl_sourcerange(TD).getBegin()).str()<<"]"<<inst_info.insert_typedef.count(TD->getNameAsString())<<"\n";
        std::cout <<"attrs:"<<pointerUsed<<" "<<isRefTD<<"\n";
        bool flag_fp=0;
        bool isPointerUnder = false;
        std::string pointer="";
        std::unordered_set<const clang::RecordDecl *> after_insert;
        clang::QualType underType = TD->getUnderlyingType();
        
        if (const auto *pointerType = underType->getAs<clang::PointerType>()) {//指针类型，包括普通指针和函数指针
            clang::QualType pointee = pointerType->getPointeeType();
            std::cout <<"pointer>>";
            isPointerUnder = true;
            process_functionProtoType_typedef(pointee, TD, after_insert, pointer, flag_fp, pointerUsed, isRefTD);//是否函数指针， 函数内会处理函数指针类型。更新after_insert, pointer, flag_fp
        }
        if(!flag_fp){
            std::string str = "\n"+get_decl_string(TD)+";\n";
            clang::QualType nT=underType;
            clang::RecordDecl *record_a=nullptr;// = underType->getAs<clang::RecordType>()->getDecl();
            pointer="";
            if(const clang::TypedefType *TT = underType->getAs<clang::TypedefType>()) {
                clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                get_typedef(Typedef, isPointerUnder||pointerUsed, isRefTD);
            }
            if(const clang::PointerType* pointerType = underType->getAs<clang::PointerType>()) {
                std::cout <<underType.getAsString()<<" dereference to ";
                underType = pointerType->getPointeeType();
                std::cout <<underType.getAsString()<<" \n";
                isPointerUnder = true;
                if(const clang::RecordType *recordType_t = underType->getAs<clang::RecordType>()){
                    clang::RecordDecl *recordDecl = recordType_t->getDecl();
                    if(recordDecl->isUnion()){
                        str = "\ntypedef union "+recordDecl->getNameAsString()+" "+TD->getNameAsString()+";\n";
                        insertRecordDeclare(recordDecl,"union");
                    }else{
                        str = "\ntypedef struct "+recordDecl->getNameAsString()+" "+TD->getNameAsString()+";\n";
                        insertRecordDeclare(recordDecl,"struct");
                    }
                    if(isRefTD)
                        get_record_string(recordDecl);
                }else{
                    process_QualType(underType);
                }
                pointer="*";
            }
            if(const clang::TypedefType *TT = underType->getAs<clang::TypedefType>()) {
                clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
                get_typedef(Typedef, isPointerUnder||pointerUsed, isRefTD);
            }
            else if(const clang::ElaboratedType *ET = underType->getAs<clang::ElaboratedType>()) {
                underType = ET->getNamedType();
            }
            if(const clang::RecordType *recordType_t = nT->getAs<clang::RecordType>()){
                std::cout <<"[in get_typedef] recordType found\n";
                clang::RecordDecl *recordDecl = recordType_t->getDecl();
                std::string record_name = recordDecl->getNameAsString();
                if(recordDecl->getNameAsString()==""){ //匿名结构体
                    std::cout <<"niming struct\n";
                    std::string s = get_record_string(recordDecl);
                    if(s.size()>2 && inst_info.insert_typedef.count(TD->getNameAsString())==0){
                        str = "\n/*Ano*/typedef "+get_decl_string(recordDecl)+" "+TD->getNameAsString()+";\n";
                        std::cout <<"write non name typedef:"<<str;

                        for(auto &rd : after_insert){
                            std::cout <<"after_insert record in non name typedef "<<rd->getNameAsString()<<"\n";
                            get_record_string(rd);
                        }

                        writeAnyInType(str);
                        inst_info.insert_typedef.insert(TD->getNameAsString());
                        inst_info.out_typedef.insert(TD);
                    }
                }else{
                    record_a = recordDecl;
                    // 含有左括号（完整定义） 或 没有typedef字样（宏）
                    // if(_rewriter.getRewrittenText(get_decl_sourcerange(TD)).find("{")!=std::string::npos ||
                    //     _rewriter.getRewrittenText(get_decl_sourcerange(TD)).find("typedef")==std::string::npos){
                    //     get_record_string(recordDecl);
                    // }
                    if(recordDecl->isUnion()){
                        str = "\ntypedef union "+recordDecl->getNameAsString()+" "+TD->getNameAsString()+";\n";
                        insertRecordDeclare(recordDecl,"union");
                    }else{
                        str = "\ntypedef struct "+recordDecl->getNameAsString()+" "+TD->getNameAsString()+";\n";
                        insertRecordDeclare(recordDecl,"struct");
                    }
                    if(inst_info.insert_typedef.count(TD->getNameAsString())==0){
                        std::cout <<"insert typedef recordDecl:"<<TD->getNameAsString()<<"\n";
                        inst_info.insert_typedef.insert(TD->getNameAsString());
                        writeAnyInType("/*TD*/"+get_decl_string(TD)+";\n");
                    }
                    // 只有完整定义才插入
                    std::cout <<recordDecl->getNameAsString()<<" which "<<!pointerUsed<<" "<<isRefTD<<" "<<((!isPointerUnder) && pointerUsed)<<"\n";
                    if(!pointerUsed || isRefTD){
                        std::cout <<"after_insert.insert "<<recordDecl->getNameAsString()<<"\n";
                        after_insert.insert(recordDecl);
                    }
                }
            }else if(const clang::EnumType *enumType = nT->getAs<clang::EnumType>()){
                const clang::EnumDecl *enumDecl = enumType->getDecl();
                if(enumDecl->getNameAsString()=="" && inst_info.insert_typedef.count(TD->getNameAsString())==0){
                    inst_info.out_enum.insert(enumDecl);
                    inst_info.insert_typedef.insert(TD->getNameAsString());
                    str = "\ntypedef "+get_decl_string(enumDecl)+" "+TD->getNameAsString()+";\n";
                    writeAnyInType(str);
                }else{
                    str = "\ntypedef enum "+enumDecl->getNameAsString()+" "+TD->getNameAsString()+";\n";
                    get_enum(enumDecl);
                }
            }else {
                process_functionType(underType, after_insert, pointerUsed, isRefTD);
            }
        }
        if(inst_info.out_typedef.count(TD)==0 && !after_insert.empty()){
            for(auto &rd : after_insert){
                std::cout <<"after_insert record in typedef "<<rd->getNameAsString()<<"\n";
                get_record_string(rd);
            }
            inst_info.out_typedef.insert(TD);
        }

        if(inst_info.insert_typedef.count(TD->getNameAsString())==0){
            inst_info.insert_typedef.insert(TD->getNameAsString());
            std::cout <<"insert typedef:"<<TD->getNameAsString()<<"\n";
            writeAnyInType("/*TD*/"+get_decl_string(TD)+";\n");
        }
        std::cout <<"--end of get_typedef :"<<TD->getNameAsString()<<"--\n";
    }

    void remove_with_left(clang::Stmt *stmt){
        // // std::cout <<"remove_with_left "<<get_stmt_string(stmt)<<"\t";
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceRange SR =get_sourcerange(stmt);
        clang::SourceLocation startLoc=SR.getBegin(), endLoc=SR.getEnd();
        int col_v = SM.getPresumedColumnNumber(startLoc);
        int line=SM.getPresumedLineNumber(startLoc);
        int i;
        bool flag_d=0;
        for(i=1;i<col_v;i++){
            startLoc = SM.translateLineCol(SM.getMainFileID(), line, col_v - i);
            const char *p=SM.getCharacterData(startLoc);//提取从startoc开始的代码
            if(p[0]=='+'||p[0]=='-'||p[0]=='*'||p[0]=='/'||p[0]=='%'||p[0]=='('){
                break;
            }
            if(p[0]=='='||p[0]=='|'||p[0]=='&'||p[0]=='<'||p[0]=='>'){
                if(flag_d==0){
                    flag_d=1;continue;
                }
                else{
                    break;
                }
            }

            if(flag_d==1) {
                startLoc = SM.translateLineCol(SM.getMainFileID(), line, col_v - i + 1);
                break;
            }
        }
        if(i==col_v&&flag_d==0){
            int line_cut=line;
            bool flag=false;
            for(int d=1;d<line&&!flag;d++)
            {
                line_cut=line-d;
                startLoc = SM.translateLineCol(SM.getMainFileID(), line_cut, 1);
                const char *p=SM.getCharacterData(startLoc);//提取从startoc开始的代码
                int l=1;//这一行的长度
                for(l=1;p[l]!='\n';l++);
                for(int i=l-1;i>0;i--){
                    // // std::cout <<"("<<p[i]<<")";
                    if(p[i]=='+'||p[i]=='-'||p[i]=='*'||p[i]=='/'||p[i]=='%'||p[i]=='('){
                        startLoc = SM.translateLineCol(SM.getMainFileID(), line_cut, i+1);
                        flag=true;
                        break;
                    }
                    if(p[i]=='='||p[i]=='|'||p[i]=='&'||p[i]=='<'||p[i]=='>'){
                        if(flag_d==0){
                            flag_d=1;continue;
                        }
                        else{
                            startLoc = SM.translateLineCol(SM.getMainFileID(), line_cut, i+1);
                            flag=true;
                            break;
                        }
                    }
                    if(flag_d==1) {
                        startLoc = SM.translateLineCol(SM.getMainFileID(), line_cut, i+2);
                        flag=true;
                        break;
                    }
                }
            }
        }
        // // std::cout <<"Cut start at:"<<SM.getPresumedLineNumber(startLoc)<<" ,"<<SM.getPresumedColumnNumber(startLoc)<<"\n";
        _rewriter.RemoveText(clang::SourceRange(startLoc,endLoc));
    }
    
    //仅考虑匿名联合体，未考虑结构体内定义的匿名结构体 + 匿名结构体(考虑全局变量定义)
    bool isAnonymousRecord(clang::ValueDecl *VD) {
        if (!VD)
            return false;

        clang::QualType Type = VD->getType();
        if(const clang::TypedefType *TT = Type->getAs<clang::TypedefType>()){
            return false;
        }

        if (const clang::RecordType *RT = Type->getAs<clang::RecordType>()) {
            if (const clang::RecordDecl *RD = RT->getDecl()) {
                return RD->getDeclName().isEmpty();
            }
        }

        return false;
    }
    
    bool VisitFunctionDecl(clang::FunctionDecl *func_decl)
    {
        clang::SourceLocation loc = func_decl->getLocation();
        if (loc.isInvalid() || isInSystemHeader(loc))
            return true;
        
        // clang::SourceRange SR=get_decl_sourcerange(func_decl);
        // clang::SourceLocation loc = SR.getBegin().getLocWithOffset(-1);//函数声明的前一行
        clang::SourceManager &SM = _ctx->getSourceManager();
        std::string func_name = func_decl->getNameAsString();
        std::string func_path = get_loc_file(loc);
        std::string filePath_abs = normalize_path(make_absolute_path(func_path)); //标准化路径


        if (!_ctx->getSourceManager().isInMainFile(loc)){ // 头文件中的函数
            // std::cout <<"Header File Function "<<func_name<<" in file: "<<func_path<<" normalize"<<filePath_abs<<"\n";
            if(inst_info.define_files.find(filePath_abs)==inst_info.define_files.end()){
                return true;
            }
            std::cout <<"not in main file's function <"<<func_name<<"> |"<<filePath_abs<<"\n";
        }
        if (func_decl == func_decl->getDefinition() && flag==true){//对剩余的函数声明二次处理
            std::cout <<"Second Time to Function "<<func_name<<" \t";
            clang::SourceRange SR=get_decl_sourcerange(func_decl);
            if (pre_msg.nd_funcs.find(func_name)
                != pre_msg.nd_funcs.end()){//存储使用到的函数名
                get_declare_func(func_decl);
            /*
                std::string str = "";
                auto FD=func_decl;
                if(FD->hasBody()){
                    auto FB=FD->getBody();
                    if(FB &&SM.isInMainFile(get_sourcerange(FB).getBegin())){
                        const char *p=SM.getCharacterData(SR.getBegin());
                        std::string str2(p);
                        int length = str2.find_first_of('{');
                        std::string f=get_decl_string(FD);
                        str=f.substr(0,f.find_first_of('{'));
                        _rewriter.ReplaceText(SR.getBegin(),length,str);
                    }else return true;
                }else{
                    str+=get_decl_string(FD)+";\n";
                    std::string file_name = SM.getFilename(SR.getBegin()).str().substr(SM.getFilename(SR.getBegin()).str().find_last_of("/")+1);
                    
                    str="\n"+str;
                    _rewriter.ReplaceText(SR,str);
                } */
                // // std::cout <<"!!!!"<<SM.getPresumedLineNumber(SR.getBegin())<<" ~ "<<SM.getPresumedLineNumber((SR.getEnd()))<<'\n';
               
            }else{
                // _rewriter.RemoveText(SR);
                std::cout <<"Slice off FunctionDecl\n";
                // llvm::outs()<< "Need to Cut off, FuncitonDecl's  SourceRange: " << SR.getBegin().printToString(SM) << " - " << SR.getEnd().printToString(SM) << "\n";
            }
            return true;
        } 

        // pre_msg.insert_funcdec.insert(func_decl);
        std::string filePath = filePath_abs;
        if(filePath.find("src/")!=std::string::npos){
            int pos = filePath.find("src/");
            size_t idx = filePath.find_last_of("/", pos);
        
            if (idx != std::string::npos) {
                // 提取从最后一个 '/' 到字符串末尾的子字符串
                filePath = filePath.substr(idx + 1);
            }
        }
        std::cout <<"\n***in VisitFunctionDecl | Function " << func_name << " is defined in file: " << filePath << "\n";
        
        int param1=0;
        int line_start=0, line_end=0,col_end=0;
        if (inst_info.define_funcs.find(func_name)
            != inst_info.define_funcs.end())
        {
            clang::QualType retType = func_decl->getReturnType();
            process_QualType(retType);
            const auto& params = func_decl->parameters();
            for (auto param : params) {
                process_Decl(param);
            }
            // pre_msg.nd_funcs.insert(func_name);
            bool cutall=true;//是否全部被删除
            std::cout <<"[astSlicer] in function\n";
            if (func_decl == func_decl->getDefinition())
            {
                std::cout <<"START-\n";
                clang::Stmt *body_stmt = func_decl->getBody();
                clang::SourceRange SRF=body_stmt->getSourceRange();
                int func_end = SM.getPresumedLineNumber(SRF.getEnd());
                //处理stmt
                bool first_stmt=true;
                clang::SourceLocation last_stmt,pre_stmt;
                for (auto stmt : body_stmt->children())
                {
                    if(instSlice(SM,stmt,func_name,filePath, param1,func_end,cutall,0)) last_stmt = pre_stmt;

                }
                //全部被删
                if(cutall){
                    clang::SourceRange SR_all=get_decl_sourcerange(func_decl);
                    line_start = SM.getPresumedLineNumber(SR_all.getBegin());
                    line_end   = SM.getPresumedLineNumber(SR_all.getEnd());
                    col_end   = SM.getPresumedColumnNumber(SR_all.getEnd());
                
                    clang::SourceLocation targetLoc1 = SM.translateLineCol(SM.getMainFileID(), line_start, 1); 
                    clang::SourceLocation targetLoc2 = SM.translateLineCol(SM.getMainFileID(), line_end, col_end+1); 
                    _rewriter.RemoveText(clang::SourceRange(targetLoc1,targetLoc2));
                }
            }else{
                // std::cout <<"Function Declaration, exit.\n";
            }
        }
        else{
           std::cout <<"slice whole Function\n";
            //删除整个函数
            if (func_decl == func_decl->getDefinition())
            {
                clang::SourceRange SRB = func_decl->getBody()->getSourceRange();
                _rewriter.ReplaceText(SRB,"; //Slice  Function");
            }
        }
        return true;
    }
    void findGlobalInit(const clang::Stmt *S) {
        for(auto child : S->children()){
            if(!child) continue;
            if(const clang::CStyleCastExpr *csExpr=llvm::dyn_cast<clang::CStyleCastExpr>(child)){
                child = csExpr->getSubExpr();
            }
            if(const clang::ImplicitCastExpr *imExpr=llvm::dyn_cast<clang::ImplicitCastExpr>(child)){
                clang::CastKind castKind = imExpr->getCastKind();
                if(const clang::DeclRefExpr *declRefExpr= llvm::dyn_cast<clang::DeclRefExpr>(imExpr->getSubExpr())){
                    clang::QualType QT =declRefExpr->getDecl()->getType();
                    if (castKind == clang::CK_FunctionToPointerDecay) {//使用的函数 如果额外套了一层函数指针,就是LValueToRValue
                        pre_msg.nd_funcs.insert(declRefExpr->getDecl()->getNameAsString());
                        if(const clang::FunctionDecl *FD = llvm::dyn_cast<clang::FunctionDecl>(declRefExpr->getDecl())){
                            get_declare_func(const_cast<clang::FunctionDecl*>(FD));
                        }
                    }else if(const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(declRefExpr->getDecl())){
                        bool isGlobal = (VD->getParentFunctionOrMethod() == nullptr && !VD->isLocalVarDeclOrParm());
                        bool notProcessed = (inst_info.insert_gvar.count(VD) == 0);
                        // 如果是 static 变量，必须有初始值
                        bool isStatic = (VD->getStorageClass() == clang::SC_Static);
                        bool staticHasInit = (isStatic ? VD->hasInit() : true); // 非 static 则跳过该检查
                        if(isGlobal && notProcessed){
                            if(VD->getNameAsString() != get_stmt_string(S)) {
                                //只考虑定义
                                std::string str = get_decl_string(VD);
                                inst_info.insert_gvar.insert(VD);
                                if (isStatic && staticHasInit){
                                    findGlobalInit(VD->getInit());
                                    findFunctionUsedInStmt(VD->getInit());
                                }
                                else if(str.find("=")!=std::string::npos){
                                    size_t  pos = str.find("=");
                                    str = str.substr(0,pos);
                                }
                                std::cout <<"OUT TO HEADER"<<VD->getNameAsString()<<"\n";
                                if(str.substr(0,7)!="extern " && str.substr(0,7)!="static ") str = "extern "+str;
                                writeAnyInFunc(str+";");
                                process_QualType(VD->getType());
                            }
                            process_global_VarDecl(const_cast<clang::VarDecl*>(VD));
                        }
                    }
                }
                else{
                    findGlobalInit(imExpr->getSubExpr());
                }
            }
            else if (const clang::DeclRefExpr *DRE = llvm::dyn_cast<clang::DeclRefExpr>(child)) {
                if(const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl())){
                    bool isGlobal = (VD->getParentFunctionOrMethod() == nullptr && !VD->isLocalVarDeclOrParm());
                    bool notProcessed = (inst_info.insert_gvar.count(VD) == 0);
                    // 如果是 static 变量，必须有初始值
                    bool isStatic = (VD->getStorageClass() == clang::SC_Static);
                    if(isGlobal && notProcessed){
                        if(VD->getNameAsString() != get_stmt_string(S)) {
                            //只考虑定义
                            std::string str = get_decl_string(VD);
                            inst_info.insert_gvar.insert(VD);
                            if (isStatic){
                                findGlobalInit(VD->getInit());
                                findFunctionUsedInStmt(VD->getInit());
                            }
                            else if(str.find("=")!=std::string::npos){
                                size_t  pos = str.find("=");
                                str = str.substr(0,pos);
                            }
                            std::cout <<"OUT TO HEADER"<<VD->getNameAsString()<<"\n";
                            if(str.substr(0,7)!="extern " && str.substr(0,7)!="static ") str = "extern "+str;
                            writeAnyInFunc(str+";");
                            process_QualType(VD->getType());
                        }
                        process_global_VarDecl(const_cast<clang::VarDecl*>(VD));
                    }
                }
            }else{
                findGlobalInit(child);
            }
        
        }
    }
    
    void insert_ano_enum(){
        for(auto &ae : inst_info.ano_enum){
            std::cout <<"after_insert ano enum\n";
            get_enum(ae,true);
        }
    }
    void setFlag(){
        flag=true;
        insert_ano_enum();
    }


  private:
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    InstInfo &inst_info;
    PreMessage pre_msg;
    bool flag=false;
};

class InstASTConsumer : public clang::ASTConsumer
{
  public:
    explicit InstASTConsumer(clang::CompilerInstance &CI,/*clang::ASTContext *ctx,*/
                             clang::Rewriter &R,
                             InstInfo &inst_i)
        :  _visitor(&CI.getASTContext(), R, inst_i)
    {
        
    }

    virtual void HandleTranslationUnit(clang::ASTContext &ctx)
    {
        std::cout <<"-----------start----------------\n\n";
        _visitor.TraverseDecl(ctx.getTranslationUnitDecl());
        _visitor.setFlag();
        std::cout <<"\n-----------second----------------\n\n";
        _visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }

  private:
    InstASTVisitor _visitor;
};

class InstFrontendAction : public clang::ASTFrontendAction
{
  public:
    InstFrontendAction(InstInfo &inst_i) : inst_info{ inst_i }
    {
    }
    virtual std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance &compiler,
                          llvm::StringRef in_file)
    {
        _rewriter.setSourceMgr(compiler.getSourceManager(),
                               compiler.getLangOpts());

        return std::make_unique<InstASTConsumer>(
            compiler/*.getASTContext()*/, _rewriter, inst_info);
    }
  static void replace_suffix(std::string &fn, std::string with)
    {
    int index = fn.find_last_of(".");
    fn        = fn.substr(0, index) + with+fn.substr(index);
    }

    void EndSourceFileAction() override
{
    clang::SourceManager &SM = _rewriter.getSourceMgr();
    std::error_code ec;
    // 获取所有被修改的文件
    for (auto it = SM.fileinfo_begin(); it != SM.fileinfo_end(); ++it) {
#if LLVM_VERSION_MAJOR >= 18
        // LLVM 18+: 直接使用FileEntryRef
        clang::FileEntryRef fileEntryRef = it->getFirst();
        clang::FileID fileID = SM.translateFile(&fileEntryRef.getFileEntry());
#else
        // 旧版本处理
        clang::FileID fileID = it->first;
#endif

        // 检查该文件是否有修改
        if (_rewriter.getEditBuffer(fileID).begin() != _rewriter.getEditBuffer(fileID).end()) {
            
            // 生成输出文件名
            llvm::StringRef raw_path;

#if LLVM_VERSION_MAJOR >= 14
            // LLVM 14+：使用 FileEntryRef
            if (auto fileEntryRefOpt = SM.getFileEntryRefForID(fileID)) {
                raw_path = fileEntryRefOpt->getName();
            } else {
                // 对于没有FileEntry的文件（如宏展开等），跳过处理
                continue;
            }
#else
            // LLVM 14-：使用旧 API
            const clang::FileEntry* fileEntry = SM.getFileEntryForID(fileID);
            if (fileEntry) {
                raw_path = fileEntry->getName();
            } else {
                // 对于没有FileEntry的文件，跳过处理
                continue;
            }
#endif
            // 只处理源文件和头文件
            std::string filePath_abs, filename = raw_path.str(); 
            filePath_abs = normalize_path(make_absolute_path(raw_path.str())); //标准化路径
            if (filePath_abs.empty()|| inst_info.define_files.find(filePath_abs)==inst_info.define_files.end()){
                continue;
            }
            std::cout <<"output:"<<filePath_abs<<"\n";
            
            // 修改文件名（为所有处理的文件添加后缀）
            std::string output_filename = filename;
            replace_suffix(output_filename, "_InstSlice");
            // 创建输出目录（如果需要）
            std::string directory = llvm::sys::path::parent_path(output_filename).str();
            if (!directory.empty()) {
                llvm::sys::fs::create_directories(directory);
            }
            
            // 写入修改后的内容
            llvm::raw_fd_ostream fd(output_filename, ec, llvm::sys::fs::OF_None);
            if(ec) {
                llvm::errs() << "错误：无法创建输出文件 " << output_filename << ": " << ec.message() << "\n";
                continue;
            }
            
            _rewriter.getEditBuffer(fileID).write(fd);
            fd.close();
        }
    }
}


  private:
    clang::Rewriter _rewriter;
    InstInfo &inst_info;
};

class InstFactory : public clang::tooling::FrontendActionFactory
{
  public:
    InstFactory(InstInfo &inst_i) : inst_info{ inst_i } {}
    std::unique_ptr<clang::FrontendAction> create() override
    {
        return std::make_unique<InstFrontendAction>(inst_info);
    }

  private:
    InstInfo &inst_info;
};
}  // namespace astslicer
#endif