// tool.h
#ifndef TOOL_H
#define TOOL_H
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Basic/SourceManager.h"
#include <vector>
#include <string>
#include <functional>

namespace astslicer
{
    // 定义宏信息结构体
    struct MacroExpansionInfo {
        std::string macroName;
        clang::SourceLocation expansionLoc;
        clang::SourceRange spellingRange;
        std::string expandedText;
        
        // 重载相等运算符用于去重
        bool operator==(const MacroExpansionInfo& other) const {
            return macroName == other.macroName && 
                expandedText == other.expandedText;
        }
        bool isMacroNameInExpansion() const {
            if (macroName.empty() || expandedText.empty()) {
                return false;
            }
            return expandedText.find(macroName) != std::string::npos;
        }
    };

    struct StmtSourceInfo {
        clang::SourceRange sourceRange;
        std::vector<MacroExpansionInfo> macroExpansions;
    };

    class MacroTools {
    private:
        clang::ASTContext *_ctx;
        clang::Rewriter &_rewriter;
        
    public:
        MacroTools(clang::ASTContext *ctx, clang::Rewriter &rewriter) 
            : _ctx(ctx), _rewriter(rewriter) {}
        
        // 获取语句的源信息（包含宏信息）
        StmtSourceInfo get_stmt_source_info(clang::Stmt *stmt);
        
        // 收集语句中的所有宏展开
        bool collectMacroExpansionsInStmt(clang::Stmt *stmt, std::vector<MacroExpansionInfo> &macros);
        
        // 通过字符串匹配识别宏展开
        bool detectMacroExpansionsByStringMatching(const std::string &astText, 
                                                const std::string &sourceText,
                                                clang::SourceRange range,
                                                std::vector<MacroExpansionInfo> &macros);
        
        // 获取源代码文本
        std::string getSourceText(clang::SourceLocation loc);
        std::string getSourceText(clang::SourceLocation start, clang::SourceLocation end);
        std::string getSourceText(clang::SourceRange range);
        
    private:
        // 辅助函数
        bool addMacroExpansion(clang::Stmt *stmt, std::vector<MacroExpansionInfo> &macros);
        void traverseExpression(clang::Expr *expr, std::vector<MacroExpansionInfo> &macros);
        std::vector<std::string> tokenizeString(const std::string &str);
        bool compareTokenSequences(const std::vector<std::string> &sourceTokens,
                                const std::vector<std::string> &astTokens,
                                clang::SourceRange range,
                                std::vector<MacroExpansionInfo> &macros);
        bool canTokensAlignAfterExpansion(const std::vector<std::string> &sourceTokens,
                                        const std::vector<std::string> &astTokens,
                                        int srcStart, int astStart);
        bool isIdentifier(const std::string &str);
        int countTokens(const std::string &text);
        clang::SourceRange get_sourcerange(clang::Stmt *stmt);
        std::string get_stmt_string(const clang::Stmt *S);
    };
}  // namespace astslicer
#endif // TOOL_H