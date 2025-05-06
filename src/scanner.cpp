#include "../headers/scanner.h"
#include <algorithm>
#include <iostream>
#include <cctype>

using namespace std;

static const char * tokenNames_[] = {
        "end of file",
        "illegal token",
        "identifier",
        "number",
        "'BEGIN'",
        "'END'",
        "'IF'",
        "'THEN'",
        "'ELSE'",
        "'FI'",
        "'WHILE'",
        "'DO'",
        "'OD'",
        "'WRITE'",
        "'READ'",
        "'BREAK'",
        "'CONTINUE'",
        "'TRUE'",
        "'FALSE'",
        "':='",
        "'+' or '-'",
        "'*' or '/'",
        "comparison operator",
        "'('",
        "')'",
        "';'",
        "'&'",
        "'|'",
        "'&&'",
        "'||'",
        "'!'",
};

void Scanner::nextToken()
{
    skipSpace();

    // Handle comments
    while(ch_ == '/') {
        nextChar();
        if(ch_ == '*') {  // Block comment
            nextChar();
            bool inside = true;
            while(inside) {
                while(ch_ != '*' && !input_.eof()) {
                    nextChar();
                }

                if(input_.eof()) {
                    token_ = T_EOF;
                    return;
                }

                nextChar();
                if(ch_ == '/') {
                    inside = false;
                    nextChar();
                }
            }
        }
        else if(ch_ == '/') {  // Line comment
            while(ch_ != '\n' && !input_.eof()) {
                nextChar();
            }
            if(ch_ == '\n') {
                ++lineNumber_;
                nextChar();
            }
        }
        else {  // Division operator
            token_ = T_MULOP;
            arithmeticValue_ = A_DIVIDE;
            return;
        }

        skipSpace();
    }

    if(input_.eof()) {
        token_ = T_EOF;
        return;
    }

    if(isdigit(ch_)) {
        int value = 0;
        while(isdigit(ch_)) {
            value = value * 10 + (ch_ - '0');
            nextChar();
        }
        token_ = T_NUMBER;
        intValue_ = value;
    }
    else if(isIdentifierStart(ch_)) {
        string buffer;
        while(isIdentifierBody(ch_)) {
            buffer += ch_;
            nextChar();
        }

        transform(buffer.begin(), buffer.end(), buffer.begin(), ::tolower);

        map<string, Token>::iterator kwd = keywords_.find(buffer);
        if(kwd == keywords_.end()) {
            token_ = T_IDENTIFIER;
            stringValue_ = buffer;
        }
        else {
            token_ = kwd->second;
        }
    }
    else {
        switch(ch_) {
            case '(':
                token_ = T_LPAREN;
                nextChar();
                break;
            case ')':
                token_ = T_RPAREN;
                nextChar();
                break;
            case ';':
                token_ = T_SEMICOLON;
                nextChar();
                break;
            case ':':
                nextChar();
                if(ch_ == '=') {
                    token_ = T_ASSIGN;
                    nextChar();
                }
                else {
                    token_ = T_ILLEGAL;
                }
                break;
            case '<':
                token_ = T_CMP;
                nextChar();
                if(ch_ == '=') {
                    cmpValue_ = C_LE;
                    nextChar();
                }
                else {
                    cmpValue_ = C_LT;
                }
                break;
            case '>':
                token_ = T_CMP;
                nextChar();
                if(ch_ == '=') {
                    cmpValue_ = C_GE;
                    nextChar();
                }
                else {
                    cmpValue_ = C_GT;
                }
                break;
            case '!':
                nextChar();
                if(ch_ == '=') {
                    token_ = T_CMP;
                    cmpValue_ = C_NE;
                    nextChar();
                }
                else {
                    token_ = T_NOT;
                }
                break;
            case '=':
                token_ = T_CMP;
                cmpValue_ = C_EQ;
                nextChar();
                break;
            case '&':
                nextChar();
                if(ch_ == '&') {
                    token_ = T_AND;
                    nextChar();
                }
                else {
                    token_ = T_BITAND;
                }
                break;
            case '|':
                nextChar();
                if(ch_ == '|') {
                    token_ = T_OR;
                    nextChar();
                }
                else {
                    token_ = T_BITOR;
                }
                break;
            case '+':
                token_ = T_ADDOP;
                arithmeticValue_ = A_PLUS;
                nextChar();
                break;
            case '-':
                token_ = T_ADDOP;
                arithmeticValue_ = A_MINUS;
                nextChar();
                break;
            case '*':
                token_ = T_MULOP;
                arithmeticValue_ = A_MULTIPLY;
                nextChar();
                break;
            default:
                token_ = T_ILLEGAL;
                nextChar();
                break;
        }
    }
}

void Scanner::skipSpace()
{
    while(isspace(ch_)) {
        if(ch_ == '\n') {
            ++lineNumber_;
        }
        nextChar();
    }
}

void Scanner::nextChar()
{
    ch_ = input_.get();
}

const char * tokenToString(Token t)
{
    return tokenNames_[t];
}