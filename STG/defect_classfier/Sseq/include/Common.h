#ifndef BASE_COMMON_H
#define BASE_COMMON_H

#include <vector>

#include <clang/Frontend/ASTUnit.h>

#include "Config.h"

using namespace clang;

struct locINFO {
  int nodeID;
  std::string beginLoc;
  std::string endLoc;
};

std::vector<std::string> initialize(std::string astList);

namespace common {

enum CheckerName {
  AST2Graph
};

std::unique_ptr<ASTUnit> loadFromASTFile(std::string AST);
// std::vector<FunctionDecl *> getFunctions(std::vector<std::string> ASTs, std::list<std::unique_ptr<ASTUnit>> &ASTQueue);
std::vector<const FunctionDecl *> getFunctions(std::string AST, std::list<std::unique_ptr<ASTUnit>> &ASTQueue);

std::vector<const FunctionDecl *> getFunctions(ASTContext &Context);
std::vector<const VarDecl *> getVariables(const FunctionDecl *FD);
std::vector<const VarDecl *> getVariables(Stmt *stmt);

std::vector<const FunctionDecl *> getCalledFunctions(const FunctionDecl *FD);
std::vector<const CallExpr *> getCallExpr(const FunctionDecl *FD);
std::vector<const CallExpr *> getCallExpr(Stmt *stmt);

std::string makeValidFileName(const std::string& originalFileName);
std::string getFullName(const FunctionDecl *FD);
std::string getFuncBegin(const FunctionDecl *FD);
std::string getFuncEnd(const FunctionDecl *FD);
std::string getFileExtension(const FunctionDecl *FD);
std::string getParams(const FunctionDecl *FD);

std::string getString_of_Expr(const Expr *expr);
std::string getString_of_Stmt(const Stmt *stmt);
std::string getString_of_VarDecl(const VarDecl *vd);

std::string string_replace_all(std::string resource_str, std::string sub_str, std::string new_str);

void process_bar(float progress);

} // end of namespace common

#endif
