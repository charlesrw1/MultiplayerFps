
import unittest

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
        self.dict[CLASSBASE] = cg.ClassDef([CLASSBASE],cg.ClassDef.TYPE_CLASS)
        self.CLASS_BASE = self.dict[CLASSBASE]

    def add_class(self,name:str,inherits:str) -> cg.ClassDef:
        c = cg.ClassDef([name,inherits],cg.ClassDef.TYPE_CLASS)
        self.dict[name] = c
        return c
    def add_struct(self,name:str) -> cg.ClassDef:
        c = cg.ClassDef([name],cg.ClassDef.TYPE_STRUCT)
        self.dict[name] = c
        return c
    def add_enum(self,name:str) -> cg.ClassDef:
        c = cg.ClassDef([name],cg.ClassDef.TYPE_ENUM)
        self.dict[name] = c
        return c
    def finish(self):
        cg.ClassDef.fixup_types(self.dict)





class CodegenUnitTest(unittest.TestCase):
    # test macro parsing

    def make_testing_dict(self) -> dict[str,cg.ClassDef]:
        BASE_CLASS = cg.ClassDef(["Base"],cg.ClassDef.TYPE_CLASS)
        PLAYER_CLASS = cg.ClassDef(["Player","Base"],cg.ClassDef.TYPE_CLASS)
        MODEL_CLASS = cg.ClassDef(["Model","Base"],cg.ClassDef.TYPE_CLASS)

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
        os.chdir(this_dir+SOURCE_DIR)
        cg.do_codegen('.', DIRS_TO_SKIP,True)


    def test_parse_reflect_macro(self):
        out = cg.parse_reflect_macro(" REFLECT( \"type\"=xyz )")
        self.assertTrue(out.custom_type=="xyz")
        out = cg.parse_reflect_macro("REFLECT(hide)")
        self.assertTrue(out.hide)
    def test_type_parse(self):
        out = cg.parse_type("std::vector<int>",{})
        self.assertTrue(out.type==cg.ARRAY_TYPE)
        self.assertTrue(len(out.template_args)==1 and out.template_args[0].type == cg.INT_TYPE)
        out = cg.parse_type("vector<int>",{})
        self.assertTrue(len(out.template_args)==1 and out.template_args[0].type == cg.INT_TYPE)


        BASE_CLASS = cg.ClassDef(["Base"],cg.ClassDef.TYPE_CLASS)
        PLAYER_CLASS = cg.ClassDef(["Player","Base"],cg.ClassDef.TYPE_CLASS)
        MODEL_CLASS = cg.ClassDef(["Model","Base"],cg.ClassDef.TYPE_CLASS)

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
        BASE_CLASS = cg.ClassDef(["Base"],cg.ClassDef.TYPE_CLASS)
        PLAYER_CLASS = cg.ClassDef(["Player","Base"],cg.ClassDef.TYPE_CLASS)
        MODEL_CLASS = cg.ClassDef(["Model","Base"],cg.ClassDef.TYPE_CLASS)

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
        cg.do_codegen('.', DIRS_TO_SKIP,True)


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

if __name__ == "__main__":
    unittest.main()