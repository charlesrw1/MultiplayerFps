#pragma once
#include <string>
#include "Factory.h"

// useful helper
template<typename T, typename BASE>
struct AddClassToFactory
{
    AddClassToFactory(Factory<std::string, BASE>& factory, const char* name) {
        factory.registerClass<T>(name);
    }
};