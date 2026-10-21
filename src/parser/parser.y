/* parser.y - Bison grammar for the mini-language. Builds the AST. */

%skeleton "lalr1.cc"
%require "3.2"

/* The generated parser is the C++ class aic::BisonParser. */
%define api.namespace {aic}
%define api.parser.class {BisonParser}

/* 'variant' lets grammar symbols carry real C++ types like std::string. */
%define api.value.type variant
%define api.token.constructor
%define api.token.prefix {TOK_}
%define parse.assert
%define parse.error verbose
%locations

/* Goes into the generated header: types the parser class mentions. */
%code requires {
    #include <memory>
    #include <string>
    #include "ast/ast.h"
    #include "parser/parser.h"
    #include "parser/token_stream.h"
}

/* The parser object is built with these two arguments... */
%parse-param { TokenStream& token_stream }
%parse-param { ParseResult& parse_result }
/* ...and the first one is passed on to yylex() to fetch tokens. */
%lex-param { TokenStream& token_stream }

/* Goes into the generated .cc file: declare our token-fetching function. */
%code {
    static aic::BisonParser::symbol_type yylex(TokenStream& token_stream);
}

/* ---- Tokens. The quoted text is how a token is named in error messages. ---- */
%token END_OF_FILE 0 "end of file"
%token INT "int"
%token PRINT "print"
%token INPUT "input"
%token <std::string> IDENTIFIER "identifier"
%token <std::string> NUMBER "number"
%token ASSIGN "="
%token PLUS "+"
%token MINUS "-"
%token STAR "*"
%token SLASH "/"
%token LPAREN "("
%token RPAREN ")"
%token SEMICOLON ";"
%token LBRACE "{"
%token RBRACE "}"

/* ---- Precedence: LATER lines bind TIGHTER. %left = left-associative. ---- */
%left PLUS MINUS
%left STAR SLASH

/* Grammar symbols now produce AST nodes (owned by unique_ptr). */
%type <std::unique_ptr<Expr>> expr
%type <std::unique_ptr<Stmt>> statement

%%

program
    : statement_list
    ;

statement_list
    : %empty
    | statement_list statement   { parse_result.program->addStatement(std::move($2)); }
    ;

statement
    : INT IDENTIFIER ASSIGN expr SEMICOLON
        { $$ = std::make_unique<DeclarationStmt>($2, std::move($4),
                                                 @1.begin.line, @1.begin.column); }
    | IDENTIFIER ASSIGN expr SEMICOLON
        { $$ = std::make_unique<AssignmentStmt>($1, std::move($3),
                                                @1.begin.line, @1.begin.column); }
    | PRINT LPAREN expr RPAREN SEMICOLON
        { $$ = std::make_unique<PrintStmt>(std::move($3),
                                           @1.begin.line, @1.begin.column); }
    ;

expr
    : NUMBER
        { $$ = std::make_unique<NumberExpr>(std::stoi($1),
                                            @1.begin.line, @1.begin.column); }
    | IDENTIFIER
        { $$ = std::make_unique<VariableExpr>($1,
                                              @1.begin.line, @1.begin.column); }
    | INPUT LPAREN RPAREN
        { $$ = std::make_unique<InputExpr>(@1.begin.line, @1.begin.column); }
    | expr PLUS expr
        { $$ = std::make_unique<BinaryExpr>(BinaryOp::ADD, std::move($1), std::move($3),
                                            @2.begin.line, @2.begin.column); }
    | expr MINUS expr
        { $$ = std::make_unique<BinaryExpr>(BinaryOp::SUB, std::move($1), std::move($3),
                                            @2.begin.line, @2.begin.column); }
    | expr STAR expr
        { $$ = std::make_unique<BinaryExpr>(BinaryOp::MUL, std::move($1), std::move($3),
                                            @2.begin.line, @2.begin.column); }
    | expr SLASH expr
        { $$ = std::make_unique<BinaryExpr>(BinaryOp::DIV, std::move($1), std::move($3),
                                            @2.begin.line, @2.begin.column); }
    | LPAREN expr RPAREN
        { $$ = std::move($2); }   /* parentheses only group; they leave no node */
    ;

%%

/* ---- User code ---- */

/* Bison calls this when it finds a syntax error. We store it, not print it. */
void aic::BisonParser::error(const location_type& loc, const std::string& message) {
    parse_result.errors.push_back(Diagnostic{loc.begin.line, loc.begin.column, message});
}

/* Adapter between our lexer and Bison: converts our Token into Bison's token. */
static aic::BisonParser::symbol_type yylex(TokenStream& token_stream) {
    using Parser = aic::BisonParser;

    const Token& t = token_stream.next();
    const int length = static_cast<int>(t.text.size());
    const aic::location loc(aic::position(nullptr, t.line, t.column),
                            aic::position(nullptr, t.line, t.column + length));

    switch (t.type) {
        case TokenType::END_OF_FILE: return Parser::make_END_OF_FILE(loc);
        case TokenType::INT:         return Parser::make_INT(loc);
        case TokenType::PRINT:       return Parser::make_PRINT(loc);
        case TokenType::INPUT:       return Parser::make_INPUT(loc);
        case TokenType::IDENTIFIER:  return Parser::make_IDENTIFIER(t.text, loc);
        case TokenType::NUMBER:      return Parser::make_NUMBER(t.text, loc);
        case TokenType::ASSIGN:      return Parser::make_ASSIGN(loc);
        case TokenType::PLUS:        return Parser::make_PLUS(loc);
        case TokenType::MINUS:       return Parser::make_MINUS(loc);
        case TokenType::STAR:        return Parser::make_STAR(loc);
        case TokenType::SLASH:       return Parser::make_SLASH(loc);
        case TokenType::LPAREN:      return Parser::make_LPAREN(loc);
        case TokenType::RPAREN:      return Parser::make_RPAREN(loc);
        case TokenType::SEMICOLON:   return Parser::make_SEMICOLON(loc);
        case TokenType::LBRACE:      return Parser::make_LBRACE(loc);
        case TokenType::RBRACE:      return Parser::make_RBRACE(loc);
    }
    return Parser::make_END_OF_FILE(loc);  // unreachable; keeps the compiler happy
}

ParseResult Parser::parse(const std::vector<Token>& tokens) {
    ParseResult result;
    result.program = std::make_unique<Program>();
    if (tokens.empty()) {
        return result;
    }

    TokenStream stream(tokens);
    aic::BisonParser parser(stream, result);
    const int status = parser.parse();  // 0 = success

    // Safety net: a failed parse must never look like a success.
    if (status != 0 && result.errors.empty()) {
        result.errors.push_back(Diagnostic{0, 0, "parsing failed"});
    }
    return result;
}
