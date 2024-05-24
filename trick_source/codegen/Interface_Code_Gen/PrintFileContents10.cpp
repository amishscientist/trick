#include <algorithm>
#include <sstream>

#include "PrintFileContents10.hh"
#include "FieldDescription.hh"
#include "ClassValues.hh"
#include "EnumValues.hh"
#include "Utilities.hh"

extern llvm::cl::opt< bool > global_compat15 ;

PrintFileContents10::PrintFileContents10() {}

std::string PrintFileContents10::createIOSrcFileName(std::string base_file_name) {
    return std::string("io_") + base_file_name + std::string(".cpp") ;
}

/** Prints the io_src header information */
void PrintFileContents10::printIOHeader(std::ostream & ostream , std::string header_file_name) {

     if ( ! header_file_name.compare("S_source.hh") ) {
         header_file_name = "../S_source.hh" ;
     } else {
         header_file_name = almostRealPath(header_file_name.c_str()) ;
     }
     ostream << "/**\n"
             << " * This file was automatically generated by the ICG based on the file:\n"
             << " * " << header_file_name << "\n"
             << " * This file contains database parameter declarations specific to the\n"
             << " * data structures and enumerated types declared in the above file.\n"
             << " * These database parameters are used by the Trick input and\n"
             << " * data recording processors to gain access to important simulation\n"
             << " * variable information.\n"
             << " */\n"
             << "\n"
             << "#define TRICK_IN_IOSRC\n"
             << "#include <stdlib.h>\n"
             << "#include \"trick/MemoryManager.hh\"\n"
             << "#include \"trick/attributes.h\"\n"
             << "#include \"trick/parameter_types.h\"\n"
             << "#include \"trick/ClassSizeCheck.hh\"\n"
             << "#include \"trick/UnitsMap.hh\"\n"
             << "#include \"trick/checkpoint_stl.hh\"\n"
             << "#include \"" << header_file_name << "\"\n"
             << "\n" ;
}

/** Prints enumeration attributes */
void PrintFileContents10::print_enum_attr(std::ostream & ostream , EnumValues * e ) {
    print_open_extern_c(ostream) ;
    ostream << "ENUM_ATTR enum" << e->getFullyQualifiedTypeName("__") << "[] = {\n" ;
    std::string name = e->getNamespacesAndContainerClasses();
    for (auto& pair : e->getPairs()) {
        ostream << "{\"" << name << pair.first << "\", " << pair.second << ", 0x0},\n" ;
    }
    ostream << "{\"\", 0, 0x0}\n};\n" ;
    print_close_extern_c(ostream) ;
}

/** Prints attributes for a field */
void PrintFileContents10::print_field_attr(std::ostream & ostream ,  FieldDescription & fdes ) {
    int array_dim ;

    ostream << "{\"" << fdes.getName() << "\""                               // name
            << ", \"" << fdes.getFullyQualifiedMangledTypeName("__") << "\"" // type_name
            << ", \"" << fdes.getUnits() << "\""                             // units
            << ", \"\", \"\"," << std::endl                                   // alias, user_defined
            << "  \"" << fdes.getDescription() << "\"," << std::endl         // description
            << "  " << fdes.getIO()                                          // io
            << "," << fdes.getEnumString() ;                                 // type
    // There are several cases when printing the size of a variable.
    if ( fdes.isBitField() ) {
        // bitfields are handled in 4 byte (32 bit) chunks
        ostream << ", 4" ;
    } else if (  fdes.isRecord() or fdes.isEnum() or fdes.getTypeName().empty() ) {
        // records enums use io_src_get_size. The sentinel has no typename
        ostream << ", 0" ;
    } else {
        // print size of the underlying type
        ostream << ", sizeof(" << fdes.getTypeName() << ")" ;
    }
    ostream << ", 0, 0, Language_CPP" ; // range_min, range_max, language
    // mods (see attributes.h for descriptions)
    ostream << ", " << fdes.isReference() + (fdes.isStatic() << 1) + (fdes.isDashDashUnits() << 2) + (fdes.isReference() ? (((fdes.getFieldWidth() / 8) & 0x3F) << 3) : 0) << "," << std::endl ; // mods
    if ( fdes.isBitField() ) {
        // For bitfields we need the offset to start on 4 byte boundaries because that is what our
        // insert and extract bitfield routines work with.
        ostream << "  " << (fdes.getFieldOffset() - (fdes.getFieldOffset() % 32)) / 8 ; // offset
    } else {
        ostream << "  " << (fdes.getFieldOffset() / 8) ; // offset
    }
    ostream << ", NULL" ; // attr
    ostream << ", " << fdes.getNumDims() ;                // num_index

    ostream << ", {" ;
    if ( fdes.isBitField() ) {
        ostream << "{" << fdes.getBitFieldWidth() ; // size of bitfield
        ostream << ", " << 32 - (fdes.getFieldOffset() % 32) - fdes.getBitFieldWidth() << "}" ; // start bit
    } else {
        array_dim = fdes.getArrayDim(0) ;
        if ( array_dim < 0 ) array_dim = 0 ;
        ostream << "{" << array_dim << ", 0}" ; // index 0
    }
    unsigned int ii ;
    for ( ii = 1 ; ii < 8 ; ii++ ) {
        array_dim = fdes.getArrayDim(ii) ;
        if ( array_dim < 0 ) array_dim = 0 ;
        ostream << ", {" << array_dim << ", 0}" ; // indexes 1 through 7
    }
    ostream << "}," << std::endl ;
    ostream << "  NULL, NULL, NULL, NULL" ;
    ostream << "}" ;
}

/** Prints class attributes */
void PrintFileContents10::print_class_attr(std::ostream & ostream , ClassValues * c ) {

    print_open_extern_c(ostream) ;
    ostream << "ATTRIBUTES attr" << c->getFullyQualifiedMangledTypeName("__") << "[] = {" << std::endl ;

    for (FieldDescription* fieldDescription : getPrintableFields(*c)) {
            print_field_attr(ostream, *fieldDescription) ;
            ostream << "," << std::endl ;
    }
    // Print an empty sentinel attribute at the end of the class.
    FieldDescription new_fdes(std::string("")) ;
    print_field_attr(ostream, new_fdes) ;
    ostream << " };" << std::endl ;

    print_close_extern_c(ostream) ;
}

/** Prints init_attr function for each class */
void PrintFileContents10::print_field_init_attr_stmts( std::ostream & ostream , FieldDescription * fdes ,
 ClassValues * cv , unsigned int index ) {

    const std::string className = cv->getName();
    const std::string mangledClassName = cv->getMangledTypeName();
    const std::string fieldName = fdes->getName();
    const std::string fullyQualifiedClassNameColons = cv->getFullyQualifiedTypeName();
    const std::string fullyQualifiedMangledClassNameColons = cv->getFullyQualifiedMangledTypeName();
    const std::string fullyQualifiedMangledClassNameUnderscores = cv->getFullyQualifiedMangledTypeName("__");
    std::ostringstream oss;
    oss << "    attr" << fullyQualifiedMangledClassNameUnderscores << "[" << index << "].";
    const std::string prefix = oss.str();

    if ( fdes->isStatic() ) {
        ostream << prefix << "offset = (long)(void *)&" << fullyQualifiedClassNameColons << "::" << fieldName << " ;\n" ;
    }
    else if ( global_compat15 or cv->isCompat15() ) {
        if ( fdes->isBitField() ) {
            ostream << prefix << "offset = " << fdes->getBitFieldByteOffset() << " ;\n" ;
            ostream << prefix << "size = sizeof(unsigned int) ;\n" ;
        }
        else if ( fdes->isVirtualInherited() ) {
            ostream << prefix << "offset = " << fdes->getBaseClassOffset() << " + offsetof(" << fdes->getContainerClass() << ", " << fieldName << ") ;\n" ;
        }
        else if ( mangledClassName != className ) {
            ostream << prefix << "offset = " << "offsetof(" << mangledClassName << ", " << fieldName << ") ;\n" ;
        }
        else {
            ostream << prefix << "offset = " << "offsetof(" << fullyQualifiedMangledClassNameColons << ", " << fieldName << ") ;\n" ;
        }
    }

    if ( fdes->isSTL() ) {
        auto print = [&](const std::string& field) {
            ostream << prefix << field << " = " << field << "_" << fullyQualifiedMangledClassNameUnderscores + "_" + sanitize(fieldName) + " ;\n";
        };

        if ( fdes->isCheckpointable() ) {
            print("checkpoint_stl");
            print("post_checkpoint_stl");
        }

        if ( fdes->isRestorable() ) {
            print("restore_stl");
            if ( fdes->hasSTLClear() ) {
                print("clear_stl");
            }
        }
    }

    if ( fdes->isRecord() or fdes->isEnum() ) {
        ostream << "    trick_MM->add_attr_info(std::string(attr" << fullyQualifiedMangledClassNameUnderscores << "[" << index << "].type_name) , &attr" << fullyQualifiedMangledClassNameUnderscores << "[" << index << "], __FILE__ , __LINE__ ) ;\n" ;
    }
}

/** Prints add_attr_info statements for each inherited class */
void PrintFileContents10::print_inherited_add_attr_info( std::ostream & ostream , ClassValues * cv ) {
    auto inheritedClasses = cv->getInheritedClasses();
    if ( inheritedClasses.size() > 0 ) {
        ostream << "\n    ATTRIBUTES temp_attr ;\n\n" ;
    }
    for ( auto& inheritedClass : inheritedClasses ) {
        ostream << "    trick_MM->add_attr_info( std::string(\"" << inheritedClass << "\"), &temp_attr , __FILE__ , __LINE__ ) ;\n" ;
    }
}

/** Prints init_attr function for each class */
void PrintFileContents10::print_init_attr_func( std::ostream & ostream , ClassValues * cv ) {
    cv->printOpenNamespaceBlocks(ostream) ;
    ostream << "\nvoid init_attr" << cv->getFullyQualifiedMangledTypeName("__") << "() {\n\n"
            << "    static int initialized ;\n"
            << "    if (initialized) {\n"
            << "        return;\n"
            << "    }\n"
            << "    initialized = 1;\n\n" ;

    unsigned int ii = 0;
    for ( FieldDescription* field : getPrintableFields(*cv) ) {
        print_field_init_attr_stmts(ostream, field, cv, ii++) ;
    }
    print_inherited_add_attr_info(ostream, cv ) ;
    ostream << "}\n" ;
    cv->printCloseNamespaceBlocks(ostream) ;
}

/** Prints the io_src_sizeof function for enumerations */
void PrintFileContents10::print_enum_io_src_sizeof( std::ostream & ostream , EnumValues * ev ) {
    print_open_extern_c(ostream) ;
    ostream << "size_t io_src_sizeof_" << ev->getFullyQualifiedName("__") << "( void ) {\n" ;
    if ( ev->getHasDefinition() ) {
        ostream << "    return sizeof(" << ev->getFullyQualifiedName() << ")" ;
    } else {
        ostream << "    return sizeof(int)" ;
    }
    ostream << ";\n}\n" ;
    print_close_extern_c(ostream) ;
}

/** Prints the C linkage init_attr function */
void PrintFileContents10::print_init_attr_c_intf( std::ostream & ostream , ClassValues * cv ) {
    ostream << "void init_attr" << cv->getFullyQualifiedMangledTypeName("__") << "_c_intf() {\n    " ;
    cv->printNamespaces( ostream, "::" ) ;
    ostream << "init_attr" << cv->getFullyQualifiedMangledTypeName("__") << "() ;\n"
            << "}\n\n" ;
}

/** Prints the io_src_sizeof function */
void PrintFileContents10::print_io_src_sizeof( std::ostream & ostream , ClassValues * cv ) {
    ostream << "size_t io_src_sizeof_" << cv->getFullyQualifiedMangledTypeName("__") << "() {\n" ;
    ostream << "    return sizeof(" << cv->getFullyQualifiedNameIfEqual() << ") ;\n}\n\n" ;
}

/** Prints the io_src_allocate function */
void PrintFileContents10::print_io_src_allocate( std::ostream & ostream , ClassValues * cv ) {
    if ( cv->isPOD() or ( !cv->isAbstract() and cv->getHasDefaultConstructor()) ) {
        const std::string name = cv->getFullyQualifiedNameIfEqual();
        ostream << "void* io_src_allocate_" << cv->getFullyQualifiedMangledTypeName("__") <<  "(int num) {\n" ;
        ostream << "    " << name << "* temp = (" << name << "*)calloc(num, sizeof(" << name << "));\n" ;
        if ( ! cv->isPOD() ) {
            ostream << "    for (int ii = 0; ii < num; ++ii) {\n" ;
            ostream << "        new(&temp[ii]) " << name << "();\n    }\n" ;
        }
        ostream << "    return (void*)temp;\n" << "}\n\n" ;
    }
}

/** Prints the io_src_allocate function */
void PrintFileContents10::print_io_src_destruct( std::ostream & ostream , ClassValues * cv ) {
    if ( cv->getHasPublicDestructor() ) {
        ostream << "void io_src_destruct_" << cv->getFullyQualifiedMangledTypeName("__") << "(void* address __attribute__((unused)), int num __attribute__((unused))) {\n" ;
        if ( ! cv->isPOD() ) {
            // Add a using statement so we can call the destructor without fully qualifying it.
            auto namespaces = cv->getNamespaces();
            if (namespaces.size()) {
                ostream << "    using namespace " ;
                auto last = namespaces[namespaces.size() - 1];
                for (auto& name : namespaces) {
                    ostream << name;
                    if (name != last) {
                        ostream << "::";
                    }
                }
                ostream << ";\n    " ;
            }

            const std::string name = cv->getName();
            const std::string qualifiedName = cv->getFullyQualifiedNameIfEqual();

            ostream << qualifiedName << "* temp = (" << qualifiedName << "*)address;\n" ;
            if ( cv->getMangledTypeName() == name ) {
                ostream << "    for (int ii = 0; ii < num; ++ii) {\n"
                        << "        temp[ii].~" << name << "();\n"
                        << "    }\n" ;
            }
        }
        ostream << "}\n\n" ;
    }
}

void PrintFileContents10::print_io_src_delete( std::ostream & ostream , ClassValues * cv ) {
    if ( cv->getHasPublicDestructor() ) {
        ostream << "void io_src_delete_" << cv->getFullyQualifiedMangledTypeName("__") << "(void* address" ;
        if ( ! cv->isPOD() ) {
            ostream << ") {\n    delete (" << cv->getFullyQualifiedNameIfEqual() <<  "*)address;\n" ;
        }
        else {
            ostream << " __attribute__((unused))) {" ;
        }
        ostream << "}\n" ;
    }
}

void PrintFileContents10::print_checkpoint_stl(std::ostream & ostream , FieldDescription * fdes , ClassValues * cv ) {
    printStlFunction("checkpoint", "void* start_address, const char* obj_name , const char* var_name", "checkpoint_stl(*stl, obj_name, var_name)", ostream, *fdes, *cv);
}

void PrintFileContents10::print_post_checkpoint_stl(std::ostream & ostream , FieldDescription * fdes , ClassValues * cv ) {
    printStlFunction("post_checkpoint", "void* start_address, const char* obj_name , const char* var_name", "delete_stl(*stl, obj_name, var_name)", ostream, *fdes, *cv);
}

void PrintFileContents10::print_restore_stl(std::ostream & ostream , FieldDescription * fdes , ClassValues * cv ) {
    printStlFunction("restore", "void* start_address, const char* obj_name , const char* var_name", "restore_stl(*stl, obj_name, var_name)",ostream, *fdes, *cv);
}

void PrintFileContents10::print_clear_stl(std::ostream & ostream , FieldDescription * fdes , ClassValues * cv ) {
    printStlFunction("clear", "void* start_address", "stl->clear()",ostream, *fdes, *cv);
}

void PrintFileContents10::print_stl_helper(std::ostream & ostream , ClassValues * cv ) {
    std::vector<FieldDescription*> fieldDescriptions = getPrintableFields(*cv, 0x3 << 2);
    fieldDescriptions.erase(std::remove_if(fieldDescriptions.begin(), fieldDescriptions.end(), [](FieldDescription* field) {return !field->isSTL();}), fieldDescriptions.end());

    if (!fieldDescriptions.size()) {
        return;
    }

    print_open_extern_c(ostream) ;

    for (FieldDescription* field : fieldDescriptions) {
        if (field->isCheckpointable()) {
            print_checkpoint_stl(ostream, field, cv) ;
            print_post_checkpoint_stl(ostream, field, cv) ;
        }
        if (field->isRestorable()) {
            print_restore_stl(ostream, field, cv) ;
            if (field->hasSTLClear()) {
                print_clear_stl(ostream, field, cv) ;
            }
        }
    }

    print_close_extern_c(ostream) ;
}

void PrintFileContents10::printClass( std::ostream & ostream , ClassValues * cv ) {
    print_class_attr(ostream, cv) ;
    print_stl_helper(ostream, cv) ;
    print_init_attr_func(ostream, cv) ;
    ostream << std::endl;
    print_open_extern_c(ostream) ;
    print_init_attr_c_intf(ostream, cv) ;
    print_io_src_sizeof(ostream, cv) ;
    print_io_src_allocate(ostream, cv) ;
    print_io_src_destruct(ostream, cv) ;
    print_io_src_delete(ostream, cv) ;
    print_close_extern_c(ostream) ;
    print_units_map(ostream, cv) ;
}

void PrintFileContents10::printEnum( std::ostream & ostream , EnumValues * ev ) {
    print_enum_attr(ostream, ev) ;
    print_enum_io_src_sizeof(ostream, ev) ;
}

void PrintFileContents10::printClassMapHeader( std::ostream & ostream , std::string function_name ) {
     ostream <<
"/*\n"
" * This file was automatically generated by the ICG\n"
" * This file contains the map from class/struct names to attributes\n"
" */\n\n"
"#include <map>\n"
"#include <string>\n\n"
"#include \"trick/AttributesMap.hh\"\n"
"#include \"trick/EnumAttributesMap.hh\"\n"
"#include \"trick/attributes.h\"\n\n"
"void " << function_name << "() {\n\n"
"    Trick::AttributesMap * class_attribute_map = Trick::AttributesMap::attributes_map();\n\n" ;
}

void PrintFileContents10::printClassMap( std::ostream & ostream , ClassValues * cv ) {
    std::string name = cv->getFullyQualifiedMangledTypeName("__");
    ostream << "    // " << cv->getFileName() << std::endl
            << "    extern ATTRIBUTES  attr" << name << "[] ;" << std::endl
            << "    class_attribute_map->add_attr(\"" << cv->getFullyQualifiedMangledTypeName() << "\" , attr" << name << ") ;" << std::endl ;
}

void PrintFileContents10::printClassMapFooter( std::ostream & ostream ) {
     ostream << "}" << std::endl << std::endl ;
}

void PrintFileContents10::printEnumMapHeader( std::ostream & ostream , std::string function_name ) {
     ostream << "void " << function_name << "() {\n"
             << "    Trick::EnumAttributesMap* enum_attribute_map __attribute__((unused)) = Trick::EnumAttributesMap::attributes_map();\n\n" ;
}

void PrintFileContents10::printEnumMap( std::ostream & ostream , EnumValues * ev ) {
    std::string name = ev->getFullyQualifiedTypeName("__");
    ostream << "    extern ENUM_ATTR enum" << name << "[];" << std::endl
            << "    enum_attribute_map->add_attr(\"" << ev->getFullyQualifiedTypeName() << "\", enum" << name << ");" << std::endl ;
}

void PrintFileContents10::printEnumMapFooter( std::ostream & ostream ) {
     ostream << "}" << std::endl << std::endl ;
}

void PrintFileContents10::printStlFunction(const std::string& name, const std::string& parameters, const std::string& call, std::ostream& ostream, FieldDescription& fieldDescription, ClassValues& classValues) {
    const std::string typeName = fieldDescription.getTypeName();
    const std::string functionName = name + "_stl";
    ostream << "void " << functionName << "_" << classValues.getFullyQualifiedMangledTypeName("__") << "_" << sanitize(fieldDescription.getName())
            << "(" << parameters << ") {" << std::endl
            << "    " << typeName << "* stl = reinterpret_cast<" << typeName << "*>(start_address);" << std::endl
            << "    " << call << ";" << std::endl
            << "}" << std::endl ;
}
