
#include "../include/action.h"
namespace sseq
{
    
    namespace fs = std::filesystem;
    // 路径标准化函数
    std::string normalize_path(const std::string& input_path) {
        // 1. 构造 path 对象（自动处理分隔符，兼容 \ 和 /）
        fs::path path_obj(input_path);
        // 2. 标准化路径（处理 ./、../、连续分隔符）
        fs::path normalized_path = path_obj.lexically_normal();
        // 3. 转为字符串返回（输出分隔符自动适配当前系统，也可强制用 /）
        return normalized_path.string();  // 或 u8string()（UTF-8）
    }

    std::string make_absolute_path(const std::string& relative_path) {
        try {
            std::filesystem::path abs_path = std::filesystem::absolute(relative_path);
            return abs_path.string();
        } catch (const std::filesystem::filesystem_error& e) {
            llvm::errs() << "错误：无法转换路径: " << e.what() << "\n";
            return relative_path;
        }
    }

    
    bool ASTPointerMatch::VisitStmt(clang::Stmt *S){
        if(!S) return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceRange SR = Tool::get_sourcerange(_ctx,S);
        int line_start = SM.getPresumedLineNumber(SR.getBegin());
        int col_start  = SM.getPresumedColumnNumber(SR.getBegin());
        int line_end = SM.getPresumedLineNumber(SR.getEnd());
        int col_end  = SM.getPresumedColumnNumber(SR.getEnd());
        // 匹配行列开头一致，且行差值最小，列差值最大的
        if(line_start == point->line && col_start == point->col){
            if(line_end - line_start < point->line_diff){
                point->line_diff = line_end - line_start;
                point->col_diff = col_end - col_start;
                point->node = S;
            }

            else if(line_end - line_start == point->line_diff){
                // std::cout<<"point->col_diff = "<<point->col_diff<<"\n";
                if(col_end - col_start > point->col_diff){//删掉了等号，否则宏的情况出大事
                    point->col_diff = col_end - col_start ;
                    // std::cout<<"update p-col:"<<Tool::get_stmt_string(S)<<"\n";
                    point->node = S;
                }
            }
        }
        /* point->node是否包含在S-body里, 记录S备用 */
        if(!point->isInBranch && Tool::isBranchStmt(S)){
            int isInclude = Tool::isInclude(_ctx, S, point->node);
            if(isInclude!=0){
                bool isElse = (isInclude == 2);
                point->isInBranch = Tool::getBranchState(_ctx,S,isElse);
                point->branchStmt = S;
            }
        }
        if(!point->isInLoop && Tool::isLoopStmt(S) && Tool::isInclude(_ctx, S, point->node)){
            point->isInLoop = 1;
            point->loopStmt = S;
        }
        return true;
    }

    /* ASTTypeChecker */
    ASTTypeChecker* ASTTypeChecker::checker = nullptr;
    std::mutex ASTTypeChecker::typeCheckerMutex;

    //type2 > type1
    bool ASTTypeChecker::isTypesCompatible(clang::QualType type1, clang::QualType type2) {
            // 首先检查Clang默认的类型兼容性判断
            if (_ctx->typesAreCompatible(type1, type2)) {
                return true;
            }
        
            // 处理整数类型的情况，检查是否存在向下的隐式转换
            if (type1->isIntegerType() && type2->isIntegerType()) {
                
                uint64_t size1 = _ctx->getTypeSize(type1);
                uint64_t size2 = _ctx->getTypeSize(type2);
                if (size1 <= size2) {
                    return true;
                    std::cout<<"size-1 < size_2:"<<size1<<" < "<<size2<<"\n";
                }
                // const clang::BuiltinType* builtinType1 = llvm::dyn_cast<clang::BuiltinType>(type1.getTypePtr());
                // const clang::BuiltinType* builtinType2 = llvm::dyn_cast<clang::BuiltinType>(type2.getTypePtr());
                // if (builtinType1 && builtinType2) {
                //     uint64_t size1 = _ctx->getTypeSize(builtinType1);
                //     uint64_t size2 = _ctx->getTypeSize(builtinType2);
                //     if (size1 <= size2) {
                //         return true;
                //     }
                // }
            }
        
            return false;
    }
    bool ASTTypeChecker::VisitDeclStmt(clang::DeclStmt *DS){
        if(!DS) return true;
        auto &SM = _ctx->getSourceManager();
        for (auto decl : DS->decls()) {
            if(llvm::isa<clang::VarDecl>(decl)){ // int x;
                clang::VarDecl *VD=llvm::dyn_cast<clang::VarDecl>(decl);
                clang::QualType expectedType = VD->getType().getCanonicalType();//.getUnqualifiedType().getNonReferenceType();
                checkType(expectedType);// 变量定义类型
                if (const clang::Expr *init = Tool::getVarDeclInit(VD))
                {
                    clang::QualType initType = init->getType().getCanonicalType();//.getUnqualifiedType().getNonReferenceType();
                    type_info.BO_types_info |= (expectedType.getTypePtr()->isSignedIntegerType() == initType.getTypePtr()->isSignedIntegerType());//有符号数、无符号数
                    type_info.BO_types_info |= (expectedType == initType) <<1; //直接比较类型
                    type_info.BO_types_info |= (isTypesCompatible(initType,expectedType))<<2;
                }
                
            }
        }
        return true;
    }
    bool ASTTypeChecker::VisitBinaryOperator(clang::BinaryOperator *BO){// 比较运算符两端的变量类型
        if(func_flag) return true;
        const clang::Expr *lhs = Tool::getBinaryOperatorLHS(BO);
        const clang::Expr *rhs = Tool::getBinaryOperatorRHS(BO);
        clang::QualType lhsType = lhs->getType().getCanonicalType();
        clang::QualType rhsType = rhs->getType().getCanonicalType();

        // std::cout<<"handle BinaryOperator :"<<Tool::get_stmt_string(BO)<<" ->";
        // std::cout<<Tool::get_qualType_string(lhsType) <<" ==? "<<Tool::get_qualType_string(rhsType)<<"  compatible:"<<(_ctx->typesAreCompatible(lhsType, rhsType))\
        // <<"  equal:"<<(lhsType == rhsType)<< " sign_match:"<<(lhsType.getTypePtr()->isSignedIntegerType() == rhsType.getTypePtr()->isSignedIntegerType())<<"\n";

        type_info.BO_types_info |= (!(lhsType.getTypePtr()->isSignedIntegerType() == rhsType.getTypePtr()->isSignedIntegerType())) ;//有符号数、无符号数
        type_info.BO_types_info |= (!(lhsType == rhsType))<<1; //直接比较类型
        // type_info.BO_types_info |= (!(_ctx->typesAreCompatible(lhsType, rhsType)))<<2;// 类型兼容性 
        type_info.BO_types_info |= (isTypesCompatible(rhsType,lhsType))<<2;
        return true;
    } 
    bool ASTTypeChecker::VisitReturnStmt(clang::ReturnStmt *RS){// 比较函数返回类型
        if(check_func==nullptr) return true;//不用处理
        if(!func_flag) return true;
        //规范类型 // 去除修饰符 // 去除引用 
        clang::QualType expectedType = check_func->getReturnType().getCanonicalType().getUnqualifiedType().getNonReferenceType();//函数签名的规范类型
        // isVoidType 
        const clang::Expr *RetV = Tool::getReturnValue(RS);// 返回的表达式
        if(RetV == nullptr) return true;
        clang::QualType retType = RetV->getType().getCanonicalType().getUnqualifiedType().getNonReferenceType();//返回表达式规范类型getType
        
        type_info.return_types_info |= (!(expectedType.getTypePtr()->isSignedIntegerType() == retType.getTypePtr()->isSignedIntegerType()));//有符号数、无符号数 
        type_info.return_types_info |= (!(expectedType == retType))<<1; //直接比较类型
        // type_info.return_types_info |= (!(_ctx->typesAreCompatible(expectedType, retType)))<<2;// 类型兼容性
        type_info.BO_types_info |= (isTypesCompatible(retType,expectedType))<<2;
        // std::cout<<"handle return stmt :"<<Tool::get_stmt_string(RS)<<" ->";
        // std::cout<<Tool::get_qualType_string(expectedType) <<" ==? "<<Tool::get_qualType_string(retType)<<"  compatible:"<<(_ctx->typesAreCompatible(retType, expectedType))\
        // <<"  equal:"<<(expectedType == retType)<< " sign_match:"<<(expectedType.getTypePtr()->isSignedIntegerType() == retType.getTypePtr()->isSignedIntegerType())<<"\n";
        return true;
    }  
    // FIX ME: 对ExplicitCastExpr处理
    bool ASTTypeChecker::VisitCStyleCastExpr(clang::CStyleCastExpr *CSC){ // 使用强制类型转换
        if(func_flag) return true; 
        // std::cout<<"used CStyleCast! "<<Tool::get_stmt_string(CSC)<<" from "<<Tool::get_qualType_string(CSC->getSubExpr()->getType())<<" to "<<Tool::get_qualType_string(CSC->getTypeAsWritten())<<"\n";
        type_info.used_CScast = true;
        return true;
    }
    bool ASTTypeChecker::isMultiDimensionalArray(const clang::QualType& QT) {
        // 检查是否为数组类型
        if (QT.getTypePtr()->isArrayType()) {
            clang::QualType elementType = QT->getAsArrayTypeUnsafe()->getElementType();
            return elementType.getTypePtr()->isArrayType();
        }
        return false;
    }
    bool ASTTypeChecker::isSecondLevelPointer(const clang::QualType& QT) {
        const clang::Type* typePtr = QT.getTypePtr();
        if (typePtr->isPointerType()) {
            clang::QualType pointeeType = QT->getPointeeType();
            return pointeeType.getTypePtr()->isPointerType();
        }
    
        return false;
    }
    void ASTTypeChecker::checkRecord(const clang::RecordDecl *RD, int isTpdef){
        if(RD == nullptr) return;
        if(RD->getNameAsString()==""){
            //typedef匿名
            type_info.structure_info |= 2<<isTpdef; // 2 or 4
        }
        if(RD->isUnion()){//联合体
            type_info.structure_info |= 8;
        }else {//结构体
            //DO something
        }
        
        for(auto d:RD->decls()){
            //在结构体内定义的结构体和联合体
            if(const clang::RecordDecl* rd = clang::dyn_cast<clang::RecordDecl>(d)){
                type_info.structure_info |= 1;
            }
            else if(const clang::EnumDecl* ed = clang::dyn_cast<clang::EnumDecl>(d)){
                // type_info.structure_info |= 1;
                //Enum
            }
            else if(const clang::FieldDecl* fd = clang::dyn_cast<clang::FieldDecl>(d)){
                //field
                if(fd->isBitField()){
                    type_info.used_bitfield |=1;
                    std::cout<<"define bitfield:"<<fd->getBitWidthValue(*_ctx)<<" by "<<Tool::get_decl_string(fd)<<"\n";
                    if(fd->isUnnamedBitfield()){//填充位
                        type_info.used_bitfield |=2;
                    } 
                }
            }
        }
        
        return;
    }
    void ASTTypeChecker::checkType(const clang::QualType &QT){
        if(isMultiDimensionalArray(QT)){
            //多维数组
            type_info.used_MD_Array |= 1;
        }
        else if(isSecondLevelPointer(QT)){
            //二级指针
            type_info.used_MD_Array |= 2;
        }
        clang::QualType varType = Tool::get_normal_type_decl(QT); //获取最前端的类型，屏蔽数组和指针,不处理typedef
        if(const clang::TypedefType *TT = varType->getAs<clang::TypedefType>()) {//typedef
            clang::TypedefDecl *Typedef = clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
            //检查是不是typedef定义的匿名结构体
            if(Tool::isInSystemHeader(_ctx,Typedef->getLocation())) return;
            const clang::TypeSourceInfo *TInfo = Typedef->getTypeSourceInfo();
            if(!TInfo) return;
            clang::QualType QT2 = TInfo->getType();
            const clang::Type *T = QT2.getTypePtrOrNull();
            if(!T) return;

            //check 结构体
            if(const clang::RecordType *RT = T->getAs<clang::RecordType>()){
                const clang::RecordDecl *RD = RT->getDecl();
                checkRecord(RD,1);
            }
        }
        else if(const clang::RecordType *recordType = varType->getAsStructureType()){
            const clang::RecordDecl *RD = recordType->getDecl();
            std::string recordName = RD->getNameAsString();
            checkRecord(RD);
        }
        else if(const clang::RecordType *recordType = varType->getAsUnionType()){
            // const clang::RecordDecl *recordDecl = recordType->getDecl();
            // std::string recordName = recordDecl->getNameAsString();
            clang::RecordDecl * RD = recordType->getDecl();
            auto SR = Tool::get_decl_sourcerange(_ctx,RD);
            if(RD->getSourceRange().isInvalid()|| Tool::isInSystemHeader(_ctx, SR.getBegin())) return;
            checkRecord(RD);
            
        }else if(const clang::EnumType *EType = varType->getAs<clang::EnumType>()){
            const clang::EnumDecl *EDecl = EType->getDecl();
             //Enum
        }
    }
    bool ASTTypeChecker::VisitDeclRefExpr(clang::DeclRefExpr *DRE){// 使用的类型场景
        if (const clang::VarDecl *var_decl = clang::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
            clang::QualType VType= var_decl->getType();
            checkType(VType);
        }
        return true;
    }
    
    bool ASTTypeChecker::VisitMemberExpr(clang::MemberExpr *ME){//A.b  A->b(A ImplicitCastExpr)
        const clang::VarDecl* VD = nullptr;
        const clang::Expr *base = Tool::getMemberBase(ME);
        if(!base) return true;
        VD = Tool::getVD_REF(base);
        if(!VD) return true;
        clang::QualType VType= VD->getType();
        checkType(VType);
        //获取Field
        if(clang::FieldDecl* FD = clang::dyn_cast<clang::FieldDecl>(ME->getMemberDecl())){
            clang::QualType FType= FD->getType();
            checkType(FType);
        }
        return true;
    }
    /* ASTMacroChecker */
    ASTMacroChecker* ASTMacroChecker::checker = nullptr;
    std::mutex ASTMacroChecker::macroCheckerMutex;
    int ASTMacroChecker::isMacroExpansion(const clang::Stmt *S){
        auto SR = Tool::get_sourcerange(_ctx,S);
        if(!SR.isValid()) return -1;
        if(Tool::isInSystemHeader(_ctx, SR.getBegin()))
            return -1;
        std::string str = _rewriter.getRewrittenText(SR);
        if(!seq_info.is_new_macro(str)){
            return 0;
        }
        if(Tool::isMacroExpansion(S)){
            std::cout<<"new macro -> "<<str<<"\n";
            seq_info.add_macro(std::move(str),1);//右值引用传递
            return 1;
        }
        return -1;
    }

    bool ASTMacroChecker::VisitExpr(clang::Expr *E){
        if(isMacroExpansion(E)!=-1){
            // std::cout<<Tool::get_stmt_string(E)<<"=====IS A NEW Macro Expansion=====\n";
        }
        return true;
    }

    ASTSpecialChecker* ASTSpecialChecker::checker = nullptr;
    std::mutex ASTSpecialChecker::specialCheckerMutex;
    int ASTSpecialChecker::isMacroExpansion(const clang::Stmt *S){
        if(S==nullptr) return -1;
        auto SR = Tool::get_sourcerange(_ctx,S);
        if(!SR.isValid()) return -1;
        std::string str = _rewriter.getRewrittenText(SR);
        if(!seq_info.is_new_macro(str)){
            return 0;
        }
        if(Tool::isMacroExpansion(S)){
            std::cout<<"new macro -> "<<str<<"\n";
            seq_info.add_macro(std::move(str),1);
            return 1;
        }
        return -1;
    }
    
    //和特殊值运算（实现了宏的）
    bool ASTSpecialChecker::VisitDeclStmt(clang::DeclStmt *DS){
        auto &SM = _ctx->getSourceManager();
        for (auto decl : DS->decls()) {
            if(llvm::isa<clang::VarDecl>(decl)){ // int x;
                clang::VarDecl *VD=llvm::dyn_cast<clang::VarDecl>(decl);
                //UnaryOperator取地址这种啥时候提取子表达式呢
                if (const clang::Expr *init = Tool::getVarDeclInit(VD))
                {
                    if(func_flag){
                        seq_info.add_varible_assign_scen(VD,4);//声明同时初始化 |4
                        return true; // 以下不在“全部函数体”处理
                    }
                    int _index = isMacroExpansion(init);
                    if( _index !=-1 ){
                        auto SR = Tool::get_sourcerange(_ctx,init);
                        std::string str = _rewriter.getRewrittenText(SR);
                        seq_info.add_macro(std::move(str),2);
                        special_info.compute_values |= 1;
                        // std::cout<<Tool::get_decl_string(VD)<<"=====VD init Macro"<<_index<<"=====\n";
                    }else{
                        // 特殊字面量暂不考虑实现
                        // if (llvm::isa<clang::IntegerLiteral>(init))
                        // {
                        //     if(seq_info.add_integerLiteral())
                        // }
                        
                    }
                    if(Tool::isGlobalUsed(init)){
                        special_info.compute_values |= 2;
                    }
                    
                }else{
                    //仅声明
                    if(func_flag){
                        seq_info.add_varible_assign_scen(VD,8);//变量声明时没有初始化 - 8
                        return true; // 以下不在“全部函数体”处理
                    }
                }
                
            }
        }
        return true;
    }
    bool ASTSpecialChecker::VisitBinaryOperator(clang::BinaryOperator *BO){
        if(!BO) return true;
        auto opcode = BO->getOpcode();
        const clang::Expr *lhs = Tool::getBinaryOperatorLHS(BO);
        if(func_flag) {
            if (opcode == clang::BinaryOperatorKind::BO_Assign) {
                //lhs是个变量
                if(const clang::VarDecl *VD = Tool::getVD_REF(lhs)){
                    seq_info.add_varible_assign_scen(VD,1);//普通赋值 |1
                    //判断是不是在条件内 
                    if(flag && branch_checker->check(BO)){
                        branch_checker->set_ret(false);
                        seq_info.add_varible_assign_scen(VD,16);// 条件内赋值 |16
                    }
                }
            }
            return true;
        }
        int _index = isMacroExpansion(lhs);
        if( _index != -1 ){
            auto SR = Tool::get_sourcerange(_ctx,lhs);
            std::string str = _rewriter.getRewrittenText(SR);
            seq_info.add_macro(std::move(str),2);
            special_info.compute_values |= 1;
            // std::cout<<Tool::get_stmt_string(lhs)<<"=====LHS Macro"<<_index<<"=====\n";
        }
        const clang::Expr *rhs = Tool::getBinaryOperatorRHS(BO);
        
        _index = isMacroExpansion(rhs);
        if( _index != -1 ){
            auto SR = Tool::get_sourcerange(_ctx,rhs);
            std::string str = _rewriter.getRewrittenText(SR);
            seq_info.add_macro(std::move(str),2);
            special_info.compute_values |= 1;
            // std::cout<<Tool::get_stmt_string(rhs)<<"=====RHS Macro"<<_index<<"=====\n";
        }
        
        if(Tool::isGlobalUsed(lhs) || Tool::isGlobalUsed(rhs)){
            special_info.compute_values |= 2;
        }
        return true;
    }

    bool ASTSpecialChecker::VisitUnaryOperator(clang::UnaryOperator *UO){
        if(!UO->isIncrementOp() && !UO->isDecrementOp()) return true;
        // a++ a-- ++a --a
        if(func_flag) {
            if(clang::DeclRefExpr *DRE = clang::dyn_cast<clang::DeclRefExpr>(UO->getSubExpr())){
                if (clang::VarDecl *VD = clang::dyn_cast<clang::VarDecl>(DRE->getDecl())){
                    seq_info.add_varible_assign_scen(VD,2);//自增自减 |2
                    if(flag && branch_checker->check(UO)){
                        branch_checker->set_ret(false);
                        seq_info.add_varible_assign_scen(VD,16);// 条件内赋值 |16
                    }
                }
            }
            return true;
        }
        
        return true;
    }

    bool ASTSpecialChecker::VisitIndirectGotoStmt(clang::IndirectGotoStmt *IGS){
        clang::Expr * gotoTatget = IGS->getTarget();
        clang::LabelDecl *gotoLabel = IGS->getConstantTarget();
        if(gotoLabel == nullptr){
            //表示不存在一个固定目标
            special_info.compute_goto_label |= 1; // goto
        }else{
            //
            special_info.compute_goto_label |= 1<<1; 
        }
        return true;
    }
    // bool ASTSpecialChecker::VisitGotoStmt(clang::GotoStmt *GS){
    //     clang::LabelDecl *gotoLabel = GS->getLabel();
    //     return true;
    // }

    bool ASTSpecialChecker::VisitCaseStmt(clang::CaseStmt *CS){
        clang::Expr *lhs = CS->getLHS();
        // if(lhs!=nullptr){
        //     int vars = common::getVariables(lhs).size();
        //     clang::Type *lhs_type = lhs->getType().getTypePtr();
        //     if (clang::isa<clang::EnumType>(lhs_type)) {
        //         //使用枚举
        //     }
        // }
        clang::Expr *rhs = CS->getRHS();// 标准C语言只有单操作数，非标准扩展才有范围case需要用到RHS
        if(rhs!=nullptr){
            special_info.compute_case |= 1 ;//非标准扩展
        }
        return true;
    }
    bool ASTSpecialChecker::VisitSwitchStmt(clang::SwitchStmt *SS){
        clang::Expr *cond = SS->getCond();
        clang::VarDecl *condVar = SS->getConditionVariable();//只和一个变量相关的话会返回，否则nullptr
        try{
            if(cond!=nullptr){
                int vars = common::getVariables(cond).size(); // 条件中涉及的变量数量
                if(vars>1){
                    special_info.compute_switch |= 1;
                }
            }
        }catch (...) {
            // 捕获所有类型的异常
            std::cerr << "An exception was caught in VisitSwitchStmt." << std::endl;
        }
        
        return true;
    }
    ASTControlFChecker* ASTControlFChecker::checker = nullptr;
    std::mutex ASTControlFChecker::controlFCheckerMutex;
    bool ASTControlFChecker::VisitSwitchStmt(clang::SwitchStmt *SS){
        const clang::SwitchCase *switchCaseList = SS->getSwitchCaseList();
        bool has_default = false;
        while (switchCaseList) {
            if (const clang::DefaultStmt *DS=llvm::dyn_cast<clang::DefaultStmt>(switchCaseList)) {
                has_default = true;
                break;
            }
            switchCaseList = switchCaseList->getNextSwitchCase();
        }
        if(!has_default) control_info.switch_default |= 1;
        return true;        
    }
    
    bool ASTControlFChecker::VisitIfStmt(clang::IfStmt *IS){
        //获取cond
        clang::Expr *cond = IS->getCond();
        if(!cond) return true;
        if (Tool::checkCond(_ctx, cond)==0) {
            if(IS->getElse()){//2:if(false){} else{}
                control_info.dead_code |= 2;
            }else{
                control_info.dead_code |= 1;//1:if(false){} 
            }
        }else if (Tool::checkCond(_ctx, cond)==1) {
            if(IS->getElse()){//4:if(true){} else{deadcode}
                control_info.dead_code |= 4;
            }
        }
        return true;   
    }
    
    bool ASTControlFChecker::VisitForStmt(clang::ForStmt *FS){
        clang::Expr *cond = FS->getCond();
        if(!cond) return true;
        if (Tool::checkCond(_ctx, cond)==0) {
            control_info.dead_code |= 8;
        }
        else if (Tool::checkCond(_ctx, cond)==1) {
            control_info.loop_state |= 1;
        }
        return true;                
    }
    bool ASTControlFChecker::VisitWhileStmt(clang::WhileStmt *WS){
        clang::Expr *cond = WS->getCond();
        if(!cond) return true;
        if (Tool::checkCond(_ctx, cond)==0) {
            control_info.dead_code |= 8;
        }
        else if (Tool::checkCond(_ctx, cond)==1) {
            control_info.loop_state |= 1;
        }
        return true;       
    }
    bool ASTControlFChecker::VisitDoStmt(clang::DoStmt *DS){
        clang::Expr *cond = DS->getCond();
        if(!cond) return true;
        if (Tool::checkCond(_ctx, cond)==0) {
            control_info.dead_code |= 8;
        }
        else if (Tool::checkCond(_ctx, cond)==1) {
            control_info.loop_state |= 1;
        }
        return true;                
    }


    ASTFieldChecker* ASTFieldChecker::checker = nullptr;
    std::mutex ASTFieldChecker::fieldCheckerMutex;
    bool ASTFieldChecker::VisitDeclStmt(clang::DeclStmt *DS){
        auto &SM = _ctx->getSourceManager();
        for (auto decl : DS->decls()) {
            if(llvm::isa<clang::VarDecl>(decl)){ // int x;
                clang::VarDecl *VD=llvm::dyn_cast<clang::VarDecl>(decl);
                
                clang::QualType QT = VD->getType().getLocalUnqualifiedType().getNonReferenceType();
                if(QT.getTypePtr()->isArrayType()){// check 是否数组
                    std::cout<<"define Array!"<<Tool::get_decl_string(VD)<<"\n";
                    // seq_info.add_used_array(VD,-1);
                }
                if (QT.getTypePtr()->isRecordType()) {// A a = XX;
                    std::cout<<Tool::get_stmt_string(DS) << " define Record!"<<Tool::get_decl_string(VD)<<"\n";
                    if (const clang::RecordType* recordType = QT->getAsStructureType()){
                        const clang::RecordDecl* recordDecl = recordType->getDecl();
                    }
                    else if (const clang::RecordType* recordType = QT->getAsUnionType()) {
                        const clang::RecordDecl* recordDecl = recordType->getDecl();
                        // std::cout<<"define Union!"<<Tool::get_decl_string(VD)<<"\n";
                    }

                }else if(QT.getTypePtr()->isEnumeralType()){ //枚举类型

                }

                /* 
                    形如 A* a = (A*)malloc(sizeof(A));
                */
               
                clang::QualType varType = Tool::get_normal_type_decl(QT);// A
                if (const clang::Expr *init = Tool::getVarDeclInit_Cast(VD)){
                    if(const clang::CStyleCastExpr *CSC = clang::dyn_cast<clang::CStyleCastExpr>(init)){ //(A*)malloc(sizeof(A))
                        if(varType.getTypePtr()->isRecordType() && allocationChecker(CSC,varType)){
                            const clang::RecordDecl* recordDecl = nullptr;
                            if (const clang::RecordType* recordType = varType->getAsStructureType()){
                                recordDecl = recordType->getDecl();
                            }else if (const clang::RecordType* recordType = varType->getAsUnionType()) {
                                recordDecl = recordType->getDecl();
                            }
                            seq_info.add_record_vars(recordDecl,VD);
                        }
                    }
                }
                
            }
        }
        return true;
    }
    bool ASTFieldChecker::VisitMemberExpr(clang::MemberExpr *ME){//A.b  A->b(A ImplicitCastExpr)
        //获取Field
        if(clang::FieldDecl* FD = clang::dyn_cast<clang::FieldDecl>(ME->getMemberDecl())){
            if(clang::RecordDecl * RD = clang::dyn_cast<clang::RecordDecl>(FD->getParent())){
                seq_info.add_used_record(RD,FD);
                //获取base
                const clang::VarDecl* VD = nullptr;
                const clang::Expr *base = Tool::getMemberBase(ME);
                if(!base) return true;
                VD = Tool::getVD_REF(base);
                
                if(!VD){
                    // std::cout<< "ME's Base not a Varible\n";
                    return true;
                }
                seq_info.add_record_vars(RD,VD);//VD包括指针，如A *sb = (A *)malloc(sizeof(A));
            }
        }
        return true;
    }
    bool ASTFieldChecker::VisitArraySubscriptExpr(clang::ArraySubscriptExpr *ASE){
        const clang::VarDecl* VD = nullptr;
        const clang::Expr *base = Tool::getArrayBase(ASE);
        if(!base) return true;
        VD = Tool::getVD_REF(base);
        if(!VD){
            // std::cout<< "ASE's Base not a Varible\n";
            return true;
        }
        const clang::Expr *idx = Tool::getArrayIndex(ASE);
        if (const clang::IntegerLiteral* intLit = clang::dyn_cast<clang::IntegerLiteral>(idx)) {
            llvm::APInt value = intLit->getValue();
            int64_t intValue = value.getSExtValue();
            seq_info.add_used_array(VD,intValue);
            // std::cout<< "ASE's index(L) = "<<intValue<<"\n";
        }else if (const clang::DeclRefExpr* declRefExpr = clang::dyn_cast<clang::DeclRefExpr>(idx)) {
            // clang::ValueDecl* valueDecl = declRefExpr->getDecl();
            // if (clang::VarDecl* varDecl = clang::dyn_cast<clang::VarDecl>(valueDecl)) {
            //     
            // }
            // std::cout<< "ASE's index(D) = "<<Tool::get_stmt_string(ASE->getIdx())<<"\n";
            seq_info.add_used_array(VD,-1);
        }
        else{
            seq_info.add_used_array(VD,-2);
            // std::cout<< "ASE's index not a Varible or an IntegerLiteral\n";
        }
        return true;
    }
    
    bool ASTFieldChecker::allocationChecker(const clang::CStyleCastExpr *CSC, clang::QualType lhsType){
        if(CSC == nullptr) return false;
        auto subExpr = CSC->getSubExpr();
        if(const clang::CallExpr *CE = clang::dyn_cast<clang::CallExpr>(subExpr)){ //malloc(sizeof(A))
            if(Tool::ismemoryAllocationCall(CE)){
                const clang::FunctionDecl *FD = CE->getDirectCallee();
                unsigned numArgs = CE->getNumArgs();
                if(numArgs == 1){//malloc只有一个参数
                    const clang::Expr *arg = CE->getArg(0);//sizeof(A)
                    if(const clang::UnaryExprOrTypeTraitExpr *UETTE = clang::dyn_cast<clang::UnaryExprOrTypeTraitExpr>(arg)){
                        if (UETTE->getKind() == UETT_SizeOf) {

                            clang::QualType castType = Tool::get_normal_type_decl(CSC->getTypeAsWritten());//去掉指针
                            clang::QualType sizeofType = UETTE->getTypeOfArgument();
                            // 比较两种类型是否一致
                            if (castType.getCanonicalType() == sizeofType.getCanonicalType()) {
                                std::cout << "转换类型和sizeof类型一致\n";
                            } else {
                                std::cout << "转换类型和sizeof类型不一致\n";
                            }
                            if (lhsType.getCanonicalType() == castType.getCanonicalType()){
                                std::cout << "转换类型和左操作数类型一致\n";
                            } else {
                                std::cout << "转换类型和左操作数类型不一致\n";
                            }
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
    bool ASTFieldChecker::VisitBinaryOperator(clang::BinaryOperator *BO){
        /* 
            形如 a = (A*)malloc(sizeof(A));
        */
       
        if (BO->getOpcode() != clang::BinaryOperatorKind::BO_Assign) return true;
        //只关注assign
        const clang::Expr *lhs = Tool::getBinaryOperatorLHS(BO);
        const clang::Expr *rhs = Tool::getBinaryOperatorRHS_Cast(BO);
        if(lhs==nullptr || rhs==nullptr) return true;
        clang::QualType lhsType = Tool::get_normal_type_decl(lhs->getType());
        if(!lhsType.getTypePtr()->isRecordType()) return true;//
        if(const clang::VarDecl *VD = Tool::getVD_REF(lhs)){
            if(const clang::CStyleCastExpr *CSC = clang::dyn_cast<clang::CStyleCastExpr>(rhs)){ //(A*)malloc(sizeof(A))
                if(allocationChecker(CSC,lhsType)){
                    const clang::RecordDecl* recordDecl = nullptr;
                    if (const clang::RecordType* recordType = lhsType->getAsStructureType()){
                        recordDecl = recordType->getDecl();
                    }else if (const clang::RecordType* recordType = lhsType->getAsUnionType()) {
                        recordDecl = recordType->getDecl();
                    }
                    seq_info.add_record_vars(recordDecl,VD);
                }
            }
        }
        return true;
    }

    /* ControlFlowChecker */
    ASTControlFlowCounter* ASTControlFlowCounter::counter = nullptr;
    std::mutex ASTControlFlowCounter::counterMutex;

    //ASTControlFlowCounter::ASTControlFlowCounter(clang::ASTContext* ctx, clang::Rewriter& rewriter, SeqInfo& seqInfo)
    //: _ctx(ctx), _rewriter(rewriter), seq_info(seqInfo) {}

    bool ASTControlFlowCounter::TraverseStmt(clang::Stmt *S) {
        return RecursiveASTVisitor<ASTControlFlowCounter>::TraverseStmt(S);
    }
    void ASTControlFlowCounter::reset() {
        controlflow_info = ControlFlowInfo(); // 直接重置为默认构造的结构体
    }
    bool ASTControlFlowCounter::VisitIfStmt(clang::IfStmt *If) {
        //seq_info.add_control_flow("if");
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation beginLoc = If->getBeginLoc();
        clang::SourceLocation endLoc = If->getEndLoc();
         if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
        // 获取 if 语句的起始和结束位置
        clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
        clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
        if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
        // 判断是否和 trace 中的某个点重合或包含
        //seq_info.print_trace();
        for (auto &point :seq_info.get_trace()) {
            std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
            if (filename1 != presumedStart.getFilename()) continue;
            bool line_match = (point.line >= presumedStart.getLine() && point.line <= presumedEnd.getLine());
            if (line_match) {
                controlflow_info.if_count++;
                break;
            }
        }
        
        return true;
    }

    bool ASTControlFlowCounter::VisitForStmt(clang::ForStmt *FS) {
        //seq_info.add_control_flow("for");
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation beginLoc = FS->getBeginLoc();
        clang::SourceLocation endLoc = FS->getEndLoc();
         if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
        // 获取 if 语句的起始和结束位置
        clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
        clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
        if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
        // 判断是否和 trace 中的某个点重合或包含
        //seq_info.print_trace();
        for (auto &point :seq_info.get_trace()) {
            std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
            if (filename1 != presumedStart.getFilename()) continue;
            bool line_match = (point.line >= presumedStart.getLine() && point.line <= presumedEnd.getLine());
            if (line_match) {
                controlflow_info.for_count++;
                break;
            }
        }
        
        return true;
    }

    bool ASTControlFlowCounter::VisitWhileStmt(clang::WhileStmt *WS) {
        //seq_info.add_control_flow("while");
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation beginLoc = WS->getBeginLoc();
        clang::SourceLocation endLoc = WS->getEndLoc();
         if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
        // 获取 if 语句的起始和结束位置
        clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
        clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
        if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
        // 判断是否和 trace 中的某个点重合或包含
        //seq_info.print_trace();
        for (auto &point :seq_info.get_trace()) {
            std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
            if (filename1 != presumedStart.getFilename()) continue;
            bool line_match = (point.line >= presumedStart.getLine() && point.line <= presumedEnd.getLine());
            if (line_match) {
                controlflow_info.while_count++;
                break;
            }
        }
        
        return true;
    }

    bool ASTControlFlowCounter::VisitDoStmt(clang::DoStmt *DS) {
        //seq_info.add_control_flow("do");
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation beginLoc = DS->getBeginLoc();
        clang::SourceLocation endLoc = DS->getEndLoc();
         if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
        // 获取 if 语句的起始和结束位置
        clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
        clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
        if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
        // 判断是否和 trace 中的某个点重合或包含
        //seq_info.print_trace();
        for (auto &point :seq_info.get_trace()) {
            std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
            if (filename1 != presumedStart.getFilename()) continue;
            bool line_match = (point.line >= presumedStart.getLine() && point.line <= presumedEnd.getLine());
            if (line_match) {
                controlflow_info.do_while_count++;
                break;
            }
        }
        
        return true;
    }

    bool ASTControlFlowCounter::VisitSwitchStmt(clang::SwitchStmt *SS) {
        //seq_info.add_control_flow("switch");
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation beginLoc = SS->getBeginLoc();
        clang::SourceLocation endLoc = SS->getEndLoc();
         if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
        // 获取 if 语句的起始和结束位置
        clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
        clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
        if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
        // 判断是否和 trace 中的某个点重合或包含
        //seq_info.print_trace();
        for (auto &point :seq_info.get_trace()) {
            std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
            if (filename1 != presumedStart.getFilename()) continue;
            bool line_match = (point.line >= presumedStart.getLine() && point.line <= presumedEnd.getLine());
            if (line_match) {
                controlflow_info.switch_count++;
                break;
            }
        }
        
        return true;
    }

    bool ASTControlFlowCounter::VisitCaseStmt(clang::CaseStmt *CS) {
        //seq_info.add_control_flow("case");
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation beginLoc = CS->getBeginLoc();
        clang::SourceLocation endLoc = CS->getEndLoc();
         if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
        // 获取 if 语句的起始和结束位置
        clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
        clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
        if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
        // 判断是否和 trace 中的某个点重合或包含
        //seq_info.print_trace();
        for (auto &point :seq_info.get_trace()) {
            std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
            if (filename1 != presumedStart.getFilename()) continue;
            bool line_match = (point.line >= presumedStart.getLine() && point.line <= presumedEnd.getLine());
            if (line_match) {
                controlflow_info.case_count++;
                break;
            }
        }
        
        return true;
    }

    bool ASTControlFlowCounter::VisitGotoStmt(clang::GotoStmt *GS) {
        //seq_info.add_control_flow("goto");
        return true;
    }

    bool ASTControlFlowCounter::VisitLabelStmt(clang::LabelStmt *LS) {
        //seq_info.add_control_flow("label");
        return true;
    }

    std::mutex ASTResourceAnalyzer::counterMutex;
    ASTResourceAnalyzer* ASTResourceAnalyzer::counter = nullptr;

    bool ASTResourceAnalyzer::TraverseStmt(clang::Stmt *S) {
        return RecursiveASTVisitor<ASTResourceAnalyzer>::TraverseStmt(S);
    }

    bool ASTResourceAnalyzer::VisitCallExpr(clang::CallExpr *call) {
        const FunctionDecl *callee = call->getDirectCallee();
        std::string name;
    
        if (callee) {
            name = callee->getNameAsString();
        } else {
            const Expr *calleeExpr = call->getCallee()->IgnoreImpCasts();
            if (const DeclRefExpr *declRef = llvm::dyn_cast<DeclRefExpr>(calleeExpr)) {
                name = declRef->getNameInfo().getAsString();
            }
        }
    
        if (!name.empty()) {
            //std::cout << "[CallExpr] Found function call: " << name << " ---------------------------------" << std::endl;
    
            if (name == "open" || name == "fopen") resource_info.file_open++;
            else if (name == "close" || name == "fclose") resource_info.file_close++;
            else if (name == "malloc" || name == "calloc" || name == "realloc") resource_info.memory_alloc++;
            else if (name == "free") resource_info.memory_free++;
            else if (name == "socket") resource_info.net_socket++;
            else if (name == "connect") resource_info.net_connect++;
            else if (name.find("mysql_") != std::string::npos || name.find("sqlite3_") != std::string::npos) resource_info.db_access++;
        }
    
        return true;
    }
    std::mutex ASTExceptionAnalyzer::analyzerMutex;
    ASTExceptionAnalyzer* ASTExceptionAnalyzer::analyzer = nullptr;

    bool ASTExceptionAnalyzer::VisitCXXTryStmt(clang::CXXTryStmt *stmt) {
        exception_info.try_count++;
        current_nesting_depth++;
        if (current_nesting_depth > exception_info.max_nesting_depth)
            exception_info.max_nesting_depth = current_nesting_depth;

        // 遍历子语句
        for (unsigned i = 0; i < stmt->getNumHandlers(); ++i) {
            TraverseStmt(stmt->getHandler(i));
        }

        TraverseStmt(stmt->getTryBlock());

        current_nesting_depth--;
        return true;
    }

    bool ASTExceptionAnalyzer::VisitCXXCatchStmt(clang::CXXCatchStmt *stmt) {
        exception_info.catch_count++;
        current_nesting_depth++;
        if (current_nesting_depth > exception_info.max_nesting_depth) {
            exception_info.max_nesting_depth = current_nesting_depth;
        }

    
        if (clang::Stmt *handlerBlock = stmt->getHandlerBlock()) {
            TraverseStmt(handlerBlock);
        }

        current_nesting_depth--;
        return true;
    }
    
    bool ASTExceptionAnalyzer::VisitCXXThrowExpr(clang::CXXThrowExpr *stmt) {
        exception_info.throw_count++;
        if (current_nesting_depth > exception_info.max_nesting_depth) {
        exception_info.max_nesting_depth = current_nesting_depth;
        }
        return true;
    }
    
    bool ASTExceptionAnalyzer::VisitCallExpr(clang::CallExpr *callExpr) {
        if (clang::FunctionDecl *FD = callExpr->getDirectCallee()) {
            if (FD && FD->getNameInfo().getAsString().find("setjmp") != std::string::npos||FD->getNameInfo().getAsString() == "setjmp") {//FD->getNameInfo().getAsString() == "setjmp"
                ++exception_info.setjmp_count;
                if (current_nesting_depth > exception_info.max_nesting_depth) {
                exception_info.max_nesting_depth = current_nesting_depth;
            }
            } else if (FD && FD->getNameInfo().getAsString().find("setjmp") != std::string::npos||FD->getNameInfo().getAsString() == "longjmp") {//FD->getNameInfo().getAsString() == "longjmp"
                ++exception_info.longjmp_count;
                if (current_nesting_depth > exception_info.max_nesting_depth) {
                exception_info.max_nesting_depth = current_nesting_depth;
            }
            }
        }
        
        return true;
    }

    std::mutex ASTNestedBlockAnalyzer::analyzerMutex;
    ASTNestedBlockAnalyzer* ASTNestedBlockAnalyzer::analyzer = nullptr;
    
    void ASTNestedBlockAnalyzer::reset() {
        nested_block_info = NestedBlockInfo(); // 重置为默认状态
        current_nesting_depth = 0;
        trace_flag =0;
        max_depth_in_current_func = 1;
    }
    bool ASTNestedBlockAnalyzer::VisitIfStmt(clang::IfStmt *stmt) {
        current_nesting_depth++;
        max_depth_in_current_func = std::max(max_depth_in_current_func, current_nesting_depth);
        // 处理 if-if 嵌套
        
        if(trace_flag==0){
            clang::SourceManager &SM = _ctx->getSourceManager();
            clang::SourceLocation beginLoc = stmt->getBeginLoc();
            clang::SourceLocation endLoc = stmt->getEndLoc();
            if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
            // 获取 if 语句的起始和结束位置
            clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
            clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
            if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
            // 判断是否和 trace 中的某个点重合或包含
            //seq_info.print_trace();
            for (auto &point :seq_info.get_trace()) {
                std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
                if (filename1 != presumedStart.getFilename()) continue;
                bool point_in_stmt =
                (point.line > presumedStart.getLine() || (point.line == presumedStart.getLine() )) &&
                (point.line_end < presumedEnd.getLine() || (point.line_end == presumedEnd.getLine() ));

                bool stmt_in_point =
                (presumedStart.getLine() > point.line || (presumedStart.getLine() == point.line )) &&
                (presumedEnd.getLine() < point.line || (presumedEnd.getLine() == point.line ));

                if (point_in_stmt || stmt_in_point){
                    trace_flag = 1;
                    
                }
            }
        }
        if(trace_flag == 0)
        {
            current_nesting_depth--;
            return true;
        }
        clang::Stmt *body = stmt->getThen();
        if (body){
            if (clang::isa<clang::IfStmt>(stmt->getThen())|| (stmt->getElse()&&clang::isa<clang::IfStmt>(stmt->getElse()))) {
                nested_block_info.if_if_count++;
            }
            else if (auto *compound = clang::dyn_cast<clang::CompoundStmt>(body)) {
                    for (auto *child : compound->body()) {
                        if (clang::isa<clang::IfStmt>(child)) {
                            nested_block_info.if_if_count++;
                            break; // 找到一个就够了，不用继续找
                        }
                    }
                }

            // 处理 if-loop 嵌套
            if (clang::isa<clang::ForStmt>(stmt->getThen()) || clang::isa<clang::WhileStmt>(stmt->getThen())
                ||(stmt->getElse()&&(clang::isa<clang::ForStmt>(stmt->getElse()) || clang::isa<clang::WhileStmt>(stmt->getElse())))) {
                nested_block_info.if_loop_count++;
            }
            else if (auto *compound = clang::dyn_cast<clang::CompoundStmt>(body)) {
                    for (auto *child : compound->body()) {
                        if (clang::isa<clang::ForStmt>(child)||clang::isa<clang::WhileStmt>(child)) {
                            nested_block_info.if_loop_count++;
                            break; // 找到一个就够了，不用继续找
                        }
                    }
                
            }
        }
        body = stmt->getElse();
        if (body){
            if (clang::isa<clang::IfStmt>(stmt->getThen())|| (stmt->getElse()&&clang::isa<clang::IfStmt>(stmt->getElse()))) {
                nested_block_info.if_if_count++;
            }
            else if (auto *compound = clang::dyn_cast<clang::CompoundStmt>(body)) {
                    for (auto *child : compound->body()) {
                        if (clang::isa<clang::IfStmt>(child)) {
                            nested_block_info.if_if_count++;
                            break; // 找到一个就够了，不用继续找
                        }
                    }
                }

            // 处理 if-loop 嵌套
            if (clang::isa<clang::ForStmt>(stmt->getThen()) || clang::isa<clang::WhileStmt>(stmt->getThen())
                ||(stmt->getElse()&&(clang::isa<clang::ForStmt>(stmt->getElse()) || clang::isa<clang::WhileStmt>(stmt->getElse())))) {
                nested_block_info.if_loop_count++;
            }
            else if (auto *compound = clang::dyn_cast<clang::CompoundStmt>(body)) {
                    for (auto *child : compound->body()) {
                        if (clang::isa<clang::ForStmt>(child)||clang::isa<clang::WhileStmt>(child)) {
                            nested_block_info.if_loop_count++;
                            break; // 找到一个就够了，不用继续找
                        }
                    }
                
            }
        }

        // 遍历 Then 语句
        TraverseStmt(stmt->getThen());
        // 处理 else 语句
        if (stmt->getElse()) {
            TraverseStmt(stmt->getElse());
        }
        trace_flag = 0;
        current_nesting_depth--;
        return true;
    }

    // 访问 For 语句
    bool ASTNestedBlockAnalyzer::VisitForStmt(clang::ForStmt *stmt) {
        current_nesting_depth++;
        max_depth_in_current_func = std::max(max_depth_in_current_func, current_nesting_depth);
        // 处理 loop-loop 嵌套
        /*if (clang::isa<clang::ForStmt>(stmt->getBody())||clang::isa<clang::WhileStmt>(stmt->getBody())) {
            nested_block_info.loop_loop_count++;
        }*/
        // 遍历 For 语句的 body
        
        if(trace_flag==0){
            clang::SourceManager &SM = _ctx->getSourceManager();
            clang::SourceLocation beginLoc = stmt->getBeginLoc();
            clang::SourceLocation endLoc = stmt->getEndLoc();
            if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
            // 获取 if 语句的起始和结束位置
            clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
            clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
            if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
            // 判断是否和 trace 中的某个点重合或包含
            //seq_info.print_trace();
            for (auto &point :seq_info.get_trace()) {
                std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
                if (filename1 != presumedStart.getFilename()) continue;
                bool point_in_stmt =
                (point.line > presumedStart.getLine() || (point.line == presumedStart.getLine() )) &&
                (point.line_end < presumedEnd.getLine() || (point.line_end == presumedEnd.getLine() ));

                bool stmt_in_point =
                (presumedStart.getLine() > point.line || (presumedStart.getLine() == point.line )) &&
                (presumedEnd.getLine() < point.line || (presumedEnd.getLine() == point.line ));

                if (point_in_stmt || stmt_in_point){
                    trace_flag = 1;
                    
                }
            }
        }
        if(trace_flag == 0)
        {
            current_nesting_depth--;
            return true;
        }
        clang::Stmt *body = stmt->getBody();
        if (body) {
            // 如果是单个 ForStmt（没有花括号包裹）
            if (clang::isa<clang::ForStmt>(body)||clang::isa<clang::WhileStmt>(body)) {
                nested_block_info.loop_loop_count++;
            }
            // 如果是复合语句块（例如 { ... }）
            else if (auto *compound = clang::dyn_cast<clang::CompoundStmt>(body)) {
                for (auto *child : compound->body()) {
                    if (clang::isa<clang::ForStmt>(child)||clang::isa<clang::WhileStmt>(child)) {
                        nested_block_info.loop_loop_count++;
                        break; // 找到一个就够了，不用继续找
                    }
                }
            }
        }
        TraverseStmt(stmt->getBody());
        trace_flag = 0;
        current_nesting_depth--;
        return true;
    }

    // 访问 While 语句
    bool ASTNestedBlockAnalyzer::VisitWhileStmt(clang::WhileStmt *stmt) {
        current_nesting_depth++;
        max_depth_in_current_func = std::max(max_depth_in_current_func, current_nesting_depth);
        // 处理 loop-loop 嵌套
        /*if (clang::isa<clang::WhileStmt>(stmt->getBody())||clang::isa<clang::ForStmt>(stmt->getBody())) {
            nested_block_info.loop_loop_count++;
            std::cout<<nested_block_info.loop_loop_count<<"____________________________________________"<<"\n";
        }*/
        
        if(trace_flag==0){
            clang::SourceManager &SM = _ctx->getSourceManager();
            clang::SourceLocation beginLoc = stmt->getBeginLoc();
            clang::SourceLocation endLoc = stmt->getEndLoc();
            if (beginLoc.isInvalid() || endLoc.isInvalid()) return true;
            // 获取 if 语句的起始和结束位置
            clang::PresumedLoc presumedStart = SM.getPresumedLoc(beginLoc);
            clang::PresumedLoc presumedEnd = SM.getPresumedLoc(endLoc);
            if (!presumedStart.isValid() || !presumedEnd.isValid()) return true;
            // 判断是否和 trace 中的某个点重合或包含
            //seq_info.print_trace();
            for (auto &point :seq_info.get_trace()) {
                std::string filename1 = point.filepath.substr(point.filepath.find_last_of('/') + 1);
                if (filename1 != presumedStart.getFilename()) continue;
                //std::cout<<"line_match" <<" "<<point.line<<""<<point.line_end<<" "<<presumedStart.getLine()<<" "<< presumedEnd.getLine()<<"\n";
                bool point_in_stmt =
                (point.line > presumedStart.getLine() || (point.line == presumedStart.getLine() )) &&
                (point.line_end < presumedEnd.getLine() || (point.line_end == presumedEnd.getLine() ));

                bool stmt_in_point =
                (presumedStart.getLine() > point.line || (presumedStart.getLine() == point.line )) &&
                (presumedEnd.getLine() < point.line || (presumedEnd.getLine() == point.line ));

                if (point_in_stmt || stmt_in_point){
                    trace_flag = 1;
                    
                }
                
            }
        }
        if(trace_flag == 0)
        {
            current_nesting_depth--;
            return true;
        }
        clang::Stmt *body = stmt->getBody();
        if (body) {
            // 如果是单个 ForStmt（没有花括号包裹）
            if (clang::isa<clang::ForStmt>(body)||clang::isa<clang::WhileStmt>(body)) {
                nested_block_info.loop_loop_count++;
            }
            // 如果是复合语句块（例如 { ... }）
            else if (auto *compound = clang::dyn_cast<clang::CompoundStmt>(body)) {
                for (auto *child : compound->body()) {
                    if (clang::isa<clang::ForStmt>(child)||clang::isa<clang::WhileStmt>(child)) {
                        nested_block_info.loop_loop_count++;
                        break; // 找到一个就够了，不用继续找
                    }
                }
            }
        }

        // 遍历 While 语句的 body
        TraverseStmt(stmt->getBody());

        trace_flag = 0;
        current_nesting_depth--;
        return true;
    }
    
    
    bool ASTAssignLoad::VisitBinaryOperator(clang::BinaryOperator *BO){
        auto opcode = BO->getOpcode();
        if (opcode == clang::BinaryOperatorKind::BO_AddAssign ||
            opcode == clang::BinaryOperatorKind::BO_AndAssign ||
            opcode == clang::BinaryOperatorKind::BO_DivAssign ||
            opcode == clang::BinaryOperatorKind::BO_MulAssign ||
            opcode == clang::BinaryOperatorKind::BO_OrAssign ||
            opcode == clang::BinaryOperatorKind::BO_RemAssign ||
            opcode == clang::BinaryOperatorKind::BO_ShlAssign ||
            opcode == clang::BinaryOperatorKind::BO_ShrAssign ||
            opcode == clang::BinaryOperatorKind::BO_SubAssign ||
            opcode == clang::BinaryOperatorKind::BO_XorAssign ||
            opcode == clang::BinaryOperatorKind::BO_Assign) {
            const clang::Expr *lhs = Tool::getBinaryOperatorLHS(BO);
            if(const clang::VarDecl *VD = Tool::getVD_REF(lhs)){
                if(loop_checker.check(BO) && not_for_init(BO)){
                    loop_checker.set_ret(false);
                    seq_info.add_loop_assign_times(VD,1);
                    seq_info.add_assign_times(VD,2);
                    // std::cout<<VD->getNameAsString()<<" assign times +N[by assign in loop] in "<<Tool::get_stmt_string(BO)<<"\n";
                }else{
                    seq_info.add_assign_times(VD,1);
                    // std::cout<<VD->getNameAsString()<<" assign times ++[by assign]\n";
                }
            }
            
        }
        return true;
    }
    bool ASTAssignLoad::not_for_init(clang::Stmt *stmt){
        PDGNode *pn = seq_info.get_pdg(func)->get_node_stmt(stmt);
        if(pn != nullptr){
            std::unordered_set<int> nodes = seq_info.get_pdg(func)->get_ancestor_nodes(pn->getID());
            for(auto n:nodes){
                auto ancestor = seq_info.get_pdg(func)->get_node_ID(n);
                if(ancestor==nullptr) continue;
                if(clang::ForStmt *forStmt = clang::dyn_cast<clang::ForStmt>(ancestor)){
                    if(forStmt->getInit() == stmt) return false;
                }
            }
        }
        return true;
    }
    bool ASTAssignLoad::VisitUnaryOperator(clang::UnaryOperator *UO){
        if(!UO->isIncrementOp() && !UO->isDecrementOp()) return true;
        // a++ a-- ++a --a
        if(clang::DeclRefExpr *DRE = clang::dyn_cast<clang::DeclRefExpr>(UO->getSubExpr())){
            if (clang::VarDecl *VD = clang::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
                if(loop_checker.check(UO) && not_for_init(UO)){
                    loop_checker.set_ret(false);
                    seq_info.add_loop_assign_times(VD,1);
                    seq_info.add_assign_times(VD,2);
                    // std::cout<<VD->getNameAsString()<<" assign times +N[by Increment or Decrement in loop]\n";
                }else{
                    seq_info.add_assign_times(VD,1);
                    // std::cout<<VD->getNameAsString()<<" assign times ++[by Increment or Decrement]\n";
                }
            }
        }
        return true;
    }
    bool ASTAssignLoad::VisitDeclStmt(clang::DeclStmt *DS){
        for (auto decl : DS->decls()) {
            if(llvm::isa<clang::VarDecl>(decl)){ // int x;
                clang::VarDecl *VD=llvm::dyn_cast<clang::VarDecl>(decl);
                if(VD->hasInit()){
                    if(loop_checker.check(DS)  && not_for_init(DS) ){
                        loop_checker.set_ret(false);
                        seq_info.add_loop_assign_times(VD,1);
                        seq_info.add_assign_times(VD,2);
                        // std::cout<<VD->getNameAsString()<<" assign times +N[by init in loop]\n";
                    }else{
                        seq_info.add_assign_times(VD,1);
                        // std::cout<<VD->getNameAsString()<<" assign times ++[by init]\n";
                    }            
                }
            }
        }
        return true;
    }
    bool ASTAssignLoad::VisitVarDecl(clang::VarDecl *VD){
        return true; //在DeclStmt处理
        if(VD->hasInit()){
            if(is_loop){
                seq_info.add_loop_assign_times(VD,1);
                seq_info.add_assign_times(VD,2);
                // std::cout<<VD->getNameAsString()<<" assign times +N[by init in loop]\n";
            }else{
                seq_info.add_assign_times(VD,1);
                // std::cout<<VD->getNameAsString()<<" assign times ++[by init]\n";
            }            
        }
        return true;
    }
    void ASTAssignLoad::handle_CE(const clang::CallExpr *CE){
        const clang::FunctionDecl *FD = CE->getDirectCallee(); // 未使用
        unsigned numArgs = CE->getNumArgs();
        for (unsigned i = 0; i < numArgs; ++i) {
            const clang::Expr *arg = CE->getArg(i);
            if(const clang::ImplicitCastExpr *argE = clang::dyn_cast<clang::ImplicitCastExpr>(arg)){
                arg=argE->getSubExpr();
            }
            if(const clang::CStyleCastExpr *castExpr= clang::dyn_cast<clang::CStyleCastExpr>(arg)){
                arg = castExpr->getSubExpr();
            }
            /* FIX ME : 其他参数情况，如C++的形参可以是引用，不用直接传地址 */
            if(const clang::VarDecl * VD =Tool::getVD_REF(arg)){
                /* 如果是指针 就计数 */
                bool isPointer = VD->getType().getTypePtr()->isPointerType();
                if(isPointer){
                    seq_info.add_assign_times(const_cast<clang::VarDecl*>(VD),2);
                    // std::cout<<var_decl->getNameAsString()<<" assign times + N [by arg is pointer]\n";
                }
                
            }
            else if(const clang::UnaryOperator *unary_op = clang::dyn_cast<clang::UnaryOperator>(arg)){
                auto opcode = unary_op->getOpcode();
                if(opcode == clang::UnaryOperatorKind::UO_AddrOf){/* 传入地址 就计数 */ 
                    if(const clang::DeclRefExpr *declRefExpr = clang::dyn_cast<clang::DeclRefExpr>(unary_op->getSubExpr())){
                        if (const clang::VarDecl *var_decl = clang::dyn_cast<clang::VarDecl>(declRefExpr->getDecl())) {
                            seq_info.add_assign_times(const_cast<clang::VarDecl*>(var_decl),2);
                            // std::cout<<var_decl->getNameAsString()<<" assign times + N [by arg is address]\n";
                        }
                    }
                }
                
            }
            else if(const clang::CallExpr *callExpr = clang::dyn_cast<clang::CallExpr>(arg)){
                handle_CE(callExpr);
            }
        }
    }
    bool ASTAssignLoad::VisitCallExpr(clang::CallExpr *CE){
        if(CE->getDirectCallee()!=nullptr)
            handle_CE(CE);
        else{
            /*
                间接函数调用：例如通过函数指针、回调等进行的调用。[C]
                虚函数调用：通过虚函数机制进行的调用。
                模板和内联展开：对于一些特殊的模板实例化或内联函数
                调用约定或特殊语言特性：如操作符重载或用户定义字面量等
            */
            // call by function pointer 该分析场景只关注参数，不关注调用的函数
            handle_CE(CE);
        }
        return true;
    }
    

    // bool SeqASTVisitor::isInSystemHeader(const clang::SourceLocation &loc) {
    //     auto &SM = _ctx->getSourceManager();
    //     if(SM.getFilename(loc)=="") return true;
    //     std::string filename = SM.getFilename(loc).str();
    //     if(filename.find("/usr/include/")!=std::string::npos) return true;
    //     if(filename.find("llvm-12.0.0.obj/")!=std::string::npos) return true;
    //     return false;
    // }
    clang::SourceRange SeqASTVisitor::get_decl_sourcerange(clang::Decl *stmt){
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
    clang::SourceRange SeqASTVisitor::get_sourcerange(clang::Stmt *stmt){
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
        
    void SeqInfo::compute_flowS(){
        if(info_vec["Sensitive_Flow"] == 1) return;
        for(auto var : rela_variables){
            if(assign_times.count(var)>0 && assign_times[var]>1){
                info_vec["Sensitive_Flow"] = 1;
                break;
            }
        }
    }

    void SeqInfo::compute_pathS(){
        if(info_vec["Sensitive_Path"] == 31) return;
        /* 如果给出警报路径 */
        /* 1: Any points in Loop or Branch */
        for(auto &p : trace){
            std::string filename1 = p.filepath.substr(p.filepath.find_last_of('/') + 1);
            if((p.filepath != current_file_name) && (filename1!=current_file_name)) continue;
            if(p.isInBranch){
                info_vec["Sensitive_Path"] |= 1;
            }
            if(p.isInLoop){
                info_vec["Sensitive_Path"] |= 2;
                // 循环条件是否依赖于条件分支语句 
                if(p.conditions_related.size()>0) {
                    info_vec["Sensitive_Path"] |= 16; 
                }
            }
            
            for(auto C : common::getCallExpr(p.node)){
                // 排除系统调用
                if(_ctx == nullptr || !Tool::isInSystemHeader(_ctx, C->getDirectCallee())){
                    if(Tool::isRecusiveFunction(C->getDirectCallee())){/* 2: callee is recursive function */
                        info_vec["Sensitive_Path"] |= 4;
                    }
                }
                
                
                if(Tool::isFunctionPointerCall(C)){ /* 3: call by FP -> callee is uncertain  */
                    info_vec["Sensitive_Path"] |= 8;
                    p.isFPCall = 1;
                }
                
            }

        }
    }
    
    void SeqInfo::compute_fieldS(){
        if(info_vec["Sensitive_Field"] == 15) return;
        int cnt_pointer = 0;
        int cnt = 0;
        for(auto &it : record_vars){
            cnt = it.second.size();
            if(cnt > 1 ){//查看是不是指针
                for(auto it2 : it.second){
                    if (it2->getType()->isPointerType()) {
                        cnt_pointer++;
                    }
                }
                if(cnt - cnt_pointer >1){
                    info_vec["Sensitive_Field"] |= 1; //多个变量
                }
                if(cnt_pointer > 1){
                    info_vec["Sensitive_Field"] |= 8; //多个指针变量
                }
            }
        }
        for(auto &it : used_array){
            if(it.second.size() > 1 || it.second.count(-1) > 0 || it.second.count(-2)>0){
                info_vec["Sensitive_Field"] |= 2; //数组下标不同常量 or 其他表达式
            }
        }
        for(auto &it : used_record){
            if(it.second.size() > 1 ){
                info_vec["Sensitive_Field"] |= 4; //不同成员变量
            }
        }
    }
    void SeqInfo::compute_contextS(){
        if(info_vec["Sensitive_Context"] == 31) return;
        /* 如果给出警报路径 */
        std::unordered_map<const clang::FunctionDecl *,int> calltimes;
        
        for(auto &p : trace){
            std::string filename1 = p.filepath.substr(p.filepath.find_last_of('/') + 1);
            if((p.filepath != current_file_name) && (filename1!=current_file_name)) continue;
            /* 1: callee is recursive function */
            for(auto C : common::getCallExpr(p.node)){
                if(Tool::isRecusiveFunction(C->getDirectCallee())){
                    info_vec["Sensitive_Context"] |= 1;
                }
                if(Tool::isFunctionDepanceExtern(C->getDirectCallee())){
                    info_vec["Sensitive_Context"] |= 16;
                }
                if(p.isInLoop) info_vec["Sensitive_Context"] |= 2; /* 2: call in loop */
                if(int(C->getNumArgs() )> 0 && C->getDirectCallee()!=nullptr){ // normal call
                    auto *F = C->getDirectCallee();
                    // 排除系统调用
                    if(_ctx != nullptr && Tool::isInSystemHeader(_ctx,F)){
                        continue;
                    }
                    if(calltimes.count(F)==0) calltimes[F]=1;
                    else {
                        calltimes[F]++;
                        info_vec["Sensitive_Context"] |= 4; /* 3: call more than 2 times*/
                    }
                }else if(Tool::isFunctionPointerCall(C)){ 
                    if(const clang::VarDecl *VD = Tool::getCallee_FP(C)){
                        fp_used[VD].insert(C);
                    }
                }
            }

        }
        for(auto &pair :fp_used){
            if(pair.second.size()>1){/* 4: call a FP more than 2 times  */
                info_vec["Sensitive_Context"] |= 8;
                break;
            }
        }
    }

    void SeqInfo::compute_macro(){
        print_used_macro();
        for(auto &[a,b] : macro_map){
            if(b!=0){
                if(info_vec.count("Semantic_Macro_"+a) ==0) 
                    info_vec["Semantic_Macro_"+a] = b;
                else info_vec["Semantic_Macro_"+a] |= b;
            }
        }
    }
    void SeqInfo::compute_typeSafe(){
        info_vec["Semantic_TypeSafe_RETURN_types"] = all_types_info.return_types_info;
        info_vec["Semantic_TypeSafe_BO_types"] = all_types_info.BO_types_info;
        info_vec["Semantic_TypeSafe_E_CAST"] = all_types_info.used_CScast;
        info_vec["Semantic_TypeSafe_RECORD_DEF"] = all_types_info.structure_info;
        info_vec["Semantic_TypeSafe_MD_Array"] = all_types_info.used_MD_Array;
        info_vec["Semantic_TypeSafe_BitField"] = all_types_info.used_bitfield;
        // info_vec["Semantic_TypeSafe_ALL"] = all_types_info.return_types_info || all_types_info.BO_types_info || all_types_info.used_CScast\
                                             || all_types_info.structure_info  || all_types_info.used_MD_Array  || all_types_info.used_bitfield;
    }
    void SeqInfo::compute_special(){
        info_vec["Semantic_Special_Op_Values"] = all_special_info.compute_values;
        int left = all_special_info.bit;
        info_vec["Semantic_Special_Pos"] = all_special_info.compute_goto_label | (all_special_info.compute_switch << left) | (all_special_info.compute_case<<left<<left);
        
        // info_vec["Semantic_Special_Assign"] = all_special_info.assign_scenario;
        for(auto var :rela_variables){
            if(varible_assign_scen.count(var)>0 && varible_assign_scen[var]!=0){
                if(varible_assign_scen[var]==8) continue;//只声明 没有其他赋值
                info_vec["Semantic_Special_Assign"] |= varible_assign_scen[var];
            }
        }
        // info_vec["Semantic_Special_ALL"] = info_vec["Semantic_Special_Op_Values"]||info_vec["Semantic_Special_Pos"]||info_vec["Semantic_Special_Assign"];
    }
    void SeqInfo::compute_control(){
        info_vec["Semantic_Control_DeadCode"] = all_control_info.dead_code;
        info_vec["Semantic_Control_Switch_Default"] = all_control_info.switch_default;
        info_vec["Semantic_Control_LoopState"] = all_control_info.loop_state;
        // info_vec["Semantic_Control_ALL"] = info_vec["Semantic_Control_DeadCode"]||info_vec["Semantic_Control_Switch_Default"]||;
    }
    void SeqInfo::compute_controlFlow() {
        info_vec["Semantic_ControlFlow_If"] = all_controlflow_info.if_count;
        info_vec["Semantic_ControlFlow_Switch"] = all_controlflow_info.switch_count;
        info_vec["Semantic_ControlFlow_Case"] = all_controlflow_info.case_count;
        info_vec["Semantic_ControlFlow_For"] = all_controlflow_info.for_count;
        info_vec["Semantic_ControlFlow_While"] = all_controlflow_info.while_count;
        info_vec["Semantic_ControlFlow_DoWhile"] = all_controlflow_info.do_while_count;
    
        // 控制流是否存在：任意一个计数不为零就表示存在
        info_vec["Semantic_ControlFlow_ALL"] =
        all_controlflow_info.if_count > 0 ||
        all_controlflow_info.switch_count > 0 ||
        all_controlflow_info.case_count > 0 ||
        all_controlflow_info.for_count > 0 ||
        all_controlflow_info.while_count > 0 ||
        all_controlflow_info.do_while_count > 0;
    }
    
    void SeqInfo::compute_resource() {
        info_vec["Semantic_Resource_File_Open"] = all_resource_info.file_open;
        info_vec["Semantic_Resource_File_Close"] = all_resource_info.file_close;
        info_vec["Semantic_Resource_Memory_Alloc"] = all_resource_info.memory_alloc;
        info_vec["Semantic_Resource_Memory_Free"] = all_resource_info.memory_free;
        info_vec["Semantic_Resource_Net_Socket"] = all_resource_info.net_socket;
        info_vec["Semantic_Resource_Net_Connect"] = all_resource_info.net_connect;
        info_vec["Semantic_Resource_DB_Access"] = all_resource_info.db_access;
    
        info_vec["Semantic_Resource_ALL"] = all_resource_info.file_open || all_resource_info.file_close ||
                                   all_resource_info.memory_alloc || all_resource_info.memory_free ||
                                   all_resource_info.net_socket || all_resource_info.net_connect ||
                                   all_resource_info.db_access;
    }
    
    void SeqInfo::compute_exception() {
        // 将异常相关的计数信息存入 info_vec
        info_vec["Semantic_Exception_Try"] = all_exception_info.try_count;
        info_vec["Semantic_Exception_Catch"] = all_exception_info.catch_count;
        info_vec["Semantic_Exception_Throw"] = all_exception_info.throw_count;
        info_vec["Semantic_Exception_Setjmp"] = all_exception_info.setjmp_count;
        info_vec["Semantic_Exception_Longjmp"] = all_exception_info.longjmp_count;
        info_vec["Semantic_Exception_MaxNestingDepth"] = all_exception_info.max_nesting_depth;
    
        // 异常处理是否存在：任意一个计数不为零就表示存在
        info_vec["Semantic_Exception_ALL"] =
            all_exception_info.try_count > 0 ||
            all_exception_info.catch_count > 0 ||
            all_exception_info.throw_count > 0 ||
            all_exception_info.setjmp_count > 0 ||
            all_exception_info.longjmp_count > 0;
    }
    
    void SeqInfo::compute_nestedblock() {
        // 将嵌套结构相关的计数信息存入 info_vec
        info_vec["Semantic_NestedBlock_MaxDepth"] = all_nestedblock_info.max_nesting_depth;
        info_vec["Semantic_NestedBlock_IF_IF"] = all_nestedblock_info.if_if_count;
        info_vec["Semantic_NestedBlock_IF_LOOP"] = all_nestedblock_info.if_loop_count;
        info_vec["Semantic_NestedBlock_LOOP_LOOP"] = all_nestedblock_info.loop_loop_count;
    
        // 是否存在嵌套结构（任意一种嵌套存在即为 true）
        info_vec["Semantic_NestedBlock_ALL"] =
            all_nestedblock_info.max_nesting_depth > 1 ||
            all_nestedblock_info.if_if_count > 0 ||
            all_nestedblock_info.if_loop_count > 0 ||
            all_nestedblock_info.loop_loop_count > 0;
    }
    

    bool SeqASTVisitor::MatchPoint(Point &p, clang::Stmt *stmt, std::string filePath){//检查这个stmt和p是否匹配
        int point_line = p.line;
        // std::string point_file = p.filepath;
        std::string &point_file = p.filename;
        if(point_file != filePath) return false;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceRange SR = get_sourcerange(stmt);//有宏的可能性所以用函数获取
        
        int line_start = SM.getPresumedLineNumber(SR.getBegin());
        int col_start  = SM.getPresumedColumnNumber(SR.getBegin());
        int line_end = SM.getPresumedLineNumber(SR.getEnd());
        int col_end  = SM.getPresumedColumnNumber(SR.getEnd());
        // std::cout<<"Matching Point:"<<point_file<<" "<<point_line<<" with Stmt:"<<Tool::get_stmt_string(stmt)<<" ["<<line_start<<","<<line_end<<"]\n";
        if(point_line>=line_start && point_line<=line_end){
            // std::cout<<"MATCH>>0\n";
            if(p.node == nullptr || abs(p.col - col_start) < abs(p.col - p.node_col)){
                p.node = stmt;
                p.node_col = col_start;
                p.node_cole = col_end;
                std::cout<<"MATCH NEW:"<<Tool::get_stmt_string(stmt)<<"\n";
                if(Tool::isMacroExpansion(stmt)){
                    p.isInMacro = true;
                }else{
                    p.pnode_str = "";
                    p.isInMacro = false;//取决于匹配到的最新的是不是宏
                }
            }
            else return false;
            if(p.isInMacro){
                p.pnode_str += Tool::get_stmt_string(stmt);
            }else{
                // std::cout<<"ASTPointerMatch:"<<Tool::get_stmt_string(stmt)<<"\n";
                ASTPointerMatch apm(_ctx,&p,stmt);
                apm.TraverseStmt(stmt);
            }
            
           
            /* */
            if(!p.isInBranch && Tool::isBranchStmt(stmt)){
                int isInclude = Tool::isInclude(_ctx, stmt, p.node);
                if(isInclude!=0){
                    bool isElse = (isInclude == 2);
                    p.isInBranch = Tool::getBranchState(_ctx,stmt,isElse);
                    p.branchStmt = stmt;
                }
            }
            if(!p.isInLoop && Tool::isLoopStmt(stmt) && Tool::isInclude(_ctx, stmt, p.node)){
                p.isInLoop= 1;
                p.loopStmt = stmt;
            }
            if(p.get_warning() == true){
                std::vector<const VarDecl *> temp_VDs = common::getVariables(p.node);
                for(auto &VD : temp_VDs){
                    seq_info.add_rela_variables(VD); // init variable
                }
            }
            /* 在这里添加对路径上点的操作 */
            ASTFieldChecker *afc = ASTFieldChecker::getInstance(_ctx,seq_info);
            afc->TraverseStmt(p.node);

            // 增加宏
            ASTMacroChecker *amc = ASTMacroChecker::getInstance(_ctx,_rewriter,seq_info);
            amc->TraverseStmt(p.node);
            
            ASTSpecialChecker *asc = ASTSpecialChecker::getInstance(_ctx,_rewriter,seq_info);
            asc->TraverseStmt(p.node);

            ASTTypeChecker *atc = ASTTypeChecker::getInstance(_ctx,_rewriter,seq_info);
            atc->TraverseStmt(p.node);

           
            //ASTControlFlowCounter *cfc = ASTControlFlowCounter::getInstance(_ctx, _rewriter, seq_info);
            //cfc->TraverseStmt(p.node);

            //ASTResourceAnalyzer *ra = ASTResourceAnalyzer::getInstance(_ctx, _rewriter, seq_info);
            //ra->TraverseStmt(p.node);

            //ASTExceptionAnalyzer *ea = ASTExceptionAnalyzer::getInstance(_ctx, _rewriter, seq_info);
            //ea->TraverseStmt(p.node);

            //ASTNestedBlockAnalyzer *nba = ASTNestedBlockAnalyzer::getInstance(_ctx, _rewriter, seq_info);
            //nba->TraverseStmt(p.node);
            

            return true;
        }
        return false;
    }


    bool SeqASTVisitor::MatchPoint_Decl(Point &p, clang::Decl *decl, std::string filePath){
        //检查这个decl（全局变量、构造函数、析构函数）和p是否匹配
        int point_line = p.line;
        std::string &point_file = p.filename;
        if(point_file != filePath) return false;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceRange SR = get_decl_sourcerange(decl);//有宏的可能性所以用函数获取
        
        int line_start = SM.getPresumedLineNumber(SR.getBegin());
        int col_start  = SM.getPresumedColumnNumber(SR.getBegin());
        int line_end = SM.getPresumedLineNumber(SR.getEnd());
        int col_end  = SM.getPresumedColumnNumber(SR.getEnd());
        // 全局变量 不需要更深入匹配,也不需要扩展宏的其他语句
        if(point_line>=line_start && point_line<=line_end && p.decl_node == nullptr){
            p.decl_node = decl;
            p.node_col = col_start;
            p.node_cole = col_end;
            return true;
        }
        return false;
    }
    bool SeqASTVisitor::VisitVarDecl(clang::VarDecl *var_decl){
        if (!_ctx->getSourceManager().isInMainFile(var_decl->getLocation()))
            return true;
        if (Tool::isInSystemHeader(_ctx, var_decl->getLocation()))
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        std::string filePath_abs = SM.getFilename(var_decl->getLocation()).str(); //ccjson中，args里的文件路径
        std::string filePath = filePath_abs;
        std::string::size_type iPos = filePath.find_last_of(kPathSeparator) + 1;
        filePath = filePath.substr(iPos, filePath.length() - iPos);
        seq_info.set_current_file_name(filePath_abs);
        if (clang::dyn_cast<clang::TranslationUnitDecl>(var_decl->getDeclContext())) {
            for(auto &p :seq_info.get_trace()){
                std::string filename1 = p.filepath.substr(p.filepath.find_last_of('/') + 1);
                if((p.filepath != seq_info.get_current_file_name()) && (filename1!=seq_info.get_current_file_name())) continue;
                if(MatchPoint_Decl(p,var_decl,filePath)){
                    p.set_func_sign("__global__");
                    p.pnode_str = Tool::get_decl_string(var_decl);
                }
            }
        }
        return true;
    }
    bool SeqASTVisitor::VisitFunctionDecl(clang::FunctionDecl *func_decl){
        
        clang::SourceLocation loc = func_decl->getLocation();
        if (loc.isInvalid() || Tool::isInSystemHeader(_ctx, loc))
            return true;
        
        if (!_ctx->getSourceManager().isInMainFile(loc))
            return true;

        clang::SourceManager &SM = _ctx->getSourceManager();
        std::string func_path = Tool::get_loc_file(_ctx, loc);
        std::string filePath_abs = normalize_path(make_absolute_path(func_path)); //标准化路径

        std::string filePath = filePath_abs;

        std::string::size_type iPos = filePath.find_last_of(kPathSeparator) + 1;
        filePath = filePath.substr(iPos, filePath.length() - iPos);

        if(filePath.find("src/")!=std::string::npos){
            int pos = filePath.find("src/");
            size_t idx = filePath.find_last_of("/", pos);
        
            if (idx != std::string::npos) {
                // 提取从最后一个 '/' 到字符串末尾的子字符串
                filePath = filePath.substr(idx + 1);
            }
        }
        std::string func_name = Tool::get_qualified_method_name(func_decl);
        // std::cout<<"\n***in VisitFunctionDecl | Function " << func_name << " is defined in file: " << filePath << "\n";
        if (seq_info.find_function(func_name))
        {

        std::cout<<"\n| Function [" << func_name << "] defined in: " << filePath<<"  ["<<filePath_abs << "]\n";
        seq_info.set_current_file_name(filePath_abs);
            if (func_decl == func_decl->getDefinition()) 
            {
                clang::Stmt *body_stmt = func_decl->getBody();
                std::string func_sign = Tool::getFunctionSignature(func_decl);
                
                /* build PDG*/
                seq_info.set_pdg(func_decl, new PDG(func_decl, seq_info.root_path, seq_info.program_name));
                seq_info.get_pdg(func_decl)->dumpJson();
               
                // if(func_name == "null_pointer_017_func_001") {
                    // seq_info.get_pdg(func_decl)->printGraph();
                    // func_decl->dump();
                // }
                 /* Traverse to get assign times */

                /* Traverse to get assign times */
                ASTAssignLoad aal(func_decl, _ctx, seq_info);/*仅在此调用*/
                aal.TraverseStmt(body_stmt);

                ASTSpecialChecker *asc = ASTSpecialChecker::getInstance(_ctx,_rewriter,seq_info);
                asc->set_check_func(func_decl);//设置所在函数
                asc->set_func_flag(true);
                asc->TraverseStmt(body_stmt);//检查赋值场景
                asc->set_func_flag(false);// 恢复
                
                ASTTypeChecker *atc = ASTTypeChecker::getInstance(_ctx,_rewriter,seq_info);
                atc->set_check_func(func_decl);//设置所在函数
                atc->set_func_flag(true);
                atc->TraverseStmt(body_stmt);//为了检查所有return语句
                atc->set_func_flag(false);// 恢复
                
                ASTControlFlowCounter *cfc = ASTControlFlowCounter::getInstance(_ctx, _rewriter, seq_info);
                cfc->reset();
                cfc->set_check_func(func_decl);

                ASTResourceAnalyzer *ra = ASTResourceAnalyzer::getInstance(_ctx, _rewriter, seq_info);
                ra->reset();
                ra->set_check_func(func_decl);          
                ra->TraverseStmt(body_stmt);                                      
                          
                
                ASTExceptionAnalyzer *ea = ASTExceptionAnalyzer::getInstance(_ctx, _rewriter, seq_info);
                ea->reset();
                ea->set_check_func(func_decl);          // 设置当前函数（便于记录所属函数名）
                ea->set_func_flag(true);                // 标记函数进入，开始遍历
                ea->TraverseStmt(body_stmt);            // 遍历函数体，收集控制流结构
                ea->set_func_flag(false);

                ASTNestedBlockAnalyzer *nba = ASTNestedBlockAnalyzer::getInstance(_ctx, _rewriter, seq_info);
                nba->reset();
                nba->set_check_func(func_decl);          // 设置当前函数（便于记录所属函数名）

                ASTControlFChecker *acc = ASTControlFChecker::getInstance(_ctx,_rewriter,seq_info);
                acc->set_check_func(func_decl);//设置所在函数
                acc->TraverseStmt(body_stmt);

                bool CXX_CD_flag = false;
                if(clang::isa<clang::CXXConstructorDecl>(func_decl) || clang::isa<clang::CXXDestructorDecl>(func_decl)) CXX_CD_flag = true;
                if(CXX_CD_flag){
                    for(auto &p :seq_info.get_trace()){
                        std::string filename1 = p.filepath.substr(p.filepath.find_last_of('/') + 1);
                        if((p.filepath != seq_info.get_current_file_name()) && (filename1!=seq_info.get_current_file_name())) continue;
                            if(MatchPoint_Decl(p,func_decl,filePath)){
                                p.set_func_sign(func_name);
                                p.pnode_str = Tool::get_decl_string(func_decl);
                        }
                    }
                }
                else{
                    for (auto stmt : body_stmt->children())
                    {
                        if(!stmt) continue;
                        //
                        std::cout<<"Stmt : "<<Tool::get_stmt_string(stmt)<<"\n";
                        for(auto &p :seq_info.get_trace()){
                            std::string filename1 = p.filepath.substr(p.filepath.find_last_of('/') + 1);
                            if((p.filepath != seq_info.get_current_file_name()) && (filename1!=seq_info.get_current_file_name())) continue;
                            
                            // std::cout<<"Try to match point : "<<p.id<<"\n";
                            if(MatchPoint(p,stmt,filePath)){
                                std::cout<<"Matched Point id:"<<p.id<<" in function "<<func_name<<"\n";
                                p.pnode = seq_info.get_pdg(func_decl)->get_node_stmt(p.node);
                                p.set_func_sign(func_sign);
                                if(p.isInMacro==false) p.pnode_str = Tool::get_stmt_string(p.node);
                                if(p.isInLoop && p.pnode != nullptr){//in loop, check condition
                                    std::cout<<Tool::get_stmt_string(p.node)<<" in loop, check condition, start at "<<p.pnode->getID()<<"\n";
                                    std::unordered_set<Link, Link_hash, Link_equal> nodes = seq_info.get_pdg(func_decl)->get_rela_nodes(p.pnode->getID());
                                    //记录cond？
                                    std::vector<int> conditions;
                                    int c = -1;
                                    std::cout<<"Totally check "<<nodes.size()<<" nodes\n";
                                    for(const Link &lk:nodes){
                                        int n = lk.nodeID;
                                        LinkType T = lk.T;
                                        c = -1;
                                        auto ancestor = seq_info.get_pdg(func_decl)->get_node_ID(n);
                                        if(ancestor==nullptr) continue;
                                        // std::cout<<" ancestor "<<Tool::get_stmt_string(ancestor)<<"\n";
                                        if(clang::ForStmt *forStmt = clang::dyn_cast<clang::ForStmt>(ancestor)){
                                            if(forStmt->getCond()!=nullptr) {
                                                c = seq_info.get_pdg(func_decl)->get_node_stmt(forStmt->getCond())->getID();
                                            }
                                        }
                                        else if(clang::WhileStmt *whileStmt = clang::dyn_cast<clang::WhileStmt>(ancestor)){
                                            if(whileStmt->getCond() !=nullptr) {
                                                c = seq_info.get_pdg(func_decl)->get_node_stmt(whileStmt->getCond())->getID();
                                                // std::cout<<Tool::get_stmt_string(whileStmt)<<" find "<<n<<"\n";
                                            }
                                        }
                                        else if(clang::DoStmt *doStmt = clang::dyn_cast<clang::DoStmt>(ancestor)){
                                            if(doStmt->getCond() !=nullptr) {
                                                c = seq_info.get_pdg(func_decl)->get_node_stmt(doStmt->getCond())->getID();
                                            }
                                        }
                                        if(c!=-1){
                                            conditions.push_back(c);
                                            // c是loopStmt的条件
                                            // std::cout<<"LinkType ["<<lk.printT()<<"] "<<Tool::get_stmt_string(p.node)<<" check related "<<c<<"\n";
                                            std::unordered_set<Link, Link_hash, Link_equal> rnodes = seq_info.get_pdg(func_decl)->get_rela_nodes(n);
                                            for(const Link &lk:rnodes){
                                                int x = lk.nodeID;
                                                if(seq_info.get_pdg(func_decl)->is_branch_condition(x)){
                                                    p.conditions_related.insert(x);
                                                    auto ancestor = seq_info.get_pdg(func_decl)->get_node_ID(x);
                                                    // std::cout<<"Loop condition related branch condition!! " << Tool::get_stmt_string(ancestor)\
                                                    //         <<"\t"<<"LinkType ["<<lk.printT()<<"]\n";
                                                }
                                            } 
                                            // std::cout<<"check end "<<"\n";  
                                        }
                                        
                                    }
                                }
                                if(p.get_warning() == true){
                                    if(p.pnode == nullptr){
                                        std::cout << "[]Warning Point not matched a PDGNode !\n";
                                        continue;
                                    }
                                    /* get dependent nodes */
                                    std::unordered_set<Link, Link_hash, Link_equal> nodes = seq_info.get_pdg(func_decl)->get_rela_nodes(p.pnode->getID());

                                    /* 处理告警点依赖的节点 */
                                    /* find var in nodes */
                                    std::cout<<"Have "<<nodes.size()<<" related nodes:";
                                    
                                    for(const Link &lk:nodes){
                                        int n = lk.nodeID;
                                        LinkType T = lk.T;
                                        clang::Stmt *rela_stmt = seq_info.get_pdg(func_decl)->get_node_ID(n);
                                        // std::cout<<"LinkType ["<<lk.printT()<<"] node["<<n<<"] "<<Tool::get_stmt_string(rela_stmt)<<"\n";
                                        if(rela_stmt==nullptr) continue;
                                        std::vector<const VarDecl *> temp_VDs = seq_info.get_pdg(func_decl)->get_vars(n);
                                        for(auto &VD : temp_VDs){
                                            seq_info.add_rela_variables(VD); // related variable
                                        }
                                        // ASTTypeChecker *atc = ASTTypeChecker::getInstance(_ctx,_rewriter,seq_info); //类型检查
                                        atc->TraverseStmt(rela_stmt);
                                    } 
                                    std::cout<<"==end of warning point.\n";
                                    
                                }
                                // std::cout<<"match!"<<p.id<<"\n";
                            }
                        }
                    }
                }
                
                
                cfc->TraverseStmt(body_stmt);            
                nba->TraverseStmt(body_stmt);            

                atc->update_type_info();
                asc->update_special_info();
                
                cfc->update_controlflow_info();
                ra->update_resource_info();
                ea->update_exception_info();
                nba->update_nestedblock_info();
                
                
                try{
                    atc->update_type_info();
                    asc->update_special_info();
                    acc->update_control_info();
                }catch (const std::exception& e) {
                    std::cout << "捕获到异常: " << e.what() << std::endl;
                }
                
                compute_sensitive();
                std::cout<<"==end of a function.\n";
            }

        }
        //ASTControlFlowCounter cf_counter(_ctx, _rewriter, seq_info);
        //cf_counter.set_check_func(func_decl);
        //cf_counter.TraverseStmt(func_decl->getBody());
        //cf_counter.update_info(); // 存入 seq_info

        return true;
    }
    void SeqInfo::print_used_array(){
        std::cout<<"========print_used_array index!========\n";
        for(auto &it : used_array){
            std::cout<<Tool::get_decl_string(it.first)<<" [";
            for(auto &i:it.second) std::cout<<i<<", ";
            std::cout<<"]\n";
        }
        std::cout<<"=======================================\n";
    }
    void SeqInfo::print_used_record(){
        std::cout<<"========print_used_record field!========\n";
        for(auto &it : used_record){
            std::cout<<Tool::get_decl_string(it.first)<<" [";
            for(auto &i:it.second) std::cout<<Tool::get_decl_string(i)<<", ";
            std::cout<<"]\n";
        }
        std::cout<<"==========================================\n";
    }
    void SeqInfo::print_record_vars(){
        std::cout<<"========print_record_vars numbers!========\n";
        for(auto &it : record_vars){
            std::cout<<Tool::get_decl_string(it.first)<<" [";
            for(auto &i:it.second) std::cout<<Tool::get_decl_string(i)<<", ";
            std::cout<<"]\n";
        }
        std::cout<<"==========================================\n";
    }
    void SeqInfo::print_used_macro(){
        std::cout<<"========print_used_macro!========\n";
        std::cout<<"[";
        for(auto &[a,b]:macro_map){
            if(b==0)continue;
            std::cout<< a<<"\t";
        }
        std::cout<<"]\n";
        std::cout<<"==========================================\n";
    }
    void SeqInfo::update_all_types_info(){
        std::cout<<"========update_all_types_info========\n";
        for(auto &[a,b]:func_types_info){
            // std::cout<<a->getNameAsString()<<" "<<b<<"\n";
            all_types_info |= b;
        }
        std::cout<<"============all_types_info "<<all_types_info<<"=======\n\n";
    }

    
    void SeqInfo::update_all_special_info(){
        std::cout<<"========update_all_special_info========\n";
        for(auto &[a,b]:func_special_info){
            // std::cout<<a->getNameAsString()<<" "<<b<<"\n";
            all_special_info |= b;
        }
        std::cout<<"============all_special_info "<<all_special_info<<"=======\n\n";
    }
    void SeqInfo::update_all_control_info(){
        std::cout<<"========update_all_control_info========\n";
        for(auto &[a,b]:func_control_info){
            // std::cout<<a->getNameAsString()<<" "<<b<<"\n";
            all_control_info |= b;
        }
        std::cout<<"============all_control_info "<<all_control_info<<"=======\n\n";
    }
    SeqInfo::~SeqInfo(){
        for (auto& [_, v] : pdgs) {
            // v->printGraph();
            delete v;
        }
        ASTFieldChecker::destroyInstance();//销毁
        ASTSpecialChecker::destroyInstance();
        ASTMacroChecker::destroyInstance();
        ASTTypeChecker::destroyInstance();
        ASTControlFChecker::destroyInstance();
    }
    
    void SeqInfo::update_all_controlflow_info(){
        std::cout<<"========update_all_controlflow_info========\n";
        // all_controlflow_info.if_count = 0;
        // all_controlflow_info.switch_count = 0;
        // all_controlflow_info.case_count = 0;
        // all_controlflow_info.for_count = 0;
        // all_controlflow_info.while_count = 0;
        // all_controlflow_info.do_while_count = 0;
        for(auto &[a, b] : func_controlflow_info){
            // std::cout<<a->getNameAsString()<<" -> "<<b<<"\n";
            all_controlflow_info |= b;
        }
        std::cout<<"============all_controlflow_info "<<all_controlflow_info<<"=======\n\n";
    }
    
    void SeqInfo::update_all_resource_info() {
        std::cout << "========update_all_resource_info========\n";
        all_resource_info.file_open=0;
        all_resource_info.file_close=0;
        all_resource_info.memory_alloc=0;
        all_resource_info.memory_free=0;
        all_resource_info.net_socket=0;
        all_resource_info.net_connect=0;
        all_resource_info.db_access=0;
        for (auto &[FD, info] : func_resource_info) {
            // std::cout << FD->getNameAsString() << " -> "
            //           << "FileOpen: " << info.file_open << ", "
            //           << "FileClose: " << info.file_close << ", "
            //           << "MemAlloc: " << info.memory_alloc << ", "
            //           << "MemFree: " << info.memory_free << ", "
            //           << "Socket: " << info.net_socket << ", "
            //           << "Connect: " << info.net_connect << ", "
            //           << "DBAccess: " << info.db_access << "\n";
            all_resource_info |= info;
        }
        std::cout << "============all_resource_info========\n"
                  << "FileOpen: " << all_resource_info.file_open << ", "
                  << "FileClose: " << all_resource_info.file_close << ", "
                  << "MemAlloc: " << all_resource_info.memory_alloc << ", "
                  << "MemFree: " << all_resource_info.memory_free << ", "
                  << "Socket: " << all_resource_info.net_socket << ", "
                  << "Connect: " << all_resource_info.net_connect << ", "
                  << "DBAccess: " << all_resource_info.db_access << "\n\n";
    }
    
    void SeqInfo::update_all_exception_info() {
        std::cout << "========update_all_exception_info========\n";
        all_exception_info.try_count=0;
        all_exception_info.catch_count=0;
        all_exception_info.throw_count=0;
        all_exception_info.setjmp_count=0;
        all_exception_info.longjmp_count=0;
        // 遍历存储函数异常信息的容器
        for (auto &[FD, exception_info] : func_exception_info) {
            // std::cout << FD->getNameAsString() << " -> "
            //           << "Try: " << exception_info.try_count
            //           << ", Catch: " << exception_info.catch_count
            //           << ", Throw: " << exception_info.throw_count
            //           << ", Setjmp: " << exception_info.setjmp_count
            //           << ", Longjmp: " << exception_info.longjmp_count
            //           << ", Max Depth: " << exception_info.max_nesting_depth << "\n";
            
            // 合并当前函数的异常信息到全局信息
            all_exception_info |= exception_info;  // 这个运算符重载会合并两个 ExceptionHandlingInfo 对象
        }
    
        // 输出最终的累计异常信息
        std::cout << "============all_exception_info========\n"
                  << "Try: " << all_exception_info.try_count
                  << ", Catch: " << all_exception_info.catch_count
                  << ", Throw: " << all_exception_info.throw_count
                  << ", Setjmp: " << all_exception_info.setjmp_count
                  << ", Longjmp: " << all_exception_info.longjmp_count
                  << ", Max Depth: " << all_exception_info.max_nesting_depth << "\n\n";
    }    

    void SeqInfo::update_all_nestedblock_info() {
        std::cout << "========update_all_nestedblock_info========\n";
    
        all_nestedblock_info.if_if_count=0;
        all_nestedblock_info.if_loop_count=0;
        all_nestedblock_info.loop_loop_count=0;
        // 遍历存储每个函数嵌套结构信息的容器
        for (auto &[FD, nested_info] : func_nestedblock_info) {
            // std::cout << FD->getNameAsString() << " -> "
            //           << "IF-IF: " << nested_info.if_if_count
            //           << ", IF-LOOP: " << nested_info.if_loop_count
            //           << ", LOOP-LOOP: " << nested_info.loop_loop_count
            //           << ", Max Depth: " << nested_info.max_nesting_depth << "\n";
    
            // 合并当前函数的嵌套结构信息到全局信息
            
                 
            all_nestedblock_info |= nested_info; 
        }
    
        // 输出最终的累计嵌套信息
        std::cout << "============all_nestedblock_info========\n"
                  << "IF-IF: " << all_nestedblock_info.if_if_count
                  << ", IF-LOOP: " << all_nestedblock_info.if_loop_count
                  << ", LOOP-LOOP: " << all_nestedblock_info.loop_loop_count
                  << ", Max Depth: " << all_nestedblock_info.max_nesting_depth << "\n\n";
    }
    

    SeqASTVisitor::~SeqASTVisitor(){
        compute_seq(); 
    }
    void SeqASTVisitor::compute_sensitive(){
        seq_info.setCtx(_ctx);
        seq_info.print_used_array();
        seq_info.update_all_types_info();
        seq_info.update_all_special_info();
        seq_info.update_all_controlflow_info();
        seq_info.update_all_resource_info();
        seq_info.update_all_exception_info();
        seq_info.update_all_nestedblock_info();


        // if(seq_info.get_config("Sensitive_Flow")==true)
        seq_info.compute_flowS();

        // if(seq_info.get_config("Sensitive_Path")==true)
        seq_info.compute_pathS();

        // if(seq_info.get_config("Sensitive_Field")==true)
        seq_info.compute_fieldS();

        // if(seq_info.get_config("Sensitive_Context")==true)
        seq_info.compute_contextS();
    }
    void SeqASTVisitor::compute_seq(){
        seq_info.setCtx(_ctx);
        try{
            seq_info.update_all_types_info();
            seq_info.update_all_special_info();
            seq_info.update_all_control_info();
        }catch (const std::exception& e) {
        std::cout << "捕获到异常: " << e.what() << std::endl;
        }

        // if(seq_info.get_config("Semantic_Macro_ALL")==true)
        seq_info.compute_macro();

        // if(seq_info.get_config("Semantic_TypeSafe_ALL")==true)
        seq_info.compute_typeSafe();

        // if(seq_info.get_config("Semantic_Special_ALL")==true)
        seq_info.compute_special();
        

        seq_info.compute_controlFlow();
        seq_info.compute_resource();
        seq_info.compute_exception();
        seq_info.compute_nestedblock();
        

        // if(seq_info.get_config("Semantic_Control_ALL")==true)
        seq_info.compute_control();

    }

    void SeqASTVisitor::refrash_seq_info(){
        seq_info.refrash_all_info();
    }
    SeqFactory::SeqFactory(SeqInfo &seq_i): seq_info{ seq_i }{
        std::cout<<"Bug type is "<<seq_i.get_bug_type()<<"\n";
    }
};

   