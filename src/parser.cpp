#include "parser.h"
#include <fstream>
#include <iostream>

namespace wxbasic {

Parser::Parser(const std::string &sourcecode, const std::string &file_name,
               SymbolTable &symbol_table)
    : sym_table(symbol_table) {
    source_name = file_name;
    source = sourcecode;
}

Parser::Parser(const std::string &file_name, SymbolTable &symbol_table)
    : sym_table(symbol_table) {
    source_name = file_name;

    // try to load the file
    std::ifstream input_file(source_name);

    if (!input_file)
        throw Error("Can't open file \"" + source_name + "\"");

    input_file.seekg(0, std::ios::end);
    source.reserve(input_file.tellg());
    input_file.seekg(0, std::ios::beg);

    source.assign((std::istreambuf_iterator<char>(input_file)),
                  std::istreambuf_iterator<char>());
}


// Parse the source code
Code Parser::parse() {

    // Load the content
    tokenizer.load(source, source_name);

    // Vector of bytecode
    code.clear();

    while (!tokenizer.is_token(TokenType::TOK_EOF)) {
        parse_statement();
        tokenizer.next_token();
    }
    return code;
}

// Parse a single statement
void Parser::parse_statement() {

    // empty line
    if (is_seperator()) {
        parse_seperator();
        return;
    }

    switch (tokenizer.token()->type) {
    case TokenType::TOK_PRINT:
        parse_print();
        break;
    default:
        break;
    }
}

void Parser::parse_print() {
    skip();
    if (is_seperator()) {
        // empty print statement.
        code.emit_op(OpcodeType::OP_EMITLN);
        return;
    }
    while (1) {
        // leading ;
        if (tokenizer.is_token(TokenType::TOK_SEMICOLON)) {
            skip();
            if (is_seperator())
                break;
        }

        // leading ,
        else if (tokenizer.is_token(TokenType::TOK_COMMA)) {
            skip();
            code.emit_op(OpcodeType::OP_EMITTAB);
            if (is_seperator()) {
                code.emit_op(OpcodeType::OP_EMITLN);
                break;
            }
        } else if (is_seperator()) {
            break;
        } else {
            parse_expression(0);
            // trailing values
            if (tokenizer.is_token(TokenType::TOK_SEMICOLON)) {
                skip();
                code.emit_op(OpcodeType::OP_PRINT);
            } else if (tokenizer.is_token(TokenType::TOK_COMMA)) {
                skip();
                code.emit_op(OpcodeType::OP_EMITTAB);
                if (is_seperator()) {
                    code.emit_op(OpcodeType::OP_EMITLN);
                }
            } else {
                break;
            }
        }
    }
}

void Parser::parse_expression(int prior_strength) {}

void Parser::parse_operand() {
    switch (tokenizer.token_type()) {

        // unary NOT
    case TokenType::TOK_NOT:
        skip();
        parse_seperator(false);
        parse_operand();
        code.emit_op(OpcodeType::OP_NOT);
        break;

        // unary inverse
    case TokenType::TOK_INV:
        skip();
        parse_seperator(false);
        parse_operand();
        code.emit_op(OpcodeType::OP_INV);
        break;

    case TokenType::TOK_LPAREN:
        skip();
        parse_seperator(false);
        parse_expression(0);
        expect(TokenType::TOK_RPAREN, ")");
        break;

    case TokenType::TOK_BINOP:
        // might be unary + or -
        if (tokenizer.token()->content == "+") {
            skip();
            parse_seperator(false);
            parse_expression(0);
        } else if (tokenizer.token()->content == "-") {
            skip();
            parse_seperator(false);
            if (tokenizer.token_type() == TokenType::TOK_FLOAT) {
                // Add - to the float value
                tokenizer.token()->value.float_val =
                    -tokenizer.token()->value.float_val;
                parse_expression(0);
            } else if (tokenizer.token_type() == TokenType::TOK_INTEGER) {
                // Add - to the integer value
                tokenizer.token()->value.int_val =
                    -tokenizer.token()->value.int_val;
                parse_expression(0);
            } else {
                parse_expression(0);
                code.emit_op(OpcodeType::OP_NEGATE);
            }
        } else {
            throw ParserError("Expected an expression", *this);
        }

        break;

    default:
        throw ParserError("Expected an expression", *this);
    }
}

void Parser::expect(TokenType expected, const std::string &exp_str) {
    if (tokenizer.token_type() != expected)
        throw ParserError("Expected " + exp_str + " not " +
                              tokenizer.token()->content,
                          *this);
    skip();
}

bool Parser::is_seperator() {
    return tokenizer.is_token(TokenType::TOK_SEPERATOR) ||
           tokenizer.is_token(TokenType::TOK_EOF);
}

void Parser::skip() {
    // get next token
    tokenizer.next_token();
}

void Parser::parse_seperator(bool must_exist) {
    if (!is_seperator()) {
        if (must_exist)
            throw ParserError("Expected end of line", *this);
    } else if (!tokenizer.is_token(TokenType::TOK_EOF))
        skip();
}

void Parser::print_tokens() {
    tokenizer.load(source, source_name);
    while (tokenizer.next_token()->type != TokenType::TOK_EOF) {
        std::cout << tokenizer.token()->str() << std::endl;
    }
}

TokenType Parser::skip_line() {

    // skip until EOF or EOL is encountered.
    while (tokenizer.token_type() != TokenType::TOK_EOF &&
           tokenizer.token_type() != TokenType::TOK_EOF)
        tokenizer.next_token();
    // Get next token
    tokenizer.next_token();
    // return its type
    return tokenizer.token_type();
}
// Scans for routines and classes in the source
// This will help us to resolve forward references
void Parser::scan_routines() {
    ClassSymbol *current_class = NULL;
    int flags;
    // parse until EOF is reached
    while (skip_line() != TokenType::TOK_EOF) {
        if (tokenizer.is_token(TokenType::TOK_ABSTRACT) &&
            current_class != NULL) {
            flags = SYM_ISABSTRACT;
            skip();
        } else if (tokenizer.is_token(TokenType::TOK_SHARED) &&
                   current_class != NULL) {
            flags = SYM_ISSHARED;
            skip();
        } else if (tokenizer.is_token(TokenType::TOK_VIRTUAL) &&
                   current_class != NULL) {
            flags = SYM_ISVIRTUAL;
            skip();
        } else
            flags = 0;

        if (tokenizer.is_token(TokenType::TOK_FUNCTION)) {
            if (current_class != NULL && current_class->is_abstract() &&
                (flags & SYM_ISABSTRACT) != 0) {
                throw ParserError(
                    "Abstract methods can only be defined in Abstract Classes",
                    *this);
            }

            // Get the function name
            skip();
            if (!tokenizer.is_token(TokenType::TOK_IDENTIFIER)) {
                throw ParserError("\"" + tokenizer.token()->content +
                                      "\" is not a valid name",
                                  *this);
            }
            // Make sure that the function is not already defined
            auto prev = sym_table.unused(tokenizer.token()->content);
            if (prev) {
                if (prev->parent == NULL)
                    throw ParserError(
                        "\"" + prev->name + "\" is already defined", *this);
                throw ParserError(
                    "\"" + prev->name + "\" is already defined as " +
                        prev->symbol_name() + " of " + prev->parent->name,
                    *this);
            }

            // Inherit virtual if prior declaration was virtual
            if(current_class != 0)
            {
                // Find method in the parents of class
            }
        }
    }
}

// find method of a class
std::shared_ptr<FunctionSymbol> ClassSymbol::find_method(const std::string&)
{
    // No super class
    if(superclass == NULL)
        return NULL;
    return NULL;
}

} // namespace wxbasic
