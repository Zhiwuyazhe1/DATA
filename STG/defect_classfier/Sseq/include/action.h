#ifndef FCOSAL_ACTION_H
#define FCOSAL_ACTION_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtCXX.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <clang/Rewrite/Core/Rewriter.h>

#include "seq_info.h"
#include "tool.h"
#include "Common.h"
class SeqInfo;
class Point;
namespace sseq
{
  /*用于匹配路径上的点到AST节点 |计划改为单例模式*/
class ASTPointerMatch : public clang::RecursiveASTVisitor<ASTPointerMatch> {
private:
  clang::ASTContext *_ctx;
  /* 匹配到最细粒度的stmt记录在p.node */
  Point *point;/* 待匹配的 */
  clang::Stmt *stmt;
  std::vector<clang::VarDecl *> variables;
public:
explicit ASTPointerMatch(clang::ASTContext *ctx, Point *p,clang::Stmt *s): _ctx(ctx), point{ p }, stmt{ s }
    {}
  bool VisitStmt(clang::Stmt *S);
  void set_point(Point *p){ point = p;}
};

/* 检查域敏感相关内容 -> 路径  |改为单例模式*/
class ASTFieldChecker : public RecursiveASTVisitor<ASTFieldChecker> {
public:
  static ASTFieldChecker* getInstance(clang::ASTContext *ctx, SeqInfo &seq_i){
    std::lock_guard<std::mutex> lock(fieldCheckerMutex);
    if(checker==nullptr) checker = new ASTFieldChecker(ctx,seq_i);//目前不用判断ctx和seq的相等关系
    if(checker->_ctx != ctx){
      delete checker;
      checker = new ASTFieldChecker(ctx,seq_i);
    }
    return checker;
  }
  static void destroyInstance() { // 在 ~SeqInfo() 调用销毁
    std::lock_guard<std::mutex> lock(fieldCheckerMutex);
    delete checker;
    checker = nullptr;
  } 
  bool allocationChecker(const clang::CStyleCastExpr *CSC, clang::QualType lhsType);
  bool VisitDeclStmt(clang::DeclStmt *DS);
  bool VisitMemberExpr(clang::MemberExpr *ME);//A.b  A->b(A ImplicitCastExpr)
  // bool VisitDeclRefExpr(clang::DeclRefExpr *DRE);
  bool VisitArraySubscriptExpr(clang::ArraySubscriptExpr *ASE);
  bool VisitBinaryOperator(clang::BinaryOperator *BO);
private:
  static ASTFieldChecker* checker;
  static std::mutex fieldCheckerMutex;
  explicit ASTFieldChecker(clang::ASTContext *ctx, SeqInfo &seq_i): _ctx(ctx), seq_info{ seq_i } 
    {}
  ASTFieldChecker(const ASTFieldChecker&) = delete;  // 禁止拷贝构造
  ASTFieldChecker& operator=(const ASTFieldChecker&) = delete;  // 禁止赋值操作
  clang::ASTContext *_ctx;
  SeqInfo &seq_info;
};

/* 展开包括Expr的宏-记录 */
class ASTMacroChecker : public RecursiveASTVisitor<ASTMacroChecker> {
  public:
    static ASTMacroChecker* getInstance(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i){
      std::lock_guard<std::mutex> lock(macroCheckerMutex);
      if(checker==nullptr) checker = new ASTMacroChecker(ctx,R,seq_i);//目前不用判断相等关系
      if(checker->_ctx != ctx){
        delete checker;
        checker = new ASTMacroChecker(ctx,R,seq_i);
      }
      return checker;
    }
    static void destroyInstance() { // 在 ~SeqInfo() 调用销毁
      std::lock_guard<std::mutex> lock(macroCheckerMutex);
      delete checker;
      checker = nullptr;
    } 
    bool VisitExpr(clang::Expr *E);

  private:
    static ASTMacroChecker* checker;
    static std::mutex macroCheckerMutex;
    ASTMacroChecker(const ASTMacroChecker&) = delete;  // 禁止拷贝构造
    ASTMacroChecker& operator=(const ASTMacroChecker&) = delete;  // 禁止赋值操作
    explicit ASTMacroChecker(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i):\
         _ctx(ctx), _rewriter{R}, seq_info{ seq_i } {
      }

    int isMacroExpansion(const clang::Stmt *S);
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    SeqInfo &seq_info;
};

/* 类型转换 + 类型定义场景 */
class ASTTypeChecker : public RecursiveASTVisitor<ASTTypeChecker> {
  public:
    static ASTTypeChecker* getInstance(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i){
      std::lock_guard<std::mutex> lock(typeCheckerMutex);
      if(checker==nullptr) checker = new ASTTypeChecker(ctx,R,seq_i);// 目前不用判断相等关系 set_functionDecl?
      if(checker->_ctx != ctx){
        delete checker;
        checker = new ASTTypeChecker(ctx,R,seq_i);
      }
      return checker;
    }
    static void destroyInstance() { // 在 ~SeqInfo() 调用销毁
      std::lock_guard<std::mutex> lock(typeCheckerMutex);
      if(checker!=nullptr){
        delete checker;
        checker = nullptr;
      }
    } 
    bool isMultiDimensionalArray(const clang::QualType& QT);
    bool isSecondLevelPointer(const clang::QualType& QT);
    void checkRecord(const clang::RecordDecl *RD, int isTpdef = 0);
    void checkType(const clang::QualType &QT);
    bool isTypesCompatible(clang::QualType type1, clang::QualType type2);
    bool VisitDeclStmt(clang::DeclStmt *DS); // var init
    bool VisitBinaryOperator(clang::BinaryOperator *BO); // 比较运算符两端的变量类型
    bool VisitReturnStmt(clang::ReturnStmt *RS);  // 比较函数返回类型
    bool VisitCStyleCastExpr(clang::CStyleCastExpr *CSC); // 使用强制类型转换
    
    bool VisitDeclRefExpr(clang::DeclRefExpr *DRE);//使用的类型场景
    bool VisitMemberExpr(clang::MemberExpr *ME);

    void set_check_func(clang::FunctionDecl *FD){ check_func = FD;}
    void set_func_flag(bool flag){ func_flag = flag;}
    void update_type_info(){if(check_func!=nullptr) {seq_info.update_type_info(check_func, type_info);}}
//指针类型运算前是否在同一函数的上一次赋值后? -> PDG处理

  private:
    static ASTTypeChecker* checker;
    static std::mutex typeCheckerMutex;
    ASTTypeChecker(const ASTTypeChecker&) = delete;  // 禁止拷贝构造
    ASTTypeChecker& operator=(const ASTTypeChecker&) = delete;  // 禁止赋值操作
    explicit ASTTypeChecker(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i):\
         _ctx(ctx), _rewriter{R}, seq_info{ seq_i },type_info{TypeInfo()} {
      }

    TypeInfo type_info;// define in seq_info.h
    bool func_flag;// 是不是遍历了整个函数节点->ret
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    clang::FunctionDecl *check_func;
    SeqInfo &seq_info;
};


/* 检查特殊数值相关内容 -> 路径语句 */
/*
  #define NULL ((void*)0) 
  --limits.h库--
  //其他如SCHAR_MIN SHRT_MAX等等 后续以相同逻辑添加到macroList即可 --> 自动添加没见过的宏，必须要检查的宏写入config.txt
  判断宏而不是展开后的值
  #define INT_MAX   2147483647
  #define INT_MIN    (-INT_MAX - 1) [2147483648]
*/
/*
UnaryExprOrTypeTraitExpr   'unsigned long' sizeof 'A':'A'

在特殊位置的运算
*/
class ASTSpecialChecker : public RecursiveASTVisitor<ASTSpecialChecker> {
  public:
    static ASTSpecialChecker* getInstance(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i){
      std::lock_guard<std::mutex> lock(specialCheckerMutex);
      if(checker==nullptr) checker = new ASTSpecialChecker(ctx,R,seq_i);//目前不用判断相等关系
      if(checker->_ctx != ctx){
        delete checker;
        checker = new ASTSpecialChecker(ctx,R,seq_i);
      }
      return checker;
    }
    static void destroyInstance() { // 在 ~SeqInfo() 调用销毁
      std::lock_guard<std::mutex> lock(specialCheckerMutex);
      delete checker;
      checker = nullptr;
    } 
    bool VisitDeclStmt(clang::DeclStmt *DS);//初始化
    bool VisitBinaryOperator(clang::BinaryOperator *BO); // 二元运算
    bool VisitUnaryOperator(clang::UnaryOperator *UO);
    bool VisitIndirectGotoStmt(clang::IndirectGotoStmt *IGS);
    // bool VisitGotoStmt(clang::GotoStmt *GS);
    bool VisitCaseStmt(clang::CaseStmt *CS);
    bool VisitSwitchStmt(clang::SwitchStmt *SS);
    void set_check_func(clang::FunctionDecl *FD){ check_func = FD;}
    void set_func_flag(bool flag){ func_flag = flag;}
    void update_special_info(){if(check_func!=nullptr) seq_info.update_special_info(check_func, special_info);}
  private:
    static ASTSpecialChecker* checker;
    static std::mutex specialCheckerMutex;
    ASTSpecialChecker(const ASTSpecialChecker&) = delete;  // 禁止拷贝构造
    ASTSpecialChecker& operator=(const ASTSpecialChecker&) = delete;  // 禁止赋值操作
    explicit ASTSpecialChecker(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i):\
         _ctx(ctx), _rewriter{R}, seq_info{ seq_i },special_info{SpecialInfo()} {
          flag = true;
          try{
            branch_checker = std::make_unique<Tool::isStmtInBranch>(_ctx);
          }catch(const std::invalid_argument& e){
            std::cerr << "捕获到异常: " << e.what() << std::endl;
            flag = false;
          }
      }

    int isMacroExpansion(const clang::Stmt *S);
    SpecialInfo special_info;// define in seq_info.h
    bool flag;
    bool func_flag;// 是不是遍历了整个函数节点
    std::unique_ptr<Tool::isStmtInBranch> branch_checker;
    clang::FunctionDecl *check_func;
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    SeqInfo &seq_info;
};

class ASTControlFChecker : public RecursiveASTVisitor<ASTControlFChecker> {
  public:
    static ASTControlFChecker* getInstance(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i){
      std::lock_guard<std::mutex> lock(controlFCheckerMutex);
      if(checker==nullptr) checker = new ASTControlFChecker(ctx,R,seq_i);
      if(checker->_ctx != ctx){
        delete checker;
        checker = new ASTControlFChecker(ctx,R,seq_i);
      }
      return checker;
    }
    static void destroyInstance() { // 在 ~SeqInfo() 调用销毁
      std::lock_guard<std::mutex> lock(controlFCheckerMutex);
      delete checker;
      checker = nullptr;
    } 
    int checkCond(const clang::Expr *Cond);
    bool VisitSwitchStmt(clang::SwitchStmt *SS);//switch_default
    bool VisitIfStmt(clang::IfStmt *IS);//dead_code
    //loop_state dead_code
    bool VisitForStmt(clang::ForStmt *FS);
    bool VisitWhileStmt(clang::WhileStmt *WS);
    bool VisitDoStmt(clang::DoStmt *DS);
    void set_check_func(clang::FunctionDecl *FD){ check_func = FD;}
    void update_control_info(){if(check_func!=nullptr) seq_info.update_control_info(check_func, control_info);}
  private:
    static ASTControlFChecker* checker;
    static std::mutex controlFCheckerMutex;
    ASTControlFChecker(const ASTControlFChecker&) = delete;  // 禁止拷贝构造
    ASTControlFChecker& operator=(const ASTControlFChecker&) = delete;  // 禁止赋值操作
    explicit ASTControlFChecker(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i):\
         _ctx(ctx), _rewriter{R}, seq_info{ seq_i },control_info{ControlInfo()} {}

    ControlInfo control_info;// define in seq_info.h
    clang::FunctionDecl *check_func;
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    SeqInfo &seq_info;
};


/* 统计赋值次数 -> 涉及函数的所有语句*/ // 计划添加赋值场景统计,变量-场景
class ASTAssignLoad : public RecursiveASTVisitor<ASTAssignLoad> {
  public:
    // explicit ASTAssignLoad(clang::ASTContext *ctx, SeqInfo &seq_i)
    //     : _ctx(ctx), seq_info{ seq_i } ,is_loop{ false }
    // {
    //   // loop_checker = Tool::isStmtInLoop(ctx);
    // }
    explicit ASTAssignLoad(clang::FunctionDecl *func_decl, clang::ASTContext *ctx, SeqInfo &seq_i, bool loop = false)
        : func{func_decl}, _ctx(ctx), seq_info{ seq_i } ,is_loop{ loop }, loop_checker{ Tool::isStmtInLoop(_ctx)}
    {
    }
    ~ASTAssignLoad(){
    }
    bool VisitBinaryOperator(clang::BinaryOperator *BO);
    bool VisitUnaryOperator(clang::UnaryOperator *UO);
    /*
    isArithmeticOp()包含
      UO_PostInc：后缀自增操作符（x++）
      UO_PostDec：后缀自减操作符（x--）
      UO_PreInc：前缀自增操作符（++x）
      UO_PreDec：前缀自减操作符（--x）
      UO_AddrOf：取地址操作符（&x）
      UO_Deref：解引用操作符（*x）
      UO_Plus：正号操作符（+x）
      UO_Minus：负号操作符（-x）
      UO_Not：逻辑非操作符（!x）
      UO_LNot：按位非操作符（~x）
  */
    bool VisitVarDecl(clang::VarDecl *VD);
    bool VisitDeclStmt(clang::DeclStmt *DS);
    bool VisitCallExpr(clang::CallExpr *CE);
    void handle_CE(const clang::CallExpr *CE);
    bool not_for_init(clang::Stmt *stmt);
    // bool VisitForStmt(clang::ForStmt *FS);
    // bool VisitWhileStmt(clang::WhileStmt *WS);
    // bool VisitDoStmt(clang::DoStmt *DS);
  private:
    clang::ASTContext *_ctx;
    clang::FunctionDecl *func;
    Tool::isStmtInLoop loop_checker;
    SeqInfo &seq_info;
    bool is_loop;
};

/* 控制流 特殊内容计数 */
class ASTControlFlowCounter : public RecursiveASTVisitor<ASTControlFlowCounter> {
  public:
      static ASTControlFlowCounter* getInstance(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i) {
          std::lock_guard<std::mutex> lock(counterMutex);
          if (counter == nullptr)
              counter = new ASTControlFlowCounter(ctx, R, seq_i);
          if(counter->_ctx != ctx){
            delete counter;
            counter = new ASTControlFlowCounter(ctx, R, seq_i);
          } 
          return counter;
      }
      
      static void destroyInstance() {
          std::lock_guard<std::mutex> lock(counterMutex);
          if (counter != nullptr) {
              delete counter;
              counter = nullptr;
          }
      }

      void update_all_controlflow_info() {
          if (check_func != nullptr) {
              seq_info.set_controlflow_info(check_func, controlflow_info);
          }
      }
      bool TraverseStmt(clang::Stmt *S);
      void reset();
      // 统计结构控制流语句
      bool VisitIfStmt(clang::IfStmt *stmt) ;
      bool VisitSwitchStmt(clang::SwitchStmt *stmt);
      bool VisitForStmt(clang::ForStmt *stmt) ;
      bool VisitWhileStmt(clang::WhileStmt *stmt) ;
      bool VisitDoStmt(clang::DoStmt *stmt) ;
      bool VisitCaseStmt(clang::CaseStmt *CS);
      bool VisitGotoStmt(clang::GotoStmt *GS);
      bool VisitLabelStmt(clang::LabelStmt *LS);
      void set_check_func(clang::FunctionDecl *FD){ check_func = FD;}
      void set_func_flag(bool flag){ func_flag = flag;}
      void update_controlflow_info(){if(check_func!=nullptr) seq_info.update_controlflow_info(check_func, controlflow_info);}
  private:
      static ASTControlFlowCounter* counter;
      static std::mutex counterMutex;
      ASTControlFlowCounter(const ASTControlFlowCounter&) = delete;
      ASTControlFlowCounter& operator=(const ASTControlFlowCounter&) = delete;
  
      explicit ASTControlFlowCounter(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i)
          : _ctx(ctx), _rewriter(R), seq_info(seq_i) {}
      ControlFlowInfo controlflow_info; // 定义在 seq_info.h 中
      clang::FunctionDecl *check_func = nullptr;
      bool func_flag;
      clang::ASTContext *_ctx;
      clang::Rewriter &_rewriter;
      SeqInfo &seq_info;
  };
  
  /* 资源操作节点计数 */
  class ASTResourceAnalyzer : public RecursiveASTVisitor<ASTResourceAnalyzer> {
    public:
        ASTContext *Context;
        ResourceUsageInfo info;
        static ASTResourceAnalyzer* getInstance(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i) {
          std::lock_guard<std::mutex> lock(counterMutex);
          if (counter == nullptr)
              counter = new ASTResourceAnalyzer(ctx, R, seq_i);
          if(counter->_ctx != ctx){
            delete counter;
            counter = new ASTResourceAnalyzer(ctx, R, seq_i);
          } 
          return counter;
      }
        static void destroyInstance() {
          std::lock_guard<std::mutex> lock(counterMutex);
          if (counter != nullptr) {
              delete counter;
              counter = nullptr;
          }
      }
      void update_all_resource_info() {
        if (check_func != nullptr) {
            seq_info.set_resource_info(check_func, resource_info);
        }
    }
        void reset() {resource_info = ResourceUsageInfo(); }
        void set_check_func(clang::FunctionDecl *FD) {check_func = FD;}
        void set_func_flag(bool flag) {func_flag = flag;}
        void update_resource_info() {seq_info.update_resource_info(check_func, resource_info);}
        bool TraverseStmt(clang::Stmt *S);
        bool VisitCallExpr(CallExpr *call) ;
        ResourceUsageInfo getInfo() const { return info; }
    private:
       static ASTResourceAnalyzer* counter;
       static std::mutex counterMutex;
        clang::ASTContext *_ctx;
        clang::Rewriter &_rewriter;
        SeqInfo &seq_info;
        explicit ASTResourceAnalyzer(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i)
          : _ctx(ctx), _rewriter(R), seq_info(seq_i) {}
        clang::FunctionDecl *check_func = nullptr;
        bool func_flag = false;

        ResourceUsageInfo resource_info;

        
    };

  
    class ASTExceptionAnalyzer : public clang::RecursiveASTVisitor<ASTExceptionAnalyzer> {
      public:
          // 获取单例实例
          static ASTExceptionAnalyzer* getInstance(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i) {
              std::lock_guard<std::mutex> lock(analyzerMutex);  // 锁住，保证线程安全
              if (analyzer == nullptr) {
                  analyzer = new ASTExceptionAnalyzer(ctx, R, seq_i);  // 如果没有实例，则创建一个新的
              }
              if(analyzer->_ctx != ctx){
                delete analyzer;
                analyzer = new ASTExceptionAnalyzer(ctx, R, seq_i);
              } 
              return analyzer;
          }
          // 销毁单例实例
          static void destroyInstance() {
              std::lock_guard<std::mutex> lock(analyzerMutex);  // 锁住，保证线程安全
              if (analyzer != nullptr) {
                  delete analyzer;  // 删除实例
                  analyzer = nullptr;  // 设置为 nullptr，避免悬空指针
              }
          }
          // 更新异常处理信息
          void update_all_exception_info() {
              if (check_func != nullptr) {
                  seq_info.set_exception_info(check_func, exception_info);
              }
          }
          bool TraverseStmt(clang::Stmt *S) {return RecursiveASTVisitor<ASTExceptionAnalyzer>::TraverseStmt(S);}
          void reset() {exception_info = ExceptionHandlingInfo(); }
          bool VisitCXXTryStmt(clang::CXXTryStmt *stmt);// 访问 try-catch 块
          bool VisitCXXCatchStmt(clang::CXXCatchStmt *stmt);// 访问 catch 块
          bool VisitCXXThrowExpr(clang::CXXThrowExpr *stmt);// 访问 throw 表达式
          bool VisitCallExpr(clang::CallExpr *callExpr);// 访问 setjmp/longjmp 调用
          void set_check_func(clang::FunctionDecl *FD) {check_func = FD;}
          void set_func_flag(bool flag) {func_flag = flag;}
          void update_exception_info() {
              if (check_func != nullptr)
                  seq_info.update_exception_info(check_func, exception_info);
          }
      private:
          static ASTExceptionAnalyzer* analyzer;
          static std::mutex analyzerMutex;
          ASTExceptionAnalyzer(const ASTExceptionAnalyzer&) = delete;
          ASTExceptionAnalyzer& operator=(const ASTExceptionAnalyzer&) = delete;
      
          explicit ASTExceptionAnalyzer(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i)
              : _ctx(ctx), _rewriter(R), seq_info(seq_i) {}
          ExceptionHandlingInfo exception_info;  // 异常处理信息
          clang::FunctionDecl *check_func = nullptr;  // 当前检查的函数
          bool func_flag = false;  // 函数标志
      
          clang::ASTContext *_ctx;
          clang::Rewriter &_rewriter;
          SeqInfo &seq_info;
      
          int current_nesting_depth = 0;  // 当前异常嵌套深度
      };
      
      class ASTNestedBlockAnalyzer : public clang::RecursiveASTVisitor<ASTNestedBlockAnalyzer> {
        public:
            static ASTNestedBlockAnalyzer* getInstance(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i) {
                std::lock_guard<std::mutex> lock(analyzerMutex);
                if (analyzer == nullptr) {
                    analyzer = new ASTNestedBlockAnalyzer(ctx, R, seq_i);
                }
                if(analyzer->_ctx != ctx){
                  delete analyzer;
                  analyzer = new ASTNestedBlockAnalyzer(ctx, R, seq_i);
                } 
                return analyzer;
            }
    
            static void destroyInstance() {
                std::lock_guard<std::mutex> lock(analyzerMutex);
                if (analyzer != nullptr) {
                    delete analyzer;
                    analyzer = nullptr;
                }
            }
            void reset();
            void update_nestedblock_info(){if(check_func!=nullptr)  nested_block_info.max_nesting_depth = max_depth_in_current_func;seq_info.update_nestedblock_info(check_func, nested_block_info);}
            //void update_nestedblock_info() {nested_block_info.max_nesting_depth = current_nesting_depth;}
            
            void set_check_func(clang::FunctionDecl *FD) {check_func = FD;}
            void set_func_flag(bool flag) {func_flag = flag;}
            NestedBlockInfo getNestedBlockInfo() const {return nested_block_info;}
            bool VisitIfStmt(clang::IfStmt *stmt);
            bool VisitForStmt(clang::ForStmt *stmt);
            bool VisitWhileStmt(clang::WhileStmt *stmt) ;
        private:
            ASTNestedBlockAnalyzer(clang::ASTContext *ctx, clang::Rewriter &R, SeqInfo &seq_i)
                : _ctx(ctx), _rewriter(R), seq_info(seq_i), current_nesting_depth(0), func_flag(false) {}
            static ASTNestedBlockAnalyzer* analyzer;   
            static std::mutex analyzerMutex;           
    
            NestedBlockInfo nested_block_info;         
            clang::FunctionDecl *check_func = nullptr; 
            bool func_flag = false;                   
            bool trace_flag = false;
            clang::ASTContext *_ctx;  
            clang::Rewriter &_rewriter; 
            SeqInfo &seq_info; 
    
            int current_nesting_depth=1; // 当前嵌套深度
            int max_depth_in_current_func = 1;
          
        };
    
      
class SeqASTVisitor : public clang::RecursiveASTVisitor<SeqASTVisitor>
{
    private:
        clang::ASTContext *_ctx;
        clang::Preprocessor &_preprocessor;
        clang::Rewriter &_rewriter;
        SeqInfo &seq_info;
    public:
    explicit SeqASTVisitor(clang::ASTContext *ctx,
                          clang::Preprocessor &P,
                          clang::Rewriter &R,
                          SeqInfo &seq_i)
        : _ctx(ctx), _preprocessor{P}, _rewriter{R}, seq_info{ seq_i }
    {
    }
    ~SeqASTVisitor();

    // bool isInSystemHeader(const clang::SourceLocation &loc);
    bool MatchPoint(Point &p, clang::Stmt *stmt, std::string filePath);
    bool MatchPoint_Decl(Point &p, clang::Decl *decl, std::string filePath);
    clang::SourceRange get_sourcerange(clang::Stmt *stmt);
    clang::SourceRange get_decl_sourcerange(clang::Decl *stmt);
    bool VisitFunctionDecl(clang::FunctionDecl *func_decl);
    bool VisitVarDecl(clang::VarDecl *var_decl);
    void refrash_seq_info();
    void compute_seq();
    void compute_sensitive();
};
class SeqASTConsumer : public clang::ASTConsumer
{
  public:
    explicit SeqASTConsumer(clang::CompilerInstance &CI,
                            clang::Rewriter &R,
                            SeqInfo &seq_i)
        :  _visitor(&CI.getASTContext(),CI.getPreprocessor(),R, seq_i)
    {
    }

    virtual void HandleTranslationUnit(clang::ASTContext &ctx){
      std::cout<<"----------------start HandleTranslationUnit\n";
      _visitor.refrash_seq_info();
      _visitor.TraverseDecl(ctx.getTranslationUnitDecl());
      std::cout<<"----------------end HandleTranslationUnit\n";
    }

  private:
    SeqASTVisitor _visitor;
};

class SeqFrontendAction : public clang::ASTFrontendAction
{
  public:
    SeqFrontendAction(SeqInfo &seq_i) : seq_info{ seq_i }
    {}
    virtual std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance &compiler,
                          llvm::StringRef in_file)
    {
        // _SM = compiler.getSourceManager();
        _rewriter.setSourceMgr(compiler.getSourceManager(),
                               compiler.getLangOpts());
        return std::make_unique<SeqASTConsumer>(
            compiler, _rewriter, seq_info);
    }

  private:
    clang::Rewriter _rewriter;
    SeqInfo &seq_info;
};


class SeqFactory : public clang::tooling::FrontendActionFactory
{
  public:
    SeqFactory(SeqInfo &seq_i);// : seq_info{ seq_i } {
    //     std::cout<<"Bug type is "<<seq_i.get_bug_type()<<"\n";
    // }
    std::unique_ptr<clang::FrontendAction> create() override
    {
        return std::make_unique<SeqFrontendAction>(seq_info);
    }

  private:
    SeqInfo &seq_info;
};
}  // namespace sseq
#endif