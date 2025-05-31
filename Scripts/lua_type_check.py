import sys
import os

VOID_TYPE = 0
INT_TYPE = 1
FLOAT_TYPE = 2
STRING_TYPE = 3
BOOL_TYPE = 4
ARRAY_TYPE = 5
SET_TYPE = 6
DICT_TYPE = 7
OBJECT_TYPE = 8
NULL_TYPE = 9


class LuaFunctionArg:
    def __init__(self, name, type):
        self.name = name
        self.type = type
    def __repr__(self):
        return f"LuaFunctionArg(name={self.name}, type={self.type})"

class LuaFunction:
    def __init__(self, name, return_type):
        self.name = name
        self.return_type : LuaType = return_type
        self.parameters :list[LuaFunctionArg] =[]
    def add_parameter(self, param):
        self.parameters.append(param)
    def to_string(self):
        return f"{self.name}({', '.join([str(param) for param in self.parameters])}) -> {self.return_type}"
    def __repr__(self):
        return f"LuaFunction(name={self.name}, return_type={self.return_type}, parameters={self.parameters})"

class LuaProperty:
    def __init__(self, name, type):
        self.name = name
        self.type : LuaType = type
        self.is_public = False
    def __repr__(self):
        return f"LuaProperty(name={self.name}, type={self.type})"

class LuaType:
    def __init__(self, name, type, template_types=[]):
        self.typename = name
        self.properties : list[LuaProperty] = []
        self.methods : list[LuaFunction] = []
        self.type : int = type 
        self.template_types : list[LuaType] = template_types
    def find_property(self, name):
        for prop in self.properties:
            if prop.name == name:
                return prop
        return None
    def find_function(self, name):
        for func in self.methods:
            if func.name == name:
                return func
        return None
    def __repr__(self):
        return f"LuaType(name={self.typename})"
    
    def are_types_compatible(self, other) -> bool:
        if self.type == other.type and self.type != OBJECT_TYPE:
            return True
        if self.type == other.type and self.typename == other.typename:
            return True
        if self.type == OBJECT_TYPE and other.type == NULL_TYPE or self.type == NULL_TYPE and other.type==OBJECT_TYPE:
            return True
        
        if self.type == INT_TYPE and other.type == FLOAT_TYPE or self.type == FLOAT_TYPE and other.type == INT_TYPE:
            return True
        
        return False
        


class LuaTypeDatabase:
    def __init__(self):
        self.types = {}

    def add_type(self, name, type):
        if name not in self.types:
            self.types[name] = LuaType(name, type)
        else:
            raise ValueError(f"Type {name} already exists.")

    def get_type(self, name):
        return self.types.get(name)

    def __repr__(self):
        return f"LuaTypeDatabase(types={self.types})"


# determine types in project


class Tokens:
    def __init__(self, tokens):
        self.tokens = tokens
    def expect(self, expected : str):
        if self.next() != expected:
            raise ValueError(f"Expected {expected}, but got {self.tokens[0]}")
    def peek(self) -> str:
        return self.tokens[0]
    def next(self) -> str:
        s = self.peek()
        self.tokens = self.tokens[1:]
        return s
    def is_empty(self):
        return len(self.tokens) == 0
    def split(self,token : str):
        index = 0
        for t in self.tokens:
            if t == token:
                break
            index+=1
        other = Tokens(self.tokens[:index])
        self.tokens = self.tokens[index+1:]
        return other


def string_to_tokens(string : str) -> Tokens:
    symbols = ["<", ">", ",", "(", ")", "->", "<=", "=>", "=", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "&&", "||", "!", "++", "--", "{", "}", "[", "]", ":","*", "+", "-", "/"]

    tokens = []
    current_token = ""
    in_string = False

    index : int = 0

    while index < len(string):
        char = string[index]
        index += 1
        if char == "\"":
            in_string = not in_string
            if not in_string:
                tokens.append(current_token)
                current_token = ""
        elif in_string:
            current_token += char
        else:
            if char == " ":
                if current_token:
                    tokens.append(current_token)
                    current_token = ""
            else:
                next_char = string[index] if index < len(string) else ""
                if char + next_char in symbols:
                    if current_token:
                        tokens.append(current_token)
                        current_token = ""
                    tokens.append(char + next_char)
                    index += 1
                elif (char in symbols) or (char == "." and not next_char.isnumeric()):
                    if current_token:
                        tokens.append(current_token)
                        current_token = ""
                    tokens.append(char)
                else:
                    current_token += char
    if current_token:
        tokens.append(current_token)
    return Tokens(tokens)

def parse_function_from_tokens(database : LuaTypeDatabase, tokens : Tokens) -> LuaFunction:
    if tokens.is_empty():
        raise ValueError("No tokens to parse.")
    
    name = tokens.next()
    tokens.expect("(")
    func = LuaFunction(name, VOID_TYPE)
    
    while not tokens.is_empty() and tokens.peek() != ")":
        param_type = parse_type_from_tokens(database, tokens)
        param_name = tokens.next()
        func.add_parameter(LuaFunctionArg(param_name, param_type))
        if tokens.peek() == ",":
            tokens.next()
    
    tokens.expect(")")
    if tokens.peek() == "->":
        tokens.next()
        func.return_type = parse_type_from_tokens(database, tokens)

    return func

def parse_type_from_tokens(database : LuaTypeDatabase, tokens : Tokens) -> LuaType:
    if tokens.is_empty():
        raise ValueError("No tokens to parse.")
    
    builtin_types = {
        "int": INT_TYPE,
        "float": FLOAT_TYPE,
        "string": STRING_TYPE,
        "bool": BOOL_TYPE,
        "void": VOID_TYPE,
    }
    if tokens.peek() in builtin_types:
        name = tokens.next()
        return LuaType(name, builtin_types[name])
    
    datastructure_types = {
        "array": ARRAY_TYPE,
        "set": SET_TYPE,
        "dict": DICT_TYPE,
    }
    if tokens.peek() in datastructure_types:
        type = tokens.next()
        if type == "dict":
            tokens.expect("<")
            key_type = parse_type_from_tokens(database, tokens)
            tokens.expect(",")
            value_type = parse_type_from_tokens(database, tokens)
            tokens.expect(">")
            return LuaType(type, datastructure_types[type], [key_type, value_type])
        else:
            tokens.expect("<")
            key_type = parse_type_from_tokens(database, tokens)
            tokens.expect(">")
            return LuaType(type, datastructure_types[type], [key_type])
        
    return LuaType(tokens.next(), OBJECT_TYPE)
            
class Scope:
    def __init__(self, parent=None):
        self.variables : dict[str, LuaType]= {}
        self.parent : Scope = parent

    def add_variable(self, name, type : LuaType):
        if name not in self.variables:
            self.variables[name] = type
        else:
            raise ValueError(f"Variable {name} already exists.")
    def get_variable(self, name) -> LuaType:
        if name in self.variables:
            return self.variables[name]
        elif self.parent is not None:
            return self.parent.get_variable(name)
        else:
            return None


# int a = 5
# int b = a + function_call(5,5)
# object.method(a, another.func()+1)
# object.a = 10 * object.b
# variable = 10

# get first object, descend the "." operator

# literals:
# false, true -> bool type
# nill -> null type
# 0 -> integer
# 0.1 -> float
# "some string" -> string


def get_type_of_token(tokens : Tokens, database : LuaTypeDatabase, scope : Scope) -> (LuaType | LuaFunction):
    first = tokens.next()
    if first == "false" or first == "true":
        return LuaType("bool", BOOL_TYPE)
    if first == "nill":
        return LuaType("nill", NULL_TYPE)
    if first.isnumeric():
        return LuaType("integer", INT_TYPE)
    if first.startswith("\"") and first.endswith("\""):
        return LuaType("string", STRING_TYPE)
    if first.find(".") != -1:
        return LuaType("float", FLOAT_TYPE)
    
    var : LuaType= scope.get_variable(first)
    while tokens.peek() != ".":
        tokens.next()
        if not var:
            raise ValueError("couldnt find variable")
        sub = tokens.next()
        if var.find_function(sub):
            return var.find_function(sub)
        var = var.find_property(sub).type
            
    if not var:
        raise ValueError("couldnt find variable")
    return var

OP_COMPARISON = 1
OP_LOGICAL = 2
OP_MATH = 3
OP_ASSIGNMENT = 4
OP_LPAREN = 5

def get_type_of_expression(tokens : Tokens, database : LuaTypeDatabase, scope : Scope) -> LuaType:
    output : list[LuaType|str] = []
    stack = []

    ops = ["and","or","not","<",">","<=",">=","==","!=", "+","-","*","/","=","+=","-=","*=","/=","[","]" ]

    while not tokens.is_empty():
        if tokens.peek() in ops:
            stack.append(tokens.next())
        else:
            type_or_function = get_type_of_token(tokens, database, scope)
            if isinstance(type_or_function,LuaFunction):
                tokens.expect("(")
                output = 


                output.append(type_or_function.return_type)
            else:
                output.append(type_or_function)

def type_check_line(tokens : Tokens, database : LuaTypeDatabase, scope : Scope):
    first = tokens.next()
    if database.get_type(first) is not None:
        tokens.next()
        type = database.get_type(first)
        if tokens.peek() == "=":
            tokens.next()
            value = get_type_of_expression(tokens, database, scope)
            if value != type:
                raise ValueError(f"Type mismatch: {type} != {value}")
            scope.add_variable(first, type)
    elif first == "auto":
        varname = tokens.next()
        tokens.expect("=")
        value = get_type_of_expression(tokens, database, scope)
        scope.add_variable(varname, value)
    else:
        variable = scope.get_variable(first)
        if variable is None:
            raise ValueError(f"Variable {first} not found.")
        tokens.expect("=")

def type_check_string(string : str):
    lines = string.split("\n")
    database = LuaTypeDatabase()

    cur_function : LuaFunction = None
    cur_scope = None

    for line in lines:
        line.strip()
        if not line or line.startswith("//") or line.startswith("---"):
            continue
        tokens = string_to_tokens(line)
        if tokens.is_empty():
            continue

        first = tokens.next()

        if first == "end":
            assert(cur_function is not None and cur_scope is not None)
            cur_scope = cur_scope.parent
            if cur_scope is None:
                cur_function = None
        elif first == "function":
            assert(cur_function is None and cur_scope is None)
            cur_function = parse_function_from_tokens(database, tokens)
            cur_scope = Scope()
        elif first == "if":
            assert(cur_function is not None)
            cur_scope = Scope(cur_scope)
            assert(tokens.tokens[-1]== "then")
            tokens = tokens.tokens[:-1]
            type_check_line(tokens, database, cur_scope)
       
        elif first == "while":
            assert(cur_function is not None and cur_scope is not None)
            cur_scope = Scope(cur_scope)
            assert(tokens.tokens[-1]== "then")
            tokens = tokens.tokens[:-1]
            type_check_line(tokens, database, cur_scope)
        elif first == "elseif":
            assert(cur_function is not None and cur_scope is not None)
            cur_scope = cur_scope.parent
            assert(cur_scope is not None)
            cur_scope = Scope(cur_scope)
            assert(tokens.tokens[-1]== "then")
            tokens = tokens.tokens[:-1]
            type_check_line(tokens, database, cur_scope)
        elif first == "else":
            assert(cur_function is not None and cur_scope is not None)
            cur_scope = cur_scope.parent
            cur_scope = Scope(cur_scope)
        elif first == "for":
            assert(cur_function is not None and cur_scope is not None)
            cur_scope = Scope(cur_scope)
        elif first == "return":
            assert(cur_function is not None)
            type = get_type_of_expression(tokens, database, cur_scope)
            if type != cur_function.return_type:
                raise ValueError(f"Return type {type} does not match function return type {cur_function.return_type}")
        else:
            type_check_line(string_to_tokens(line), database, cur_scope)
            



def main():
    test_strings = [
        "array<int>",
        "SomeObject",
        "dict<int, string>",
        "dict<SomeObject, array<int>>",
    ]
    for s in test_strings:
        tokens = string_to_tokens(s)
        type = parse_type_from_tokens(LuaTypeDatabase(), tokens)
        print(type.typename)

    function_test_strings = [
        "test(int a, float b) -> string",
        "test2() -> void",
        "test3(int a, float b) -> array<int>",
        "test4() -> dict<int, string>",
    ]

    for s in function_test_strings:
        tokens = string_to_tokens(s)
        func = parse_function_from_tokens(LuaTypeDatabase(), tokens)
        print(func.to_string())

if __name__ == "__main__":
    main()