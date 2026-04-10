#include "../include/tool.h"

namespace sseq{

    bool Tool::isIdentifierPart(char ch) {
        return std::isalpha(ch) || std::isdigit(ch) || (ch == '_');
    }
   // 获取 "classname::function" 格式的函数全名
    std::string Tool::get_qualified_method_name(const clang::FunctionDecl* methodDecl) {
        if (!methodDecl) {
            return "";
        }
        if(const clang::CXXMethodDecl *cxxmethod = clang::dyn_cast<clang::CXXMethodDecl>(methodDecl)){
            // 获取类名（通过方法所属的记录声明）
            const clang::CXXRecordDecl* recordDecl = cxxmethod->getParent();
            if (!recordDecl) {
                return cxxmethod->getNameAsString(); // 非成员函数
            }
            std::string className = recordDecl->getNameAsString();
            
            // 获取函数名
            std::string functionName = cxxmethod->getNameAsString();
            
            // 组合成 "classname::function" 格式
            return className + "::" + functionName;
        }else{
            return methodDecl->getNameAsString();
        }
        
    }
    std::string Tool::getFunctionSignature(clang::FunctionDecl* func_decl) {
        if (!func_decl) return "";

        clang::ASTContext& ctx = func_decl->getASTContext();
        clang::PrintingPolicy policy(ctx.getLangOpts());

        std::string signature;

        // 添加返回类型
        signature += func_decl->getReturnType().getAsString(policy);
        signature += " ";
        // 添加函数名
        signature += get_qualified_method_name(func_decl);
        // 添加参数列表
        signature += "(";
        bool first_param = true;
        for (clang::ParmVarDecl* param : func_decl->parameters()) {
            if (!first_param) {
                signature += ", ";
            }
            signature += param->getType().getAsString(policy);
            if (!param->getName().empty()) {
                signature += " " + param->getNameAsString();
            }
            first_param = false;
        }
        signature += ")";

        // 检查是否为const成员函数（兼容旧版本Clang）
        if (clang::CXXMethodDecl* method_decl = clang::dyn_cast<clang::CXXMethodDecl>(func_decl)) {
            if (method_decl->isConst()) {
                signature += " const";
            }
        }

        return signature;
    }
    int Tool::levenshteinDistance(const std::string &str1, const std::string &str2) {
        int m = str1.size();
        int n = str2.size();
        if (m < n) {
            return levenshteinDistance(str2, str1);
        }
        // 使用两个大小为 n+1 的数组交替存储动态规划表的行
        std::vector<int> prevRow(n + 1);
        std::vector<int> currRow(n + 1);
        for (int j = 0; j <= n; ++j) prevRow[j] = j;
    
        // 动态规划计算编辑距离
        for (int i = 1; i <= m; ++i) {
            currRow[0] = i;  // 初始化当前行的第一个元素
            for (int j = 1; j <= n; ++j) {
                if (str1[i - 1] == str2[j - 1]) {
                    currRow[j] = prevRow[j - 1];  // 字符相同，无需操作
                } else {
                    currRow[j] = 1 + std::min({prevRow[j],    // 删除操作
                                               currRow[j - 1], // 插入操作
                                               prevRow[j - 1]  // 替换操作
                                              });
                }
            }
            std::swap(prevRow, currRow);
        }
        return prevRow[n];
    }
    
    // 计算字符串相似度
    double Tool::stringSimilarity(const std::string &str1, const std::string &str2) {
        int distance = levenshteinDistance(str1, str2);
        int maxLength = std::max(str1.size(), str2.size());
        return 1.0 - static_cast<double>(distance) / maxLength;  // 相似度公式
    }
    
    
    
    std::string Tool::get_stmt_string(const clang::Stmt *S){
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
            std::cerr << "Error in get_stmt_string: " << e.what() << std::endl;
            return "<error>";
        } catch (...) {
            std::cerr << "Unknown error in get_stmt_string" << std::endl;
            return "<unknown error>";
        }
        // clang::LangOptions LO;
        // std::string buffer;
        // llvm::raw_string_ostream strout(buffer);
        // S->printPretty(strout, nullptr, clang::PrintingPolicy(LO));
        // return strout.str(); 
    }
    std::string Tool::get_decl_string(const clang::Decl *S){
        if(S==nullptr){
            return "0";
        }
        clang::LangOptions LO=S->getASTContext().getLangOpts();
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        S->print(strout, clang::PrintingPolicy(LO));
        std::string ret=strout.str();
        return ret;
    }
    std::string Tool::get_qualType_string(const clang::QualType &T){//这里改成了传递引用
        if(T.isNull()){
            return "0";
        }
        clang::LangOptions LO;
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        T.getLocalUnqualifiedType().getNonReferenceType().print(strout, clang::PrintingPolicy(LO));
        return strout.str();
    }

    //获取普通类型，也就是最前端的类型，屏蔽数组和指针,不处理typedef
    clang::QualType Tool::get_normal_type_decl(clang::QualType QT){     
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


    // 判断类型定义是否为函数指针类型
    bool Tool::isFunctionPointerTypedef(const clang::TypedefDecl *TD) {
        return TD->getUnderlyingType()->isFunctionPointerType();
    }

    bool Tool::isFunctionPointerCall(const clang::CallExpr* CE) {
        auto* callee = CE->getCallee();
        if(const clang::ImplicitCastExpr *imExpr=llvm::dyn_cast<clang::ImplicitCastExpr>(callee)){
            clang::CastKind castKind = imExpr->getCastKind();
            /* 未来可能需要的
                CK_LValueToRValue:将一个左值（lvalue）转换为右值（rvalue）。通常发生在将左值传递给需要右值的上下文中，例如在表达式中求值时。
                CK_NoOp:无操作转换。这表示没有实际的类型转换，仅仅是一个表达式的包装。
                CK_BaseToDerived:基类到派生类的转换，通常用于向上和向下转换的过程中，表示从一个基类类型到派生类类型的转换。
                CK_DerivedToBase:派生类到基类的转换，表示从派生类类型到基类类型的转换。
                CK_FunctionToPointer:函数到指针的转换，表示将函数名转换为指向该函数的指针。
                CK_PointerToBoolean:指针到布尔值的转换。将指针值转换为 true 或 false。
                CK_PointerToIntegral:指针到整数的转换，将指针值转换为整数类型。
                CK_IntegralToPointer:整数到指针的转换，将整数值转换为指针类型。
                CK_CPointerToVoid:将 void* 指针转换为某个类型的指针。常见于 void* 和其他类型指针之间的转换。
                CK_VoidToPointer:将 void 类型转换为指针类型。
                CK_PointerToPointer:指针到指针的转换，通常是类型转换指向的对象类型不同但都是指针。
                CK_UserDefinedConversion:用户定义的类型转换，例如在 C++ 中定义了一个自定义的转换运算符（如 operator T()）。
                CK_NullToPointer:nullptr 转换为指针类型。
                CK_NullToMemberPointer:nullptr 转换为成员指针。
                CK_BuiltinFnToFunctionPointer:内建函数到函数指针的转换。
            */
            if(const clang::DeclRefExpr *declRefExpr= llvm::dyn_cast<clang::DeclRefExpr>(imExpr->getSubExpr())){
                if(castKind == clang::CK_LValueToRValue){ // 函数指针
                    return true;
                }else if (castKind == clang::CK_FunctionToPointerDecay ){} //普通函数调用
            }
        }
        return false;
    }

    std::string Tool::get_loc_file(clang::ASTContext *_ctx, const clang::SourceLocation &loc)
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

    bool Tool::isInSystemHeader(clang::ASTContext *_ctx, const clang::FunctionDecl *FD){
        if(!FD || !_ctx) return true;
        auto &SM = _ctx->getSourceManager();
        llvm::StringRef filename_ref = SM.getFilename(FD->getLocation());
        if (filename_ref.empty())return true;
        std::string filename = filename_ref.str();
        if(filename.find("/usr/include/")!=std::string::npos) return true;
        if(filename.find("llvm-12.0.0.obj/")!=std::string::npos) return true;
        return false;
    }
    bool Tool::isInSystemHeader(clang::ASTContext *_ctx, const clang::SourceLocation &loc) {
        auto &SM = _ctx->getSourceManager();
        if(SM.getFilename(loc)=="") return true;
        std::string filename = get_loc_file(_ctx, loc);
        if(filename.find("/usr/include/")!=std::string::npos) return true;
        if(filename.find("llvm-12.0.0.obj/")!=std::string::npos) return true;
        return false;
    }
    clang::SourceRange Tool::get_decl_sourcerange(clang::ASTContext *_ctx, const clang::Decl *stmt){
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
    clang::SourceRange Tool::get_sourcerange(clang::ASTContext *_ctx, const clang::Stmt *stmt){
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
    bool Tool::isBranchStmt(const clang::Stmt *S){
        auto stmt_class = S->getStmtClass();
        if(stmt_class == clang::Stmt::IfStmtClass || stmt_class == clang::Stmt::SwitchStmtClass\
         || stmt_class == clang::Stmt::CaseStmtClass || stmt_class == clang::Stmt::DefaultStmtClass) 
            return true;
        return false;
    }
    int Tool::checkCond(clang::ASTContext *_ctx, const clang::Expr *E){//FIX ME： PDG判断cond的结果是否可能为0
        E = Tool::getUnqualifiedExpr(E);
        if (true ||E->isValueDependent()) {
            clang::Expr::EvalResult evalResult;
            //NULL展开后也是0
            if (E->EvaluateAsInt(evalResult, *_ctx)) {
                llvm::APSInt intValue = evalResult.Val.getInt();
                if (intValue.getSExtValue() == 0) {
                    return 0;
                }else {
                    return 1;
                }
            }
            llvm::APFloat floatValue(llvm::APFloat::IEEEdouble());
            if (E->EvaluateAsFloat(floatValue, *_ctx)) {
                if (floatValue.isZero()) {
                    // llvm::outs() << "The statement's condition is always false (floating point 0 case).\n";
                    return 0;
                }else {
                    // llvm::outs() << "The statement's condition is always true (floating point not 0 case).\n";
                    return 1;
                }
            }
        }
        return -1;
    }
    //返回IfStmt的条件状态 1：不确定是否执行 ，2：一定不执行的分支， 3：一定执行的分支
    int Tool::getBranchState(clang::ASTContext *_ctx,const clang::Stmt *S, bool isElse){
        if(const clang::IfStmt *IS = clang::dyn_cast<clang::IfStmt>(S)){
            int state = checkCond(_ctx, IS->getCond());
            if(state!=-1) {
                if(isElse) return (state==0) ? 3 : 2;
                return (state==0) ? 2 : 3;
            }
        }
        return 1;
    }
    bool Tool::isLoopStmt(const clang::Stmt *S){
        auto stmt_class = S->getStmtClass();
        if(stmt_class == clang::Stmt::DoStmtClass || stmt_class == clang::Stmt::ForStmtClass || stmt_class == clang::Stmt::WhileStmtClass) 
            return true;
        return false;
    }

    int Tool::isInclude(clang::ASTContext *_ctx, clang::Stmt *S, clang::Stmt *sub){
        //返回2证明在else
        if(S==nullptr) return 0;
        clang::SourceRange SR = get_sourcerange(_ctx, sub);
        if(clang::IfStmt *ifStmt = clang::dyn_cast<clang::IfStmt>(S)){
            clang::Stmt *sthen = ifStmt->getThen();
            if(sthen){
                clang::SourceRange SR_c = get_sourcerange(_ctx,sthen);
                if(SR_c.getBegin() <= SR.getBegin() && SR_c.getEnd() >= SR.getEnd()) return 1;
                if(ifStmt->hasElseStorage()){
                    clang::Stmt *selse = ifStmt->getElse();
                    SR_c = get_sourcerange(_ctx,selse);
                    if(SR_c.getBegin() <= SR.getBegin() && SR_c.getEnd() >= SR.getEnd()) return 2;
                }
            }
        }
        else  if(clang::SwitchStmt *switchStmt = clang::dyn_cast<clang::SwitchStmt>(S)){
                clang::Stmt *sbody = switchStmt->getBody();
                int ret=0;
                std::string stmtclass;
                for(auto ss : sbody->children()){
                    if(!ss) continue;
                    stmtclass = ss->getStmtClassName();
                    if(stmtclass=="CaseStmt"||stmtclass=="DefaultStmt"){
                        ret |= isInclude(_ctx, ss,sub);
                    }else{ // 套在case里的其他语句
                        clang::SourceRange SR_c = get_sourcerange(_ctx, ss);
                        if(SR_c.getBegin() <= SR.getBegin() && SR_c.getEnd() >= SR.getEnd()) return 1;
                    }
                    
                }
                return ret;
        }
        else if(clang::SwitchCase *caseStmt = clang::dyn_cast<clang::SwitchCase>(S)){
            clang::Stmt *sbody = caseStmt->getSubStmt();
            clang::SourceRange SR_c = get_sourcerange(_ctx,sbody);
            if(SR_c.getBegin() <= SR.getBegin() && SR_c.getEnd() >= SR.getEnd()) return 1;
        }
        else if(clang::ForStmt *forStmt = clang::dyn_cast<clang::ForStmt>(S)){
            clang::Stmt *sbody = forStmt->getBody();
            clang::SourceRange SR_c = get_sourcerange(_ctx,sbody);
            if(SR_c.getBegin() <= SR.getBegin() && SR_c.getEnd() >= SR.getEnd()) return 1;
        }
        else if(clang::WhileStmt *whileStmt = clang::dyn_cast<clang::WhileStmt>(S)){
            clang::Stmt *sbody = whileStmt->getBody();
            clang::SourceRange SR_c = get_sourcerange(_ctx,sbody);
            if(SR_c.getBegin() <= SR.getBegin() && SR_c.getEnd() >= SR.getEnd()) return 1;
        }
        else if(clang::DoStmt *doStmt = clang::dyn_cast<clang::DoStmt>(S)){
            clang::Stmt *sbody = doStmt->getBody();
            clang::SourceRange SR_c = get_sourcerange(_ctx,sbody);
            if(SR_c.getBegin() <= SR.getBegin() && SR_c.getEnd() >= SR.getEnd()) return 1;
        }
        return 0;
    }

    bool Tool::isRecusiveFunction(const clang::FunctionDecl *FD){
        if(!FD || !FD->hasBody()) return false;
        for(auto C : common::getCallExpr(FD)){
            if(C->getDirectCallee() == FD) return true;
        }
        return false;
    }
    const std::set<std::string> Tool::fileOperationFunctions = {
        "fopen", "open", "freopen", "fdopen"
    };
    const std::set<std::string> Tool::memoryAllocFunctions = {
        "malloc", "calloc", "realloc"
    };
    bool Tool::ismemoryAllocationCall(const clang::CallExpr *CE){
        if(!CE) return false;
        const clang::FunctionDecl *callee = CE->getDirectCallee();
        if(callee!=nullptr){
            std::string calleeName = callee->getNameAsString();
            std::cout<<"calleeName:"<<calleeName<<"\n";
            if (memoryAllocFunctions.find(calleeName) != memoryAllocFunctions.end()) {
                return true;
            }
        }
        return false;
    }
    bool Tool::isFunctionDepanceExtern(const clang::FunctionDecl *FD){
        if(!FD || !FD->hasBody()) return false;
        for(auto v : common::getVariables(FD)){
            if(whichVarStorage(v)>=2) return true;//静态变量、全局变量extern
        }
        
        for(auto C : common::getCallExpr(FD)){
            const clang::FunctionDecl *callee = C->getDirectCallee();
            if(callee!=nullptr){
                std::string calleeName = callee->getNameAsString();
                if (fileOperationFunctions.find(calleeName) != fileOperationFunctions.end()) {
                    return true;
                }
            }
        }
        return false;
    }
    const clang::Expr* Tool::getArrayBase(const clang::ArraySubscriptExpr *ASE){
        if(!ASE) return nullptr;
        const clang::Expr *base = ASE->getBase();
        while (base) {
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(base)) {
                base = castExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return base;
    }
    const clang::Expr* Tool::getMemberBase(const clang::MemberExpr *ME){
        if(!ME) return nullptr;
        const clang::Expr *base = ME->getBase();
        while (base) {
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(base)) {
                base = castExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return base;
    }
    const clang::VarDecl* Tool::getVD_REF(const clang::Expr *E){
        if(!E) return nullptr;
        if(const clang::DeclRefExpr *DRE = clang::dyn_cast<clang::DeclRefExpr>(E)){
            if (const clang::VarDecl* varDecl = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
                return varDecl; 
            }
        }
        return nullptr;
    }
    const clang::VarDecl* Tool::getCallee_FP(const clang::CallExpr *CE){
        if(!CE) return nullptr;
        auto* callee = CE->getCallee();
        if(const clang::ImplicitCastExpr *imExpr=llvm::dyn_cast<clang::ImplicitCastExpr>(callee)){
            if(const clang::VarDecl *VD = getVD_REF(imExpr->getSubExpr())){
                return VD;
            }
        }
        return nullptr;
    }
    const clang::Expr* Tool::getArrayIndex(const clang::ArraySubscriptExpr *ASE){
        if(!ASE) return nullptr;
        const clang::Expr *idx = ASE->getIdx();
        while (idx) {
            // 如果是括号表达式（ParenExpr），继续处理括号内的表达式
            if (const clang::ParenExpr* parenExpr = llvm::dyn_cast<clang::ParenExpr>(idx)) {
                idx = parenExpr->getSubExpr();
            }
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            else if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(idx)) {
                idx = castExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return idx;

    }

    const clang::Expr * Tool::getBinaryOperatorLHS(const clang::BinaryOperator *BO){
        if(!BO) return nullptr;
        return getUnqualifiedExpr(BO->getLHS());
        const clang::Expr *lhs = BO->getLHS();
        while (lhs) {
            // 如果是括号表达式（ParenExpr），继续处理括号内的表达式
            if (const clang::ParenExpr* parenExpr = llvm::dyn_cast<clang::ParenExpr>(lhs)) {
                lhs = parenExpr->getSubExpr();
            }
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            else if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(lhs)) {
                lhs = castExpr->getSubExpr();
            }
            // 如果是指针 *p = XX；（UnaryOperator），继续处理转换后的表达式
            else if (const clang::UnaryOperator* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(lhs)) {
                lhs = unaryOp->getSubExpr();
            }
            else if(const clang::CStyleCastExpr* CSExpr = llvm::dyn_cast<clang::CStyleCastExpr>(lhs)){
                lhs = CSExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return lhs;

    }

    const clang::Expr * Tool::getBinaryOperatorRHS(const clang::BinaryOperator *BO){
        if(!BO) return nullptr;
        return getUnqualifiedExpr(BO->getRHS());
        const clang::Expr *rhs = BO->getRHS();
        while (rhs) {
            // 如果是括号表达式（ParenExpr），继续处理括号内的表达式
            if (const clang::ParenExpr* parenExpr = llvm::dyn_cast<clang::ParenExpr>(rhs)) {
                rhs = parenExpr->getSubExpr();
            }
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            else if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(rhs)) {
                rhs = castExpr->getSubExpr();
            }
            // 如果是指针 *p = XX；（UnaryOperator），继续处理转换后的表达式
            else if (const clang::UnaryOperator* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(rhs)) {
                rhs = unaryOp->getSubExpr();
            }
            else if(const clang::CStyleCastExpr* CSExpr = llvm::dyn_cast<clang::CStyleCastExpr>(rhs)){
                rhs = CSExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return rhs;

    }

    const clang::Expr * Tool::getBinaryOperatorRHS_Cast(const clang::BinaryOperator *BO){//为了 a = (A*)malloc(sizeof(A)); 服务
        if(!BO) return nullptr;
        const clang::Expr *rhs = BO->getRHS();
        while (rhs) {
            // 如果是括号表达式（ParenExpr），继续处理括号内的表达式
            if (const clang::ParenExpr* parenExpr = llvm::dyn_cast<clang::ParenExpr>(rhs)) {
                rhs = parenExpr->getSubExpr();
            }
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            else if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(rhs)) {
                rhs = castExpr->getSubExpr();
            }
            // 如果是指针 *p = XX；（UnaryOperator），继续处理转换后的表达式
            else if (const clang::UnaryOperator* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(rhs)) {
                rhs = unaryOp->getSubExpr();
            }
            else {
                break;
            }
        }
        return rhs;

    }

    const clang::Expr * Tool::getUnqualifiedExpr(const clang::Expr *E){
        if(!E) return nullptr;
        while (E) {
            // 如果是括号表达式（ParenExpr），继续处理括号内的表达式
            if (const clang::ParenExpr* parenExpr = llvm::dyn_cast<clang::ParenExpr>(E)) {
                E = parenExpr->getSubExpr();
            }
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            else if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(E)) {
                E = castExpr->getSubExpr();
            }
            // 如果是指针 *p = XX；（UnaryOperator），继续处理转换后的表达式
            else if (const clang::UnaryOperator* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(E)) {
                E = unaryOp->getSubExpr();
            }
            else if(const clang::CStyleCastExpr* CSExpr = llvm::dyn_cast<clang::CStyleCastExpr>(E)){
                E = CSExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return E;
    }
    const clang::Expr * Tool::getVarDeclInit_Cast(const clang::VarDecl *VD){
        if(!VD) return nullptr;
        if(!VD->hasInit()) return nullptr;
        const clang::Expr *init = VD->getInit();
        while (init) {
            // 如果是括号表达式（ParenExpr），继续处理括号内的表达式
            if (const clang::ParenExpr* parenExpr = llvm::dyn_cast<clang::ParenExpr>(init)) {
                init = parenExpr->getSubExpr();
            }
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            else if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(init)) {
                init = castExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return init;

    }
    const clang::Expr * Tool::getVarDeclInit(const clang::VarDecl *VD){
        if(!VD) return nullptr;
        if(!VD->hasInit()) return nullptr;
        const clang::Expr *init = VD->getInit();
        while (init) {
            // 如果是括号表达式（ParenExpr），继续处理括号内的表达式
            if (const clang::ParenExpr* parenExpr = llvm::dyn_cast<clang::ParenExpr>(init)) {
                init = parenExpr->getSubExpr();
            }
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            else if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(init)) {
                init = castExpr->getSubExpr();
            }
            // // 如果是指针 *p = XX；（UnaryOperator），继续处理转换后的表达式
            // else if (const clang::UnaryOperator* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(init)) {
            //     init = unaryOp->getSubExpr();
            // }
            else if(const clang::CStyleCastExpr* CSExpr = llvm::dyn_cast<clang::CStyleCastExpr>(init)){
                init = CSExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return init;

    }
    const clang::Expr * Tool::getReturnValue(const clang::ReturnStmt *RS){
        if(!RS) return nullptr;
        const clang::Expr *ret = RS->getRetValue();
        while (ret) {
            // 如果是括号表达式（ParenExpr），继续处理括号内的表达式
            if (const clang::ParenExpr* parenExpr = llvm::dyn_cast<clang::ParenExpr>(ret)) {
                ret = parenExpr->getSubExpr();
            }
            // 如果是隐式类型转换表达式（ImplicitCastExpr），继续处理转换后的表达式
            else if (const clang::ImplicitCastExpr* castExpr = llvm::dyn_cast<clang::ImplicitCastExpr>(ret)) {
                ret = castExpr->getSubExpr();
            }
            else {
                break;
            }
        }
        return ret;
    }
    bool Tool::isMacroExpansion(const clang::Stmt *S){
        clang::SourceRange SR = S->getSourceRange();
        clang::SourceLocation loc = SR.getBegin();
        clang::SourceLocation end = SR.getEnd();
        if(loc.isMacroID() && end.isMacroID()) return true;
        return false;
    }
    int Tool::whichVarStorage(const clang::VarDecl *VD){
        // hasLocalStorage 非静态局部变量
        // isStaticLocal 静态局部变量
        // hasExternalStorage 跨文件的变量引用：extern 或 private_extern 存储类型
        // hasGlobalStorage 全局变量、静态全局变量以及具有 extern 存储类型的变量
        if(VD->hasLocalStorage()) return 1;
        if(VD->isStaticLocal()) return 2;
        if(VD->hasExternalStorage()) return 3;
        if(VD->hasGlobalStorage()) return 4;
        return 0;
    }
    bool Tool::isGlobalUsed(const clang::Expr *expr){
        if(const clang::VarDecl *VD = Tool::getVD_REF(expr)){
            // std::cout<<"Panding--global?"<<get_decl_string(VD)<<"  "<<Tool::whichVarStorage(VD)<<"\n";
            if(Tool::whichVarStorage(VD) == 4){// 全局变量、静态全局变量以及具有 extern 存储类型的变量
                return true;
            }
        }
        return false;
    }
} //namespace sseq
