#ifndef PDG_H
#define PDG_H
#include <fstream>
#include <iostream>
#include <vector>
#include <queue>
#include <sstream>
#include <stack>
#include <string>
#include <tuple>

#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Analysis/CFG.h>
#include <clang/Frontend/ASTConsumers.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <nlohmann/json.hpp>
#include "Common.h"
#include "tool.h"

using namespace clang;
using namespace llvm;
using namespace clang::driver;
using namespace clang::tooling;

using json = nlohmann::json;

class PDGNode {
public:
  PDGNode(Stmt *stmt, int ID);
  PDGNode(std::string stmt, int ID);

  Stmt* getStmt();
  std::string getStmtString();
  int getID();

  void addWriteVars(int64_t id);
  void addReadVars(int64_t id);
  void addConditionalWriteVars(int64_t id);
  void addGotoVars(int64_t id);

  int64_t getWriteVars();
  int64_t getReadVars();
  int64_t getConditionalWriteVars();
  int64_t getGotoVars();


private:
  Stmt *stmt;
  std::string stmtString;
  int ID;
  int64_t writeVars;
  int64_t readVars;
  int64_t conditionalWriteVars;             // write the vars in branch, such as for, while, do, (if)
  int64_t gotoVars;
};
enum LinkType{SOURCE, AST, CONTROL, DATA, CD, DC, MIX};
/* 状态机 ,T表示所有LinkType类型
    T + AST -> T

    SOURCE + T -> T

    CONTROL + DATA -> CD
    CD + DATA -> CD
    DC + DATA = MIX

    DATA + CONTROL -> DC
    DC + CONTROL -> DC
    CD + CONTROL = MIX
*/
struct Link{
  int nodeID;
  LinkType T;
  Link(int id, LinkType t) : nodeID(id), T(t) {}
  Link NextLink(int id, LinkType t){ //LinkType t
    if(T == SOURCE && t!=AST) {
      return {id,t};
    }
    Link next = {id,T};//初始类型和当前 Link 相同
    switch (t)
    {
    case AST:
      next.T = T; // T + AST -> T
      break;
    case DATA:
      if(T == CONTROL || T == CD)
        next.T = CD;
      else if(T == DC)
        next.T = MIX;
      
      break;
    case CONTROL:
      if(T == DATA || T == DC)
        next.T = DC;
      else if(T == CD)
        next.T = MIX;
      break;
    
    default:
      break;
    }

    return next;
  }

  std::string printT() const {
    switch (T) {
      case LinkType::SOURCE:
          return "SOURCE";
      case LinkType::AST:
          return "AST";
      case LinkType::CONTROL:
          return "CONTROL";
      case LinkType::DATA:
          return "DATA";
      case LinkType::CD:
          return "CD";
      case LinkType::DC:
          return "DC";
      case LinkType::MIX:
          return "MIX";
      default:
          return "UNKNOWN";
    }
  }
};

struct Link_hash{
  size_t operator()(const Link &l1) const{
    return std::hash<int>()(l1.nodeID) ^ std::hash<int>()(l1.T);
  }
};
struct Link_equal{
  bool operator()(const Link &l1, const Link &l2) const noexcept{
    return l1.nodeID == l2.nodeID && l1.T == l2.T;
  }
};
class PDG {
public:
  PDG(FunctionDecl *FD, std::string root_path, std::string program_name);
  ~PDG();
  void addNode(PDGNode *node);

  enum edgeType{controlDependenceEdge, dataDependenceEdge, astEdge};
  enum operationType{defaultOperation, read, write, conditionalWrite, readAndWrite, readAndConditionalWrite, inBranch};
  void addControlDependenceEdge(int firstID, int secondID);
  void addDataDependenceEdge(int firstID, int secondID);
  void addASTEdge(int firstID, int secondID);
  void addEdges(int firstID, int secondID, edgeType t);

  void printGraph();
  void dumpDot();
  void dumpJson();

  int handleStmt(Stmt *stmt, int rootID, edgeType et, operationType operType);
  int handleDecl(Decl* decl, int rootID, edgeType et, operationType operType, bool isAddNewNode);

  void computeDataDependence();
  std::unordered_set<int> get_ancestor_nodes(int initID);//获得AST上所有前驱节点（在这个函数定义范围内）
  bool has_ancestor_nodes(std::unordered_set<int> &st, int ID);
  bool is_ancestor_nodes(int ID, int AID);//AID是不是ID的祖先
  bool is_branch_condition(int ID);//ID是不是分支条件

  std::unordered_set<Link, Link_hash, Link_equal> get_rela_nodes(int initID);
  // unused 
  std::unordered_set<int> get_data_rela_nodes(int initID);
  std::unordered_set<int> get_cond_rela_nodes(int initID);

  std::vector<const VarDecl*> get_vars(int nodeID);
  bool stmt_is_node(const Stmt* stmt);
  clang::Stmt* get_node_ID(int nodeID);
  PDGNode* get_node_stmt(const Stmt* stmt);
  std::vector<std::string> splitString(std::string stmtString, std::string pattern);
private:
  FunctionDecl* currentFD;

  std::vector<PDGNode*> nodeList;
  std::vector<int> nodeEmbedding;
  int totalNodeNum;

  std::unordered_map<int, PDGNode*> node_map;
  std::unordered_map<const Stmt*, PDGNode*> stmt_node;
  std::map<int, std::set<int>> controlDependenceEdges;
  std::map<int, std::set<int>> dataDependenceEdges;
  std::map<int, std::set<int>> astEdges;
  std::map<int, std::set<int>> astPredecessor;//AST前驱节点

  std::vector<struct locINFO> locINFOS;

  std::map<std::pair<int, int>, std::pair<int, int>> ifElsePair;

  std::string root_path;
  std::string program_name;
};

class PDG2Graph {
public:
  PDG2Graph(std::vector<FunctionDecl *> funcs) {
    funcList = funcs;
  };
  void transform();
  void setRootPath(std::string path) {
    root_path = path;
  }
  void setProgramName(std::string name) {
    program_name = name;
  }

private:
  std::vector<FunctionDecl *> funcList;
  std::string root_path;
  std::string program_name;
};
#endif