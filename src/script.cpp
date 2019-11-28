#include "script.h"
#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scriptarray/scriptarray.h>
#include <scriptdictionary/scriptdictionary.h>
#include <scriptfile/scriptfile.h>
#include "util.h"
#include "as_object/common.h"
//#include "as_object/CCVar.h"

#define REGISTER_GENERIC_REF_CLASS(cname) \
		engine->RegisterObjectType(#cname, 0, asOBJ_REF); \
		engine->RegisterObjectBehaviour(#cname, asBEHAVE_FACTORY, #cname "@ f()", asFUNCTION(CCVar_Factory), asCALL_CDECL); \
		engine->RegisterObjectBehaviour(#cname, asBEHAVE_ADDREF, "void f()", asMETHOD(CCVar, Addref), asCALL_THISCALL); \
		engine->RegisterObjectBehaviour(#cname, asBEHAVE_RELEASE, "void f()", asMETHOD(CCVar, Release), asCALL_THISCALL);

#define REGISTER_GENERIC_VALUE_CLASS(cname) \
		engine->RegisterObjectType(#cname, sizeof(CCVar), asOBJ_VALUE); \
		engine->RegisterObjectBehaviour(#cname, asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Constructor), asCALL_CDECL_OBJLAST); \
		engine->RegisterObjectBehaviour(#cname, asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Destructor), asCALL_CDECL_OBJLAST);

asIScriptEngine* create_script_engine();
asIScriptContext * compile_script(asIScriptEngine* engine, std::string script_path);

// Implement a simple message callback function
void MessageCallback(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if (msg->type == asMSGTYPE_WARNING)
		type = "WARN";
	else if (msg->type == asMSGTYPE_INFORMATION)
		type = "INFO";
	printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

void print(string &msg)
{
	printf("%s", msg.c_str());
}

void Constructor(void *memory)
{
	// Initialize the pre-allocated memory by calling the
	// object constructor with the placement-new operator
}

void Destructor(void *memory)
{
	// Uninitialize the memory by calling the object destructor
}

void execute_script(std::string path) {
	cout << "zomg executing script zomg " << path << endl;
	path = path.substr(0, path.size()-3); // strip ".as" from path

	asIScriptEngine* engine = create_script_engine();
	asIScriptContext* ctx = compile_script(engine, path);

	if (!ctx)
		return;

	int r = ctx->Execute();
	if (r != asEXECUTION_FINISHED)
	{
		// The execution didn't complete as expected. Determine what happened.
		if (r == asEXECUTION_EXCEPTION)
		{
			// An exception occurred, let the script writer know what happened so it can be corrected.
			printf("An exception '%s' occurred. Please correct the code and try again.\n", ctx->GetExceptionString());
		}
	}
	ctx->Release();
	engine->ShutDownAndRelease();
}

asIScriptEngine* create_script_engine() {
	asIScriptEngine *engine = asCreateScriptEngine();
	int r = engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
	if (r < 0) {
		cout << "OH NO FAILED TO SET MESSAGE CALLBACK\n";
		return NULL;
	}
	RegisterScriptArray(engine, true);
	RegisterStdString(engine);
	RegisterScriptDictionary(engine);
	RegisterScriptFile(engine);
	RegisterScriptAny(engine);

	r = engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(print), asCALL_CDECL);
	if (r < 0) {
		cout << "UH OH FAILED TO REGISTER GLOBAL FUNC\n";
	}

	engine->RegisterInterface("ScriptBasePlayerWeaponEntity");
	engine->RegisterInterface("ScriptBaseEntity");
	engine->RegisterInterface("ScriptBaseMonsterEntity");
	engine->RegisterInterface("ScriptBaseAnimating");
	engine->RegisterInterface("ScriptBaseTankEntity");
	engine->RegisterInterface("ScriptBasePlayerItemEntity");
	engine->RegisterInterface("ScriptBasePlayerAmmoEntity");
	engine->RegisterInterface("ScriptBaseItemEntity");
	/*
	REGISTER_GENERIC_REF_CLASS(CCVar);
	REGISTER_GENERIC_REF_CLASS(CCommand);
	REGISTER_GENERIC_REF_CLASS(CTextMenu);
	REGISTER_GENERIC_REF_CLASS(CItemInventory);
	REGISTER_GENERIC_REF_CLASS(CBasePlayer);
	REGISTER_GENERIC_REF_CLASS(CBasePlayerWeapon);
	REGISTER_GENERIC_REF_CLASS(CBaseMonster);
	REGISTER_GENERIC_REF_CLASS(CPathTrack);
	REGISTER_GENERIC_REF_CLASS(CTextMenuItem);
	REGISTER_GENERIC_REF_CLASS(CBaseEntity);
	REGISTER_GENERIC_REF_CLASS(SayParameters);
	REGISTER_GENERIC_REF_CLASS(edict_t);
	REGISTER_GENERIC_REF_CLASS(entvars_t);
	REGISTER_GENERIC_REF_CLASS(MonsterEvent);
	REGISTER_GENERIC_REF_CLASS(Schedule);

	REGISTER_GENERIC_VALUE_CLASS(EHandle);
	REGISTER_GENERIC_VALUE_CLASS(Vector);
	REGISTER_GENERIC_VALUE_CLASS(Vector2D);
	REGISTER_GENERIC_VALUE_CLASS(USE_TYPE);
	REGISTER_GENERIC_VALUE_CLASS(HUDSpriteParams);
	REGISTER_GENERIC_VALUE_CLASS(HookReturnCode);
	REGISTER_GENERIC_VALUE_CLASS(ItemInfo);
	REGISTER_GENERIC_VALUE_CLASS(TraceResult);
	REGISTER_GENERIC_VALUE_CLASS(char);

	engine->RegisterEnum("NetworkMessageDest");
	engine->RegisterEnum("SOUND_CHANNEL");
	engine->RegisterEnum("Activity");

	engine->RegisterFuncdef("void TextMenuPlayerSlotCallback()");
	//engine->RegisterGlobalFunction("void SetCallback(CallbackFunc @cb)", asFUNCTION(SetCallback), asCALL_CDECL);
	*/

	RegisterSvenModules(engine);
	return engine;
}

asIScriptContext * compile_script(asIScriptEngine* engine, std::string script_path) {
	// The CScriptBuilder helper is an add-on that loads the file,
	// performs a pre-processing pass if necessary, and then tells
	// the engine to build a script module.
	CScriptBuilder builder;
	int r = builder.StartNewModule(engine, "MyModule");
	if (r < 0)
	{
		// If the code fails here it is usually because there
		// is no more memory to allocate the module
		printf("Unrecoverable error while starting a new module.\n");
		return NULL;
	}
	r = builder.AddSectionFromFile(script_path.c_str());
	if (r < 0)
	{
		// The builder wasn't able to load the file. Maybe the file
		// has been removed, or the wrong name was given, or some
		// preprocessing commands are incorrectly written.
		printf("Please correct the errors in the script and try again.\n");
		return NULL;
	}
	r = builder.BuildModule();
	if (r < 0)
	{
		// An error occurred. Instruct the script writer to fix the 
		// compilation errors that were listed in the output stream.
		printf("Please correct the errors in the script and try again.\n");
		return NULL;
	}

	asIScriptModule *mod = engine->GetModule("MyModule");
	asIScriptFunction *func = mod->GetFunctionByDecl("void MapInit()");
	if (func == 0)
	{
		// The function couldn't be found. Instruct the script writer
		// to include the expected function in the script.
		printf("The script must have the function 'void MapInit()'. Please add it and try again.\n");
		return NULL;
	}

	// Create our context, prepare it, and then execute
	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(func);

	return ctx;
}
