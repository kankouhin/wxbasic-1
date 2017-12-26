#include "symbol.h"

namespace wxbasic {

Symbol::~Symbol() {}

SymbolTable::SymbolTable() {
    // Craete the global scope
    enter_scope();
    // Declare the global function
    add_symbol(std::shared_ptr<FunctionSymbol>(new FunctionSymbol("__global")));
}

int SymbolTable::enter_scope(int scope) {
    // Create a new index for the scope
    if (scope == -1) {
        scope = index.size();
        index.push_back(SymbolTableObj());
    }
    current_scope = scope;
    return current_scope;
}

SymbolTable::~SymbolTable() {}

std::shared_ptr<Symbol> SymbolTable::unused(const std::string &name) {
    auto symbol = find_symbol(name);

    // If symbol not found, do nothing
    if (symbol == NULL)
        return NULL;
    else if (current_scope != global_scope &&
             symbol->type == SymbolType::SYM_VARIABLE &&
             symbol->scope == global_scope) {
        // symbol is global variable and we are in local scope. Do nothing
    } else if (class_scope != 0 && symbol->scope == class_scope) {
        // Global symbol but we're inside a class
    } else {
        return symbol;
    }
    return NULL;
}

void SymbolTable::add_symbol(std::shared_ptr<Symbol> sym) {
    // Append it to the symbol list
    table.push_back(sym);
    // Store the index in hash table
    index[current_scope][sym->name] = table.size() - 1;
}

std::shared_ptr<Symbol>
SymbolTable::find_symbol(const std::string &symbol_name) {
    // tries to find the symbol in local scope. If not found, tries to find
    // it in class scope. If not found, tries to find it in global scope
    if (index[current_scope].count(symbol_name) != 0)
        return table[index[current_scope].at(symbol_name)];

    if (class_scope > 0 && index[class_scope].count(symbol_name) != 0)
        return table[index[class_scope].at(symbol_name)];

    if (index[global_scope].count(symbol_name) == 1)
        return table[index[global_scope].at(symbol_name)];

    // No symbol found
    return NULL;
}
} // namespace wxbasic
