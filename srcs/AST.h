#ifndef ZULLANG_AST_H
#define ZULLANG_AST_H

#include <string>
#include <utility>
#include "Lexer.h"

class AST {
public:
    virtual ~AST() = default;
};

class ImmRealAST : public AST {
    double val;
public:
    explicit ImmRealAST(double val) : val(val) {}
};

class ImmIntAST : public AST {
    long long val;
public:
    explicit ImmIntAST(long long val) : val(val) {}
};

class ImmStringAST : public AST {
    std::string val;
public:
    explicit ImmStringAST(std::string val) : val(std::move(val)) {}
};

class VariableAST : public AST {
    std::u16string name;
    int type;
public:
    VariableAST(std::u16string name, int type) : name(std::move(name)), type(type) {}
};

class BinOpAST : public AST {
    std::unique_ptr<AST> left, right;
    Token op;
public:
    BinOpAST(std::unique_ptr<AST> left, std::unique_ptr<AST> right, Token op) : left(std::move(left)), right(std::move(right)), op(op) {}
};

class UnaryOpAST : public AST {
    std::unique_ptr<AST> ast;
    Token op;
public:
    UnaryOpAST(std::unique_ptr<AST> ast, Token op) : ast(std::move(ast)), op(op) {}
};

class FuncCallAST : public AST {
    std::u16string name;
    std::vector<std::unique_ptr<AST>> args;
};

class FuncAST : public AST {
    int return_type = 0;
    std::vector<VariableAST> params;
    std::unique_ptr<AST> body;
public:
    FuncAST(int return_type, std::vector<VariableAST> params, std::unique_ptr<AST> body)
            : return_type(return_type), params(std::move(params)), body(std::move(body)) {}
};

#endif //ZULLANG_AST_H
