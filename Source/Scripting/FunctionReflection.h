#pragma once
#include "Framework/ReflectionProp.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Framework/MulticastDelegate.h"

struct ClassTypeInfo;
class Entity;
class IAsset;
class EntityComponent;

IAsset* get_iasset_from_lua(lua_State* L, int index);
EntityComponent* get_component_from_lua(lua_State* L, int index);
Entity* get_entity_from_lua(lua_State* L, int index);

template<typename T>
T get_from_lua(lua_State* L, int index) {
    if (std::is_base_of<IAsset, typename std::remove_pointer<T>>::value)
        return (T)get_iasset_from_lua(L,index);
    else if (std::is_base_of<Entity, typename std::remove_pointer<T>>::value)
        return (T)get_entity_from_lua(L,index);
    else if (std::is_base_of<EntityComponent, typename std::remove_pointer<T>>::value)
        return (T)get_component_from_lua(L, index);
    return T();
}

template<>
int get_from_lua(lua_State* L, int index);
template<>
float get_from_lua(lua_State* L, int index);
template<>
const char* get_from_lua(lua_State* L, int index);
template<>
glm::vec3 get_from_lua(lua_State* L, int index);
template<>
glm::quat get_from_lua(lua_State* L, int index);
template<>
bool get_from_lua(lua_State* L, int index);
template<>
const ClassTypeInfo* get_from_lua(lua_State* L, int index);

void push_iasset_to_lua(lua_State* L, IAsset* a);
void push_entity_to_lua(lua_State* L, Entity* e);
void push_entitycomponent_to_lua(lua_State* L, EntityComponent* ec);

template<typename T>
void push_to_lua(lua_State* L, T value) 
{
    using Decayed = typename std::decay<T>::type;

    if constexpr (std::is_base_of<IAsset, typename std::remove_pointer<Decayed>::type>::value)
        push_iasset_to_lua(L, (IAsset*)value);
    else if constexpr (std::is_base_of<Entity, typename std::remove_pointer<Decayed>::type>::value)
        push_entity_to_lua(L, (Entity*)value);
    else if constexpr (std::is_base_of<EntityComponent, typename std::remove_pointer<Decayed>::type>::value)
        push_entitycomponent_to_lua(L, (EntityComponent*)value);
    else
        static_assert(0, "");
}


template<>
void push_to_lua(lua_State* L, bool value);
template<>
void push_to_lua(lua_State* L, int value);
template<>
void push_to_lua(lua_State* L, float value);
template<>
void push_to_lua(lua_State* L, const char* value);
template<>
void push_to_lua(lua_State*, glm::vec3 v);
template<>
void push_to_lua(lua_State*, glm::quat v);
template<>
void push_to_lua(lua_State*, const ClassTypeInfo*);

template<typename T>
struct function_traits;

template<typename Ret, typename T, typename... Args>
struct function_traits<Ret(T::*)(Args...) const> {
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using class_type = T;
};

template<typename Ret, typename T, typename... Args>
struct function_traits<Ret(T::*)(Args...)> {
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using class_type = T;
};
template <typename... Args, std::size_t... I>
std::tuple<Args...> get_args_from_lua(std::tuple<Args...>* p, lua_State* L, std::index_sequence<I...>) {
    return std::make_tuple(get_from_lua<Args>(L, I + 2)...);
}

void* get_class_from_stack(lua_State* L);

template<auto m>
int lua_callable_func(lua_State* L)
{
    using MethodType = decltype(m);
    using Traits = function_traits<MethodType>;
    using Ret = typename Traits::return_type;
    using ArgsTuple = typename Traits::args_tuple;
    using ClassType = typename Traits::class_type;

    auto obj = (ClassType*)get_class_from_stack(L);

    ArgsTuple args = get_args_from_lua((ArgsTuple*)0, L, std::make_index_sequence<std::tuple_size_v<ArgsTuple>>{});

    if constexpr (std::is_void_v<Ret>) {
        std::apply([obj](auto&&... args) { (obj->*m)(std::forward<decltype(args)>(args)...); }, args);
        return 0;
    }
    else {
        Ret result = std::apply([obj](auto&&... args) { return (obj->*m)(std::forward<decltype(args)>(args)...); }, args);
        push_to_lua(L, result);
        return 1;
    }
}


inline PropertyInfo make_function_prop_info(const char* name, int(*call_func)(lua_State*)) {
    PropertyInfo p;
    p.flags = 0;
    p.type = core_type_id::Function;
    p.call_function = call_func;
    p.name = name;
    return p;
}


template <typename... Args>
void push_args_to_lua(lua_State* L, Args&&... args) {
    (push_to_lua(L, std::forward<Args>(args)), ...);  // Fold expression to push all arguments
}


int call_lua_func_internal(lua_State* L, const char* func_name, int num_args);
template <typename... Args>
int call_lua_function(lua_State* L, const char* function_name, Args&&... args) {
    push_args_to_lua(L, std::forward<Args>(args)...);
    return call_lua_func_internal(L, function_name, sizeof...(args));
}

template<typename ... Args>
void add_multi(void* vself, lua_State* L, const char* func_name)
{
    auto self = (MulticastDelegate<Args...>*)vself;
    self->add(L, [L, func_name](Args...args)
        {
            call_lua_function<Args...>(L, func_name, std::forward<Args>(args)...);
        });
}

template<typename ... Args>
void remove_multi(void* vself, lua_State* L)
{
    auto self = (MulticastDelegate<Args...>*)vself;
    self->remove(L);
}


struct multicast_funcs
{
    void(*add)(void* self, lua_State* L, const char*)=nullptr;
    void(*remove)(void* self, lua_State* L) = nullptr;
};

template<typename ... Args>
inline PropertyInfo make_delegate_prop(MulticastDelegate<Args...>* dummy, const char* name, int offset) {
    static multicast_funcs funcs = {
        add_multi<Args...>,
        remove_multi<Args...>
    };
    PropertyInfo p;
    p.flags = 0;
    p.type = core_type_id::MulticastDelegate;
    p.multicast = &funcs;
    p.offset = offset;
    p.name = name;
    return p;
}

#define REG_FUNCTION(func) make_function_prop_info(#func, lua_callable_func<& TYPE_FROM_START ::func>)
#define REG_FUNCTION_EXPLICIT_NAME(func, name) make_function_prop_info(name, lua_callable_func<& TYPE_FROM_START ::func>)
#define REG_MULTICAST_DELEGATE(name) make_delegate_prop(&((TYPE_FROM_START*)0)->name,#name, offsetof(TYPE_FROM_START,name))