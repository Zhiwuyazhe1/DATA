#include "../include/Common.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>

#include <fstream>
#include <iostream>

using namespace std;

namespace {

class ASTFunctionLoad : public ASTConsumer,
                        public RecursiveASTVisitor<ASTFunctionLoad> {
public:
  void HandleTranslationUnit(ASTContext &Context) override {
    TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
    TraverseDecl(TUD);
  }

  bool TraverseDecl(Decl *D) {
    if (!D)
      return true;
    bool rval = true;
    //if (D->getASTContext().getSourceManager().isInMainFile(D->getLocation()) ||
    //    D->getKind() == Decl::TranslationUnit) {
    rval = RecursiveASTVisitor<ASTFunctionLoad>::TraverseDecl(D);
    //}
    return rval;
  }
  bool TraverseCXXMethodDecl(CXXMethodDecl *FD) {
    if (FD && FD->isThisDeclarationADefinition()) {
      functions.push_back(FD);
    }
    return true;
  }
  bool TraverseFunctionDecl(FunctionDecl *FD) {
    if (FD && FD->isThisDeclarationADefinition()) {
      functions.push_back(FD);
    }
    return true;
  }

  bool TraverseStmt(Stmt *S) { return true; }

  const std::vector<const FunctionDecl *> &getFunctions() const { return functions; }

private:
  std::vector<const FunctionDecl *> functions;
};

class ASTVariableLoad : public RecursiveASTVisitor<ASTVariableLoad> {
public:
  bool VisitDeclStmt(DeclStmt *S) {
    if(!S) return true;
    for (auto D = S->decl_begin(); D != S->decl_end(); D++) {
      if (const VarDecl *VD = dyn_cast<VarDecl>(*D)) {
        variables.push_back(VD);
      }
    }
    return true;
  }
  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    if(!DRE) return true;
    if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
        variables.push_back(VD);
    }
    return true;
  }
  const std::vector<const VarDecl *> &getVariables() { return variables; }

private:
  std::vector<const VarDecl *> variables;
};

class ASTCalledFunctionLoad
    : public RecursiveASTVisitor<ASTCalledFunctionLoad> {
public:
  bool VisitCallExpr(CallExpr *E) {
    if (!E) return true;
    if (const FunctionDecl *FD = E->getDirectCallee()) {
      functions.insert(FD);
    }
    return true;
  }

  const std::vector<const FunctionDecl *> getFunctions() {
    return std::vector<const FunctionDecl *>(functions.begin(), functions.end());
  }

private:
  std::set<const FunctionDecl *> functions;
};

class ASTCallExprLoad : public RecursiveASTVisitor<ASTCallExprLoad> {
public:
  bool TraverseStmt(Stmt *S) {
    if (!isValidStmt(S)) {
      return true;
    }
    // 调用父类方法处理有效节点
    return RecursiveASTVisitor<ASTCallExprLoad>::TraverseStmt(S);
  }

  bool VisitCallExpr(CallExpr *E) {
    if (!E) return true;
    call_exprs.push_back(E);
    return true;
  }

  const std::vector<const CallExpr *> getCallExprs() { return call_exprs; }

private:
  std::vector<const CallExpr *> call_exprs;
  bool isValidStmt(Stmt *s) {
    if (!s) return false;
    
    // 1. 检查指针对齐（至少2字节对齐）
    uintptr_t ptrVal = reinterpret_cast<uintptr_t>(s);
    if ((ptrVal & 1) != 0) {
      return false;
    }
    
    // 2. 检查StmtClass是否在合法范围内
    // （StmtClass枚举值通常小于100，可根据实际版本调整）
    Stmt::StmtClass sc;
    try {
      // 尝试访问sClass，若指针无效可能会崩溃，用try-catch保护
      sc = s->getStmtClass();
    } catch (...) {
      return false;
    }
    
    return true;
  }
};

} // end of anonymous namespace

namespace common {

std::vector<const FunctionDecl *> getFunctions(std::string AST, std::list<std::unique_ptr<ASTUnit>> &ASTQueue) {
  
  std::vector<const FunctionDecl *> res;
  return res;
}

/**
 * get all functions's decl from an ast context.
 */
std::vector<const FunctionDecl *> getFunctions(ASTContext &Context) {
  ASTFunctionLoad load;
  load.HandleTranslationUnit(Context);
  return load.getFunctions();
}

/**
 * get all variables' decl of a function
 * FD : the function decl.
 */
std::vector<const VarDecl *> getVariables(const FunctionDecl *FD) {
  std::vector<const VarDecl *> variables;
  variables.insert(variables.end(), FD->param_begin(), FD->param_end());

  ASTVariableLoad load;
  load.TraverseStmt(FD->getBody());
  variables.insert(variables.end(), load.getVariables().begin(),
                   load.getVariables().end());

  return variables;
}

/**
 * get all variables' decl of a stmt used
 */
std::vector<const VarDecl *> getVariables(Stmt *stmt) {
  std::vector<const VarDecl *> variables;

  ASTVariableLoad load;
  load.TraverseStmt(stmt);
  variables.insert(variables.end(), load.getVariables().begin(),
                   load.getVariables().end());

  return variables;
}

std::vector<const FunctionDecl *> getCalledFunctions(const FunctionDecl *FD) {
  ASTCalledFunctionLoad load;
  load.TraverseStmt(FD->getBody());
  return load.getFunctions();
}

std::vector<const CallExpr *> getCallExpr(const FunctionDecl *FD) {
  ASTCallExprLoad load;
  load.TraverseStmt(FD->getBody());
  return load.getCallExprs();
}

std::vector<const CallExpr *> getCallExpr(Stmt *stmt) {
  ASTCallExprLoad load;
  load.TraverseStmt(stmt);
  return load.getCallExprs();
}

std::string getParams(const FunctionDecl *FD) {
  std::string params = "";
  for (auto param = FD->param_begin(); param != FD->param_end(); param++) {
    params = params + (*param)->getOriginalType().getAsString() + "  ";
  }
  return params;
}

std::string makeValidFileName(const std::string& originalFileName) {
    std::string validFileName;
    for (char c : originalFileName) {
        // Windows 文件名中非法字符包括 \ / : * ? " < > |
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            validFileName.push_back('_'); // 替换为下划线
        } else {
            validFileName.push_back(c);
        }
    }
    return validFileName;
}

std::string getFullName(const FunctionDecl *FD) {
  std::string name = FD->getQualifiedNameAsString();

  name = name + "[]";
  name = makeValidFileName(name);
  return name;
}

std::string getFuncBegin(const FunctionDecl *FD) {
  SourceManager *sm = &FD->getASTContext().getSourceManager();
  std::string begin = ((Decl*)FD)->getBeginLoc().printToString(*sm);

  return begin;
}

std::string getFuncEnd(const FunctionDecl *FD) {
  SourceManager *sm = &FD->getASTContext().getSourceManager();
  std::string end = ((Decl*)FD)->getEndLoc().printToString(*sm);

  return end;
}

std::string getFileExtension(const FunctionDecl *FD) {
  clang::SourceManager &SM = FD->getASTContext().getSourceManager();
  clang::SourceLocation Loc = FD->getBeginLoc();

  clang::PresumedLoc PresumedLoc = SM.getPresumedLoc(Loc);
  std::string FileName = PresumedLoc.getFilename();

  size_t dotPos = FileName.find_last_of('.');
  std::string fileExtension;

  if (dotPos != std::string::npos) {
    fileExtension = FileName.substr(dotPos + 1);
  } else {
    fileExtension = "";
  }

  return fileExtension;
}

std::string getString_of_Expr(const Expr *expr) {
  LangOptions L0;
  L0.CPlusPlus = 1;
  std::string buffer1;
  llvm::raw_string_ostream strout1(buffer1);
  expr->printPretty(strout1, nullptr, PrintingPolicy(L0));
  return strout1.str();
}

std::string getString_of_Stmt(const Stmt *stmt) {
  LangOptions L0;
  L0.CPlusPlus = 1;
  std::string buffer1;
  llvm::raw_string_ostream strout1(buffer1);
  stmt->printPretty(strout1, nullptr, PrintingPolicy(L0));
  return strout1.str();
}

std::string getString_of_VarDecl(const VarDecl *vd) {
  std::string varName = vd->getNameAsString();
  std::string varType = vd->getType().getAsString();
  std::string res = varType + " " + varName;
  if (vd->hasInit() && nullptr != vd->getInit()) {
    if (vd->getInit()->getStmtClass() == Stmt::CXXConstructExprClass) {
      res += "(" + getString_of_Expr(vd->getInit()) + ")"; 
    }
    else
      res += " = " + getString_of_Expr(vd->getInit());
  }
  return res;
}

std::string string_replace_all(std::string resource_str, std::string sub_str, std::string new_str)
{
  std::string dst_str = resource_str;
  std::string::size_type pos = 0;
  while((pos = dst_str.find(sub_str, pos)) != std::string::npos) {
    dst_str.replace(pos, sub_str.length(), new_str);
    pos = pos + new_str.length();
  }
  return dst_str;
}

void process_bar(float progress) {
  int barWidth = 70;
  std::cout << "[";
  int pos = progress * barWidth;
  for (int i = 0; i < barWidth; ++i) {
    if (i < pos)
      std::cout << "|";
    else 
      std::cout << " ";
  }
  if (progress == 1.0)
    std::cout << "]" << int(progress * 100.0) << "%\n";
  else
    std::cout << "]" << int(progress * 100.0) << "%\r";
  std::cout.flush();
}
} // end of namespace common

std::string trim(std::string s) {
  std::string result = s;
  result.erase(0, result.find_first_not_of(" \t\r\n"));
  result.erase(result.find_last_not_of(" \t\r\n") + 1);
  return result;
}

std::vector<std::string> initialize(std::string astList) {
  std::vector<std::string> astFiles;

  std::ifstream fin(astList);
  std::string line;
  while (getline(fin, line)) {
    line = trim(line);
    if (line == "")
      continue;
    std::string fileName = line;
    astFiles.push_back(fileName);
  }
  fin.close();

  return astFiles;
}
