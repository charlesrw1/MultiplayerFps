
import unittest
import shutil
import tempfile

import codegen_run as cg


import os

# test parsing (failures/bad parses)
# test output reflection
# test function codegen

# test parsing of REFLECT(...)
# test parsing of a type

CLASSBASE = "ClassBase"

class TestTypeBuilder:
    def __init__(self):
        self.dict : dict[str,cg.ClassDef] = {}
        self.dict[CLASSBASE] = cg.ClassDef([CLASSBASE],cg.ClassDef.TYPE_CLASS,False)
        self.CLASS_BASE = self.dict[CLASSBASE]

    def add_class(self,name:str,inherits:str) -> cg.ClassDef:
        c = cg.ClassDef([name,inherits],cg.ClassDef.TYPE_CLASS,False)
        self.dict[name] = c
        return c
    def add_struct(self,name:str) -> cg.ClassDef:
        c = cg.ClassDef([name],cg.ClassDef.TYPE_STRUCT,False)
        self.dict[name] = c
        return c
    def add_enum(self,name:str) -> cg.ClassDef:
        c = cg.ClassDef([name],cg.ClassDef.TYPE_ENUM,False)
        self.dict[name] = c
        return c
    def finish(self):
        cg.ClassDef.fixup_types(self.dict)





class CodegenUnitTest(unittest.TestCase):
    # test macro parsing

    def make_testing_dict(self) -> dict[str,cg.ClassDef]:
        BASE_CLASS = cg.ClassDef(["Base"],cg.ClassDef.TYPE_CLASS,False)
        PLAYER_CLASS = cg.ClassDef(["Player","Base"],cg.ClassDef.TYPE_CLASS,False)
        MODEL_CLASS = cg.ClassDef(["Model","Base"],cg.ClassDef.TYPE_CLASS,False)

        typenames : dict[str,cg.ClassDef] = {
            "Base":BASE_CLASS,
           "Player": PLAYER_CLASS,
           "Model" : MODEL_CLASS
        }
        return typenames
    
    def test_typename_heirarchy(self):
        builder = TestTypeBuilder()
        myclass = builder.add_class("Animal",CLASSBASE)
        dog = builder.add_class("Dog", "Animal")
        poodle = builder.add_class("Poodle","Dog")
        hippo = builder.add_class("Hippo","Animal")
        building = builder.add_class("Building",CLASSBASE)
        book = builder.add_struct("Book")
        booktype = builder.add_enum("BookType")

        builder.finish()
        self.assertTrue(myclass.super_type_def==builder.CLASS_BASE)
        self.assertTrue(dog.super_type_def==myclass)
        self.assertTrue(poodle.super_type_def==dog)
        self.assertTrue(hippo.super_type_def==myclass)
        self.assertTrue(building.super_type_def==builder.CLASS_BASE)
        self.assertTrue(book.super_type_def==None and book.object_type==cg.ClassDef.TYPE_STRUCT)
        self.assertTrue(booktype.object_type==cg.ClassDef.TYPE_ENUM)


        # now do some test
        empty_enum = enumerate[str]("")
        dogdef = cg.find_class_in_typenames("class Dog : public Animal {", empty_enum, builder.dict)
        self.assertTrue(dogdef==dog)
        poodledef = cg.find_class_in_typenames("class Poodle : public Dog {", empty_enum, builder.dict)
        self.assertTrue(poodledef==poodle)
        bookdef = cg.find_struct_in_typenames("struct Book",empty_enum,builder.dict)
        self.assertTrue(bookdef==book)

        funcdef = cg.parse_property("REF Dog* mydog = nullptr;",empty_enum,builder.dict)
        self.assertTrue(funcdef.name=="mydog")
        self.assertTrue(funcdef.new_type.is_pointer)
        self.assertTrue(funcdef.new_type.type == cg.OTHER_CLASS_TYPE)
        self.assertTrue(funcdef.new_type.typename == dog)

        funcdef = cg.parse_property("REF void mydogfunc(const Poodle* p)",empty_enum,builder.dict)
        self.assertTrue(funcdef.name=="mydogfunc")
        self.assertTrue(funcdef.new_type.type == cg.FUNCTION_TYPE)
        self.assertTrue(funcdef.return_type.type == cg.NONE_TYPE)
        self.assertTrue(len(funcdef.func_args)==1)
        arg = funcdef.func_args[0]
        self.assertTrue(arg[0].is_pointer)
        self.assertTrue(arg[0].constant)
        self.assertTrue(arg[0].typename==poodle)
        self.assertTrue(arg[1]=="p")

        funcdef = cg.parse_property("REF Book a_good_book;",empty_enum,builder.dict)
        self.assertTrue(funcdef.name=="a_good_book")
        self.assertTrue(funcdef.new_type.type == cg.STRUCT_TYPE)
        self.assertTrue(funcdef.new_type.typename == book)

        something = cg.parse_property("REF BookType type;",empty_enum,builder.dict)
        self.assertTrue(something.name=="type")
        self.assertTrue(something.new_type.type == cg.ENUM_TYPE)
        self.assertTrue(something.new_type.typename == booktype)

        something = cg.parse_property("REF std::vector<Book*> books;",empty_enum,builder.dict)
        self.assertTrue(something.new_type.type == cg.ARRAY_TYPE)
        self.assertTrue(something.new_type.template_args[0].typename == book)
        self.assertTrue(something.new_type.template_args[0].is_pointer)
        self.assertTrue(something.new_type.template_args[0].type == cg.STRUCT_TYPE)

        something = cg.parse_property("REF std::vector<uptr<Dog>> dogs;",empty_enum,builder.dict)
        self.assertTrue(something.new_type.type == cg.ARRAY_TYPE)
        self.assertTrue(something.new_type.template_args[0].type == cg.UNIQUE_PTR_TYPE)
        self.assertTrue(something.new_type.template_args[0].template_args[0].typename==dog)

        SOURCE_DIR = "/../TestFiles/CodeGen/"
        DIRS_TO_SKIP = ["./.generated","./External"]
        this_dir = os.path.dirname(os.path.abspath(__file__))
        fixtures = this_dir + SOURCE_DIR
        # Run codegen in an isolated temp tree so we don't pollute TestFiles/.
        prev_cwd = os.getcwd()
        with tempfile.TemporaryDirectory() as tmp:
            sandbox = os.path.join(tmp, "src")
            shutil.copytree(fixtures, sandbox, ignore=shutil.ignore_patterns(".generated"))
            lua_out = os.path.join(tmp, "lua")
            os.makedirs(lua_out, exist_ok=True)
            os.chdir(sandbox)
            try:
                cg.do_codegen(lua_out, '.', DIRS_TO_SKIP, True)
            finally:
                os.chdir(prev_cwd)


    def test_parse_reflect_macro(self):
        out = cg.parse_reflect_macro(" REFLECT( \"type\"=xyz )")
        self.assertTrue(out.custom_type=="xyz")
        out = cg.parse_reflect_macro("REFLECT(hide)")
        self.assertTrue(out.hide)

    def test_codegen_error_format(self):
        # CodegenError formats as a compiler-style file:line:col: error: msg
        e = cg.CodegenError("oops", path="foo.h", line=42)
        self.assertIn("foo.h:42:", str(e))
        self.assertIn("error: oops", str(e))
        # No location info → still includes a placeholder
        e2 = cg.CodegenError("naked")
        self.assertIn("error: naked", str(e2))

    def test_strict_unknown_type_errors(self):
        # In strict mode, an unknown type raises CodegenError instead of silent degrade.
        # Flag lives on codegen_lib, not the codegen_run alias used here.
        import codegen_lib
        t = self.make_testing_dict()
        try:
            codegen_lib.STRICT_UNKNOWN_TYPES = True
            with self.assertRaises(cg.CodegenError):
                cg.parse_type("Mysterion", t)
            # Sanity: a known type still parses fine.
            cpp = cg.parse_type("int", t)
            self.assertEqual(cpp.type, cg.INT_TYPE)
        finally:
            codegen_lib.STRICT_UNKNOWN_TYPES = False

    def test_fixup_missing_parent_errors(self):
        # Unknown supername must produce a CodegenError naming both child + missing parent.
        b = TestTypeBuilder()
        bad = b.add_class("BadChild", "DoesNotExist")
        with self.assertRaises(cg.CodegenError) as ctx:
            b.finish()
        msg = str(ctx.exception)
        self.assertIn("BadChild", msg)
        self.assertIn("DoesNotExist", msg)

    def test_entityptr_default_target(self):
        # EntityPtr with no template arg gets Entity auto-filled via the declarative table.
        b = TestTypeBuilder()
        b.add_class("Entity", CLASSBASE)
        b.finish()
        cpp = cg.parse_type("EntityPtr", b.dict)
        self.assertEqual(cpp.type, cg.HANDLE_PTR_TYPE)
        self.assertEqual(len(cpp.template_args), 1)
        self.assertEqual(cpp.template_args[0].typename.classname, "Entity")

    def test_reflect_attributes(self):
        # min/max/step parse as floats
        out = cg.parse_reflect_macro("REFLECT(min=0, max=1, step=0.01)")
        self.assertEqual(out.attr_min, 0.0)
        self.assertEqual(out.attr_max, 1.0)
        self.assertAlmostEqual(out.attr_step, 0.01)
        self.assertTrue(out.has_any_attrs())

        # category is a string, hidden/readonly are flags
        out = cg.parse_reflect_macro('REFLECT(category="Physics", hidden, readonly)')
        self.assertEqual(out.attr_category, "Physics")
        self.assertTrue(out.attr_hidden)
        self.assertTrue(out.attr_readonly)

        # absence: no attrs, empty emit string
        out = cg.parse_reflect_macro("REFLECT(hide)")
        self.assertFalse(out.has_any_attrs())
        self.assertEqual(out.emit_attrs_struct(), "")

        # emit shape contains designated-init fields and a category string literal
        out = cg.parse_reflect_macro('REFLECT(min=-2.5, max=2.5, category="Speed")')
        emitted = out.emit_attrs_struct()
        self.assertIn(".min=-2.5f", emitted)
        self.assertIn(".max=2.5f", emitted)
        self.assertIn('.category="Speed"', emitted)
        self.assertTrue(emitted.startswith("PropertyAttributes{"))

    def test_reflect_no_nil(self):
        # default: no_nil is False
        out = cg.parse_reflect_macro("REFLECT(hide)")
        self.assertFalse(out.no_nil)

        # standalone
        out = cg.parse_reflect_macro("REFLECT(no_nil)")
        self.assertTrue(out.no_nil)

        # combined with other args
        out = cg.parse_reflect_macro("REFLECT(hide, no_nil)")
        self.assertTrue(out.hide and out.no_nil)

        # lua type string suppresses |nil for pointer-like types
        import codegen_generate as cgen
        class FakeName:
            classname = "Entity"
        t_other = cg.CppType("Entity", FakeName(), cg.OTHER_CLASS_TYPE, is_ptr=True)
        self.assertEqual(cgen.get_lua_type_string(t_other), "Entity|nil")
        self.assertEqual(cgen.get_lua_type_string(t_other, True), "Entity")

        t_handle = cg.CppType("Handle<Entity>", None, cg.HANDLE_PTR_TYPE,
                              templates=[cg.CppType("Entity", FakeName(), cg.OTHER_CLASS_TYPE)])
        self.assertEqual(cgen.get_lua_type_string(t_handle), "Entity|nil")
        self.assertEqual(cgen.get_lua_type_string(t_handle, True), "Entity")

        # array element no_nil propagates through templates
        t_arr = cg.CppType("std::vector<Entity*>", None, cg.ARRAY_TYPE, templates=[t_other])
        self.assertEqual(cgen.get_lua_type_string(t_arr), "Entity|nil[]")
        self.assertEqual(cgen.get_lua_type_string(t_arr, True), "Entity[]")
    def test_type_parse(self):
        out = cg.parse_type("std::vector<int>",{})
        self.assertTrue(out.type==cg.ARRAY_TYPE)
        self.assertTrue(len(out.template_args)==1 and out.template_args[0].type == cg.INT_TYPE)
        out = cg.parse_type("vector<int>",{})
        self.assertTrue(len(out.template_args)==1 and out.template_args[0].type == cg.INT_TYPE)


        BASE_CLASS = cg.ClassDef(["Base"],cg.ClassDef.TYPE_CLASS,False)
        PLAYER_CLASS = cg.ClassDef(["Player","Base"],cg.ClassDef.TYPE_CLASS,False)
        MODEL_CLASS = cg.ClassDef(["Model","Base"],cg.ClassDef.TYPE_CLASS,False)

        typenames : dict[str,cg.ClassDef] = {
            "Base":BASE_CLASS,
           "Player": PLAYER_CLASS,
           "Model" : MODEL_CLASS
        }
        cg.ClassDef.fixup_types(typenames)
        self.assertTrue(PLAYER_CLASS.super_type_def==BASE_CLASS)

        out = cg.parse_type("const Player* p",typenames)
        self.assertTrue(out.type==cg.OTHER_CLASS_TYPE)
        self.assertTrue(out.typename == PLAYER_CLASS)
        self.assertTrue(out.is_pointer == True)

        out = cg.parse_type("const AssetPtr<const Model> my_model = ",typenames)
        self.assertTrue(out.type==cg.ASSET_PTR_TYPE)
        self.assertTrue(len(out.template_args)==1 and out.template_args[0].typename == MODEL_CLASS)
        
        out = cg.parse_type("MulticastDelegate<int,Model*> my_model = ",typenames)
        self.assertTrue(out.type==cg.MULTICAST_TYPE)
        self.assertTrue(len(out.template_args)==2 and out.template_args[0].type==cg.INT_TYPE and out.template_args[1].type ==cg.OTHER_CLASS_TYPE)
        

    def test_optional_type_parse(self):
        out = cg.parse_type("std::optional<int>", {})
        self.assertEqual(out.type, cg.OPTIONAL_TYPE)
        self.assertEqual(len(out.template_args), 1)
        self.assertEqual(out.template_args[0].type, cg.INT_TYPE)

        out = cg.parse_type("opt<float>", {})
        self.assertEqual(out.type, cg.OPTIONAL_TYPE)
        self.assertEqual(out.template_args[0].type, cg.FLOAT_TYPE)

    def test_pair_type_parse(self):
        out = cg.parse_type("std::pair<bool,int>", {})
        self.assertEqual(out.type, cg.PAIR_TYPE)
        self.assertEqual(len(out.template_args), 2)
        self.assertEqual(out.template_args[0].type, cg.BOOL_TYPE)
        self.assertEqual(out.template_args[1].type, cg.INT_TYPE)

    def test_optional_pair_function_codegen(self):
        import codegen_generate as cgen
        base = cg.ClassDef(["Base"], cg.ClassDef.TYPE_CLASS, False)
        typenames: dict[str, cg.ClassDef] = {"Base": base}
        emptyenum = enumerate(iter(""))

        prop = cg.parse_property("REF opt<int> my_value()", emptyenum, typenames)
        self.assertEqual(prop.return_type.type, cg.OPTIONAL_TYPE)
        code = cgen.write_script_function(base, prop)
        self.assertIn("push_std_optional_to_lua", code)
        self.assertIn("return 1;", code)
        self.assertEqual(cgen.get_lua_type_string(prop.return_type, False), "integer|nil")

        prop2 = cg.parse_property("REF pair<bool,int> slider(int changed)", emptyenum, typenames)
        self.assertEqual(prop2.return_type.type, cg.PAIR_TYPE)
        code2 = cgen.write_script_function(base, prop2)
        self.assertIn("return_value.first", code2)
        self.assertIn("return_value.second", code2)
        self.assertIn("return 2;", code2)
        self.assertEqual(cgen.get_lua_return_annotation(prop2.return_type, False), "boolean, integer")

    def test_parse_class_decl(self):
        out = cg.parse_class_decl("class myclass")
        self.assertTrue(out[0]=="myclass")
        out = cg.parse_class_decl("class myclass{")
        self.assertTrue(out[0]=="myclass")
        out = cg.parse_class_decl("class myclass: public someother, public something2{")
        self.assertTrue(out[0]=="myclass" and out[1]=="someother" and out[2]=="something2")
        out = cg.parse_class_decl("class myclass: someother")
        self.assertTrue(out[0]=="myclass"and out[1]=="someother")

        out = cg.parse_struct_decl("struct mystruct { // comment")
        self.assertTrue(out=="mystruct")


    def test_parse_property(self):
        BASE_CLASS = cg.ClassDef(["Base"],cg.ClassDef.TYPE_CLASS,False)
        PLAYER_CLASS = cg.ClassDef(["Player","Base"],cg.ClassDef.TYPE_CLASS,False)
        MODEL_CLASS = cg.ClassDef(["Model","Base"],cg.ClassDef.TYPE_CLASS,False)

        typenames : dict[str,cg.ClassDef] = {
            "Base":BASE_CLASS,
           "Player": PLAYER_CLASS,
           "Model" : MODEL_CLASS
        }

        emptyenum = enumerate(iter(""))

        name,args = cg.parse_function("void myfunction(int x, int y)",{})
        self.assertTrue(name=="myfunction" and len(args)==2)
        prop = cg.parse_property("REF static int func()",emptyenum,{})
        self.assertTrue(prop.name == "func")
        self.assertTrue(prop.new_type.type == cg.FUNCTION_TYPE and prop.is_static)
        self.assertTrue(prop.return_type.type == cg.INT_TYPE)
        self.assertTrue(len(prop.func_args)==0)

        func = cg.parse_property("REF virtual void update()",emptyenum,typenames)
        self.assertTrue(func.is_virtual)
        self.assertTrue(func.return_type.type==cg.NONE_TYPE)

        prop = cg.parse_property("REF std::vector<AssetPtr<const int*>> mods;",emptyenum,typenames)
        tostr = prop.new_type.get_raw_type_string()
        self.assertTrue(tostr=="std::vector<AssetPtr<const int*>>")


        prop = cg.parse_property("REF AssetPtr<Model> mymodel;", emptyenum, typenames)
        self.assertTrue(prop.name == "mymodel" and prop.new_type.type == cg.ASSET_PTR_TYPE)
        prop = cg.parse_property("REF static int is_static;", emptyenum, typenames)
        self.assertTrue(prop.name == "is_static" and prop.is_static)
    def test_remove_comment(self):
        removed = cg.remove_comment_from_end("REF bool simulate_physics = false;\t\t// if true, then object is a DYNAMIC object driven by the physics simulation")
        self.assertTrue(removed=="REF bool simulate_physics = false;\t\t")
    def test_parse_class_line(self):
        out = cg.parse_class_from_start("class ForwardDeclared;// some comment lol",enumerate(""))
        self.assertTrue(out==None)
    
class MyTester(unittest.TestCase):
    def run_test_file(self):
        SOURCE_DIR = "/../TestFiles/CodeGen/"
        DIRS_TO_SKIP = ["./.generated","./External"]
        this_dir = os.path.dirname(os.path.abspath(__file__))
        os.chdir(this_dir+SOURCE_DIR)
        with tempfile.TemporaryDirectory() as lua_out:
            cg.do_codegen(lua_out, '.', DIRS_TO_SKIP, True)


        """
    def test_file_output(self):
        FILE = "../TestFiles/testfile.h"
        typedict : dict[str,int] ={}
        typedict["ClassTypeInfo"] = cg.STRUCT_OBJECT
        typedict["Component"] = cg.STRUCT_OBJECT
        typedict["Entity"] = cg.STRUCT_OBJECT


        classes,_ = cg.parse_file(FILE,typedict)
        self.assertTrue(len(classes)==1)
        c = classes[0]
        self.assertTrue(c.classname=="Entity")
        self.assertTrue(c.supername=="BaseUpdater")
        self.assertTrue(c.object_type == cg.CLASS_OBJECT)
        """

import io
import tempfile
import os

def parse_newenum_text(text: str):
    """Helper: run both codegen passes on a string, return parsed classes."""
    typenames = cg.read_typenames_from_text(io.StringIO(text), "<test>")
    cg.ClassDef.fixup_types(typenames)
    with tempfile.NamedTemporaryFile(mode='w', suffix='.h', delete=False) as f:
        f.write(text)
        tmp_path = f.name
    try:
        classes, _ = cg.parse_file(tmp_path, typenames)
    finally:
        os.unlink(tmp_path)
    return classes

class TestNewenum(unittest.TestCase):
    def test_multiline_enum(self):
        text = "NEWENUM(guiAlignment, uint8_t){\n\tLeft,\n\tCenter,\n\tRight,\n\tFill\n};\n"
        classes = parse_newenum_text(text)
        self.assertEqual(len(classes), 1)
        c = classes[0]
        self.assertEqual(c.object_type, cg.ClassDef.TYPE_ENUM)
        names = [p.name for p in c.properties]
        self.assertEqual(names, ["Left", "Center", "Right", "Fill"])

    def test_singleline_enum(self):
        text = "NEWENUM(rootmotion_setting, uint8_t){keep, remove, add_velocity};\n"
        classes = parse_newenum_text(text)
        self.assertEqual(len(classes), 1)
        c = classes[0]
        self.assertEqual(c.object_type, cg.ClassDef.TYPE_ENUM)
        names = [p.name for p in c.properties]
        self.assertEqual(names, ["keep", "remove", "add_velocity"])

    def test_singleline_enum_with_values_after_clangformat(self):
        # clang-format collapses: NEWENUM(Foo, int){A, B, C, D};
        text = "NEWENUM(Foo, int){A, B, C, D};\n"
        classes = parse_newenum_text(text)
        self.assertEqual(len(classes), 1)
        names = [p.name for p in classes[0].properties]
        self.assertEqual(names, ["A", "B", "C", "D"])

    def test_multivalue_per_line_enum(self):
        # opening brace on NEWENUM line, values comma-separated on next line
        text = "NEWENUM(Easing, uint8_t){\n\tLinear, CubicEaseIn, CubicEaseOut, CubicEaseInOut, Constant,\n};\n"
        classes = parse_newenum_text(text)
        self.assertEqual(len(classes), 1)
        names = [p.name for p in classes[0].properties]
        self.assertEqual(names, ["Linear", "CubicEaseIn", "CubicEaseOut", "CubicEaseInOut", "Constant"])

class TestInterfaceParsing(unittest.TestCase):
    def test_interface_body_detected(self):
        text = """class IMyInterface {
public:
    INTERFACE_BODY();
    REF virtual int get_value() { return 0; }
};
"""
        typenames = cg.read_typenames_from_text(io.StringIO(text), "<test>")
        self.assertIn("IMyInterface", typenames)
        self.assertEqual(typenames["IMyInterface"].object_type, cg.ClassDef.TYPE_INTERFACE)

    def test_class_with_interface(self):
        text = """class IMyInterface {
public:
    INTERFACE_BODY();
};
class MyBase {
public:
    CLASS_BODY(MyBase);
};
class MyChild : public MyBase, public IMyInterface {
public:
    CLASS_BODY(MyChild);
};
"""
        typenames = cg.read_typenames_from_text(io.StringIO(text), "<test>")
        cg.ClassDef.fixup_types(typenames)
        child = typenames["MyChild"]
        self.assertEqual(len(child.interface_defs), 1)
        self.assertEqual(child.interface_defs[0].classname, "IMyInterface")

    def test_max_interfaces_exceeded(self):
        ifaces = ""
        for i in range(5):
            ifaces += f"class I{i} {{\npublic:\n    INTERFACE_BODY();\n}};\n"
        names = ", ".join(f"public I{i}" for i in range(5))
        text = ifaces + f"""class MyBase {{
public:
    CLASS_BODY(MyBase);
}};
class Bad : public MyBase, {names} {{
public:
    CLASS_BODY(Bad);
}};
"""
        typenames = cg.read_typenames_from_text(io.StringIO(text), "<test>")
        with self.assertRaises(cg.CodegenError):
            cg.ClassDef.fixup_types(typenames)

    def test_duplicate_interface_rejected(self):
        text = """class IFoo {
public:
    INTERFACE_BODY();
};
class MyBase {
public:
    CLASS_BODY(MyBase);
};
class Bad : public MyBase, public IFoo, public IFoo {
public:
    CLASS_BODY(Bad);
};
"""
        typenames = cg.read_typenames_from_text(io.StringIO(text), "<test>")
        with self.assertRaises(cg.CodegenError):
            cg.ClassDef.fixup_types(typenames)

    def test_non_interface_rejected(self):
        text = """class NotAnInterface {
public:
    CLASS_BODY(NotAnInterface);
};
class MyBase {
public:
    CLASS_BODY(MyBase);
};
class Bad : public MyBase, public NotAnInterface {
public:
    CLASS_BODY(Bad);
};
"""
        typenames = cg.read_typenames_from_text(io.StringIO(text), "<test>")
        with self.assertRaises(cg.CodegenError):
            cg.ClassDef.fixup_types(typenames)

    def test_interface_properties_parsed(self):
        text = """class IMyInterface {
public:
    INTERFACE_BODY();
    REF virtual int get_value() { return 0; }
    REF virtual void do_stuff() {}
};
"""
        typenames = cg.read_typenames_from_text(io.StringIO(text), "<test>")
        cg.ClassDef.fixup_types(typenames)
        with tempfile.NamedTemporaryFile(mode='w', suffix='.h', delete=False) as f:
            f.write(text)
            tmp_path = f.name
        try:
            classes, _ = cg.parse_file(tmp_path, typenames)
        finally:
            os.unlink(tmp_path)
        self.assertEqual(len(classes), 1)
        iface = classes[0]
        self.assertEqual(iface.object_type, cg.ClassDef.TYPE_INTERFACE)
        func_names = [p.name for p in iface.properties if p.new_type.type == cg.FUNCTION_TYPE]
        self.assertEqual(func_names, ["get_value", "do_stuff"])


class TestLuaSharpBinding(unittest.TestCase):
    HEADER = """class ClassBase {
public:
    CLASS_BODY(ClassBase);
};
NEWENUM(Mode, uint8_t){
    Off = 2,
    On = 4,
};
class Animal;
struct Hit {
    STRUCT_BODY();
    REF Animal* who = nullptr;
    REF obj<Animal> owner;
    REF Mode mode = Mode::Off;
    REF float dist = 0;
    REF std::vector<int> tags;
};
struct Wrap {
    STRUCT_BODY();
    REF Hit inner;
    REF int n = 3;
};
class Animal : public ClassBase {
public:
    CLASS_BODY(Animal, scriptable);
    REF virtual Wrap wrap(const Wrap& w) { return w; }
    REF virtual void speak(int times) {}
    REF virtual Animal* best(const std::string& tag) { return nullptr; }
    REF int legs = 4;
};
class ClassTypeInfo : public ClassBase {
public:
    CLASS_BODY(ClassTypeInfo);
    REFLECT(type_of)
    static const ClassTypeInfo* find(const std::string& classname);
};
class Dog : public Animal {
public:
    CLASS_BODY(Dog);
    REF void speak(int times) override {}
    REF static Dog* find(const std::string& name);
    REF glm::vec3 where(Mode m, Animal* other);
    REF std::vector<int> bones();
    REF std::vector<Hit> hits(const std::vector<Animal*>& pack);
    REF std::vector<std::vector<int>> grid();
    REF Hit probe(const Hit& h);
    REF obj<Animal> leader;
    REF Hit last;
    REF void bark();
    REF void bark(int loud);
    REF void in();
    REFLECT(script_read)
    float speed = 1;
    REFLECT(hide)
    int hidden = 0;
    REFLECT(lua_generic)
    Animal* pick(const ClassTypeInfo* type, int n);
    REFLECT(lua_generic)
    static Animal* make(const ClassTypeInfo* type);
    REFLECT(debug_view)
    int age() const;
};
"""

    def parse(self):
        typenames = cg.read_typenames_from_text(io.StringIO(self.HEADER), "<test>")
        cg.ClassDef.fixup_types(typenames)
        with tempfile.NamedTemporaryFile(mode='w', suffix='.h', delete=False) as f:
            f.write(self.HEADER)
            tmp_path = f.name
        try:
            classes, _ = cg.parse_file(tmp_path, typenames)
        finally:
            os.unlink(tmp_path)
        return classes, typenames

    def generate(self):
        import codegen_luasharp as ls
        classes, typenames = self.parse()
        return ls.generate(classes, typenames, "TEST")

    def test_declarations(self):
        declarations = self.generate().declarations
        self.assertIn("enum Mode { Off, On }", declarations)
        self.assertIn("extern class Animal : ClassBase {\n    virtual Wrap wrap(Wrap w);\n    virtual void speak(int times);\n    virtual Animal best(string tag);\n    int legs;\n", declarations)
        self.assertIn("extern class Dog : Animal {\n    static Dog find(string name);\n    Vec3 where(Mode m, Animal other);\n    int[] bones();\n    Hit[] hits(Animal[] pack);\n    Hit probe(Hit h);\n    Animal leader;\n    Hit last;\n    void bark();\n    float speed;\n", declarations)
        self.assertNotIn("void speak", declarations.split("extern class Dog")[1])  # the Animal thunk dispatches the override
        self.assertIn("// skipped grid(): std::vector<std::vector<int>>", declarations)  # nested lists do not cross
        self.assertIn("// skipped bark(): overload", declarations)
        self.assertIn("// skipped in(): name is a LuaSharp keyword", declarations)
        self.assertNotIn("hidden", declarations)
        self.assertIn("    [TypeOf] static ClassTypeInfo find(string classname);\n", declarations)
        self.assertIn("    T pick<T : Animal>(int n);\n    static T make<T : Animal>();\n    [DebugView] int age();\n", declarations)

    def test_thunks(self):
        out = self.generate()
        cpp = out.prelude + out.cpp
        self.assertIn("static const int64_t ls_enum_Mode[] = {static_cast<int64_t>(Mode::Off), static_cast<int64_t>(Mode::On)};", cpp)
        self.assertIn("static void* ls_native_Dog_find(void* name) { return lsbind::proxy_of(Dog::find(std::string(lsbind::view(name)))); }", cpp)
        self.assertIn("lsbind::object<Dog>(self)->where(static_cast<Mode>(lsbind::enum_value(ls_enum_Mode, std::size(ls_enum_Mode), m)), lsbind::object<Animal>(other))", cpp)
        self.assertIn("lsbind::field_offset(Dog::get_props(), \"speed\")", cpp)
        self.assertIn("static void ls_native_Dog__set_speed(void*, double) { lsbind::report_error(\"field 'Dog.speed' is read-only\"); }", cpp)
        self.assertIn("{\"ls_native_Animal__get_legs\", reinterpret_cast<const void*>(&ls_native_Animal__get_legs)}", cpp)
        self.assertIn("\t{\"Dog\", \"Animal\"},\n", cpp)
        self.assertIn("static const lsbind::Registrar ls_registrar{\"TEST\", ls_declarations, ls_symbols, ls_classes, ls_scriptables, ls_structs};", cpp)

    def test_scriptable_slots(self):
        out = self.generate()
        self.assertEqual(list(out.slots), ["Animal"])  # Dog is not scriptable
        self.assertEqual(out.slots["Animal"]["speak"], "if (auto ls_fn = reinterpret_cast<void (*)(void*, int64_t)>(ls_slots[1])) { ls_fn(get_script_proxy(), static_cast<int64_t>(times)); return; }")
        self.assertEqual(out.slots["Animal"]["best"], "if (auto ls_fn = reinterpret_cast<void* (*)(void*, void*)>(ls_slots[2])) return lsbind::object<Animal>(lsbind::native_of(ls_fn(get_script_proxy(), lsbind::string(tag))));")
        self.assertIn("static void ls_native_Animal__base_speak(void* self, int64_t times) { lsbind::object<Animal>(self)->Animal::speak(static_cast<int>(times)); }", out.cpp)
        self.assertIn("static const char* const ls_slot_names_Animal[] = {\n\t\"wrap\",\n\t\"speak\",\n\t\"best\",\n};", out.cpp)
        self.assertIn("\t{\"Animal\", ls_slot_names_Animal, &lsbind::attach<ScriptImpl_Animal>},\n", out.cpp)
        self.assertIn("{\"ls_native_Animal__base_best\", reinterpret_cast<const void*>(&ls_native_Animal__base_best)}", out.cpp)

    def test_scriptable_class_calls_slots(self):
        import codegen_generate as gen
        out = self.generate()
        classes, _ = self.parse()
        animal = next(c for c in classes if c.classname == "Animal")
        impl = gen.write_scriptable_class(animal, out.slots["Animal"])
        self.assertIn("void* const* ls_slots = nullptr;", impl)
        self.assertIn("if (ls_slots) { " + out.slots["Animal"]["speak"] + " return Animal::speak(times); }", impl)
        self.assertNotIn("ls_slots", gen.write_scriptable_class(animal))  # the Lua-only class is unchanged

    def test_struct_declarations(self):
        declarations = self.generate().declarations
        self.assertIn("extern struct Hit {\n    Animal who; // = nullptr\n    Animal owner;\n    Mode mode; // = Mode::Off\n    float dist; // = 0\n    int[] tags;\n}", declarations)
        self.assertIn("extern struct Wrap {\n    Hit inner;\n    int n; // = 3\n}", declarations)  # the C++ initializer as a comment
        self.assertIn("    Hit probe(Hit h);\n    Animal leader;\n    Hit last;\n", declarations)
        self.assertIn("    virtual Wrap wrap(Wrap w);\n", declarations)

    def test_struct_marshaling(self):
        out = self.generate()
        cpp = out.prelude + out.cpp
        self.assertIn("struct ls_S_Hit { void* who; void* owner; int64_t mode; double dist; void* tags; };", cpp)
        self.assertIn("struct ls_S_Wrap { ls_S_Hit inner; int64_t n; };", cpp)
        self.assertLess(cpp.index("struct ls_S_Hit"), cpp.index("struct ls_S_Wrap"))  # contained structs first
        self.assertIn("r.who = lsbind::proxy_of(v.who); r.owner = lsbind::proxy_of(v.owner.get()); r.mode = lsbind::enum_ordinal(ls_enum_Mode, std::size(ls_enum_Mode), static_cast<int64_t>(v.mode));", cpp)
        self.assertIn("out.who = lsbind::object<Animal>(lsbind::native_of(v.who)); out.owner = obj<Animal>(lsbind::object<Animal>(lsbind::native_of(v.owner)));", cpp)
        self.assertIn("[[maybe_unused]] Hit ls_to_cpp_Hit(const ls_S_Hit& v) { Hit r{}; ls_assign_Hit(v, r); return r; }", cpp)
        self.assertIn("static ls_S_Hit ls_native_Dog_probe(void* self, ls_S_Hit h) { return ls_to_ls_Hit(lsbind::object<Dog>(self)->probe(ls_to_cpp_Hit(h))); }", cpp)
        self.assertIn("{ ls_assign_Hit(value, lsbind::field<Hit>(lsbind::object<Dog>(self), ls_offset_Dog_last())); }", cpp)  # in place
        self.assertIn("static void* ls_native_Dog__get_leader(void* self) { return lsbind::proxy_of(lsbind::field<obj<Animal>>(lsbind::object<Dog>(self), ls_offset_Dog_leader()).get()); }", cpp)
        self.assertIn("= obj<Animal>(lsbind::object<Animal>(value)); }", cpp)
        self.assertIn("\t{\"Hit\", sizeof(ls_S_Hit), ls_layout_Hit},\n", cpp)
        self.assertIn("\t{\"dist\", offsetof(ls_S_Hit, dist)},\n", cpp)

    def test_lists(self):
        out = self.generate()
        cpp = out.prelude + out.cpp
        self.assertIn("return lsbind::to_list<int64_t>(lsbind::object<Dog>(self)->bones(), lsbind::ElementKind::Int, nullptr, [](const auto& item) { return static_cast<int64_t>(item); });", cpp)
        self.assertIn("static void* ls_native_Dog_hits(void* self, void* pack) { return lsbind::to_list<ls_S_Hit>(lsbind::object<Dog>(self)->hits("
                      "lsbind::from_list<void*>(pack, [](const auto& item) { return lsbind::object<Animal>(lsbind::native_of(item)); })), "
                      "lsbind::ElementKind::Struct, \"Hit\", [](const auto& item) { return ls_to_ls_Hit(item); }); }", cpp)
        self.assertIn("r.tags = lsbind::to_list<int64_t>(v.tags, lsbind::ElementKind::Int, nullptr,", cpp)  # a struct field
        self.assertIn("out.tags = lsbind::from_list<int64_t>(v.tags, [](const auto& item) { return static_cast<int>(item); });", cpp)
        self.assertIn("static ls_S_Hit ls_native_Hit_new() { return ls_to_ls_Hit(Hit{}); }", cpp)  # T() is the C++ default
        self.assertIn("{\"ls_native_Hit_new\", reinterpret_cast<const void*>(&ls_native_Hit_new)}", cpp)
        self.assertIn("ls_classes, ls_scriptables, ls_structs};", cpp)

    def test_struct_conversions_come_before_classes(self):
        out = self.generate()
        self.assertIn("struct ls_S_Wrap", out.prelude)  # ScriptImpl_Animal::wrap calls ls_to_cpp_Wrap
        self.assertIn("static const int64_t ls_enum_Mode[]", out.prelude)
        self.assertNotIn("ls_S_Wrap {", out.cpp)

    def test_struct_slot(self):
        out = self.generate()
        self.assertEqual(out.slots["Animal"]["wrap"], "if (auto ls_fn = reinterpret_cast<ls_S_Wrap (*)(void*, ls_S_Wrap)>(ls_slots[0])) return ls_to_cpp_Wrap(ls_fn(get_script_proxy(), ls_to_ls_Wrap(w)));")

    def test_empty_pass(self):
        import codegen_luasharp as ls
        self.assertEqual(ls.generate([], {}, "EMPTY").cpp, "")


if __name__ == "__main__":
    unittest.main()