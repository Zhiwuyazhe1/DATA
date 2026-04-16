// tool.cpp
#include "tool.h"
#include "clang/Lex/Lexer.h"
#include <algorithm>
#include <sstream>
#include <iostream>
using namespace clang;
namespace astslicer
{
    std::string MacroTools::get_stmt_string(const clang::Stmt *S){
        if(S==nullptr){
            return "<nullptr>";
        }
        try {
            clang::LangOptions LO;
            clang::PrintingPolicy Policy(LO);
            std::string buffer;
            llvm::raw_string_ostream strout(buffer);
            S->printPretty(strout, nullptr, Policy);
            strout.flush();
            // 检查最后一个字符是否为换行符
            while (!buffer.empty() && buffer.back() == '\n') {
                // 删除最后一个字符
                buffer.pop_back();
            }
            return buffer;
        } catch (const std::exception& e) {
            return "<error>";
        } catch (...) {
            return "<unknown error>";
        }
    }

    //获得stmt的range
    clang::SourceRange MacroTools::get_sourcerange(clang::Stmt *stmt){
        clang::SourceRange SR = stmt->getSourceRange();
        auto &SM = _ctx->getSourceManager();
        auto &LO = _ctx->getLangOpts();
        clang::SourceLocation loc = SR.getBegin();
        clang::SourceLocation end = SR.getEnd();
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
    // 获取源代码文本的实现
    std::string MacroTools::getSourceText(SourceLocation loc) {
        return getSourceText(loc, loc);
    }

    std::string MacroTools::getSourceText(SourceLocation start, SourceLocation end) {
        auto &SM = _ctx->getSourceManager();
        auto &LO = _ctx->getLangOpts();
        
        if (start.isInvalid() || end.isInvalid()) {
            return "";
        }
        
        // 确保结束位置包含完整的token
        end = Lexer::getLocForEndOfToken(end, 0, SM, LO);
        
        // 获取源代码文本
        CharSourceRange range = CharSourceRange::getCharRange(start, end);
        return Lexer::getSourceText(range, SM, LO).str();
    }

    std::string MacroTools::getSourceText(SourceRange range) {
        return getSourceText(range.getBegin(), range.getEnd());
    }

    // 获取语句的源信息
    StmtSourceInfo MacroTools::get_stmt_source_info(Stmt *stmt) {
        auto &SM = _ctx->getSourceManager();
        auto &LO = _ctx->getLangOpts();
        
        StmtSourceInfo info;
        info.sourceRange = get_sourcerange(stmt);
        
        // 收集宏展开信息
        collectMacroExpansionsInStmt(stmt, info.macroExpansions);
        
        return info;
    }

    // 收集语句中的所有宏展开
    bool MacroTools::collectMacroExpansionsInStmt(Stmt *stmt, std::vector<MacroExpansionInfo> &macros) {
        if (!stmt) return true;
        
        auto &SM = _ctx->getSourceManager();
        
        // 获取源文本和 AST 文本
        SourceRange range = stmt->getSourceRange();
        std::string sourceText = getSourceText(range);
        std::string astText = get_stmt_string(stmt);
        bool find_macro = false;
        // 使用字符串匹配检测宏展开
        find_macro |= detectMacroExpansionsByStringMatching(astText, sourceText, range, macros);
        
        // 原有的 AST 分析作为补充
        SourceLocation loc = stmt->getBeginLoc();
        if (loc.isMacroID()) {
            find_macro |= addMacroExpansion(stmt, macros);
        }
        
        // 对特定类型的表达式进行深入遍历
        if (auto *expr = llvm::dyn_cast<Expr>(stmt)) {
            traverseExpression(expr, macros);
        } else {
            // 对于非表达式，使用普通的子节点遍历
            for (auto *child : stmt->children()) {
                if (find_macro) break;
                if (child) {
                    find_macro |= collectMacroExpansionsInStmt(child, macros);
                }
            }
        }
        return find_macro;
    }

    // 添加宏展开信息
    bool MacroTools::addMacroExpansion(Stmt *stmt, std::vector<MacroExpansionInfo> &macros) {
        auto &SM = _ctx->getSourceManager();
        
        SourceLocation loc = stmt->getBeginLoc();
        if (!loc.isMacroID()) return false;
        
        MacroExpansionInfo macroInfo;
        
        // 获取宏名称
        SourceLocation spellingLoc = SM.getSpellingLoc(loc);
        std::string spellingText = getSourceText(spellingLoc, spellingLoc);
        macroInfo.macroName = spellingText;
        
        // 获取展开位置
        macroInfo.expansionLoc = SM.getExpansionLoc(loc);
        
        // 获取拼写范围
        SourceRange stmtRange = get_sourcerange(stmt);
        macroInfo.spellingRange = SourceRange(
            SM.getSpellingLoc(stmtRange.getBegin()),
            SM.getSpellingLoc(stmtRange.getEnd())
        );
        
        // 获取展开后的文本
        macroInfo.expandedText = getSourceText(stmtRange);
        
        // 去重检查
        bool exists = false;
        for (const auto &existing : macros) {
            if (existing == macroInfo) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            macros.push_back(macroInfo);
            return true;
        }
        return false;
    }

    // 深入遍历表达式
    void MacroTools::traverseExpression(Expr *expr, std::vector<MacroExpansionInfo> &macros) {
        if (!expr) return;
        
        // 处理成员访问表达式
        if (auto *memberExpr = llvm::dyn_cast<MemberExpr>(expr)) {
            // 遍历 base
            if (auto *base = memberExpr->getBase()) {
                collectMacroExpansionsInStmt(base, macros);
            }
            
            // 检查成员名称位置
            SourceLocation memberLoc = memberExpr->getMemberLoc();
            if (memberLoc.isMacroID()) {
                addMacroExpansion(memberExpr, macros);
            }
        }
        else if(auto *impExpr = clang::dyn_cast<clang::ImplicitCastExpr>(expr)){
            collectMacroExpansionsInStmt(impExpr->getSubExpr(), macros);
        }
        // 处理其他表达式类型...
        else {
            for (auto *child : expr->children()) {
                if (child) {
                    if(collectMacroExpansionsInStmt(child, macros)){
                        break;
                    }
                }
            }
        }
    }

    // 通过字符串匹配识别宏展开
    bool MacroTools::detectMacroExpansionsByStringMatching(const std::string &astText, 
                                                        const std::string &sourceText,
                                                        SourceRange range,
                                                        std::vector<MacroExpansionInfo> &macros) {
        
        // 如果两个字符串相同，没有宏展开
        if (astText == sourceText) {
            return false;
        }
        
        // 按 token 分割两个字符串进行比较
        std::vector<std::string> sourceTokens = tokenizeString(sourceText);
        std::vector<std::string> astTokens = tokenizeString(astText);
        // 比较 token 序列
        return compareTokenSequences(sourceTokens, astTokens, range, macros);
    }

    // 分割字符串为 token
    std::vector<std::string> MacroTools::tokenizeString(const std::string &str) {
        std::vector<std::string> tokens;
        std::string token;
        
        for (char c : str) {
            if (std::isspace(c) || c == ';' || c == ',' || c == '(' || c == ')' || 
                c == '[' || c == ']' || c == '{' || c == '}' || c == '>' || c == '<' ||
                c == '-' || c == '+' ) {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
                if (!std::isspace(c)) {
                    tokens.push_back(std::string(1, c));
                }
            } else if (c == '-' && !token.empty() && token.back() == '>') {
                // 处理 -> 操作符
                token.pop_back();
                if (!token.empty()) {
                    tokens.push_back(token);
                }
                tokens.push_back("->");
                token.clear();
            } else {
                token += c;
            }
        }
        
        if (!token.empty()) {
            tokens.push_back(token);
        }
        
        return tokens;
    }

    // 比较 token 序列来识别宏展开
    bool MacroTools::compareTokenSequences(const std::vector<std::string> &sourceTokens,
                                        const std::vector<std::string> &astTokens,
                                        SourceRange range,
                                        std::vector<MacroExpansionInfo> &macros) {
        
        int srcIdx = 0, astIdx = 0;
        int srcLen = sourceTokens.size();
        int astLen = astTokens.size();
        bool find_macro = false;
        while (srcIdx < srcLen && astIdx < astLen) {
            // 如果当前 token 相同，继续前进
            if (sourceTokens[srcIdx] == astTokens[astIdx]) {
                srcIdx++;
                astIdx++;
                continue;
            }
            // token 不同，可能是宏展开
            if (isIdentifier(sourceTokens[srcIdx]) && astIdx < astLen) {
                std::string macroName = sourceTokens[srcIdx];
                std::string expandedText;
                int startAstIdx = astIdx;
                
                // 尝试匹配后续的 tokens
                for (int lookAhead = 1; lookAhead <= 5 && (astIdx + lookAhead) <= astLen; lookAhead++) {
                    std::string potentialExpansion;
                    for (int i = 0; i < lookAhead; i++) {
                        if (i > 0) potentialExpansion += " ";
                        potentialExpansion += astTokens[astIdx + i];
                    }
                    
                    if (canTokensAlignAfterExpansion(sourceTokens, astTokens, srcIdx + 1, astIdx + lookAhead)) {
                        expandedText = potentialExpansion;
                        break;
                    }
                }
                
                if (!expandedText.empty()) {
                    MacroExpansionInfo macroInfo;
                    macroInfo.macroName = macroName;
                    macroInfo.expandedText = expandedText;
                    macroInfo.expansionLoc = range.getBegin();
                    find_macro = true;
                    // 去重
                    bool exists = false;
                    for (const auto &existing : macros) {
                        if (existing == macroInfo) {
                            exists = true;
                            break;
                        }
                    }
                    
                    if (!exists) {
                        macros.push_back(macroInfo);
                    }
                    
                    // 跳过已匹配的部分
                    srcIdx++;
                    astIdx += countTokens(expandedText);
                    continue;
                }
            }
            
            // 无法匹配，前进一个 token
            srcIdx++;
            astIdx++;
        }
        return find_macro;
    }

    // 检查跳过宏展开后 tokens 是否能对齐
    bool MacroTools::canTokensAlignAfterExpansion(const std::vector<std::string> &sourceTokens,
                                                const std::vector<std::string> &astTokens,
                                                int srcStart, int astStart) {
        int srcIdx = srcStart;
        int astIdx = astStart;
        int srcLen = sourceTokens.size();
        int astLen = astTokens.size();
        
        // 检查后续的 3-5 个 token 是否能对齐
        int checkCount = std::min(5, std::min(srcLen - srcStart, astLen - astStart));
        
        for (int i = 0; i < checkCount; i++) {
            if (srcIdx >= srcLen || astIdx >= astLen) break;
            if (sourceTokens[srcIdx] != astTokens[astIdx]) {
                return false;
            }
            srcIdx++;
            astIdx++;
        }
        
        return true;
    }

    // 辅助函数：检查是否是标识符
    bool MacroTools::isIdentifier(const std::string &str) {
        if (str.empty()) return false;
        
        for (char c : str) {
            if (!std::isalnum(c) && c != '_') {
                return false;
            }
        }
        
        return !std::isdigit(str[0]);
    }

    // 辅助函数：计算文本中的 token 数量
    int MacroTools::countTokens(const std::string &text) {
        return tokenizeString(text).size();
    }

};  // namespace astslicer