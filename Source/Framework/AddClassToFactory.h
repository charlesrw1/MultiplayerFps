#pragma once
#include <string>
#include "Framework/Factory.h"

// useful helper
template<typename T, typename BASE>
struct AddClassToFactory
{
    AddClassToFactory(Factory<std::string, BASE>& factory, const char* name) {
        factory.registerClass<T>(name);
    }
};
#define ADDTOFACTORYMACRO(Class,Base) static AddClassToFactory<Class,Base> Class##factory_12345(Base::get_factory(), #Class)