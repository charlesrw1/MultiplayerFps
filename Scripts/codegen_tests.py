import unittest
import codegen_lib


# test parsing (failures/bad parses)
# test output reflection
# test function codegen

# test parsing of REFLECT(...)
# test parsing of a type

class CodegenUnitTest(unittest.TestCase):
    # test macro parsing
    def test_parse_reflect_macro(self):
        out = codegen_lib.parse_reflect_macro(" REFLECT( \"type\"=xyz )")
        self.assertTrue(out.custom_type=="xyz")
        out = codegen_lib.parse_reflect_macro("REFLECT(hide)")
        self.assertTrue(out.hide)
    def test_type_parse(self):
        out = codegen_lib.parse_type("std::vector<int>",{})
        self.assertTrue(out.type==codegen_lib.LIST_TYPE)
        self.assertTrue(len(out.template_args)==1 and out.template_args[0].type == codegen_lib.INT_TYPE)

        out = codegen_lib.parse_type("const Player* p",{"Player":codegen_lib.STRUCT_TYPE})
        self.assertTrue(out.type==codegen_lib.STRUCT_TYPE)
        self.assertTrue(out.typename=="Player")

        out = codegen_lib.parse_type("const AssetPtr<const Model> my_model = ",{"Model":codegen_lib.STRUCT_TYPE})
        self.assertTrue(out.type==codegen_lib.ASSETPTR_TYPE)
        self.assertTrue(len(out.template_args)==1 and out.template_args[0].typename == "Model")

        out = codegen_lib.parse_type("MulticastDelegate<int,bool> my_model = ",{"Model":codegen_lib.STRUCT_TYPE})
        self.assertTrue(out.type==codegen_lib.MULTICAST_TYPE)
        self.assertTrue(len(out.template_args)==2 and out.template_args[0].type==codegen_lib.INT_TYPE)

    def test_parse_class_decl(self):
        out = codegen_lib.parse_class_decl("class myclass")
        self.assertTrue(out[0]=="myclass")
        out = codegen_lib.parse_class_decl("class myclass{")
        self.assertTrue(out[0]=="myclass")
        out = codegen_lib.parse_class_decl("class myclass: public someother, public something2{")
        self.assertTrue(out[0]=="myclass" and out[1]=="someother" and out[2]=="something2")
        out = codegen_lib.parse_class_decl("class myclass: someother")
        self.assertTrue(out[0]=="myclass"and out[1]=="someother")

    def test_parse_property(self):
        typedict = {"Model":codegen_lib.STRUCT_OBJECT}
        emptyenum = enumerate(iter(""))

        name,args = codegen_lib.parse_function("void myfunction(int x, int y)",{})
        self.assertTrue(name=="myfunction" and len(args)==2)
        prop = codegen_lib.parse_property("REF static int func()",emptyenum,{})
        self.assertTrue(prop.name == "func" and prop.prop_type == codegen_lib.FUNCTION_TYPE and prop.is_static)
        prop = codegen_lib.parse_property("REF AssetPtr<Model> mymodel;", emptyenum, typedict)
        self.assertTrue(prop.name == "mymodel" and prop.prop_type == codegen_lib.ASSETPTR_TYPE)
        prop = codegen_lib.parse_property("REF static int is_static;", emptyenum, typedict)
        self.assertTrue(prop.name == "is_static" and prop.is_static)

    

    def test_file_output(self):
        FILE = "../TestFiles/testfile.h"
        typedict : dict[str,int] ={}
        typedict["ClassTypeInfo"] = codegen_lib.STRUCT_OBJECT
        typedict["Component"] = codegen_lib.STRUCT_OBJECT
        typedict["Entity"] = codegen_lib.STRUCT_OBJECT


        classes,_ = codegen_lib.parse_file(FILE,typedict)
        self.assertTrue(len(classes)==1)
        c = classes[0]
        self.assertTrue(c.classname=="Entity")
        self.assertTrue(c.supername=="BaseUpdater")
        self.assertTrue(c.object_type == codegen_lib.CLASS_OBJECT)
    def test_remove_comment(self):
        removed = codegen_lib.remove_comment_from_end("REF bool simulate_physics = false;\t\t// if true, then object is a DYNAMIC object driven by the physics simulation")
        self.assertTrue(removed=="REF bool simulate_physics = false;\t\t")
    def test_parse_class_line(self):
        out = codegen_lib.parse_class_from_start("class ForwardDeclared;// some comment lol",enumerate(""))
        self.assertTrue(out==None)


if __name__ == "__main__":
    unittest.main()