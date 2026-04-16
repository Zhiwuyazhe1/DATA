#ifndef INST_ACTION_H
#define INST_ACTION_H

#include "inst_data.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include <clang/AST/Decl.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <unistd.h>

#include "clang/AST/AST.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <vector>
#include <algorithm>
namespace delSpace
{

 // 递归收集所有 NullStmt 的源范围（包含周围空白）
class NullStmtCollector : public clang::StmtVisitor<NullStmtCollector> {
public:
    std::vector<clang::SourceRange> NullStmtRanges;
    clang::SourceManager &SM;
    const clang::LangOptions &LO;

    // 构造函数：传入源管理器和语言选项（用于扩展范围）
    NullStmtCollector(clang::SourceManager &sm, const clang::LangOptions &lo) : SM(sm), LO(lo) {}

    // 辅助函数：判断字符是否为空白（空格、制表符、换行、回车）
    bool is_whitespace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // 扩展范围：包含 NullStmt 前后的所有空白字符
    clang::SourceRange expand_to_whitespace(clang::SourceRange original_range,
                                            clang::SourceManager &sm,
                                            const clang::LangOptions &lo) {
        if (!original_range.isValid()) return original_range;

        // 获取范围的起始和结束位置
        clang::SourceLocation start = original_range.getBegin();
        clang::SourceLocation end = original_range.getEnd();
        const clang::FileID startFileID = sm.getFileID(start);
        const clang::FileID endFileID = sm.getFileID(end); // 避免重复获取

        // 向前扩展：包含起始位置前的所有空白（修复核心：先判空再访问）
        while (true) {
            clang::SourceLocation prev = start.getLocWithOffset(-1);
            // 1. 先判断 prev 是否在文件内（避免超出文件开头）
            if (!sm.isBeforeInTranslationUnit(prev, sm.getLocForStartOfFile(startFileID))) {
                break;
            }
            // 2. 确保 prev 和 start 在同一个文件（跨文件位置无效）
            if (sm.getFileID(prev) != startFileID) {
                break;
            }
            // 3. 获取字符范围的文本（可能返回空 StringRef）
            clang::CharSourceRange charRange = clang::CharSourceRange::getCharRange(prev, start);
            llvm::StringRef text = clang::Lexer::getSourceText(charRange, sm, lo);
            // 4. 关键修复：判断 text 非空后，再访问第一个字符
            if (text.empty() || !is_whitespace(text[0])) {
                break;
            }
            // 5. 只有是空白字符，才向前扩展
            start = prev;
        }

        // 向后扩展：包含结束位置后的所有空白（同样修复判空逻辑）
        while (true) {
            clang::SourceLocation next = end.getLocWithOffset(1);
            // 1. 先判断 next 是否在文件内（避免超出文件结尾）
            if (!sm.isBeforeInTranslationUnit(next, sm.getLocForEndOfFile(endFileID))) {
                break;
            }
            // 2. 确保 next 和 end 在同一个文件（跨文件位置无效）
            if (sm.getFileID(next) != endFileID) {
                break;
            }
            // 3. 获取字符范围的文本（可能返回空 StringRef）
            clang::CharSourceRange charRange = clang::CharSourceRange::getCharRange(end, next);
            llvm::StringRef text = clang::Lexer::getSourceText(charRange, sm, lo);
            // 4. 关键修复：判断 text 非空后，再访问第一个字符
            if (text.empty() || !is_whitespace(text[0])) {
                break;
            }
            // 5. 只有是空白字符，才向后扩展
            end = next;
        }

        return clang::SourceRange(start, end);
    }

    // 处理空语句：扩展范围到包含周围空白
    void VisitNullStmt(clang::NullStmt *NS) {
        clang::SourceRange original_range = NS->getSourceRange();
        if (original_range.isValid()) {
            // 扩展范围，包含前后空白
            clang::SourceRange expanded_range = expand_to_whitespace(original_range, SM, LO);
            NullStmtRanges.push_back(expanded_range);
        }
    }

    // 处理代码块：递归遍历子语句
    void VisitCompoundStmt(clang::CompoundStmt *CS) {
        for (clang::Stmt *Child : CS->body()) {
            if (Child) Visit(Child);
        }
    }

    // 处理 if/for/while 等语句（递归子语句）
    void VisitIfStmt(clang::IfStmt *IS) {
        if (IS->getThen()) Visit(IS->getThen());
        if (IS->getElse()) Visit(IS->getElse());
    }
    void VisitForStmt(clang::ForStmt *FS) {
        if (FS->getInit()) Visit(FS->getInit());
        if (FS->getCond()) Visit(FS->getCond());
        if (FS->getInc()) Visit(FS->getInc());
        if (FS->getBody()) Visit(FS->getBody());
    }
    void VisitWhileStmt(clang::WhileStmt *WS) {
        if (WS->getCond()) Visit(WS->getCond());
        if (WS->getBody()) Visit(WS->getBody());
    }
};
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
    }
    ~InstASTVisitor(){
    }
    

    static bool isIdentifierPart(char ch) {
        return std::isalpha(ch) || std::isdigit(ch) || (ch == '_');
    }
    static void replace_suffix(std::string &fn, std::string with)
    {
        int index = fn.find_last_of(".");
        fn        = fn.substr(0, index) + with+fn.substr(index);
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

    void writeAnyInFile(std::string output_file,std::string str){
        std::ofstream ofs3(output_file,std::ios::out|std::ios::app);
        if(!ofs3.is_open()){
            std::cout <<"Error: open file "<<output_file<<" failed!\n";
            exit(1);
        }
        ofs3 << str << "\n";
        ofs3.close();
    }

    /**
    * @brief 获取源代码范围
    * 通过给定的源代码范围，获取其实际的源代码范围。如果源代码范围在宏定义中，则通过展开宏定义来获取实际的源代码范围。
    * @param SR 源代码范围
    * @return 源代码范围
    */
    clang::SourceRange get_real_sourcerange(clang::SourceRange SR){
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
        return get_real_sourcerange(SR);
    }

    //获得stmt的range
    clang::SourceRange get_sourcerange(clang::Stmt *stmt){
        clang::SourceRange SR = stmt->getSourceRange();
        return get_real_sourcerange(SR);
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
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        S->print(strout, clang::PrintingPolicy(LO));
        std::string ret=strout.str();
        return ret;
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
    void printLoc(clang::SourceLocation loc){
        auto &SM = _ctx->getSourceManager();
        std::cout <<"printLoc:[ "<<SM.getFilename(loc).str()<<" :"<<SM.getPresumedLineNumber(loc)<<","<<SM.getPresumedColumnNumber(loc)<<"]\n";
    }
    std::string remove_null_stmts_and_empty_lines(clang::FunctionDecl *func_decl) {
        if (!func_decl || !func_decl->getBody()) {
            return "";
        }

        clang::ASTContext &ctx = func_decl->getASTContext();
        clang::SourceManager &sm = ctx.getSourceManager();
        const clang::LangOptions &lo = ctx.getLangOpts();
        clang::Rewriter rewriter(sm, lo);
        // 1. 收集包含空白的 NullStmt 范围
        NullStmtCollector collector(sm,lo);
        collector.Visit(func_decl->getBody());

        // 2. 按位置从后往前删除（避免范围偏移）
        std::sort(collector.NullStmtRanges.begin(), collector.NullStmtRanges.end(),
                [&sm](const clang::SourceRange &a, const clang::SourceRange &b) {
                    return sm.isBeforeInTranslationUnit(b.getBegin(), a.getBegin());
                });

        // 删除每个扩展后的范围（包含 NullStmt 和周围空白）
        for (const auto &range : collector.NullStmtRanges) {
            rewriter.ReplaceText(range, "");  // 替换为空白，彻底删除
        }

        std::string func_code;
        
        // 2.0 处理函数签名中的返回类型和参数类型（防止宏出现在签名中未被替换）
        // 用 QualType 打印返回类型
        // 主函数中仅保留这部分简化逻辑（替换原复杂的声明头部处理代码）
        if (func_decl->getReturnTypeSourceRange().isValid()) {
            clang::QualType qt = func_decl->getReturnType();
            clang::SourceRange retSR = get_real_sourcerange(func_decl->getReturnTypeSourceRange());
            std::string retTypeStr = get_qualType_string(qt);
            std::cout << "return type:" << retTypeStr << "\n";

            std::string expandedFuncName = func_decl->getNameAsString(); // 直接从 AST 拿函数名
            clang::DeclarationNameInfo nameInfo = func_decl->getNameInfo();
            clang::SourceLocation funcNameStartLoc = nameInfo.getLoc().isValid() ? nameInfo.getLoc() : func_decl->getLocation();
            clang::SourceLocation funcNameEndLoc = funcNameStartLoc.getLocWithOffset(expandedFuncName.size() - 1); // 函数名结束位置
            clang::SourceManager &SM_local = sm;
            // 关键修复：用 Lexer 扫描函数名的真实结束位置（避免手动计算长度）
            if (funcNameStartLoc.isValid()) {
                funcNameEndLoc = clang::Lexer::getLocForEndOfToken(funcNameStartLoc, 0, SM_local, ctx.getLangOpts());
                funcNameEndLoc = funcNameEndLoc.getLocWithOffset(-1);
            } else {
                funcNameEndLoc = func_decl->getLocation();
            }
            bool prefix_preserved = false;

            if (retSR.isValid() && func_decl) {
                // 1. 定位范围：函数声明开始 → 函数名结束（整个头部+函数名）
                clang::SourceLocation funcStartLoc = get_decl_sourcerange(func_decl).getBegin();
                if (!funcStartLoc.isValid() || !funcNameEndLoc.isValid()) {
                    goto fallback_replace; // 位置无效则降级
                }

                // 完整替换范围：函数开始 到 函数名结束
                clang::SourceRange fullDeclRange(funcStartLoc, funcNameEndLoc);
                std::string fullDeclRangetext = rewriter.getRewrittenText(fullDeclRange);
        
                // 2. 拼接新的声明头部（简化：存储说明符 + 标准属性 + 新返回类型 + 函数名）
                std::vector<std::string> keepSpecifiers;
                std::string attrText;

                // 快速提取存储说明符（从 AST 直接获取，避免分词解析）
                if (func_decl->isStatic()) keepSpecifiers.push_back("static");
                if (func_decl->isInlined()) keepSpecifiers.push_back("inline");
                if (func_decl->isExternC()) keepSpecifiers.push_back("extern");

                // 快速提取属性（复用 AST，避免复杂处理）
                if (func_decl->hasAttrs()) {
                    clang::PrintingPolicy PP(ctx.getLangOpts());
                    for (const clang::Attr *A : func_decl->attrs()) {
                        std::string a_buf;
                        llvm::raw_string_ostream a_os(a_buf);
                        A->printPretty(a_os, PP);
                        a_os.flush();
                        if (!a_buf.empty()) {
                            attrText += (attrText.empty() ? "" : " ") + a_buf;
                        }
                    }
                }

                // 拼接最终字符串（顺序：存储说明符 → 属性 → 返回类型 → 函数名）
                std::string newFullDecl;
                // 存储说明符
                for (size_t i = 0; i < keepSpecifiers.size(); ++i) {
                    newFullDecl += (i == 0 ? "" : " ") + keepSpecifiers[i];
                }
                // 属性
                if (!attrText.empty()) {
                    newFullDecl += (newFullDecl.empty() ? "" : " ") + attrText;
                }
                // 返回类型
                std::string finalRetType = retTypeStr.empty() ? qt.getAsString() : retTypeStr;
                if (!finalRetType.empty()) {
                    newFullDecl += (newFullDecl.empty() ? "" : " ") + finalRetType;
                }
                // 函数名
                newFullDecl += (newFullDecl.empty() ? "" : " ") + expandedFuncName;

                // 3. 替换整个头部（函数开始到函数名结束）
                if (!newFullDecl.empty()) {
                    func_code += newFullDecl + " ";
                    rewriter.ReplaceText(fullDeclRange, newFullDecl);
                    prefix_preserved = true;
                    std::cout << "已替换函数声明头部：" << fullDeclRangetext <<" -> " <<newFullDecl<< "\n";
                }
            }

        fallback_replace:
            // 降级策略：替换失败时仅替换返回类型（兼容原逻辑，避免崩溃）
            if (!prefix_preserved) {
                if (!retTypeStr.empty()) {
                    rewriter.ReplaceText(retSR, retTypeStr);
                }
                // 2.1 函数头部：从 AST 提取可靠信息（替代全量替换）
                // 存储说明符（static/inline 等）
                std::vector<std::string> specifiers;
                if (func_decl->isStatic()) specifiers.push_back("static");
                if (func_decl->isInlined()) specifiers.push_back("inline");
                if (func_decl->isExternC()) specifiers.push_back("extern \"C\"");

                // 返回类型 + 函数名
                std::string ret_type = get_qualType_string(func_decl->getReturnType());
                std::string func_name = func_decl->getNameAsString();
                func_code += join(specifiers, " ") + " " + ret_type + " " + func_name;
            }

        }

        //--std::cout << "parameters:\n";
        // 2.1 参数列表：遍历 ParmVarDecl，替换其类型的源文本或回退到 AST 打印
        // int index = 0;
        // for (auto *P : func_decl->parameters()) {
        //     if (!P) continue;
        //     if (P->getTypeSourceInfo()) {
        //         clang::SourceRange pSR = get_decl_sourcerange(P);
        //         std::string printed = get_decl_string(P);
        //         if (!printed.empty()) rewriter.ReplaceText(pSR, printed);
        //     }
        // }    
        std::string params;
        for (size_t i = 0; i < func_decl->param_size(); ++i) {
            auto *param = func_decl->getParamDecl(i);
            params += (i > 0 ? ", " : "") + get_decl_string(param);
        }
        if (func_decl->isVariadic()) {
            if (!params.empty()) {  // 若已有固定参数，先加逗号分隔
                params += ", ";
            }
            params += "...";  // 补充可变参数标记
        }
        func_code += "(" + params + ")";
        //--std::cout << "visitAndReplace:\n";
        std::function<void(clang::Stmt*)> visitAndReplace;
        visitAndReplace = [&](clang::Stmt *S) {
            if (!S) return;

            // 1) DeclStmt 中 VarDecl 的类型
            if (auto *DS = llvm::dyn_cast<clang::DeclStmt>(S)) {
                clang::SourceRange sr = get_sourcerange(S);
                std::string src = get_stmt_string(S);
                if (!src.empty()) {
                    func_code += src + "\n";
                    rewriter.ReplaceText(sr, src);
                } 
            }

            // 2) DeclRefExpr
            else if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(S)) {
                clang::SourceRange sr = get_sourcerange(DRE);
                std::string src = get_stmt_string(DRE);
                if (!src.empty()) {
                    func_code += src + "\n"; 
                    rewriter.ReplaceText(sr, src);
                } 
            }

            // 3) MemberExpr：优先原始文本
            else if (auto *ME = llvm::dyn_cast<clang::MemberExpr>(S)) {
                clang::SourceLocation mLoc = ME->getMemberLoc();
                clang::SourceRange msr(mLoc, mLoc);
                std::string src = get_stmt_string(ME);
                if (!src.empty()){
                    func_code += src + "\n";
                    rewriter.ReplaceText(msr, src);
                } 
            }

            // 4) CallExpr 的 callee
            else if (auto *CE = llvm::dyn_cast<clang::CallExpr>(S)) {
                if (clang::Expr *Callee = CE->getCallee()) {
                    clang::SourceRange calleeSR = get_sourcerange(Callee);
                    std::string src = get_stmt_string(Callee);
                    if (!src.empty()) {
                        func_code += src + "\n";
                        rewriter.ReplaceText(calleeSR, src);
                    }
                }
            }

            // 4.1) ReturnStmt
            else if (auto *RS = llvm::dyn_cast<clang::ReturnStmt>(S)) {
                if (clang::Expr *RetE = RS->getRetValue()) {
                    clang::SourceRange retSR = get_sourcerange(RetE);
                    std::string src = get_stmt_string(RetE);
                    if (!src.empty()) {
                        func_code += src + "\n";
                        rewriter.ReplaceText(retSR, src);
                    }
                }
            }

            // 4.2) SwitchStmt
            else if (auto *SS = llvm::dyn_cast<clang::SwitchStmt>(S)) {
                if (clang::Expr *Cond = SS->getCond()) {
                    clang::SourceRange condSR = get_sourcerange(Cond);
                    std::string src = get_stmt_string(Cond);
                    if (!src.empty()) {
                        func_code += src + "\n";
                        rewriter.ReplaceText(condSR, src);
                    }
                }
                for (clang::Stmt *Child : S->children()) if (Child) visitAndReplace(Child);
            }

            // 4.3) CaseStmt: 替换 case 的标签表达式（支持 GNU case a ... b）
            else if (auto *CS = llvm::dyn_cast<clang::CaseStmt>(S)) {
                if (clang::Expr *L = CS->getLHS()) {
                    clang::SourceRange lhsSR = get_sourcerange(L);
                    std::string src = get_stmt_string(L);
                    if (!src.empty()) {
                        func_code += src + "\n";
                        rewriter.ReplaceText(lhsSR, src);
                    }
                }
                // 有些 CaseStmt 也包含 RHS（GNU 扩展的 case range）
                if (clang::Expr *R = CS->getRHS()) {
                    clang::SourceRange rhsSR = get_sourcerange(R);
                    std::string src = get_stmt_string(R);
                    if (!src.empty()) {
                        func_code += src + "\n";
                        rewriter.ReplaceText(rhsSR, src);
                    }
                }
                for (clang::Stmt *Child : S->children()) if (Child) visitAndReplace(Child);
            }

            // 5) CStyleCastExpr：先尝试源文本，否则打印类型
            else if (auto *CE = llvm::dyn_cast<clang::CStyleCastExpr>(S)) {
                clang::SourceRange sr = get_sourcerange(CE);
                std::string src = get_stmt_string(CE);
                if (!src.empty()) {
                    func_code += src + "\n";
                    rewriter.ReplaceText(sr, src);
                }
            }

            // 6) BinaryOperator
            else if (auto *BO = llvm::dyn_cast<clang::BinaryOperator>(S)) {
                clang::SourceRange sr = get_sourcerange(BO);
                std::string src = get_stmt_string(BO);
                if (!src.empty()){
                    func_code += src + "\n";
                    rewriter.ReplaceText(sr, src);
                } 
            }

            // 递归
            else{
                clang::SourceRange sr = get_sourcerange(S);
                std::string src = get_stmt_string(S);
                if (!src.empty()){
                    func_code += src + "\n";
                    rewriter.ReplaceText(sr, src);
                } 
            }
        };

        visitAndReplace(func_decl->getBody());
        
        clang::SourceRange func_range = get_decl_sourcerange(func_decl);
        if (func_range.isValid()) {
            // 用正则替换空行（匹配仅含空白的行）
            llvm::Regex empty_line_regex("(?m)^[ \t]*\r?\n");  // (?m) 表示多行模式
            func_code = empty_line_regex.sub("", func_code);    // 替换为空
            return func_code;
        }
        // 3. 二次清理：删除可能残留的空行（整行仅空白字符）
        if (func_range.isValid()) {
            std::string rewritten = rewriter.getRewrittenText(func_range);
            
        //--std::cout <<"rewritten:"<<rewritten<<"\n";
            // 用正则替换空行（匹配仅含空白的行）
            llvm::Regex empty_line_regex("(?m)^[ \t]*\r?\n");  // (?m) 表示多行模式
            rewritten = empty_line_regex.sub("", rewritten);    // 替换为空
            return rewritten;
        }

        return get_decl_string(func_decl);
    }
    // 工具函数：拼接字符串
    std::string join(const std::vector<std::string> &vec, const std::string &sep) {
        std::string res;
        for (size_t i = 0; i < vec.size(); ++i) {
            res += (i > 0 ? sep : "") + vec[i];
        }
        return res;
    }
    std::string get_absolute_path(llvm::StringRef raw_path){
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::FileManager& FM = SM.getFileManager();
        llvm::SmallVector<char, 256> path_vec(raw_path.begin(), raw_path.end());
        std::string filePath_abs; 

        bool convert_success = FM.makeAbsolutePath(path_vec);
        if (convert_success) {
            std::string abs_path_str(path_vec.begin(), path_vec.end());
#if __cplusplus >= 201703L
            try {
                std::filesystem::path p(abs_path_str);
                std::filesystem::path canon = std::filesystem::weakly_canonical(p);
                filePath_abs = canon.string();
            } catch (...) {
                filePath_abs = abs_path_str;
            }
#else
            // If filesystem isn't available, just use the absolute path string.
            filePath_abs = abs_path_str;
#endif
        }
        return filePath_abs;
    }
    bool VisitFunctionDecl(clang::FunctionDecl *func_decl)
    {
       clang::SourceLocation loc = func_decl->getLocation();
        if (loc.isInvalid() || isInSystemHeader(loc)){
            return true;
        
        }
           
        // clang::SourceRange SR=get_decl_sourcerange(func_decl);
        // clang::SourceLocation loc = SR.getBegin().getLocWithOffset(-1);//函数声明的前一行
        clang::SourceManager &SM = _ctx->getSourceManager();
        std::string func_name = func_decl->getNameAsString();
        std::string func_path = get_loc_file(loc);
        std::string filePath_abs = normalize_path(make_absolute_path(func_path)); //标准化路径


        if (!_ctx->getSourceManager().isInMainFile(loc)){ // 头文件中的函数
            if(inst_info.define_files.find(filePath_abs) == inst_info.define_files.end()){
                return true;
            }
        }
        clang::SourceRange SR=get_decl_sourcerange(func_decl);
        std::string filePath =filePath_abs;
        if(filePath.find("src/")!=std::string::npos){
            int pos = filePath.find("src/");
            size_t idx = filePath.find_last_of("/", pos);
        
            if (idx != std::string::npos) {
                // 提取从最后一个 '/' 到字符串末尾的子字符串
                filePath = filePath.substr(idx + 1);
            }
        }
        std::cout <<"\n***in VisitFunctionDecl | Function " << func_decl->getNameAsString() << " is defined in file: " << filePath << "\n";
        
        if (inst_info.define_funcs.find(func_name)
            != inst_info.define_funcs.end())
        {
            if (func_decl == func_decl->getDefinition())
            {
                std::string written = remove_null_stmts_and_empty_lines(func_decl);
                // std::string written = get_decl_string(func_decl)+"\n";
                //--std::cout <<"written to file: "<<written << "\n";
                replace_suffix(filePath_abs, "_output");
                writeAnyInFile(filePath_abs, written);
            }
        }
        return true;
    }

  private:
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    InstInfo &inst_info;
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
        // //--std::cout << "构造函数 my frontend action\n ";
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
        // llvm::errs() << "in function InstFactory::create()\n";
        return std::make_unique<InstFrontendAction>(inst_info);
    }

  private:
    InstInfo &inst_info;
};
}  // namespace delSpace
#endif