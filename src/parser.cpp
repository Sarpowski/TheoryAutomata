#include "../headers/parser.h"
#include <sstream>

//Выполняем синтаксический разбор блока program. Если во время разбора не обнаруживаем 
//никаких ошибок, то выводим последовательность команд стек-машины
void Parser::parse()
{
    program();
    if(!error_) {
        codegen_->flush();
    }
}

void Parser::program()
{
    mustBe(T_BEGIN);
    statementList();
    mustBe(T_END);
    codegen_->emit(STOP);
}

void Parser::statementList()
{

    if(see(T_END) || see(T_OD) || see(T_ELSE) || see(T_FI)) {
        return;
    }
    else {
        bool more = true;
        while(more) {
            statement();
            more = match(T_SEMICOLON);
        }
    }
}

void Parser::statement()
{
    if(see(T_IDENTIFIER)) {
        int varAddress = findOrAddVariable(scanner_->getStringValue());
        next();
        mustBe(T_ASSIGN);


        if(see(T_TRUE) || see(T_FALSE) || see(T_NOT) ||
           see(T_LPAREN) || see(T_IDENTIFIER)) {

            booleanExpression();
        } else {

            expression();
        }

        codegen_->emit(STORE, varAddress);
    }

    else if(match(T_IF)) {
        booleanExpression();

        int jumpNoAddress = codegen_->reserve();

        mustBe(T_THEN);
        statementList();
        if(match(T_ELSE)) {

            int jumpAddress = codegen_->reserve();
            codegen_->emitAt(jumpNoAddress, JUMP_NO, codegen_->getCurrentAddress());
            statementList();
            codegen_->emitAt(jumpAddress, JUMP, codegen_->getCurrentAddress());
        }
        else {
            codegen_->emitAt(jumpNoAddress, JUMP_NO, codegen_->getCurrentAddress());
        }

        mustBe(T_FI);
    }

    else if(match(T_WHILE)) {



        int conditionAddress = codegen_->getCurrentAddress();
        relation();

        int jumpNoAddress = codegen_->reserve();

        LoopContext context;
        context.conditionAddress = conditionAddress;
        context.exitAddress = jumpNoAddress;
        loopStack_.push(context);

        mustBe(T_DO);
        statementList();
        mustBe(T_OD);

        codegen_->emit(JUMP, conditionAddress);


        int exitAddress = codegen_->getCurrentAddress();


        codegen_->emitAt(jumpNoAddress, JUMP_NO, exitAddress);


        for(int breakAddr : loopStack_.top().breakAddresses) {
            codegen_->emitAt(breakAddr, JUMP, exitAddress);
        }


        loopStack_.pop();
    }
    else if(match(T_WRITE)) {
        mustBe(T_LPAREN);


        if(see(T_TRUE) || see(T_FALSE) || see(T_NOT) || see(T_LPAREN)) {

            booleanExpression();
        } else {

            expression();
        }

        mustBe(T_RPAREN);
        codegen_->emit(PRINT);
    }
    else if(match(T_READ)) {
        codegen_->emit(INPUT);
    }
    else if(match(T_BREAK)) {

        if(loopStack_.empty()) {
            reportError("'break' statement outside of loop");
        } else {
            int breakJumpAddress = codegen_->reserve();
            loopStack_.top().breakAddresses.push_back(breakJumpAddress);
        }
    }
    else if(match(T_CONTINUE)) {

        if(loopStack_.empty()) {
            reportError("'continue' statement outside of loop");
        } else {

            int conditionAddress = loopStack_.top().conditionAddress;


            codegen_->emit(JUMP, conditionAddress);
        }
    }
    else {
        reportError("statement expected.");
    }
}


void Parser::booleanExpression() {
    booleanTerm();

    while(see(T_OR) || see(T_BITOR)) {
        bool isShortCircuit = (scanner_->token() == T_OR);
        next();

        if(isShortCircuit) {
            int checkNonZeroAddr = codegen_->reserve();
            int shortCircuitAddr = codegen_->reserve();

            codegen_->emitAt(checkNonZeroAddr, DUP);
            codegen_->emitAt(shortCircuitAddr, JUMP_YES, shortCircuitAddr + 3);

            codegen_->emit(POP);
            booleanTerm();
        }
        else {

            booleanTerm();


            codegen_->emit(ADD);


            codegen_->emit(PUSH, 0);
            codegen_->emit(COMPARE, 3);
        }
    }
}
/*
void Parser::booleanTerm() {
    booleanFactor();

    while(see(T_AND) || see(T_BITAND)) {
        bool isShortCircuit = (scanner_->token() == T_AND);
        next();

        if(isShortCircuit) {

            int shortCircuitAddr = codegen_->reserve();
            codegen_->emitAt(shortCircuitAddr, JUMP_NO, shortCircuitAddr + 3);

            booleanFactor();
            codegen_->emit(MULT);
        }
        else {

            booleanFactor();

            codegen_->emit(MULT);
        }
    }
}*/

void Parser::booleanTerm() {
    booleanFactor();

    while(see(T_AND) || see(T_BITAND)) {
        bool isShortCircuit = (scanner_->token() == T_AND);
        next();

        if(isShortCircuit) {
            codegen_->emit(DUP);

            int jumpEndAddr = codegen_->reserve();


            codegen_->emit(POP);

            booleanFactor();

            codegen_->emit(MULT);

            int endAddr = codegen_->getCurrentAddress();

            codegen_->emitAt(jumpEndAddr, JUMP_NO, endAddr);
        }
        else {
            booleanFactor();

            codegen_->emit(MULT);
        }
    }
}
void Parser::booleanFactor() {
    if(match(T_NOT)) {
        /*booleanFactor();


        int notTrueAddr = codegen_->reserve();
        int jumpAddr = codegen_->reserve();

        codegen_->emitAt(notTrueAddr, JUMP_NO, notTrueAddr + 2);
        codegen_->emitAt(notTrueAddr + 1, PUSH, 0);
        codegen_->emitAt(jumpAddr, JUMP, jumpAddr + 2);
        codegen_->emit(PUSH, 1);*/
        booleanFactor();
        // Implement NOT using equality comparison: NOT x = (x == 0)
        codegen_->emit(PUSH, 0);
        codegen_->emit(COMPARE, C_EQ);
    }
    else if(match(T_TRUE)) {
        codegen_->emit(PUSH, 1);
    }
    else if(match(T_FALSE)) {
        codegen_->emit(PUSH, 0);
    }
    else if(match(T_LPAREN)) {
        booleanExpression();
        mustBe(T_RPAREN);
    }
    else {
        if(see(T_IDENTIFIER)) {
            int varAddress = findOrAddVariable(scanner_->getStringValue());
            next();
            codegen_->emit(LOAD, varAddress);
        }
        else if(see(T_NUMBER)) {
            int value = scanner_->getIntValue();
            next();
            codegen_->emit(PUSH, value);
        }
        else {
            expression();
            if(see(T_CMP)) {
                Cmp cmp = scanner_->getCmpValue();
                next();
                expression();
                codegen_->emit(COMPARE, cmp);
            }
            else {
                reportError("boolean expression expected");
            }
        }
    }
}

void Parser::expression()
{

    /*
        Арифметическое выражение описывается следующими правилами: <expression> -> <term> | <term> + <term> | <term> - <term>
        При разборе сначала смотрим первый терм, затем анализируем очередной символ. Если это '+' или '-',
		удаляем его из потока и разбираем очередное слагаемое (вычитаемое). Повторяем проверку и разбор очередного
		терма, пока не встретим за термом символ, отличный от '+' и '-'
    */

    term();
    while(see(T_ADDOP)) {
        Arithmetic op = scanner_->getArithmeticValue();
        next();
        term();

        if(op == A_PLUS) {
            codegen_->emit(ADD);
        }
        else {
            codegen_->emit(SUB);
        }
    }
}

void Parser::term()
{
    /*
		Терм описывается следующими правилами: <expression> -> <factor> | <factor> + <factor> | <factor> - <factor>
        При разборе сначала смотрим первый множитель, затем анализируем очередной символ. Если это '*' или '/',
		удаляем его из потока и разбираем очередное слагаемое (вычитаемое). Повторяем проверку и разбор очередного
		множителя, пока не встретим за ним символ, отличный от '*' и '/'
	*/
    factor();
    while(see(T_MULOP)) {
        Arithmetic op = scanner_->getArithmeticValue();
        next();
        factor();

        if(op == A_MULTIPLY) {
            codegen_->emit(MULT);
        }
        else {
            codegen_->emit(DIV);
        }
    }
}

void Parser::factor()
{
    /*
		Множитель описывается следующими правилами:
		<factor> -> number | identifier | -<factor> | (<expression>) | READ
	*/
    if(see(T_NUMBER)) {
        int value = scanner_->getIntValue();
        next();
        codegen_->emit(PUSH, value);
    }
    else if(see(T_IDENTIFIER)) {
        int varAddress = findOrAddVariable(scanner_->getStringValue());
        next();
        codegen_->emit(LOAD, varAddress);

    }
    else if(see(T_ADDOP) && scanner_->getArithmeticValue() == A_MINUS) {
        next();
        factor();
        codegen_->emit(INVERT);
    }
    else if(match(T_LPAREN)) {
        expression();
        mustBe(T_RPAREN);
    }
    else if(match(T_READ)) {
        codegen_->emit(INPUT);
    }
    else {
        reportError("expression expected.");
    }
}


void Parser::relation()
{
    if (match(T_TRUE)) {
        codegen_->emit(PUSH_TRUE);
        return;
    }
    else if (match(T_FALSE)) {
        codegen_->emit(PUSH_FALSE);
        return;
    }
    expression();
    if(see(T_CMP)) {
        Cmp cmp = scanner_->getCmpValue();
        next();
        expression();
        switch(cmp) {
            case C_EQ:
                codegen_->emit(COMPARE, 0);
                break;
            case C_NE:
                codegen_->emit(COMPARE, 1);
                break;
            case C_LT:
                codegen_->emit(COMPARE, 2);
                break;
            case C_GT:
                codegen_->emit(COMPARE, 3);
                break;
            case C_LE:
                codegen_->emit(COMPARE, 4);
                break;
            case C_GE:
                codegen_->emit(COMPARE, 5);
                break;
        };

    }
    else {
        reportError("comparison operator expected.");
    }
}

int Parser::findOrAddVariable(const string& var)
{
    VarTable::iterator it = variables_.find(var);
    if(it == variables_.end()) {
        variables_[var] = lastVar_;
        return lastVar_++;
    }
    else {
        return it->second;
    }
}

void Parser::mustBe(Token t)
{
    if(!match(t)) {
        error_ = true;

        // Подготовим сообщение об ошибке
        std::ostringstream msg;
        msg << tokenToString(scanner_->token()) << " found while " << tokenToString(t) << " expected.";
        reportError(msg.str());

        // Попытка восстановления после ошибки.
        recover(t);
    }
}

void Parser::recover(Token t)
{
    while(!see(t) && !see(T_EOF)) {
        next();
    }

    if(see(t)) {
        next();
    }
}