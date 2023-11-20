#ifndef ZULLANG_AST_H
#define ZULLANG_AST_H

#include <string>
#include <utility>
#include "Lexer.h"

enum class ZulType {
    type_int,
    type_real,
    type_string
};

class AST {
public:
    virtual ~AST() = default;
};

class ImmRealAST : public AST{
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
    std::string name;
    ZulType type;
public:
    VariableAST(std::string name, ZulType type) : name(std::move(name)), type(type) {}
};

class BinOpAST : public AST {
    std::unique_ptr<AST> left, right;
    Lexer::Token op;
public:
    BinOpAST(std::unique_ptr<AST> left, std::unique_ptr<AST> right, Lexer::Token op) : left(std::move(left)), right(std::move(right)), op(op) {}
};

class UnaryOpAST : public AST {
    std::unique_ptr<AST> ast;
    Lexer::Token op;
public:
    UnaryOpAST(std::unique_ptr<AST> ast, Lexer::Token op) : ast(std::move(ast)), op(op) {}
};

class FuncAST : public AST {

};

#endif //ZULLANG_AST_H
