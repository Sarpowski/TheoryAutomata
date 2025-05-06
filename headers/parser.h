#ifndef CMILAN_PARSER_H
#define CMILAN_PARSER_H

#include "scanner.h"
#include "codegen.h"
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <stack>

using namespace std;


struct LoopContext {
    int conditionAddress;  // Адрес начала проверки условия
    int exitAddress;       // Адрес для выхода из цикла (для break)
    vector<int> breakAddresses; // List of addresses that need to be filled with the actual exit address
};

class Parser
{
public:
    // Конструктор
    //    const string& fileName - имя файла с программой для анализа
    //
    // Конструктор создает экземпляры лексического анализатора и генератора.

    Parser(const string& fileName, istream& input)
            : output_(cout), error_(false), recovered_(true), lastVar_(0)
    {
        scanner_ = new Scanner(fileName, input);
        codegen_ = new CodeGen(output_);
        next();
    }

    ~Parser()
    {
        delete codegen_;
        delete scanner_;
    }

    void parse();

private:
    typedef map<string, int> VarTable;
    void program();
    void statementList();
    void statement();
    void expression();
    void term();
    void factor();
    void relation();





    // Новые функции
    void booleanExpression(); // Разбор логического выражения (OR, |)
    void booleanTerm();       // Разбор логического терма (AND, &)
    void booleanFactor();     // Разбор логического фактора (NOT, true, false, условие)







    // Сравнение текущей лексемы с образцом. Текущая позиция в потоке лексем не изменяется.
    bool see(Token t)
    {
        return scanner_->token() == t;
    }

    // Проверка совпадения текущей лексемы с образцом. Если лексема и образец совпадают,
    // лексема изымается из потока.

    bool match(Token t)
    {
        if(scanner_->token() == t) {
            scanner_->nextToken();
            return true;
        }
        else {
            return false;
        }
    }

    // Переход к следующей лексеме.

    void next()
    {
        scanner_->nextToken();
    }

    // Обработчик ошибок.
    void reportError(const string& message)
    {
        cerr << "Line " << scanner_->getLineNumber() << ": " << message << endl;
        error_ = true;
    }

    void mustBe(Token t); //проверяем, совпадает ли данная лексема с образцом. Если да, то лексема изымается из потока.
    //Иначе создаем сообщение об ошибке и пробуем восстановиться
    void recover(Token t); //восстановление после ошибки: идем по коду до тех пор,
    //пока не встретим эту лексему или лексему конца файла.
    int findOrAddVariable(const string&); //функция пробегает по variables_.
    //Если находит нужную переменную - возвращает ее номер, иначе добавляет ее в массив, увеличивает lastVar и возвращает его.

    Scanner* scanner_; //лексический анализатор для конструктора
    CodeGen* codegen_; //указатель на виртуальную машину
    ostream& output_; //выходной поток (в данном случае используем cout)
    bool error_; //флаг ошибки. Используется чтобы определить, выводим ли список команд после разбора или нет
    bool recovered_; //не используется
    VarTable variables_; //массив переменных, найденных в программе
    int lastVar_; //номер последней записанной переменной
    stack<LoopContext> loopStack_; // Стек для хранения информации о вложенных циклах
};

#endif