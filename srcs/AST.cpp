#include "AST.h"

using namespace std;
using namespace llvm;

FuncProtoAST::FuncProtoAST(string name, int return_type, vector<pair<string, int>> params, bool has_body,
                           bool is_var_arg) :
        name(std::move(name)), return_type(return_type), params(std::move(params)), has_body(has_body),
        is_var_arg(is_var_arg) {}

llvm::Function *FuncProtoAST::code_gen(ZulContext &zulctx) {
    vector<llvm::Type *> param_types;
    param_types.reserve(params.size());
    for (auto &param: params) {
        param_types.emplace_back(get_llvm_type(*zulctx.context, param.second));
    }
    auto func_type = llvm::FunctionType::get(get_llvm_type(*zulctx.context, return_type), param_types, is_var_arg);
    auto llvm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, *zulctx.module);
    int i = 0;
    for (auto &arg: llvm_func->args()) {
        arg.setName(params[i++].first);
    }
    return llvm_func;
}

FuncRetAST::FuncRetAST(ASTPtr body, Capture<int> return_type) :
        body(std::move(body)), return_type(std::move(return_type)) {}

ZulValue FuncRetAST::code_gen(ZulContext &zulctx) {
    ZulValue body_value = nullzul;
    if (body) {
        body_value = body->code_gen(zulctx);
        if (!body_value.first)
            return nullzul;
    }
    if (return_type.value == -1) {
        if (zulctx.ret_count == 1) {
            zulctx.builder.CreateRetVoid();
        } else {
            zulctx.builder.CreateBr(zulctx.return_block);
        }
    } else {
        if (return_type.value != body_value.second && !create_cast(zulctx, body_value, return_type.value)) {
            System::logger.log_error(return_type.loc, return_type.word_size,
                                     {"리턴 타입이 일치하지 않습니다. 반환 구문의 타입 \"", get_type_name(body_value.second),
                                      "\" 에서 리턴 타입 \"", get_type_name(return_type.value), "\" 로 캐스팅 할 수 없습니다"});
        } else if (zulctx.ret_count == 1) {
            zulctx.builder.CreateRet(body_value.first);
        } else {
            zulctx.builder.CreateStore(body_value.first, zulctx.return_var);
            zulctx.builder.CreateBr(zulctx.return_block);
        }
    }
    return {nullptr, -10};
}

bool FuncRetAST::is_const() {
    return false;
}

int FuncRetAST::get_typeid(ZulContext &zulctx) {
    return return_type.value;
}

IfAST::IfAST(CondBodyPair if_pair, std::vector<CondBodyPair> elif_pair_list, std::vector<ASTPtr> else_body) :
        if_pair(std::move(if_pair)), elif_pair_list(std::move(elif_pair_list)), else_body(std::move(else_body)) {}

ZulValue IfAST::code_gen(ZulContext &zulctx) {
    auto prev_cond = if_pair.first->code_gen(zulctx);
    if (!prev_cond.first || !to_boolean_expr(zulctx, prev_cond))
        return nullzul;

    auto func = zulctx.builder.GetInsertBlock()->getParent();
    auto body_block = llvm::BasicBlock::Create(*zulctx.context, "if", func);
    auto merge_block = llvm::BasicBlock::Create(*zulctx.context, "merge");
    auto prev_block = zulctx.builder.GetInsertBlock();

    zulctx.builder.SetInsertPoint(body_block);
    bool interrupted = false;
    for (auto &ast: if_pair.second) {
        auto code = ast->code_gen(zulctx).second;
        if (code == -10) {
            interrupted = true;
            break;
        }
    }
    if (!interrupted)
        zulctx.builder.CreateBr(merge_block);

    for (auto &elif_pair: elif_pair_list) {
        auto elif_cond_block = llvm::BasicBlock::Create(*zulctx.context, "elif_cond", func);
        zulctx.builder.SetInsertPoint(prev_block);
        zulctx.builder.CreateCondBr(prev_cond.first, body_block, elif_cond_block);

        zulctx.builder.SetInsertPoint(elif_cond_block);
        prev_cond = elif_pair.first->code_gen(zulctx);
        if (!prev_cond.first || !to_boolean_expr(zulctx, prev_cond))
            return nullzul;

        body_block = llvm::BasicBlock::Create(*zulctx.context, "elif", func);
        zulctx.builder.SetInsertPoint(body_block);
        interrupted = false;
        for (auto &ast: elif_pair.second) {
            auto code = ast->code_gen(zulctx).second;
            if (code == -10) {
                interrupted = true;
                break;
            }
        }
        if (!interrupted)
            zulctx.builder.CreateBr(merge_block);
        prev_block = elif_cond_block;
    }
    if (else_body.empty()) {
        zulctx.builder.SetInsertPoint(prev_block);
        zulctx.builder.CreateCondBr(prev_cond.first, body_block, merge_block);
    } else {
        auto else_block = llvm::BasicBlock::Create(*zulctx.context, "else", func);
        zulctx.builder.SetInsertPoint(prev_block);
        zulctx.builder.CreateCondBr(prev_cond.first, body_block, else_block);

        zulctx.builder.SetInsertPoint(else_block);
        interrupted = false;
        for (auto &ast: else_body) {
            auto code = ast->code_gen(zulctx).second;
            if (code == -10) {
                interrupted = true;
                break;
            }
        }
        if (!interrupted)
            zulctx.builder.CreateBr(merge_block);
    }
    func->insert(func->end(), merge_block);
    zulctx.builder.SetInsertPoint(merge_block);
    return nullzul;
}

bool IfAST::is_const() {
    return false;
}

int IfAST::get_typeid(ZulContext &zulctx) {
    return -1;
}

LoopAST::LoopAST(ASTPtr init_body, ASTPtr test_body, ASTPtr update_body, std::vector<ASTPtr> loop_body) :
        init_body(std::move(init_body)), test_body(std::move(test_body)), update_body(std::move(update_body)),
        loop_body(std::move(loop_body)) {}

ZulValue LoopAST::code_gen(ZulContext &zulctx) {
    auto func = zulctx.builder.GetInsertBlock()->getParent();
    auto test_block = llvm::BasicBlock::Create(*zulctx.context, "loop_test", func);
    auto start_block = llvm::BasicBlock::Create(*zulctx.context, "loop_start", func);
    auto update_block = llvm::BasicBlock::Create(*zulctx.context, "loop_update", func);
    auto end_block = llvm::BasicBlock::Create(*zulctx.context, "loop_end", func);
    zulctx.loop_update_stack.push(update_block);
    zulctx.loop_end_stack.push(end_block);
    Guard guard{[&zulctx]() { //리턴 할 때 자동으로 pop하도록 가드 생성
        zulctx.loop_update_stack.pop();
        zulctx.loop_end_stack.pop();
    }};

    if (init_body && !init_body->code_gen(zulctx).first)
        return nullzul;

    zulctx.builder.CreateBr(test_block);
    zulctx.builder.SetInsertPoint(test_block);
    if (test_body) {
        ZulValue test_cond = test_body->code_gen(zulctx);
        if (!test_cond.first || !to_boolean_expr(zulctx, test_cond))
            return nullzul;
        zulctx.builder.CreateCondBr(test_cond.first, start_block, end_block);
    } else {
        zulctx.builder.CreateBr(start_block);
    }

    bool interrupted = false;
    zulctx.builder.SetInsertPoint(start_block);
    for (auto &ast: loop_body) {
        auto code = ast->code_gen(zulctx);
        if (code.second == -10) {
            interrupted = true;
            break;
        }
    }
    if (!interrupted)
        zulctx.builder.CreateBr(update_block);

    zulctx.builder.SetInsertPoint(update_block);
    if (update_body && !update_body->code_gen(zulctx).first)
        return nullzul;
    zulctx.builder.CreateBr(test_block);

    zulctx.builder.SetInsertPoint(end_block);
    return nullzul;
}

bool LoopAST::is_const() {
    return false;
}

int LoopAST::get_typeid(ZulContext &zulctx) {
    return -1;
}

ZulValue ContinueAST::code_gen(ZulContext &zulctx) {
    zulctx.builder.CreateBr(zulctx.loop_update_stack.top());
    return {nullptr, -10};
}

bool ContinueAST::is_const() {
    return false;
}

int ContinueAST::get_typeid(ZulContext &zulctx) {
    return -1;
}

ZulValue BreakAST::code_gen(ZulContext &zulctx) {
    zulctx.builder.CreateBr(zulctx.loop_end_stack.top());
    return {nullptr, -10};
}

bool BreakAST::is_const() {
    return false;
}

int BreakAST::get_typeid(ZulContext &zulctx) {
    return -1;
}

VariableAST::VariableAST(string name) : name(std::move(name)) {}

ZulValue VariableAST::get_origin_value(ZulContext &zulctx) {
    if (zulctx.local_var_map.contains(name))
        return zulctx.local_var_map[name];
    if (zulctx.global_var_map.contains(name))
        return zulctx.global_var_map[name];
    return nullzul;
}

ZulValue VariableAST::code_gen(ZulContext &zulctx) {
    auto value = get_origin_value(zulctx);
    if (!value.first)
        return nullzul;
    value.first = zulctx.builder.CreateLoad(get_llvm_type(*zulctx.context, value.second), value.first);
    return value;
}

bool VariableAST::is_const() {
    return false;
}

int VariableAST::get_typeid(ZulContext &zulctx) {
    return get_origin_value(zulctx).second;
}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, int type, ASTPtr body, ZulContext &zulctx) :
        name(std::move(name)), type(type), body(std::move(body)) {
    register_var(zulctx);
}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, int type, ZulContext &zulctx) :
        name(std::move(name)), type(type) {
    register_var(zulctx);
}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, ASTPtr body, ZulContext &zulctx) :
        name(std::move(name)), body(std::move(body)) {
    register_var(zulctx);
}

void VariableDeclAST::register_var(ZulContext &zulctx) {
    int t = ((type == -1) ? body->get_typeid(zulctx) : type);
    zulctx.local_var_map.emplace(name.value, make_pair(nullptr, t)); //이름만 등록 해놓기
    if (!zulctx.scope_stack.empty()) { //만약 스코프 안에 있다면
        zulctx.scope_stack.top().push_back(name.value); //가장 가까운 스코프에 변수 등록
    }
}

ZulValue VariableDeclAST::code_gen(ZulContext &zulctx) {
    Value *init_val = nullptr;
    if (body) { //대입식이 있으면 식 먼저 생성
        auto result = body->code_gen(zulctx);
        if (!result.first)
            return nullzul;
        if (type != -1 && type != result.second && !create_cast(zulctx, result, type)) {
            System::logger.log_error(name.loc, name.word_size,
                                     {"대입 연산식의 타입 \"",
                                      get_type_name(result.second), "\" 에서 변수의 타입 \"",
                                      get_type_name(type),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
        if (type == -1) //타입 명시가 안됐을 때만
            type = result.second; //추론된 타입 적용
        init_val = result.first;
    }
    if (type == -1) {
        System::logger.log_error(name.loc, name.word_size, {"\"", get_type_name(type), "\" 타입의 변수를 생성할 수 없습니다"});
        return nullzul;
    }
    auto func = zulctx.builder.GetInsertBlock()->getParent();
    llvm::IRBuilder<> entry_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    auto alloca_val = entry_builder.CreateAlloca(get_llvm_type(*zulctx.context, type), nullptr, name.value);
    zulctx.local_var_map[name.value] = make_pair(alloca_val, type);

    if (body)
        zulctx.builder.CreateStore(init_val, alloca_val);
    return {alloca_val, type};
}

bool VariableDeclAST::is_const() {
    return false;
}

int VariableDeclAST::get_typeid(ZulContext &zulctx) {
    return -1;
}

VariableAssnAST::VariableAssnAST(unique_ptr<LvalueAST> target, Capture<Token> op, ASTPtr body) :
        target(std::move(target)), op(std::move(op)), body(std::move(body)) {}

ZulValue VariableAssnAST::code_gen(ZulContext &zulctx) {
    static unordered_map<Token, Token> assn_op_map = {
            {tok_mul_assn,    tok_mul},
            {tok_div_assn,    tok_div},
            {tok_mod_assn,    tok_mod},
            {tok_add_assn,    tok_add},
            {tok_sub_assn,    tok_sub},
            {tok_lshift_assn, tok_lshift},
            {tok_rshift_assn, tok_rshift},
            {tok_and_assn,    tok_bitand},
            {tok_or_assn,     tok_bitor},
            {tok_xor_assn,    tok_bitxor}
    };
    auto target_value = target->code_gen(zulctx);
    auto body_value = body->code_gen(zulctx);

    if (!target_value.first || !body_value.first ||
        target_value.second > FLOAT_TYPEID || body_value.second > FLOAT_TYPEID) {
        System::logger.log_error(op.loc, op.word_size,
                                 {"대입 연산식의 타입 \"",
                                  get_type_name(target_value.second), "\" 과 변수의 타입 \"",
                                  get_type_name(body_value.second),
                                  "\" 사이에 적절한 연산자 오버로드가 없습니다"});
        return nullzul;
    }

    if (target_value.second != body_value.second && !create_cast(zulctx, body_value, target_value.second)) {
        System::logger.log_error(op.loc, op.word_size,
                                 {"대입 연산식의 타입 \"",
                                  get_type_name(target_value.second), "\" 에서 변수의 타입 \"",
                                  get_type_name(body_value.second),
                                  "\" 로 캐스팅 할 수 없습니다"});
        return nullzul;
    }
    llvm::Value *result;
    if (op.value == tok_assn) {
        result = body_value.first;
    } else {
        auto prac_op = Capture(assn_op_map[op.value], op.loc, op.word_size);
        if (target_value.second < FLOAT_TYPEID) {
            result = create_int_operation(zulctx, target_value.first, body_value.first, prac_op);
        } else {
            result = create_float_operation(zulctx, target_value.first, body_value.first, prac_op);
        }
        if (!result)
            return nullzul;
    }
    return {zulctx.builder.CreateStore(result, target->get_origin_value(zulctx).first), target_value.second};
}

bool VariableAssnAST::is_const() {
    return false;
}

int VariableAssnAST::get_typeid(ZulContext &zulctx) {
    return -1;
}

BinOpAST::BinOpAST(ASTPtr left, ASTPtr right, Capture<Token> op) : left(std::move(left)),
                                                                   right(std::move(right)),
                                                                   op(std::move(op)) {}

ZulValue BinOpAST::short_circuit_code_gen(ZulContext &zulctx) const {
    auto lhs = left->code_gen(zulctx);
    if (!lhs.first)
        return nullzul;
    if (!to_boolean_expr(zulctx, lhs)) {
        System::logger.log_error(op.loc, op.word_size, "좌측항을 \"논리\" 자료형으로 캐스팅 할 수 없습니다");
        return nullzul;
    }

    auto origin_block = zulctx.builder.GetInsertBlock();
    auto func = zulctx.builder.GetInsertBlock()->getParent();
    auto sc_test = llvm::BasicBlock::Create(*zulctx.context, "sc_test", func);
    auto sc_end = llvm::BasicBlock::Create(*zulctx.context, "sc_end", func);

    if (op.value == tok_and)
        zulctx.builder.CreateCondBr(lhs.first, sc_test, sc_end);
    else
        zulctx.builder.CreateCondBr(lhs.first, sc_end, sc_test);

    zulctx.builder.SetInsertPoint(sc_test);
    auto rhs = right->code_gen(zulctx);
    if (!rhs.first)
        return nullzul;
    if (!to_boolean_expr(zulctx, rhs)) {
        System::logger.log_error(op.loc, op.word_size, "우측항을 \"논리\" 자료형으로 캐스팅 할 수 없습니다");
        return nullzul;
    }
    zulctx.builder.CreateBr(sc_end);

    zulctx.builder.SetInsertPoint(sc_end);
    auto phi = zulctx.builder.CreatePHI(get_llvm_type(*zulctx.context, 0), 2);
    phi->addIncoming(llvm::ConstantInt::getBool(*zulctx.context, op.value == tok_or), origin_block);
    phi->addIncoming(rhs.first, sc_test);
    return {phi, 0};
}

ZulValue BinOpAST::code_gen(ZulContext &zulctx) {
    if (op.value == tok_and || op.value == tok_or) {
        return short_circuit_code_gen(zulctx);
    }
    auto lhs = left->code_gen(zulctx);
    auto rhs = right->code_gen(zulctx);
    if (!lhs.first || !rhs.first)
        return nullzul;
    if (lhs.second > FLOAT_TYPEID || rhs.second > FLOAT_TYPEID) { //연산자 오버로딩 지원 하게되면 변경
        System::logger.log_error(op.loc, op.word_size, {"좌측항의 타입 \"",
                                                        get_type_name(lhs.second), "\" 과 우측항의 타입 \"",
                                                        get_type_name(rhs.second),
                                                        "\" 사이에 적절한 연산자 오버로드가 없습니다"});
        return nullzul;
    }
    int calc_type = lhs.second;
    if (lhs.second > rhs.second) {
        calc_type = lhs.second;
        if (!create_cast(zulctx, rhs, lhs.second)) {
            System::logger.log_error(op.loc, op.word_size,
                                     {"우측항의 타입 \"",
                                      get_type_name(rhs.second), "\" 에서 좌측항의 타입 \"",
                                      get_type_name(lhs.second),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
    } else if (lhs.second < rhs.second) {
        calc_type = rhs.second;
        if (!create_cast(zulctx, lhs, rhs.second)) {
            System::logger.log_error(op.loc, op.word_size,
                                     {"좌측항의 타입 \"",
                                      get_type_name(lhs.second), "\" 에서 우측항의 타입 \"",
                                      get_type_name(rhs.second),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
    }
    llvm::Value *ret;
    if (calc_type < FLOAT_TYPEID) {
        ret = create_int_operation(zulctx, lhs.first, rhs.first, op);
    } else {
        ret = create_float_operation(zulctx, lhs.first, rhs.first, op);
    }
    calc_type = is_cmp(op.value) ? BOOL_TYPEID : calc_type;
    return {ret, calc_type};
}

bool BinOpAST::is_const() {
    return left->is_const() && right->is_const() && op.value != tok_and && op.value != tok_or;
}

int BinOpAST::get_typeid(ZulContext &zulctx) {
    if (op.value == tok_and || op.value == tok_or)
        return 0;
    int ltype = left->get_typeid(zulctx);
    int rtype = right->get_typeid(zulctx);
    if (ltype > FLOAT_TYPEID || rtype > FLOAT_TYPEID || ltype < BOOL_TYPEID || rtype < BOOL_TYPEID)
        return -1;
    return max(ltype, rtype);
}

UnaryOpAST::UnaryOpAST(ASTPtr body, Capture<Token> op) : body(std::move(body)), op(std::move(op)) {}

ZulValue UnaryOpAST::code_gen(ZulContext &zulctx) {
    auto body_value = body->code_gen(zulctx);
    auto zero = get_const_zero(body_value.first->getType(), body_value.second);
    if (!body_value.first)
        return nullzul;
    if (body_value.second > FLOAT_TYPEID) {
        System::logger.log_error(op.loc, op.word_size, "단항 연산자를 적용할 수 없습니다");
        return nullzul;
    }
    switch (op.value) {
        case tok_add:
            break;
        case tok_sub:
            if (body_value.second < FLOAT_TYPEID)
                return {zulctx.builder.CreateSub(zero, body_value.first), body_value.second};
            else
                return {zulctx.builder.CreateFSub(zero, body_value.first), body_value.second};
        case tok_not:
            if (body_value.second < FLOAT_TYPEID)
                return {zulctx.builder.CreateICmpEQ(zero, body_value.first), 0};
            else
                return {zulctx.builder.CreateFCmpOEQ(zero, body_value.first), 0};
        case tok_bitnot:
            if (body_value.second == FLOAT_TYPEID) {
                System::logger.log_error(op.loc, op.word_size, "단항 '~' 연산자를 적용할 수 없습니다");
                return nullzul;
            }
            return {zulctx.builder.CreateNot(body_value.first), body_value.second};
        default:
            System::logger.log_error(op.loc, op.word_size, "올바른 단항 연산자가 아닙니다");
            return nullzul;
    }
    return body_value;
}

bool UnaryOpAST::is_const() {
    return body->is_const();
}

int UnaryOpAST::get_typeid(ZulContext &zulctx) {
    if (op.value == tok_not)
        return 0;
    return body->get_typeid(zulctx);
}

SubscriptAST::SubscriptAST(std::unique_ptr<VariableAST> target, Capture<ASTPtr> index) : target(std::move(target)),
                                                                                         index(std::move(index)) {}

ZulValue SubscriptAST::get_origin_value(ZulContext &zulctx) {
    auto target_val = target->get_origin_value(zulctx);
    auto index_val = index.value->code_gen(zulctx);
    if (!target_val.first || !index_val.first)
        return nullzul;
    if (target_val.second < TYPE_COUNTS) {
        System::logger.log_error(index.loc, index.word_size, "'[]' 연산자를 사용할 수 없습니다. 배열이 아닙니다.");
        return nullzul;
    }
    if (index_val.second != 2) {
        System::logger.log_error(index.loc, index.word_size, "배열의 인덱스는 정수여야 합니다");
        return nullzul;
    }
    auto elm_type = get_llvm_type(*zulctx.context, target_val.second - TYPE_COUNTS);
    auto elm_ptr = zulctx.builder.CreateGEP(elm_type, target_val.first, {index_val.first});
    return {elm_ptr, target_val.second - TYPE_COUNTS};
}

ZulValue SubscriptAST::code_gen(ZulContext &zulctx) {
    auto elm_ptr = get_origin_value(zulctx);
    auto loaded = zulctx.builder.CreateLoad(elm_ptr.first->getType(), elm_ptr.first);
    return {loaded, elm_ptr.second};
}

bool SubscriptAST::is_const() {
    return false;
}

int SubscriptAST::get_typeid(ZulContext &zulctx) {
    return target->get_typeid(zulctx) - TYPE_COUNTS;
}

FuncCallAST::FuncCallAST(FuncProtoAST &proto, vector<Capture<ASTPtr>> args)
        : proto(proto), args(std::move(args)) {}

ZulValue FuncCallAST::code_gen(ZulContext &zulctx) {
    auto target_func = zulctx.module->getFunction(proto.name);
    vector<llvm::Value *> arg_values;
    bool has_error = false;
    arg_values.reserve(args.size());
    for (int i = 0; i < args.size(); i++) {
        auto arg = args[i].value->code_gen(zulctx);
        if (!arg.first)
            return nullzul;
        if (i < proto.params.size() && arg.second != proto.params[i].second &&
            !create_cast(zulctx, arg, proto.params[i].second)) {
            //arg와 param의 타입이 맞지 않으면 캐스팅 시도
            System::logger.log_error(args[i].loc, args[i].word_size, {
                    "인자의 타입 \"", get_type_name(arg.second), "\" 에서 매개변수의 타입 \"",
                    get_type_name(proto.params[i].second), "\" 로 캐스팅 할 수 없습니다"});
            has_error = true;
        }
        arg_values.push_back(arg.first);
    }
    if (has_error)
        return nullzul;
    return {zulctx.builder.CreateCall(target_func, arg_values), proto.return_type};
}

bool FuncCallAST::is_const() {
    return false;
}

int FuncCallAST::get_typeid(ZulContext &zulctx) {
    return proto.return_type;
}

ImmBoolAST::ImmBoolAST(bool val) : val(val) {}

ZulValue ImmBoolAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(1, val)), BOOL_TYPEID};
}

bool ImmBoolAST::is_const() {
    return true;
}

int ImmBoolAST::get_typeid(ZulContext &zulctx) {
    return BOOL_TYPEID;
}

ImmCharAST::ImmCharAST(char val) : val(val) {}

ZulValue ImmCharAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(8, val)), BOOL_TYPEID + 1};
}

bool ImmCharAST::is_const() {
    return true;
}

int ImmCharAST::get_typeid(ZulContext &zulctx) {
    return BOOL_TYPEID + 1;
}

ImmIntAST::ImmIntAST(long long int val) : val(val) {}

ZulValue ImmIntAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(64, val, true)), FLOAT_TYPEID - 1};
}

bool ImmIntAST::is_const() {
    return true;
}

int ImmIntAST::get_typeid(ZulContext &zulctx) {
    return FLOAT_TYPEID - 1;
}

ImmRealAST::ImmRealAST(double val) : val(val) {}

ZulValue ImmRealAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantFP::get(*zulctx.context, llvm::APFloat(val)), FLOAT_TYPEID};
}

bool ImmRealAST::is_const() {
    return true;
}

int ImmRealAST::get_typeid(ZulContext &zulctx) {
    return FLOAT_TYPEID;
}

ImmStrAST::ImmStrAST(string val) : val(std::move(val)) {}

ZulValue ImmStrAST::code_gen(ZulContext &zulctx) {
    return {zulctx.builder.CreateGlobalStringPtr(val), 4};
}

bool ImmStrAST::is_const() {
    return true;
}

int ImmStrAST::get_typeid(ZulContext &zulctx) {
    return 4;
}

ImmArrAST::ImmArrAST(std::vector<ASTPtr> arr) : arr(std::move(arr)) {}

ZulValue ImmArrAST::code_gen(ZulContext &zulctx) {
    return nullzul;
}

bool ImmArrAST::is_const() {
    for (auto &ast: arr) {
        if (!ast->is_const())
            return false;
    }
    return true;
}

int ImmArrAST::get_typeid(ZulContext &zulctx) {
    if (arr.empty())
        return -1;
    return arr[0]->get_typeid(zulctx) + 5;
}
