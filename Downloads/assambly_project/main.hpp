#ifndef REGEX_HPP
#define REGEX_HPP

#include <regex>
#include <string>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <cinttypes>
#include <iomanip>
#include <fstream>

using namespace std;

enum class AddressingMode
{
    NONE = 0,
    IMMEDIATE = 1,          // Var, kalmalı
    DIRECT = 2,             // Var, kalmalı
    IMPLIED = 3,            // Var, kalmalı (ACCUMULATOR modunu da kapsayabilir)
    EXTENDED = 4,           // EKLENMELİ
    INDEXED = 5,            // EKLENMELİ (Genel M6800 için)
    RELATIVE = 6            // EKLENMELİ (Genel M6800 için)
    // REGISTER ve REGISTER_INDIRECT modlarını kaldırdım çünkü M6800 standartlarında bu isimlerle geçmiyorlar.
    // Eğer çok özel bir kullanımınız yoksa, kafa karışıklığını önlemek için standart modlara sadık kalmak daha iyi olur.
    // Eğer INCA, DECA gibi komutları ayrı bir modda işlemek istiyorsanız ACCUMULATOR = 7; gibi bir şey ekleyebilirsiniz.
    // Ama genelde bunlar IMPLIED içinde değerlendirilir.
};

class SymbolTable
{
public:
    void add_symbol(std::string symbol, int address)
    {
        symbols[symbol] = address;
    }

    optional<int> get_symbol(std::string symbol)
    {
        if (symbols.find(symbol) == symbols.end())
        {
            return nullopt;
        }
        return symbols[symbol];
    }

private:
    std::unordered_map<std::string, int> symbols; // label -> address
};

class ForwardReferenceTable
{
public:
    void add_reference(std::string symbol, int address)
    {
        references[symbol] = address;
    }

    optional<int> get_reference(std::string symbol)
    {
        if (references.find(symbol) == references.end())
        {
            return nullopt;
        }
        return references[symbol];
    }

private:
    std::unordered_map<std::string, int> references; // label -> offset
};

class Instruction
{
public:
    std::string instruction;
    int opcode;
    int no_of_bytes;
    AddressingMode addressing_mode;
};

class InstructionSet
{
public:
    bool is_instruction(std::string instruction)
    {
        for (auto &i : instructions)
        {
            if (i.instruction == instruction)
            {
                return true;
            }
        }
        return false;
    }

    vector<Instruction> get_instruction(std::string instruction)
    {
        vector<Instruction> inss;
        for (auto &i : instructions)
        {
            if (i.instruction == instruction)
            {
                inss.push_back(i);
            }
        }

        return inss;
    }

    optional<Instruction> get_instruction_wrt_address_mode(std::string instruction, AddressingMode addressing_mode)
    {
        for (auto &i : instructions)
        {
            if (i.instruction == instruction && i.addressing_mode == addressing_mode)
            {
                return i;
            }
        }
        return nullopt;
    }

    void add_instruction(Instruction instruction)
    {
        instructions.push_back(instruction);
    }

private:
    std::vector<Instruction> instructions;
};

#endif