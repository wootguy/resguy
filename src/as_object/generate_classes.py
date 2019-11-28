import json, os, re

enum_types = []
class_types = []
typedef_types = []

def convert_asdocs_to_json(file):

	last_type = None
	last_prop = None
	indent = 0
	with open("temp.json", "w") as j:
		with open(file) as f:
			json_text = ''
			
			# TODO: Auto-detect these
			array_types = ['Interfaces', 'Classes', 'Methods', 'Properties', 'Enums', 'Values', 'Functions', 'Typedefs', 'FuncDefs']
			array_element_types = ['Interface', 'Class', 'Method', 'Property', 'Enum', 'Value', 'Function', 'Typedef', 'FuncDef']
			array_indent = []
			
			for line in f:
				line = line.strip()
				
				if line[0] == '"':
					continue
				if len(line) == 0:
					json_text += '\n'
					continue

				if line[0] == '{':
					if last_type == 'prop':
						json_text = json_text[:-3] # remove value from last property
						if last_prop in array_types:
							line = '['
							array_indent.append(indent)
						if last_prop in array_element_types:
							json_text = json_text[:-(len(last_prop)+3)] # remove key for array element
					if last_type == '}':
						line = ',' + line
					json_text += '\n' + indent*'\t' + line
					last_type = '{'
					indent += 1
					continue

				if line[0] == '}':
					indent -= 1
					if indent in array_indent:
						array_indent.remove(indent)
						line = ']'
					json_text += '\n' + indent*'\t' + line
					last_type = '}'
					continue
				
				if (last_type != '{'):
					json_text += ','
				
				
				parts = line.split()
				key = parts[0]
				value = ' '.join(parts[1:])
				value = value.replace("\\'", "'")
				if len(value) > 0:
					if value[0] == '"':
						value = value[1:]
					if value[-1] == '"':
						value = value[0:-1]
				
				
				json_text += '\n' + indent*'\t' + '"%s": "%s"' % (key, value)
				writing_props = True
				last_type = 'prop'
				last_prop = key
				
			j.write(json_text)
			asdocs_json = json.loads(json_text)
			with open("asdocs.json", "w") as out:
				out.write(json.dumps(asdocs_json))

def add_undocumented_props(asdocs):
	# some things are missing from the doc file but exist in the API
	asdocs["Properties"].append({
		"Documentation": "Undocumented. This was added by a script.",
		"Namespace": "",
		"Declaration": "CModuleHookManager g_Hooks"
	})
	
	return asdocs

def generate_sven_api_classes(asdocs):
	global enum_types
	global class_types
	global typdef_types

	if not os.path.exists("class"):
		os.mkdir("class")
	if not os.path.exists("enum"):
		os.mkdir("enum")
		
	source_files = ['src/as_object/common.h', 'src/as_object/common.cpp', 
		'src/as_object/globals.h', 'src/as_object/globals.cpp',
		'src/as_object/typedefs.h', 'src/as_object/typedefs.cpp']
		
	common_h_file = open("common.h", "w")
	common_cpp_file = open("common.cpp", "w")
	
	heading = "//\n// This file is generated by a script.\n//\n\n"
	common_cpp_file.write(heading)
	
	common_cpp_file.write('\n\n#include "common.h"\n')
	
	enum_types = []
	for enum in asdocs['Enums']:
		if enum['Namespace']:
			enum_types.append(enum['Namespace'] + "::" + enum['Name'])
		else:
			enum_types.append(enum['Name'])
		common_cpp_file.write('\n#include "enum/%s.h"' % enum['Name'])
	
	class_types = []
	for clazz in asdocs['Classes']:
		if clazz['ClassName'] in ['string', 'char', 'File']:
			continue
		if clazz['Namespace']:
			class_types.append(clazz['Namespace'] + "::" + clazz['ClassName'])
		else:
			class_types.append(clazz['ClassName'])
		common_cpp_file.write('\n#include "class/%s.h"' % clazz['ClassName'])
	for clazz in asdocs["Interfaces"]:
		if clazz['Namespace']:
			class_types.append(clazz['Namespace'] + "::" + clazz['InterfaceName'])
		else:
			class_types.append(clazz['InterfaceName'])
		common_cpp_file.write('\n#include "class/%s.h"' % clazz['InterfaceName'])
	
	common_cpp_file.write('\n\nvoid RegisterSvenModules(asIScriptEngine* engine) {')
	common_cpp_file.write('\n\tRegisterTypedefs(engine);')
	
	common_h_file.write("#pragma once\n\n")
	
	common_h_file.write('#include <angelscript.h>\n')
	common_h_file.write('#include <scriptarray/scriptarray.h>\n')
	common_h_file.write('#include <scriptany/scriptany.h>\n')
	common_h_file.write('#include <scriptdictionary/scriptdictionary.h>\n')
	common_h_file.write('#include <string>\n')
	common_h_file.write('#include "globals.h"\n')
	common_h_file.write('\nusing namespace std;\n\n')
	
	print("Generating Enums...")
	for enum in asdocs['Enums']:
		source_files += ['src/as_object/enum/%s.h' % enum['Name'], 'src/as_object/enum/%s.cpp' % enum['Name']]
		if enum['Namespace'] and enum['Namespace'] != enum['Name']:
			common_cpp_file.write('\n\t%s::Register_%s(engine);' % (enum['Namespace'], enum['Name']))
		else:
			common_cpp_file.write('\n\tRegister_%s(engine);' % enum['Name'])
		generate_enum(enum)
	
	# Classes all need to be registered before registering methods/props
	for clazz in asdocs["Classes"] + asdocs["Interfaces"]:
		cname = clazz["ClassName"] if 'ClassName' in clazz else clazz["InterfaceName"]
		
		is_ref_type = True
		if 'Flags' in clazz:
			flags = int(clazz['Flags'])
			is_ref_type = (flags & 1) != 0
			
		if is_ref_type:
			common_cpp_file.write('\n\tengine->RegisterObjectType("%s", 0, asOBJ_REF);' % cname)
		else:
			namespaced_cname = cname
			if clazz['Namespace']:
				namespaced_cname = clazz['Namespace'] + "::" + cname
			common_cpp_file.write('\n\tengine->RegisterObjectType("%s", sizeof(%s), asOBJ_VALUE|asOBJ_APP_CLASS_C);' % (cname, namespaced_cname))
	
	common_cpp_file.write('\n\tRegisterFuncdefs(engine);')
	
	print("Generating Classes...")
	for clazz in asdocs["Interfaces"]:
		clazz["ClassName"] = clazz["InterfaceName"]
		clazz["Flags"] = "1"
		clazz["Properties"] = []
		if clazz['Namespace']:
			common_cpp_file.write('\n\t%s::Register%s(engine);' % (clazz['Namespace'], clazz['ClassName']))
		else:
			common_cpp_file.write('\n\tRegister%s(engine);' % clazz['ClassName'])
		
		source_files += ['src/as_object/class/%s.h' % clazz['ClassName'], 'src/as_object/class/%s.cpp' % clazz['ClassName']]
		generate_class(clazz)
	for idx, clazz in enumerate(asdocs['Classes']):
		if clazz['ClassName'] in ['string', 'char', 'File']:
			continue
			
		#if clazz['ClassName'] not in ['CModule', 'CScriptInfo']:
		#	continue
		#common_h_file.write('#include "class/%s.h"\n' % clazz['ClassName'])
		if clazz['Namespace']:
			common_cpp_file.write('\n\t%s::Register%s(engine);' % (clazz['Namespace'], clazz['ClassName']))
		else:
			common_cpp_file.write('\n\tRegister%s(engine);' % clazz['ClassName'])
		
		source_files += ['src/as_object/class/%s.h' % clazz['ClassName'], 'src/as_object/class/%s.cpp' % clazz['ClassName']]
		#source_files += ['src/as_object/class/%s.h\t\tsrc/as_object/class/%s.cpp' % (clazz['ClassName'], clazz['ClassName'])]
		generate_class(clazz)

	common_cpp_file.write('\n\tRegisterGlobalProperties(engine);')
	common_cpp_file.write('\n\tRegisterGlobalFunctions(engine);')
	common_cpp_file.write('\n}\n')
		
	common_h_file.write('\n\nvoid RegisterSvenModules(asIScriptEngine* engine);\n')
	
	print("Generating globals...")
	generate_globals(asdocs)
	generate_typedefs(asdocs)
		
	generate_cmake_lists(source_files)
		
def generate_cmake_lists(source_files):
	cmake_list_file = open("CMakeLists.txt", 'w')
	
	cmake_list_file.write('set(GENERATED_HEADERS')
	for file in source_files:
		if file.endswith(".h"):
			cmake_list_file.write('\n\t%s' % file)
	cmake_list_file.write('\n\tPARENT_SCOPE\n)\n')
	
	cmake_list_file.write('set(GENERATED_SOURCES')
	for file in source_files:
		if file.endswith(".cpp"):
			cmake_list_file.write('\n\t%s' % file)
	cmake_list_file.write('\n\tPARENT_SCOPE\n)\n')

def generate_typedefs(asdocs):
	typedef_h_file = open("typedefs.h", "w")
	typedef_cpp_file = open("typedefs.cpp", "w")
	
	typedef_h_file.write('#include <stdint.h>\n')
	typedef_cpp_file.write('#include "typedefs.h"')
	typedef_cpp_file.write('\n#include <angelscript.h>')
	typedef_cpp_file.write("\n\nvoid RegisterTypedefs(asIScriptEngine* engine) {")
	
	default_typedefs = [
		('uint64_t', 'uint64'),
		('int64_t', 'int64'),
		('uint32_t', 'uint32'),
		('int32_t', 'int32'),
		('uint16_t', 'uint16'),
		('int16_t', 'int16'),
		('uint32_t', 'uint'),
		('uint8_t', 'uint8'),
		('int8_t', 'int8')
	]
	for typedef in default_typedefs:
		typedef_h_file.write("\ntypedef %s %s;" % (typedef[0], typedef[1]))

	for typedef in asdocs['Typedefs']:
		typedef_cpp_file.write('\n\tengine->RegisterTypedef("%s", "%s");' % (typedef['Name'], typedef['Type']))
		
		if typedef['Name'] in ['time_t', 'size_t']:
			continue
		if typedef["Documentation"]:
			for line in typedef["Documentation"].split('\n'):
				typedef_h_file.write("\n// %s" % line)
		typedef_h_file.write("\ntypedef %s %s;" % (typedef['Type'], typedef['Name']))
		
	typedef_cpp_file.write('\n\tengine->RegisterTypedef("char", "int8");') # TODO: make a CChar class or somethign
		
	typedef_cpp_file.write("\n}\n")
	typedef_cpp_file.write("\n\nvoid RegisterFuncdefs(asIScriptEngine* engine) {")
		
	for funcdef in asdocs['FuncDefs']:
		if funcdef["Documentation"]:
			for line in funcdef["Documentation"].split('\n'):
				typedef_h_file.write("\n// %s" % line)
		func_name = funcdef['Name'].split()[1]
		func_name = func_name[:func_name.find("(")]
		typedef_h_file.write("\ntypedef void (*%s)();" % func_name)
		typedef_cpp_file.write('\n\tengine->RegisterFuncdef("%s");' % (funcdef['Name']))
		
	typedef_cpp_file.write("\n}\n")
		
	typedef_h_file.write("\n\nclass asIScriptEngine;")
	typedef_h_file.write("\nextern void RegisterTypedefs(asIScriptEngine* engine);")
	typedef_h_file.write("\nextern void RegisterFuncdefs(asIScriptEngine* engine);")
	

def generate_globals(asdocs):
	global class_types
	global enum_types

	ignoreFuncs = ['formatFloat', 'formatUInt', 'formatInt']

	# global functions
	globals_h_file = open("globals.h", "w")
	globals_cpp_file = open("globals.cpp", "w")
	
	globals_h_file.write('#pragma once\n')
	globals_h_file.write('\n#include "typedefs.h"')
	globals_h_file.write('\n#include <angelscript.h>')
	globals_h_file.write('\n#include <scriptarray/scriptarray.h>')
	globals_h_file.write('\n#include <scriptany/scriptany.h>')
	globals_h_file.write('\n#include <scriptdictionary/scriptdictionary.h>')
	globals_h_file.write('\n#include <scriptfile/scriptfile.h>')
	globals_h_file.write('\n#include <string>')
	globals_h_file.write('\nusing namespace std;')
	globals_cpp_file.write('\n#include "globals.h"')
	globals_cpp_file.write('\n#include "common.h"')
	
	included_classes = []
	for func in asdocs['Functions'] + asdocs['Properties']:		
		for type in enum_types:
			if type not in included_classes and re.sub(".+::", "", type) in func['Declaration']:
				globals_h_file.write('\n#include "enum/%s.h"' % re.sub(".+::", "", type) )
				included_classes.append(type)
	for func in asdocs['Functions'] + asdocs['Properties']:
		for type in class_types:
			if type not in included_classes and re.sub(".+::", "", type) in func['Declaration']:
				if type.find("::") != -1:
					globals_h_file.write('\nnamespace %s { class %s; }' % (re.sub("::.+", "", type), re.sub(".+::", "", type)))
				else:
					globals_h_file.write('\nclass %s;' % re.sub(".+::", "", type))
				globals_cpp_file.write('\n#include "class/%s.h"' % re.sub(".+::", "", type))
				included_classes.append(type)
	
	globals_h_file.write('\n')
	globals_cpp_file.write('\n')
	
	for prop in asdocs['Properties']:
		decl = prop["Declaration"]
		decl = decl.replace("const ", "") # ignore for now
		
		globals_h_file.write("\n")
		if prop["Documentation"]:
			for line in prop["Documentation"].split('\n'):
				globals_h_file.write("\n// %s" % line)
				
		if prop['Namespace']:
			namespaces = prop['Namespace'].split("::")
			globals_h_file.write("\n")
			globals_cpp_file.write("\n")
			for namespace in namespaces:
				globals_h_file.write("namespace %s { " % namespace)
				globals_cpp_file.write("namespace %s { " % namespace)
			globals_h_file.write("extern %s; %s" % (decl, len(namespaces)*' }'))
			globals_cpp_file.write("%s; %s" % (decl, len(namespaces)*' }'))
		else:
			globals_h_file.write("\nextern %s;" % decl)
			globals_cpp_file.write("\n%s;" % decl)
	
	# Global instances for methods that return references
	globals_h_file.write('\n')
	globals_cpp_file.write('\n')
	for type in ['string', 'CScriptDictValue', 'int8', 'float']:
		globals_h_file.write("\nextern %s g_%s;" % (type, type))
		globals_cpp_file.write('\n%s g_%s;' % (type, type))
	
	for func in asdocs['Functions']:
		decl = format_func_proto(func["Declaration"])
		#print("Generating global %s" % decl)
		
		skipThisFunc = False
		for ignoreFunc in ignoreFuncs: # already defined in the stdstring add on
			if ignoreFunc + "(" in decl:
				skipThisFunc = True
				break
		if skipThisFunc:
			continue
		
		globals_h_file.write("\n")
		if func["Documentation"]:
			for line in func["Documentation"].split('\n'):
				globals_h_file.write("\n// %s" % line)
		
		if func['Namespace']:
			globals_h_file.write("\nnamespace %s { %s; }" % (func['Namespace'], decl))
			globals_cpp_file.write("\n\nnamespace %s {" % func['Namespace'])
			implement_dummy_func(globals_cpp_file, decl)
			globals_cpp_file.write("\n\n}")
		else:
			globals_h_file.write("\n%s;" % decl)
			implement_dummy_func(globals_cpp_file, decl)
			
	# register props
	globals_h_file.write("\nextern void RegisterGlobalProperties(asIScriptEngine* engine);")
	globals_cpp_file.write("\nvoid RegisterGlobalProperties(asIScriptEngine* engine) {")
	for prop in asdocs['Properties']:
		decl = prop["Declaration"]
		decl = decl.replace("const ", "") # ignore for now
		varName = decl.split()[-1]

		namespace = prop['Namespace']
		if namespace:
			namespace = namespace
			globals_cpp_file.write('\n\t{')
			globals_cpp_file.write('\n\t\t engine->SetDefaultNamespace("%s");' % namespace)
			globals_cpp_file.write('\n\t\t engine->RegisterGlobalProperty("%s", &%s);' % (decl, namespace + "::" + varName))
			globals_cpp_file.write('\n\t\t engine->SetDefaultNamespace("");')
			globals_cpp_file.write('\n\t}')
		else:
			globals_cpp_file.write('\n\t engine->RegisterGlobalProperty("%s", &%s);' % (decl, varName))
	globals_cpp_file.write("\n}\n")
	
	# register funcs
	globals_h_file.write("\nextern void RegisterGlobalFunctions(asIScriptEngine* engine);")
	globals_cpp_file.write("\nvoid RegisterGlobalFunctions(asIScriptEngine* engine) {")
	for func in asdocs['Functions']:
		decl_escaped = func["Declaration"].replace("\"", "\\\"")
		decl = func["Declaration"]
		func_name = decl[:decl.find("(")].split()[-1]
		
		skipThisFunc = False
		for ignoreFunc in ignoreFuncs: # already defined in the stdstring add on
			if ignoreFunc + "(" in decl:
				skipThisFunc = True
				break
		if skipThisFunc:
			continue
		
		decl = format_func_proto(decl)
		arg_types = parse_arg_types(decl)
		
		namespace = func['Namespace']
		if namespace:
			namespace = namespace + "::"
			
		ret_type = parse_return_type(decl)
		globals_cpp_file.write('\n\t engine->RegisterGlobalFunction("%s", asFUNCTIONPR(%s, %s, %s), asCALL_CDECL);' % (decl_escaped, namespace + func_name, arg_types, ret_type))
	globals_cpp_file.write("\n}\n")
	

def generate_enum(enum):
	ename = enum['Name']
	namespace = enum['Namespace']
	if namespace == ename:
		namespace = '' # ignore it since it doesn't seem to be used in the api anyway

	#print("Generating enum %s" % ename)
	
	h_file = open("enum/" + ename + ".h", "w")
	cpp_file = open("enum/" + ename + ".cpp", "w")
	
	heading = "//\n// This file is generated by a script.\n//\n\n"
	h_file.write(heading)
	cpp_file.write(heading)
	
	h_file.write('\n#pragma once\n')
	h_file.write("\nclass asIScriptEngine;\n")
	
	cpp_file.write('#include "%s.h"' % ename)
	cpp_file.write('\n#include <angelscript.h>')
	
	if namespace:
		h_file.write('\nnamespace %s {\n' % namespace)
		cpp_file.write('\n\nnamespace %s {' % namespace)
	
	h_file.write('\nenum %s {' % ename)
	
	cpp_file.write('\n\nvoid Register_%s(asIScriptEngine* engine) {' % ename)
	cpp_file.write('\n\tengine->RegisterEnum("%s");' % ename)
	
	for idx, value in enumerate(enum['Values']):
		h_file.write("\n\t%s = %s" % (value['Name'], value['Value']))
		if idx < len(enum['Values'])-1:
			h_file.write(',')
		if value['Documentation']:
			h_file.write("\t\t// %s" % (value['Documentation'].replace("\n", '')))
			
		cpp_file.write('\n\tengine->RegisterEnumValue("%s", "%s", %s);' % (ename, value['Name'], value['Name']))
		
	h_file.write("\n};\n")
	cpp_file.write("\n}\n")
	
	h_file.write("\nextern void Register_%s(asIScriptEngine* engine);" % ename)	
	
	if namespace:
		h_file.write('\n}\n')
		cpp_file.write('\n}\n')

def format_func_proto(decl):
	global enum_types
	
	decl = decl.replace("@+", "*") \
				.replace("@", "*") \
				.replace(" in ", " ") \
				.replace(" out ", " ") \
				.replace("&in ", "& ") \
				.replace("&out ", "& ") \
				.replace(" &", "&") \
				.replace(" null", " NULL") \
				.replace("any ", "CScriptAny ") \
				.replace("any*", "CScriptAny*") \
				.replace("any&", "CScriptAny&") \
				.replace("File ", "CScriptFile ") \
				.replace("File*", "CScriptFile*") \
				.replace("File&", "CScriptFile&") \
				.replace("\n", "\\n") \
				.replace("'", "\"") \
				.replace("dictionaryValue ", "CScriptDictValue ") \
				.replace("dictionaryValue*", "CScriptDictValue*") \
				.replace("dictionaryValue&", "CScriptDictValue&") \
				.replace("dictionary ", "CScriptDictionary ") \
				.replace("dictionary*", "CScriptDictionary*") \
				.replace("dictionary&", "CScriptDictionary&")
				
	argNum = 0
	while True:
		oldDecl = decl
		decl = decl.replace("in,", "in%i," % argNum, 1)
		decl = decl.replace("out,", "out%i," % argNum, 1)
		argNum += 1
		if oldDecl == decl:
			break
	decl = decl.replace("in)", "in%i)" % argNum, 1)
	decl = decl.replace("out)", "out%i)" % argNum, 1)
	
	argNum = 0
	for type in ['long', 'short', 'int', 'ulong', 'ushort', 'uint', 'int16', 'uint16', 'int32', 'uint32', 'uint64', 'int64', 'int8', 'uint8', 'size_t']:
		# replace args with no names
		while True:
			oldDecl = decl
			decl = decl.replace("(" + type + ",", "(" + type + " arg%d," % argNum, 1)
			if oldDecl != decl:
				argNum += 1
				continue
			decl = decl.replace(" " + type + ",", " " + type + " arg%d," % argNum, 1)
			if oldDecl != decl:
				argNum += 1
				continue
			break
			argNum += 1
		decl = decl.replace("," + type + ")", ", " + type + " arg%d)" % argNum)
		decl = decl.replace(", " + type + ")", ", " + type + " arg%d)" % argNum)
		
		# Replace args that have types as names (e.g. "func(int32 long)")
		decl = decl.replace(" " + type + ",", " " + type + "Arg,")
		decl = decl.replace(" " + type + ")", " " + type + "Arg)")

	decl = decl.replace("?& ", "void* ")

	decl = re.sub("array<.+>", "CScriptArray", decl)
	
	'''
	for type in enum_types:
		if type.find("::") != -1 and type not in decl:
			decl = re.sub(re.sub(".+::", "", type) + "\W", type + "::\1", decl)
	'''
	
	return decl

def parse_return_type(decl):
	ret_type = decl.split()[0]
	if ret_type == "const":
		ret_type = ret_type + " " + decl.split()[1]
		
	return ret_type

def parse_arg_types(decl, add_namespace=True):
	is_const_func = decl.endswith("const")
	decl = decl.replace(") const", "")
	arg_part = decl[decl.find("(")+1:]
	args = ""
	if len(arg_part) > 1:
		args = arg_part.split(",")

	arg_types = '(void)'
	if args:	
		arg_types = "("
		for arg in args:
			arg_type = arg.split()[0]
			full_arg_type = arg_type
			if "const" in arg_type:
				arg_type = arg.split()[1]
				full_arg_type = "const " + arg.split()[1]
			if add_namespace:
				for namespaced_type in enum_types + class_types:
					no_namespace_type = re.sub(".+::", "", namespaced_type)
					raw_arg_type = re.sub('[^A-Za-z0-9_]+', '', arg_type)
					if raw_arg_type == no_namespace_type:
						full_arg_type = full_arg_type.replace(no_namespace_type, namespaced_type)
						break
				
			arg_types += full_arg_type + ", "
		arg_types = arg_types[:-2] + ")"
	
	if arg_types.endswith("))"):
		arg_types = arg_types[:-1]
	
	arg_types += ' const' if is_const_func else ''
	
	return arg_types

def remove_default_args_from_func_proto(proto):
	
	# Remove nested parentheses (constructing an object as a default arg)
	while True:
		paren_level = 0
		nest_paren_open = 0
		nest_paren_close = 0
		nest_paren_found = False
	
		for idx, c in enumerate(proto):
			if c == '(':
				paren_level += 1
				nest_paren_open = idx
			if c == ')' and paren_level > 1:
				nest_paren_found = True
				nest_paren_close = idx
				break
		if nest_paren_found:
			proto = proto[0:nest_paren_open] + proto[nest_paren_close+1:]
		else:
			break
	
	return re.sub("\s*=[\w\s\"\.f\-:\\\\]+", "", proto)

def implement_dummy_func(cpp_file, decl, cname=""):
	global enum_types
	global class_types
	
	no_namespace_class_types = [re.sub(".+::", "", type) for type in class_types]
	
	return_end = 1 if decl.split()[0] in ['const'] else 0
	return_type = ' '.join(decl.split()[:return_end+1])
	func_proto = ' '.join(decl.split()[return_end+1:])
	
	func_proto = remove_default_args_from_func_proto(func_proto)
	
	if cname:
		cpp_file.write("\n\n%s %s::%s {" % (return_type, cname, func_proto))
	else:
		cpp_file.write("\n\n%s %s {" % (return_type, func_proto))
		
	return_class = return_type.split()[-1] # remove "const" if its there
	if return_class != 'void':
		if '*' in return_class:
			#cpp_file.write('\n\treturn new %s();' % (return_class.replace("*", "")));
			cpp_file.write('\n\treturn NULL;');
		elif '&' in return_class:
			cpp_file.write('\n\treturn g_%s;' % (return_class.replace("&", "")));
		elif return_class == 'string':
			cpp_file.write('\n\treturn "";');
		elif return_class == 'bool':
			cpp_file.write('\n\treturn false;');
		elif return_class in ['int', 'float']:
			cpp_file.write('\n\treturn 0;');
		elif return_class in enum_types:
			cpp_file.write('\n\treturn (%s)0;' % return_class);
		elif return_class in class_types or return_class in no_namespace_class_types:
			cpp_file.write('\n\treturn g_%s;' % return_class);
		else:
			cpp_file.write('\n\treturn (%s)0;' % return_class);
		cpp_file.write("\n}")
	else:
		cpp_file.write("}")
	

def generate_class(clazz):
	global class_types
	
	cname = clazz['ClassName']
	desc = clazz['Documentation']
	namespace = clazz['Namespace']
	flags = int(clazz['Flags'])
	is_ref_type = (flags & 1) != 0
	
	ignoreMethods = ['opImplCast', 'opImplConv', 'opCast'] # TODO: these need to be handled specially
	
	#print("Generating class %s (%s)" % (cname, "REF" if is_ref_type else "VAL"))
	
	h_file = open("class/" + cname + ".h", "w")
	cpp_file = open("class/" + cname + ".cpp", "w")
	
	heading = "//\n// This file is generated by a script.\n//\n\n"
	h_file.write(heading)
	cpp_file.write(heading)
	
	h_file.write('\n#pragma once\n')
	
	included_classes = []
	declared_classes = []
	# can't avoid including class for value types
	for prop in clazz['Properties']:
		for type in class_types:
			if type not in included_classes and (re.sub(".+::", "", type) + " ") in prop['Declaration']:
				h_file.write('\n#include "%s.h"' % re.sub(".+::", "", type))
				included_classes.append(type)
				declared_classes.append(type)
				
	for prop in clazz['Properties'] + clazz['Methods']:
		for type in enum_types:
			if type not in included_classes and re.sub(".+::", "", type) in prop['Declaration']:
				h_file.write('\n#include "../enum/%s.h"' % re.sub(".+::", "", type))
				included_classes.append(type)
				declared_classes.append(type)

	h_file.write('\n#include "../common.h"\n')
	cpp_file.write('\n#include "%s.h"\n' % cname)
	
	if namespace:
		h_file.write('\nnamespace %s {\n' % namespace)
		
	# declare classes that don't need to be included
	for prop in clazz['Properties']:
		for type in class_types:
			if type not in declared_classes and type != cname and re.sub(".+::", "", type) in prop['Declaration']:
				h_file.write('\nclass %s;' % re.sub(".+::", "", type))
				declared_classes.append(type)
		for type in enum_types:
			if type not in declared_classes and type != cname and type in prop['Declaration']:
				h_file.write('\nenum %s;' % type)
				declared_classes.append(type)
	for prop in clazz['Methods']:
		for type in class_types:
			if type != cname and type not in declared_classes and re.sub(".+::", "", type) in prop['Declaration']:
				h_file.write('\nclass %s;' % re.sub(".+::", "", type))
				cpp_file.write('\n#include "%s.h"' % re.sub(".+::", "", type))
				declared_classes.append(type)
		for type in enum_types:
			if type != cname and type not in declared_classes and type in prop['Declaration']:
				h_file.write('\nenum %s;' % type)
				cpp_file.write('\n#include "../enum/%s.h"' % type)
				declared_classes.append(type)
				
	if namespace:
		cpp_file.write('\nnamespace %s {\n' % namespace)
	
	if clazz["Documentation"]:
		for line in clazz["Documentation"].split('\n'):
			h_file.write("\n// %s" % line)
	h_file.write("\nclass " + cname + "\n{\npublic:")
	
	
	if is_ref_type:
		h_file.write("\n\tint refCount;")
	
	for prop in clazz['Properties']:
		decl = format_func_proto(prop['Declaration'])
		decl = decl.replace("const ", "") # ignore for now
		
		if cname == "CMath":
			decl += "_"; # prevent name conflicts with C++ constants
		
		h_file.write("\n\t%s;" % decl)
		if prop["Documentation"]:
			h_file.write("\t// %s" % prop["Documentation"].replace("\n", " "))

	h_file.write("\n")
	
	# default constructor
	h_file.write("\n\t%s();" % cname)
	if is_ref_type:
		cpp_file.write("\n\n%s::%s() {" % (cname, cname))
		cpp_file.write("\n\trefCount = 1;")
		cpp_file.write("\n}")
	else:
		cpp_file.write("\n\n%s::%s() {}" % (cname, cname))
		
	if is_ref_type:
		# ref counter functions
		h_file.write("\n\tvoid AddRef();")
		cpp_file.write("\n\nvoid %s::AddRef() {" % cname)
		cpp_file.write("\n\trefCount++;")
		cpp_file.write("\n}")
		
		h_file.write("\n\tvoid Release();")
		cpp_file.write("\n\nvoid %s::Release() {" % cname)
		cpp_file.write("\n\tif (--refCount == 0)\n\t\tdelete this;")
		cpp_file.write("\n}")
	
	for method in clazz['Methods']:
		decl = format_func_proto(method['Declaration'])
		if namespace:
			decl = decl.replace(namespace + "::", "") # namespace not needed in default args but docs add it anyway
		
		skipThisMethod = False
		for ignoreMethod in ignoreMethods:
			if ignoreMethod in decl:
				skipThisMethod = True
				break
		if skipThisMethod:
			continue

		is_constructor = decl.find(" " + cname +"(") != -1
		if is_constructor:
			decl = decl[decl.find(" " + cname +"(")+1:]
			if decl.find("()") != -1:
				continue # skip default constructors. Those are always generated above
		
		h_file.write("\n")
		if method["Documentation"]:
			for line in method["Documentation"].split('\n'):
				h_file.write("\n\t// %s" % line)
		h_file.write("\n\t%s;" % decl)
		
		if is_constructor:
			if is_ref_type:
				cpp_file.write("\n\n%s::%s {" % (cname, remove_default_args_from_func_proto(decl)))
				cpp_file.write("\n\trefCount = 1;")
				cpp_file.write("\n}")
			else:
				cpp_file.write("\n\n%s::%s {}" % (cname, remove_default_args_from_func_proto(decl)))
		else:
			implement_dummy_func(cpp_file, decl, cname)
	
	h_file.write("\n};")
	
	if is_ref_type:
		# generate factory function
		h_file.write("\n\n%s* %s_Factory();" % (cname, cname))
		cpp_file.write("\n\n%s* %s_Factory() {" % (cname, cname))
		cpp_file.write("\n\treturn new %s();" % cname)
		cpp_file.write("\n}")
	else:
		# generate constructor/destructor
		h_file.write("\n\nvoid %s_Constructor(void *memory);" % cname)
		h_file.write("\n\nvoid %s_Destructor(void *memory);" % cname)
		cpp_file.write("\n\nvoid %s_Constructor(void *memory) {}" % cname)
		cpp_file.write("\n\nvoid %s_Destructor(void *memory) {}" % cname)
	
	# generate angelscript register function
	h_file.write("\n\nvoid Register%s(asIScriptEngine* engine);\n" % cname)
	cpp_file.write("\n\nvoid Register%s(asIScriptEngine* engine) {" % cname)
	if is_ref_type:
		#cpp_file.write('\n\tengine->RegisterObjectType("%s", 0, asOBJ_REF);' % cname)
		cpp_file.write('\n\tengine->RegisterObjectBehaviour("%s", asBEHAVE_FACTORY, "%s@ f()", asFUNCTION(%s_Factory), asCALL_CDECL);' % (cname, cname, cname))
		cpp_file.write('\n\tengine->RegisterObjectBehaviour("%s", asBEHAVE_ADDREF, "void f()", asMETHOD(%s, AddRef), asCALL_THISCALL);' % (cname, cname))
		cpp_file.write('\n\tengine->RegisterObjectBehaviour("%s", asBEHAVE_RELEASE, "void f()", asMETHOD(%s, Release), asCALL_THISCALL);' % (cname, cname))
	else:
		#cpp_file.write('\n\tengine->RegisterObjectType("%s", sizeof(%s), asOBJ_VALUE|asOBJ_APP_CLASS_C);' % (cname, cname))
		cpp_file.write('\n\tengine->RegisterObjectBehaviour("%s", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(%s_Constructor), asCALL_CDECL_OBJLAST);' % (cname, cname))
		cpp_file.write('\n\tengine->RegisterObjectBehaviour("%s", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(%s_Destructor), asCALL_CDECL_OBJLAST);' % (cname, cname))

	for prop in clazz['Properties']:
		decl = format_func_proto(prop['Declaration'])
		decl = decl.replace("const ", "") # ignore for now
		prop_name = decl.split()[-1]
		if cname == "CMath":
			prop_name += "_"; # prevent name conflicts with C++ constants
		cpp_file.write('\n\tengine->RegisterObjectProperty("%s", "%s", asOFFSET(%s, %s));' % (cname, prop['Declaration'], cname, prop_name))
	
	for method in clazz['Methods']:	
		decl_escaped = method['Declaration'].replace("\"", "\\\"")
		
		decl_escaped = decl_escaped.replace("ConCommandKind::", "") # TODO: no idea how to properly register methods that return this
		
		decl = format_func_proto(method['Declaration'])
		is_constructor = decl.find(" " + cname +"(") != -1
		
		skipThisMethod = False
		for ignoreMethod in ignoreMethods:
			if ignoreMethod in decl:
				skipThisMethod = True
				break
		if skipThisMethod:
			continue
		
		func_name = decl[:decl.find("(")].split()[-1]
		arg_types = parse_arg_types(decl, False)
		ret_type = parse_return_type(decl)
		if is_constructor:
			decl_escaped = decl_escaped.replace("NetworkMessages::", "") # TODO: do this right
			
			if is_ref_type:
				cpp_file.write('\n\tengine->RegisterObjectBehaviour("%s", asBEHAVE_FACTORY, "%s", asFUNCTION(%s_Factory), asCALL_CDECL);' % (cname, decl_escaped, cname))
			else:
				cpp_file.write('\n\tengine->RegisterObjectBehaviour("%s", asBEHAVE_CONSTRUCT, "%s", asFUNCTION(%s_Constructor), asCALL_CDECL_OBJLAST);' % (cname, decl_escaped, cname))
		else:
			cpp_file.write('\n\tengine->RegisterObjectMethod("%s", "%s", asMETHODPR(%s, %s, %s, %s), asCALL_THISCALL);' % (cname, decl_escaped, cname, func_name, arg_types, ret_type))
		
	cpp_file.write("\n}")
	
	# generate global instance for use in functions that return references to something
	h_file.write('\nextern %s g_%s;' % (cname, cname))
	cpp_file.write('\n\n%s g_%s;\n' % (cname, cname))
	
	if namespace:
		h_file.write('\n}\n')
		cpp_file.write('\n}\n')
	

if not os.path.exists('asdocs.json'):
	convert_asdocs_to_json('asdocs.txt')
asdocs = json.loads(open('asdocs.json').read())
asdocs = add_undocumented_props(asdocs)

generate_sven_api_classes(asdocs)

