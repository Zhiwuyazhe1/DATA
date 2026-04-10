#ifndef TOOL_H
#define TOOL_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include <clang/Rewrite/Core/Rewriter.h>
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <clang/AST/Decl.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>
#include <iostream>
#include <unistd.h>
#include "Common.h"
namespace sseq{

//
class Tool{
public:
    static const std::set<std::string> fileOperationFunctions;
    static const std::set<std::string> memoryAllocFunctions;
    static bool isIdentifierPart(char ch);
    static int levenshteinDistance(const std::string &str1, const std::string &str2);
    static double stringSimilarity(const std::string &str1, const std::string &str2);
    static std::string get_stmt_string(const clang::Stmt *S);
    static std::string get_decl_string(const clang::Decl *S);
    static std::string get_qualType_string(const clang::QualType &T);
    static std::string get_qualified_method_name(const clang::FunctionDecl* methodDecl);
    static clang::SourceRange get_decl_sourcerange(clang::ASTContext *_ctx, \
    const clang::Decl *stmt);
    static clang::SourceRange get_sourcerange(clang::ASTContext *_ctx,\
    const clang::Stmt *stmt);
    static clang::QualType get_normal_type_decl(clang::QualType QT);
    // 判断类型定义是否为函数指针类型
    static bool isFunctionPointerTypedef(const clang::TypedefDecl *TD);
    static bool isFunctionPointerCall(const clang::CallExpr* CE);
    static bool isInSystemHeader(clang::ASTContext *_ctx, const clang::FunctionDecl *FD);
    static bool isInSystemHeader(clang::ASTContext *_ctx, const clang::SourceLocation &loc);
    static std::string get_loc_file(clang::ASTContext *_ctx, const clang::SourceLocation &loc);

    static const clang::VarDecl* getVD_REF(const clang::Expr *E); //E是拆除过隐式转换的

    static const clang::VarDecl* getCallee_FP(const clang::CallExpr *CE);

    static const clang::Expr *getArrayBase(const clang::ArraySubscriptExpr *ASE);
    static const clang::Expr *getArrayIndex(const clang::ArraySubscriptExpr *ASE);
    
    static const clang::Expr *getMemberBase(const clang::MemberExpr *ME);
    static const clang::Expr *getBinaryOperatorLHS(const clang::BinaryOperator *BO);
    static const clang::Expr *getBinaryOperatorRHS(const clang::BinaryOperator *BO);
    static const clang::Expr *getVarDeclInit(const clang::VarDecl *VD);
    static const clang::Expr *getReturnValue(const clang::ReturnStmt *RS);
    static const clang::Expr *getUnqualifiedExpr(const clang::Expr *E);

    //为了a = (A*)malloc(sizeof(A)); 服务
    static const clang::Expr *getBinaryOperatorRHS_Cast(const clang::BinaryOperator *BO);
    static const clang::Expr *getVarDeclInit_Cast(const clang::VarDecl *VD);

    static bool isMacroExpansion(const clang::Stmt *S);
    static int checkCond(clang::ASTContext *_ctx, const clang::Expr *E);
    static bool isBranchStmt(const clang::Stmt *S);
    static int getBranchState(clang::ASTContext *_ctx,const clang::Stmt *S, bool isElse = false);
    static bool isLoopStmt(const clang::Stmt *S);
    static int isInclude(clang::ASTContext *_ctx, clang::Stmt *S, clang::Stmt *sub);

    static bool isRecusiveFunction(const clang::FunctionDecl *FD);
    static bool isFunctionDepanceExtern(const clang::FunctionDecl *FD);//函数是否依赖于全局变量、静态变量或外部资源
    static bool ismemoryAllocationCall(const clang::CallExpr *CE);
    static bool isGlobalUsed(const clang::Expr *expr);
    static int whichVarStorage(const clang::VarDecl *VD);

    static std::string getFunctionSignature(clang::FunctionDecl* func_decl); // 获取函数签名（包含返回类型、函数名和参数列表）

class isStmtInLoop{
    public:
        explicit isStmtInLoop(clang::ASTContext *ctx): _ctx(ctx){
            if (_ctx == nullptr) {
                throw std::invalid_argument("ASTContext pointer cannot be null");
            }
        }
        bool check(clang::Stmt *stmt) { //返回是不是循环body或条件
            if (stmt == nullptr) {
                return false;
            }
            return isStmtInFor(stmt) || isStmtInWhile(stmt) || isStmtInDo(stmt);
        }
    void set_ret(bool r){ ret = r;}
    private:
        bool isStmtInFor(clang::Stmt *stmt) {
            clang::ast_matchers::MatchFinder matcher;
            auto forMatcher = clang::ast_matchers::stmt(
                clang::ast_matchers::hasAncestor(
                    clang::ast_matchers::forStmt(
                        clang::ast_matchers::anyOf(
                            clang::ast_matchers::hasCondition(clang::ast_matchers::hasDescendant(clang::ast_matchers::stmt().bind("loopCondition"))),
                            clang::ast_matchers::hasBody(clang::ast_matchers::hasDescendant(clang::ast_matchers::stmt().bind("loopBody")))
                        )
                    )
                )
            ).bind("loopstmt");
            matcher.addMatcher(forMatcher, &loopCallback);
            matcher.match(*stmt, *_ctx);
            return ret;
        }
        bool isStmtInWhile(clang::Stmt *stmt) {
            clang::ast_matchers::MatchFinder matcher;
            auto whileMatcher = clang::ast_matchers::stmt(
                clang::ast_matchers::hasAncestor(
                    clang::ast_matchers::whileStmt(
                        clang::ast_matchers::anyOf(
                            clang::ast_matchers::hasCondition(clang::ast_matchers::hasDescendant(clang::ast_matchers::stmt().bind("loopCondition"))),
                            clang::ast_matchers::hasBody(clang::ast_matchers::hasDescendant(clang::ast_matchers::stmt().bind("loopBody")))
                        )
                    )
                )
            ).bind("loopstmt");
            matcher.addMatcher(whileMatcher, &loopCallback);
            matcher.match(*stmt, *_ctx);
            return ret;
        }

        bool isStmtInDo(clang::Stmt *stmt) {
            clang::ast_matchers::MatchFinder matcher;
            auto doMatcher = clang::ast_matchers::stmt(
                clang::ast_matchers::hasAncestor(
                    clang::ast_matchers::doStmt(
                        clang::ast_matchers::anyOf(
                            clang::ast_matchers::hasCondition(clang::ast_matchers::hasDescendant(clang::ast_matchers::stmt().bind("loopCondition"))),
                            clang::ast_matchers::hasBody(clang::ast_matchers::hasDescendant(clang::ast_matchers::stmt().bind("loopBody")))
                        )
                    )
                )
            ).bind("loopstmt");
            matcher.addMatcher(doMatcher, &loopCallback);
            matcher.match(*stmt, *_ctx);
            return ret;      
        }
        class LoopStmtCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
        public:
            explicit LoopStmtCallback(isStmtInLoop *parent) : parent(parent) {}
            void run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override {
                if (const auto *Node = Result.Nodes.getNodeAs<clang::Stmt>("loopstmt")) {
                    // std::cout << "[LoopStmtCallback] In Loop " <<get_stmt_string(Node) <<"\n";
                    parent->set_ret(true);
                }
            }
             private:
                isStmtInLoop *parent;
        };
        LoopStmtCallback loopCallback{this};
        clang::ASTContext *_ctx;
        bool ret = false;
    };


class isStmtInBranch{
    public:
        explicit isStmtInBranch(clang::ASTContext *ctx): _ctx(ctx){
            if (_ctx == nullptr) {
                throw std::invalid_argument("ASTContext pointer cannot be null");
            }
        }
        bool check(clang::Stmt *stmt) { //返回是不是在分支
            if (stmt == nullptr) {
                return false;
            }
            bool ret = isStmtInThen(stmt) || isStmtInElse(stmt) || isStmtInSwitch(stmt);
            // std::cout<<get_stmt_string(stmt)<<" -> "<<ret<<"\n";
            return ret;
        }
    void set_ret(bool r){ ret = r;}
    private:
        bool isStmtInThen(clang::Stmt *stmt) {
            clang::ast_matchers::MatchFinder matcher;
            auto ifThenMatcher = clang::ast_matchers::stmt(
                clang::ast_matchers::hasAncestor(
                    clang::ast_matchers::ifStmt(
                        clang::ast_matchers::hasThen(clang::ast_matchers::hasDescendant(clang::ast_matchers::stmt().bind("targetStmt")))
                    )
                )
            ).bind("branchstmt");
            matcher.addMatcher(ifThenMatcher, &branchCallback);
            matcher.match(*stmt, *_ctx);
            return ret;
        }
        bool isStmtInElse(clang::Stmt *stmt) {
            clang::ast_matchers::MatchFinder matcher;
            auto ifElseMatcher = clang::ast_matchers::stmt(
                clang::ast_matchers::hasAncestor(
                    clang::ast_matchers::ifStmt(
                        clang::ast_matchers::hasElse(clang::ast_matchers::hasDescendant(clang::ast_matchers::stmt().bind("targetStmt")))
                    )
                )
            ).bind("branchstmt");
            matcher.addMatcher(ifElseMatcher, &branchCallback);
            matcher.match(*stmt, *_ctx);
            return ret;
        }

        bool isStmtInSwitch(clang::Stmt *stmt) {
            clang::ast_matchers::MatchFinder matcher;
            auto switchMatcher = clang::ast_matchers::stmt(
                clang::ast_matchers::hasAncestor(clang::ast_matchers::switchStmt())
            ).bind("branchstmt");
            matcher.addMatcher(switchMatcher, &branchCallback);
            matcher.match(*stmt, *_ctx);
            return ret;      
        }
        class BranchStmtCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
        public:
            explicit BranchStmtCallback(isStmtInBranch *parent) : parent(parent) {}
            void run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override {
                if (const auto *Node = Result.Nodes.getNodeAs<clang::Stmt>("branchstmt")) {
                    // std::cout << "[LoopStmtCallback] In Loop " <<get_stmt_string(Node) <<"\n";
                    parent->set_ret(true);
                }
            }
        private:
            isStmtInBranch *parent;
        };
        
        BranchStmtCallback branchCallback{this};
        clang::ASTContext *_ctx;
        bool ret = false;
    };

};

    
} //namespace sseq
#endif