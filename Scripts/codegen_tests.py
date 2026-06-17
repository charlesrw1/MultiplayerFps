
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


if __name__ == "__main__":
    unittest.main()