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
    //	  Если список операторов пуст, очередной лексемой будет одна из возможных "закрывающих скобок": END, OD, ELSE, FI.
    //	  В этом случае результатом разбора будет пустой блок (его список операторов равен null).
    //	  Если очередная лексема не входит в этот список, то ее мы считаем началом оператора и вызываем метод statement.
    //    Признаком последнего оператора является отсутствие после оператора точки с запятой.
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

        // This is where you need to check for boolean expressions
        if(see(T_TRUE) || see(T_FALSE) || see(T_NOT) ||
           see(T_LPAREN) || see(T_IDENTIFIER)) {
            // Try to parse as a boolean expression
            booleanExpression();
        } else {
            // Parse as arithmetic expression
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

        // Jump back to condition
        codegen_->emit(JUMP, conditionAddress);

        // Get the actual exit address now that we know it
        int exitAddress = codegen_->getCurrentAddress();

        // Fill in the loop exit jump
        codegen_->emitAt(jumpNoAddress, JUMP_NO, exitAddress);

        // Fill in all break jumps with the actual exit address
        for(int breakAddr : loopStack_.top().breakAddresses) {
            codegen_->emitAt(breakAddr, JUMP, exitAddress);
        }

        // Remove the loop context
        loopStack_.pop();
    }
    else if(match(T_WRITE)) {
        mustBe(T_LPAREN);

        // В WRITE можно передавать как арифметические, так и логические выражения
        if(see(T_TRUE) || see(T_FALSE) || see(T_NOT) || see(T_LPAREN)) {
            // Пробуем разобрать как логическое выражение
            booleanExpression();
        } else {
            // Иначе разбираем как арифметическое выражение
            expression();
        }

        mustBe(T_RPAREN);
        codegen_->emit(PRINT);
    }
    else if(match(T_READ)) {
        codegen_->emit(INPUT);
    }
    else if(match(T_BREAK)) {
        // Check if we're inside a loop
        if(loopStack_.empty()) {
            reportError("'break' statement outside of loop");
        } else {
            // Reserve a spot for the jump instruction
            int breakJumpAddress = codegen_->reserve();

            // Add this to the list of addresses to update when we know the actual exit address
            loopStack_.top().breakAddresses.push_back(breakJumpAddress);
        }
    }
    else if(match(T_CONTINUE)) {
        // Проверяем, находимся ли внутри цикла
        if(loopStack_.empty()) {
            reportError("'continue' statement outside of loop");
        } else {
            // Берем адрес начала условия из текущего контекста цикла
            int conditionAddress = loopStack_.top().conditionAddress;

            // Генерируем безусловный переход на адрес начала цикла (проверки условия)
            codegen_->emit(JUMP, conditionAddress);
        }
    }
    else {
        reportError("statement expected.");
    }
}

// Функция для разбора логических выражений
void Parser::booleanExpression() {
    booleanTerm();

    while(see(T_OR) || see(T_BITOR)) {
        bool isShortCircuit = (scanner_->token() == T_OR);
        next();

        if(isShortCircuit) {
            // For ||, check if first operand is non-zero
            int checkNonZeroAddr = codegen_->reserve();
            int shortCircuitAddr = codegen_->reserve();

            codegen_->emitAt(checkNonZeroAddr, DUP);  // Duplicate top value
            codegen_->emitAt(shortCircuitAddr, JUMP_YES, shortCircuitAddr + 3);  // If non-zero, skip evaluation of second operand

            // If we get here, first operand was 0
            codegen_->emit(POP);  // Remove the 0
            booleanTerm();  // Evaluate second operand
        }
        else {
            // For |, evaluate both operands
            booleanTerm();

            // Pop two values, OR them, push result
            // For 0/1 values, we can use addition and then compare with 0
            codegen_->emit(ADD);

            // Check if result is > 0
            codegen_->emit(PUSH, 0);
            codegen_->emit(COMPARE, 3);  // 3 is GT (greater than)
        }
    }
}

void Parser::booleanTerm() {
    booleanFactor();

    while(see(T_AND) || see(T_BITAND)) {
        bool isShortCircuit = (scanner_->token() == T_AND);
        next();

        if(isShortCircuit) {
            // For &&, check if first operand is 0
            int shortCircuitAddr = codegen_->reserve();
            codegen_->emitAt(shortCircuitAddr, JUMP_NO, shortCircuitAddr + 3);  // If 0, skip evaluation of second operand

            booleanFactor();

            // Now perform AND using existing instructions
            // Pop two values, AND them, push result
            codegen_->emit(MULT);  // For 0/1 values, multiplication is equivalent to AND
        }
        else {
            // For &, evaluate both operands
            booleanFactor();

            // Pop two values, AND them, push result
            codegen_->emit(MULT);  // For 0/1 values, multiplication is equivalent to AND
        }
    }
}
void Parser::booleanFactor() {
    if(match(T_NOT)) {
        booleanFactor();
        // Instead of emit(NOT), use existing instructions:
        // Check if value is 0, if so push 1, else push 0
        int notTrueAddr = codegen_->reserve();
        int jumpAddr = codegen_->reserve();

        codegen_->emitAt(notTrueAddr, JUMP_NO, notTrueAddr + 2);  // Skip next instruction if value is 0
        codegen_->emitAt(notTrueAddr + 1, PUSH, 0);  // Push 0 (NOT true = false)
        codegen_->emitAt(jumpAddr, JUMP, jumpAddr + 2);  // Skip next instruction
        codegen_->emit(PUSH, 1);  // Push 1 (NOT false = true)
    }
    else if(match(T_TRUE)) {
        codegen_->emit(PUSH, 1);  // Use PUSH instead of PUSH_TRUE
    }
    else if(match(T_FALSE)) {
        codegen_->emit(PUSH, 0);  // Use PUSH instead of PUSH_FALSE
    }
    else if(match(T_LPAREN)) {
        booleanExpression();
        mustBe(T_RPAREN);
    }
    else {
        // This is a key change - try to parse a relation or an identifier
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
            // Try to parse a relation
            expression();
            if(see(T_CMP)) {
                Cmp cmp = scanner_->getCmpValue();
                next();
                expression();
                codegen_->emit(COMPARE, cmp);
            }
            else {
                // Not a comparison, not an identifier, not a literal - error
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
        //Если встретили число, то преобразуем его в целое и записываем на вершину стека
    }
    else if(see(T_IDENTIFIER)) {
        int varAddress = findOrAddVariable(scanner_->getStringValue());
        next();
        codegen_->emit(LOAD, varAddress);
        //Если встретили переменную, то выгружаем значение, лежащее по ее адресу, на вершину стека
    }
    else if(see(T_ADDOP) && scanner_->getArithmeticValue() == A_MINUS) {
        next();
        factor();
        codegen_->emit(INVERT);
        //Если встретили знак "-", и за ним <factor> то инвертируем значение, лежащее на вершине стека
    }
    else if(match(T_LPAREN)) {
        expression();
        mustBe(T_RPAREN);
        //Если встретили открывающую скобку, тогда следом может идти любое арифметическое выражение и обязательно
        //закрывающая скобка.
    }
    else if(match(T_READ)) {
        codegen_->emit(INPUT);
        //Если встретили зарезервированное слово READ, то записываем на вершину стека идет запись со стандартного ввода
    }
    else {
        reportError("expression expected.");
    }
}


void Parser::relation()
{


    // Check for boolean literals first
    if (match(T_TRUE)) {
        codegen_->emit(PUSH_TRUE);  // Push 1 onto stack
        return;
    }
    else if (match(T_FALSE)) {
        codegen_->emit(PUSH_FALSE);  // Push 0 onto stack
        return;
    }

    // If not a boolean literal, try to parse as expression
    expression();

    // If there's a comparison operator, compare two expressions
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