#include "util.h"
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <string.h>
#include "Mdl.h"
#include "globals.h"
#include <sys/types.h>
#include <sys/stat.h>

#if defined(WIN32) || defined(_WIN32)
#include <Windows.h>
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#define GetCurrentDir getcwd

typedef char TCHAR;

void OutputDebugString(const char *str) {}
#endif

vector<string> printlog;

str_map_set g_tracemap_req;
str_map_set g_tracemap_opt;

str_map_fileinfo file_exist_cache;
set<string, InsensitiveCompare> processed_models;

const int numContentDirs = 5;
const char * contentDirs[numContentDirs] = {"svencoop_hd", "svencoop_addon", "svencoop_downloads", "svencoop", "valve"};

ofstream log_file;

void log_init(string log_file_path)
{
	if (log_enabled)
	{
		if (log_file.is_open())
			log_file.close();
		log_file.open(log_file_path.c_str(), ios::out | ios::trunc);
		if (!log_file.is_open())
			cout << "Failed to open file for writing: " << log_file_path << endl;
	}
}

void log(string s, bool print_to_screen)
{
	if (log_enabled && log_file.is_open()) {
		log_file << s;
	}
	if (print_to_screen)
		std::cout << s;
}

void log_close()
{
	if (log_enabled && log_file.is_open())
		log_file.close();
}

void trace_missing_file(string file, string reference, bool required)
{
	if (contentExists(file, false))
		return;

	str_map_set& trace = required ? g_tracemap_req : g_tracemap_opt;

	trace[file].insert(reference);
}

uint64 getSystemTime()
{
    #if defined(WIN32) || defined(_WIN32)
	static LARGE_INTEGER s_frequency;
    static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
    if (s_use_qpc) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (1000LL * now.QuadPart) / s_frequency.QuadPart;
    } else {
        return GetTickCount();
    }
    #else
    timespec t, res;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t);
	return t.tv_sec*1000 + t.tv_nsec/1000000;
    #endif
}

string replaceChar(string s, char c, char with)
{
	replace(s.begin(), s.end(), c, with);
	return s;
}

set_icase get_replacement_file_resources(string fname)
{
	set_icase resources;

	contentExists(fname, true); // fix caps

	ifstream myfile(fname);
	if (myfile.is_open())
	{
		string line;
		while (getline(myfile, line))
		{
			line = trimSpaces(line);
			if (line.find("//") == 0)
				continue;
			string oldLine = line;

			bool badLine = false;
			for (int q = 0; q < 3; q++)
			{
				int quote = line.find_first_of("\"");
				if (quote == string::npos)
				{
					badLine = true;
					break;
				}
				line = line.substr(quote+1);
			}
			int quote = line.find_first_of("\"");
			if (quote == string::npos)
				badLine = true;

			if (badLine)
			{
				// quotes are optional (for paths without spaces)
				// so ignore any quotes and assume the files are separated by spaces/tabs
				// e.g. this is valid: "test.wav  "lol.wav"
				line = oldLine;
				line.erase(std::remove(line.begin(), line.end(), '\"'), line.end());
				int gap = line.find_first_of(" \t");
				if (gap == string::npos)
					continue;
				line = trimSpaces(line.substr(gap+1));
				
			}
			else
				line = trimSpaces(line.substr(0, quote));

			push_unique(resources, normalize_path(line));
		}
	}
	//else
	//	cout << "Failed to open: " << fname << endl; // not needed - file will be flagged as missing later
	myfile.close();

	return resources;
}

set_icase get_sentence_file_resources(string fname, string trace_path)
{
	set_icase resources;
	str_map_set references;

	contentExists(fname, true); // fix caps

	ifstream myfile(fname);
	if (myfile.is_open())
	{
		string line;
		while (getline(myfile, line))
		{
			line = trimSpaces(line);
			if (line.find("//") == 0 || line.size() == 0)
				continue;

			line = replaceChar(line, '\t', ' ');

			// strip volume/pitch settings (e.g. "hgrunt/grenade!(v90 120)")
			// Note: The game does this too - so filenames with parens in them won't work ( "alert(test)" becomes "alert" )
			size_t parenStart = line.find("(");
			size_t parenEnd = line.find(")", parenStart);
			while (parenStart != string::npos && parenEnd != string::npos)
			{
				line = line.substr(0, parenStart) + line.substr(parenEnd+1);
				parenStart = line.find("(");
				parenEnd = line.find(")", parenStart);
			}

			string sentence_name = line.substr(0, line.find_first_of(" "));

			// strip commas + periods
			// Note: The game doesn't allow file extensions (alert.wav = alert + _period + wav)
			bool usesComma = line.find(",") != string::npos;
			bool usesPeriod = line.find(".") != string::npos;
			line = replaceChar(line, ',', ' ');
			line = replaceChar(line, '.', ' ');

			// Get folder name if specified
			string folder = "vox/";
			size_t folderEnd = line.find_last_of("/");
			if (folderEnd != string::npos) {
				folder = line.substr(0, folderEnd+1);
				size_t folderStart = folder.find_last_of(" \t");
				if (folderStart != string::npos)
					folder = folder.substr(folderStart+1);
				line = line.substr(folderEnd+1);
			} else {
				// vox speech
				size_t firstSnd = line.find_first_of(" \t");
				if (firstSnd != string::npos)
					line = line.substr(firstSnd+1);
			}

			vector<string> parts = splitString(line, " ");

			if (usesComma)
				parts.push_back("_comma");
			if (usesPeriod)
				parts.push_back("_period");

			if (parts.size() == 0)
				continue;

			for (int i = 0; i < parts.size(); i++)
			{
				string snd = parts[i];
				string ext = find_content_ext(snd, "sound/" + folder);
				if (ext.length()) 
				{
					snd = normalize_path("sound/" + folder + snd + "." + ext);
				}
				else
				{
					// not required to be a .wav but the trace won't work otherwise.
					snd = normalize_path("sound/" + folder + snd + ".wav");
				}
				push_unique(resources, snd);
				references[snd].insert(sentence_name);
			}
		}
	}
	//else
	//	cout << "Failed to open: " << fname << endl; // not needed - file will be flagged as missing later
	myfile.close();

	for (set_icase::iterator iter = resources.begin(); iter != resources.end(); iter++)
	{
		set<string> refs = references[*iter];
		string sentence_name;
		if (refs.size() > 0)
		{
			sentence_name = " --> " + *refs.begin();
			if (refs.size() > 1)
				sentence_name += " (and " + to_string(refs.size()-1) + " others)";
		}
		trace_missing_file(*iter, trace_path + sentence_name, true);
	}

	return resources;
}

vector<string> parse_script_arg(string arg, string fname, string err)
{
	vector<string> ret;

	// parse weapon name from argument 1
	if (arg[0] == '"')
	{
		// It's a string. Easy!
		string val = readQuote(arg);
		if (val.length())
			ret.push_back(val);
		return ret;
	}
	else if (arg.find("[") != string::npos)
	{
		// an index into an array
		int open = arg.find("[");
		int close = arg.find("]");
		string index = trimSpaces(arg.substr(open, close));
		if (close == open+1 || index.length() == 0)
		{
			log(err + "\t Reason: array index couldn't be parsed from '" + arg + "'\n");
			return ret;
		}
		
		bool isNumber = true;
		int idx = -1;
		for (int i = 0; i < index.size(); i++)
			if (!isdigit(index[i]))
				isNumber = false;
		if (isNumber)
			idx = atoi(index.c_str());
		
		// Parse file for array definition
		string def = arg.substr(0, arg.find("["));
		vector<string> arr;
		ifstream file(fname);
		if (file.is_open())
		{
			int lineNum = 0;
			bool in_array = false;
			string line;
			while (getline(file, line))
			{
				lineNum++;

				line = trimSpaces(line);
				if (line.find("//") == 0)
					continue;

				if (!in_array)
				{
					if (line.find(def) == string::npos)
						continue;
					line = line.substr(line.find(def) + def.length());

					if (line.find("=") == string::npos)
						continue;
					line = line.substr(line.find("=") + 1);

					if (line.find("{") == string::npos)
						line = line.substr(line.find("{") + 1);

					in_array = true;
				}
				if (in_array)
				{
					if (line.find("{") == string::npos)
						line = line.substr(line.find("{") + 1);

					bool hasQuote = true;
					while(hasQuote)
					{
						string element = readQuote(line);
						if (element.length())
						{
							line = line.substr(line.find("\"") + 1);
							line = line.substr(line.find("\"") + 1);
							line = line.substr(line.find(",") + 1);
							arr.push_back(element);
						}
						else
							break;
					}

					if (line.find(";") != string::npos)
						break;
				}
				
			}
		}
		file.close();

		if (arr.size() > 0)
		{
			if (idx >= arr.size())
			{
				for (int i = 0; i < arr.size(); i++)
					ret.push_back(arr[i]);
				return ret;
			}
			else
			{
				ret.push_back(arr[idx]);
				return ret;
			}
		}

		log(err + "\t Reason: Failed to parse array definition for '" + arg + "\n");
	}
	else if (arg.find("(") != string::npos && arg.find(")") != string::npos)
	{
		// Uh oh. It's a function
		int open = arg.find("(");
		int close = arg.find(")");
		string params = trimSpaces(arg.substr(open, close));
		if (close != open+1 && params.length())
		{
			log(err + "\t Reason: function call '" + arg + "' has parameters\n");
			return ret;
		}
					
		string funcdef = "string " + arg.substr(0, arg.find("(") + 1);

		// Parse file for function definition
		// anything more complicated than { return "weapon_super"; } won't work
		ifstream file(fname);
		if (file.is_open())
		{
			int lineNum = 0;
			bool in_func_body = false;
			string line;
			while (getline(file, line))
			{
				lineNum++;

				line = trimSpaces(line);
				if (line.find("//") == 0)
					continue;

				if (line.find(funcdef) != string::npos)
					in_func_body = true;

				if (in_func_body)
				{
					int iret = line.find("return ");
					if (iret != string::npos)
					{
						line = line.substr(iret);
						int semi = line.find(";");
						if (semi == string::npos)
						{
							log(err + "\t Reason: Failed to parse return value for '" + arg + "' on line " + to_string(lineNum) + "\n");
							break;
						}
						line = trimSpaces(line.substr(0, semi));
						if (line.find("\"") == string::npos)
						{
							log(err + "\t Reason: Failed to parse return value for '" + arg + "' on line " + to_string(lineNum) + "\n");
							break;
						}
						file.close();

						string val = readQuote(line);
						if (val.length())
							ret.push_back(val);
						return ret;
					}
				}
			}
		}
		file.close();
		log(err + "\t Reason: Failed to parse function definition for '" + arg + "\n");
		return ret;
	}
	else // must be a variable
	{
		bool valid_var_name = true;
		for (int i = 0; i < arg.length(); i++)
		{
			if (!isalpha(arg[i]) && !isdigit(arg[i]) && arg[i] != '_')
			{
				valid_var_name = false;
				break;
			}
		}
		if (!valid_var_name)
		{
			log(err + "\t Reason: argument '" + arg + "' does not appear to be a string, variable, or function\n");
			return ret;
		}

		// Parse file for variable assignment
		// ...hopefully it's a global assigned at the top of the file
		ifstream file(fname);
		if (file.is_open())
		{
			int lineNum = 0;
			string line;
			while (getline(file, line))
			{
				lineNum++;

				line = trimSpaces(line);
				if (line.find("//") == 0)
					continue;

				if (line.find(arg) == string::npos)
					continue;
				line = line.substr(line.find(arg) + arg.length());

				if (line.find("=") == string::npos)
					continue;
				line = line.substr(line.find("=") + 1);

				if (line.find(";") == string::npos)
					continue;
				line = line.substr(0, line.find(";"));

				if (line.find("\"") == string::npos)
					continue;

				file.close();

				string val = readQuote(line);
				if (val.length())
					ret.push_back(val);
				return ret;
			}
		}
		file.close();
		log(err + "\t Reason: Failed to parse variable assignment for '" + arg + "\n");
	}

	return ret;
}

set_icase get_script_dependencies(string fname, set<string>& searchedScripts)
{
	set_icase resources;

	string folder = "scripts/maps/";

	// ignore sounds/models in this script. They're false positives.
	bool isWeaponCustom = toLowerCase(fname).find("weapon_custom.as") != string::npos;

	int idir = fname.find_last_of("/\\");
	if (idir != string::npos && idir > folder.length())
		folder = fname.substr(0, idir) + '/';

	contentExists(fname, true); // fix caps

	string trace = fname;

	ifstream myfile(fname);
	if (myfile.is_open())
	{
		int lineNum = 0;
		bool inCommentBlock = false;
		string line;
		while (getline(myfile, line))
		{
			lineNum++;

			line = trimSpaces(line);
			if (line.find("//") == 0)
				continue;
			
			// check for block comments
			{	
				size_t commentStart = line.find("/*");
				size_t commentEnd = line.find("*/");

				// remove inline block comments
				while (commentStart != string::npos && commentEnd != string::npos)
				{
					line = line.substr(0, commentStart) + line.substr(commentEnd+2);
					commentStart = line.find("/*");
					commentEnd = line.find("*/");
				}

				// ignore lines contained in comment blocks
				if (inCommentBlock)
				{
					if (commentEnd != string::npos)
					{
						inCommentBlock = false;
						line = line.substr(commentEnd+2);
						commentStart = commentEnd = string::npos;
					}
					else
						continue;
				}				
				else if (commentStart != string::npos)
				{
					line = line.substr(0, commentStart);
					inCommentBlock = true;
				}
			}

			line = replaceChar(line, '\t', ' ' );
			
			if (line.find("#include") == 0) 
			{
				// .as extension is optional in cfg file, but required to be ommitted in #include statements
				string include = normalize_path(folder + readQuote(line) + ".as");
				if (searchedScripts.find(include) == searchedScripts.end())
				{
					searchedScripts.insert(include);
					push_unique(resources, include);
					set_icase includeRes = get_script_dependencies(include, searchedScripts);
					resources.insert(includeRes.begin(), includeRes.end());
				}
			}

			// look for string literals
			if (!isWeaponCustom) {
				string s = line;
				vector<string> values;
				while (s.length())
				{
					if (s.find("\"") >= s.length() - 1)
						break;
					s = s.substr(s.find("\"") + 1);

					if (s.find("\"") == string::npos)
						break;

					string val = s.substr(0, s.find("\""));
					string ext = get_ext(val);

					s = s.substr(s.find("\"")+1);	

					if (val.length() == ext.length() + 1)
						continue; // probably a path constructed from vars

					values.push_back(val);

					// handle case where the full path is broken up into separate string literals for no reason
					string tmpVal = normalize_path(val);
					string sound_val = "sound/" + tmpVal;
					bool valExists = is_default_file(tmpVal) || contentExists(tmpVal, false);
					bool soundValExists = is_default_file(sound_val) || contentExists(sound_val, false);
					if (values.size() > 1 && !valExists && !soundValExists)
					{
						string concatVal = "";
						for (int i = 0; i < values.size(); i++)
							concatVal += values[i];
						if (contentExists(concatVal, false))
						{
							val = concatVal;
							
							tmpVal = normalize_path(val);
							sound_val = "sound/" + tmpVal;
							valExists = is_default_file(tmpVal) || contentExists(tmpVal, false);
							soundValExists = is_default_file(sound_val) || contentExists(sound_val, false);
						}
					}

					if (ext == "mdl")
					{
						add_model_resources(normalize_path(val), resources, trace);
					}
					else if (ext == "spr")
					{
						val = normalize_path(val);
						trace_missing_file(val, trace, true);
						push_unique(resources, val);
					}
					else
					{
						// probably safe to assume the script is using sounds correctly,
						// so it's ok if the prefix is missing. Just try to find a plausible match.
						// There will be too many false positives otherwise.
						string valpath = soundValExists ? sound_val : val;
						
						val = normalize_path(valpath);
						for (int k = 0; k < NUM_SOUND_EXTS; k++)
						{
							if (ext == g_valid_exts[k])
							{
								trace_missing_file(val, trace, true);
								push_unique(resources, val);
								break;
							}
						}
					}
				}
				
			}

			// try to parse out custom weapon sprite files
			if (!isWeaponCustom && line.find("g_ItemRegistry.RegisterWeapon(") != string::npos)
			{
				string err = "ERROR: Failed to parse custom weapon definition in " + fname + " (line " + to_string(lineNum) + ")\n";
				line = line.substr(line.find("g_ItemRegistry.RegisterWeapon(") + string("g_ItemRegistry.RegisterWeapon(").length());
				int comma = line.find_first_of(",");
				if (comma >= line.length()-1)
				{
					log(err + "\t Reason: couldn't find all arguments on this line\n");
					continue;
				}
				string arg1 = trimSpaces(line.substr(0, comma));

				line = line.substr(comma+1);
				int comma2 = line.find_first_of(",");
				if (comma2 == string::npos)
					comma2 = line.find_last_of(")"); // third arg is optional
				if (comma2 >= line.length()-1)
				{
					log(err + "\t Reason: couldn't find all arguments on this line\n");
					continue;
				}
				string arg2 = trimSpaces(line.substr(0, comma2));

				vector<string> ret1 = parse_script_arg(arg1, fname, err);
				vector<string> ret2 = parse_script_arg(arg2, fname, err);

				if (ret1.size() == 0 || ret2.size() == 0)
					continue;

				if (ret1.size() > 1 || ret2.size() > 1)
				{
					log(err + "\t Reason: array argument(s) with variable index not handled\n");
					continue;
				}

				arg1 = ret1[0];
				arg2 = ret2[0];

				// Note: code duplicated in Bsp.cpp (weapon_custom)
				string hud_file = "sprites/" + arg2 + "/" + arg1 + ".txt";
				trace_missing_file(hud_file, trace, true);
				push_unique(resources, hud_file);

				string hud_path = hud_file;
				if (contentExists(hud_path, true))
				{
					ifstream file(hud_path);
					if (file.is_open())
					{
						int lineNum = 0;
						bool in_func_body = false;
						string line;
						while (getline(file, line))
						{
							lineNum++;

							// strip comments
							size_t cpos = line.find("//");
							if (cpos != string::npos)
								line = line.substr(0, cpos);

							line = trimSpaces(line);

							line = replaceChar(line, '\t', ' ');
							vector<string> parts = splitString(line, " ");

							if (parts.size() < 3)
								continue;
							
							string spr = "sprites/" + parts[2] + ".spr";
							trace_missing_file(spr, trace + " --> " + hud_file, true);
							push_unique(resources, spr);
						}
					}
					file.close();
				}
			}
		}
	}
	//else
	//	cout << "Failed to open: " << fname << endl; // not needed - file will be flagged as missing later
	myfile.close();

	return resources;
}

void add_script_resources(string script, set_icase& resources, string traceFrom)
{
	if (parsed_scripts.find(script) != parsed_scripts.end())
	{
		// don't process the same script twice
		return;
	}
	
	parsed_scripts.insert(script);
	trace_missing_file(script, traceFrom, true);
	push_unique(server_files, script);
	push_unique(resources, script);

	set<string> searchedScripts;
	set_icase scripts = get_script_dependencies(script, searchedScripts);
	for (set_icase::iterator iter = scripts.begin(); iter != scripts.end(); iter++)
	{
		bool isScript = get_ext(*iter) == "as";
		if (isScript) {
			trace_missing_file(*iter, traceFrom + " --> " + script, true);
			push_unique(server_files, *iter);
			push_unique(resources, *iter);
		}
		else // file is a sound/model and was traced in the dependency function
		{
			push_unique(resources, *iter);
		}
	}
}

void add_model_resources(string model_path, set_icase& resources, string traceFrom)
{
	if (processed_models.find(model_path) != processed_models.end())
		return;
	processed_models.insert(model_path);

	Mdl model = Mdl(model_path);

	trace_missing_file(model_path, traceFrom, true);
	push_unique(resources, model_path);
	if (model.valid)
	{
		set_icase model_res = model.get_resources();
		for (set_icase::iterator iter = model_res.begin(); iter != model_res.end(); iter++)
		{
			trace_missing_file(*iter, traceFrom + " --> " + model_path , true);
			push_unique(resources, *iter);
		}
	}
}

void add_replacement_file_resources(string replacement_file, set_icase& resources, string traceFrom, bool modelsNotSounds)
{
	// TODO: Don't process the same file twice (will require global resource set)
	trace_missing_file(replacement_file, traceFrom, true);
	push_unique(server_files, replacement_file);
	push_unique(resources, replacement_file);
	set_icase replace_res = get_replacement_file_resources(replacement_file);
	for (set_icase::iterator it = replace_res.begin(); it != replace_res.end(); it++)
	{
		if (modelsNotSounds)
			add_model_resources(normalize_path(*it), resources, traceFrom + " --> " + replacement_file);
		else
		{
			string snd = "sound/" + *it;
			trace_missing_file(snd, traceFrom + " --> " + replacement_file, true);
			push_unique(resources, snd);
		}
	}
}

void add_sentence_file_resources(string setence_file, set_icase& resources, string traceFrom)
{
	trace_missing_file(setence_file, traceFrom, true);
	push_unique(server_files, setence_file);
	push_unique(resources, setence_file);
	set_icase sounds = get_sentence_file_resources(setence_file, traceFrom + " --> " + setence_file);
	resources.insert(sounds.begin(), sounds.end());
}

void add_force_pmodels_resources(string pmodel_list, set_icase& resources, string traceFrom)
{
	vector<string> models = splitString(pmodel_list, ";");
	for (int i = 0; i < models.size(); i++)
	{
		string model = models[i];
		if (model.length() == 0)
			continue;
		string path = "models/player/" + model + "/" + model;

		trace_missing_file(path + ".bmp", traceFrom, true);
		push_unique(resources, path + ".bmp");

		add_model_resources(normalize_path(path + ".mdl"), resources, traceFrom);
	}
}

string replaceString(string subject, string search, string replace) 
{
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != string::npos) 
	{
		 subject.replace(pos, search.length(), replace);
		 pos += replace.length();
	}
	return subject;
}

string normalize_path(string s, bool is_keyvalue)
{
	if (s.size() == 0)
		return s;

	if (is_keyvalue)
	{
		// A lot of mappers tried to use '\' as a path separator not realizing that HL interprets that as
		// an escape sequence. HL only implements the '\n' escape sequence. Everything else is
		// converted to a '\', so the following character is deleted. To counteract this, mappers add a 
		// random character after the '\' (usually "!" or "*"). This is why there are so many paths
		// that look broken but aren't (e.g. "models\!barney.mdl")
		// Update: using * indicates that HL should stream the file instead of loading all at once
		s = replaceString(s, "\\n", "\n");
		for (int i = 0; i < (int)s.size()-1; i++)
			if (s[i] == '\\')
				s.erase(i+1, 1);
	}

	s = replaceChar(s, '\\', '/');

	vector<string> parts = splitString(s, "/");
	int depth = 0;
	for (int i = 0; i < parts.size(); i++)
	{
		depth++;
		if (parts[i] == "..")
		{
			depth--;
			if (depth == 0)
			{
				continue; // can only .. up to Sven Co-op, and not any further
			}
			else if (i > 0)
			{
				parts.erase(parts.begin() + i);
				parts.erase(parts.begin() + (i-1));
				i -= 2;
			}
		}
	}
	s = "";
	for (int i = 0; i < parts.size(); i++)
	{
		if (i > 0) {
			s += '/';
		}
		s += parts[i];
	}

	if (!case_sensitive_mode)
	{
		// fix file path capitilization
		fileExists(s, true);
		if (s.find("..") == 0) // strip ../svencoop_addons
		{
			s = s.substr(s.find_first_of("\\/")+1);
			s = s.substr(s.find_first_of("\\/")+1);
		}
	}

	return s;
}

bool push_unique(set_icase& list, string val)
{
	if (list.find(val) == list.end())
	{
		list.insert(val);
		return true;
	}
	return false;
}

bool is_unique(set_icase& list, string val)
{
	return list.find(val) == list.end();
}

bool is_default_file(string file)
{
	auto defaultFile = default_content.find(file);
	if (defaultFile != default_content.end())
	{
		if (case_sensitive_mode)
			return std::equal((*defaultFile).rbegin(), (*defaultFile).rend(), file.rbegin());
		return true;
	}
	return false;
}

string toLowerCase(string str)
{
	transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}

// Note: Paths with ".." in them need to be normalize_path()'d before calling this (Linux only)
bool fileExists(string& file, bool fix_path, string from_path, int from_skip)
{
	auto finfo = file_exist_cache.find(file);
	if (finfo != file_exist_cache.end())
	{
		if (fix_path && finfo->second.exists)
			file = finfo->second.fnameCaseSensitive;
		return finfo->second.exists;
	}

	if (FILE *f = fopen(file.c_str(), "r")) {
		fclose(f);
		if (from_skip == 0)
			file_exist_cache[file] = {true, file};
		return true;
	}

#if defined(WIN32) || defined(_WIN32)
	file_exist_cache[file] = { false, "" };
	return false;
#else
	if (case_sensitive_mode)
	{
		if (from_skip == 0)
			file_exist_cache[file] = { false, "" };
		return false;
	}
	
	// check each folder in the path for correct capitilization
	string path = ".";
	string filename = file;
	if (file.find_first_of("/") != string::npos)
	{
		filename = file.substr(file.find_last_of("/")+1);
		path = file.substr(0, file.find_last_of("/"));
		vector<string> dirs = splitString(path, "/");
		path = ".";
		if (from_skip)
			path = from_path;

		if (from_skip < dirs.size())
		{
			// open first dir in path
			DIR * dir = opendir(path.c_str());
			if (!dir)
			{
				if (from_skip == 0)
					file_exist_cache[file] = { false, "" };
				return false; // shouldn't ever happen
			}

			// search all dirs that are a case-insensitive match for the next dir in the path
			string lowerDir = toLowerCase(dirs[from_skip]);
			while (true)
			{
				dirent *entry = readdir(dir);
				if(!entry)
					break;
		
				if(entry->d_type != DT_DIR)
					continue;
		
				string name = string(entry->d_name);

				string fixedFile = file;
				if (toLowerCase(name).compare(lowerDir) == 0 && 
					fileExists(fixedFile, true, path + "/" + name, from_skip+1))
				{
					if (fix_path)
						file = fixedFile;
					if (from_skip == 0)
						file_exist_cache[file] = { true, fixedFile };
					closedir(dir);
					return true;
				}
			}
			closedir(dir);
			if (from_skip == 0)
				file_exist_cache[file] = { false, file };
			return false; // next dir in path doesn't exist
		}
	}

	if (path.length() > 2 && path[0] == '.')
		path = path.substr(2); // skip "./"

	// case-insensitive search for the file
	DIR *dir = opendir(path.c_str());
		
	if(!dir)
	{
		if (from_skip == 0)
			file_exist_cache[file] = { false, file };
		return false;
	}
	
	string lowerFile = toLowerCase(filename);
	bool found = false;
	string newFile = file;
	while(true)
	{
		dirent *entry = readdir(dir);
		if (!entry)
			break;

		if (entry->d_type == DT_DIR)
			continue;
		
		string name = string(entry->d_name);
		if (toLowerCase(name).compare(lowerFile) == 0)
		{
			string oldFile = file;
			if (path.length() > 1 || path[0] != '.')
				newFile = path + '/' + name;
			else
				newFile = name;
				//cout << "Case mismatch: " << oldFile << " -> " << file << endl;
			if (fix_path)
				file = newFile;
			found = true;
			break;
		}
	}
	closedir(dir);
	if (from_skip == 0)
		file_exist_cache[file] = { found, found ? newFile : "" };
	return found;
#endif
}

bool contentExists(string& file, bool fix_path, string& full_path)
{
	if (fileExists(file, fix_path))
	{
		full_path = file;
		return true;
	}

	for (int i = 0; i < numContentDirs; i++)
	{
		string f = "../" + string(contentDirs[i]) + "/" + file;
		if (fileExists(f, fix_path)) {
			if (fix_path) 
				file = f;
			full_path = f;
			return true;
		}
	}

	return false;
}

string find_content_ext(string fname, string dir)
{
	vector<string> results;

	results = getDirFiles(dir, "*", fname, true);
	if (results.size()) return get_ext(results[0]);

	for (int i = 0; i < numContentDirs; i++)
	{
		results = getDirFiles("../" + string(contentDirs[i]) + "/" + dir, "*", fname, true);
		if (results.size()) return get_ext(results[0]);
	}

	return "";
}

string get_ext(string fname)
{
	int iext = fname.find_last_of(".");
	if (iext != string::npos && iext < fname.length() - 1)
	{
		return toLowerCase(fname.substr(iext+1));
	}
	return "";
}

string readQuote(const string& str)
{
    int begin = -1;
    int end = -1;

    for (int i = 0; i < (int)str.length(); i++)
    {
        char c = str[i];
        if (begin == -1)
        {
            if (c == '"')
                begin = i + 1;
        }
        else
        {
            if (c == '"')
                end = i;
        }
    }
    if (begin != -1 && end != -1)
        return str.substr(begin, end-begin);
   // else
   //     cout << "readQuote: Could not find quote in '" << str << "'\n";

    return "";
}

vector<string> getDirFiles( string path, string extension, string startswith, bool onlyOne)
{
    vector<string> results;
    
#if defined(WIN32) || defined(_WIN32)
	path = path + startswith + "*." + extension;
    winPath(path);
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

	//println("Target file is " + path);
	hFind = FindFirstFile(path.c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) 
	{
		//println("FindFirstFile failed " + str((int)GetLastError()) + " " + path);
		return results;
	} 
	else 
	{
		results.push_back(FindFileData.cFileName);

		while (FindNextFile(hFind, &FindFileData) != 0)
		{
			results.push_back(FindFileData.cFileName);
			if (onlyOne)
				break;
		}

		FindClose(hFind);
	}
#else
    extension = toLowerCase(extension);
    startswith = toLowerCase(startswith);
	startswith.erase(std::remove(startswith.begin(), startswith.end(), '*'), startswith.end());
    DIR *dir = opendir(path.c_str());
    
    if(!dir)
        return results;
    
    while(true)
    {
        dirent *entry = readdir(dir);
        
        if(!entry)
            break;
        
        if(entry->d_type == DT_DIR)
            continue;
        
        string name = string(entry->d_name);
        string lowerName = toLowerCase(name);
        
        if(extension.size() > name.size() || startswith.size() > name.size())
            continue;
        
        if(extension == "*" || std::equal(extension.rbegin(), extension.rend(), lowerName.rbegin()))
		{
			if (startswith.size() == 0 || std::equal(startswith.begin(), startswith.end(), lowerName.begin()))
			{
				results.push_back(name);
				if (onlyOne)
					break;
			}
		}
    }
    
    closedir(dir);
#endif
    
    return results;
}

void winPath( string& path )
{
	for (int i = 0, size = path.size(); i < size; i++)
	{
		if (path[i] == '/')
			path[i] = '\\';
	}
}

vector<string> getSubdirs( string path )
{
	vector<string> results;
    
    #if defined(WIN32) || defined(_WIN32)
    path = path + "*";
    winPath(path);
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

	//println("Target file is " + path);
	hFind = FindFirstFile(path.c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) 
	{
		//println("FindFirstFile failed " + str((int)GetLastError()) + " " + path);
		return results;
	} 
	else 
	{
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && 
			strcasecmp(FindFileData.cFileName, ".") &&
			strcasecmp(FindFileData.cFileName, "..") )
			results.push_back(FindFileData.cFileName);

		while (FindNextFile(hFind, &FindFileData) != 0)
		{
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && 
				strcasecmp(FindFileData.cFileName, ".") && 
				strcasecmp(FindFileData.cFileName, ".."))
				results.push_back(FindFileData.cFileName);
		}

		FindClose(hFind);
	}
    #else
    DIR *dir = opendir(path.c_str());
    
    if(!dir)
        return results;
    
    while(true)
    {
        dirent *entry = readdir(dir);
        
        if(!entry)
            break;
        
        string name = string(entry->d_name);
        
        if(entry->d_type == DT_DIR &&
           strcasecmp(name.c_str(), ".") &&
           strcasecmp(name.c_str(), ".."))
            results.push_back(name);
    }
    
    closedir(dir);
    #endif
	
    return results;
}

char * loadFile( string file )
{
	if (!fileExists(file))
	{
		log("file does not exist " + file + "\n");
		return NULL;
	}
	ifstream fin(file.c_str(), ifstream::in|ios::binary);
	int begin = fin.tellg();
	fin.seekg (0, ios::end);
	int size = (int)fin.tellg() - begin;
	char * buffer = new char[size];
	fin.seekg(0);
	fin.read(buffer, size);
	fin.close();
	//println("read " + str(size));
	return buffer;
}

void recurseSubdirs(string path, vector<string>& dirs)
{
	dirs.push_back(path);
	vector<string> files = getSubdirs(path);
	for (uint i = 0; i < files.size(); i++)
		recurseSubdirs(path + files[i] + '/', dirs);
}

vector<string> getAllSubdirs(string path)
{
	vector<string> dirs;
	recurseSubdirs(path, dirs);
	return dirs;
}

void insert_unique(const vector<string>& insert, vector<string>& insert_into)
{
	for (uint i = 0; i < insert.size(); ++i)
	{
		bool exists = false;
		for (uint k = 0; k < insert_into.size(); ++k)
		{
			if (strcasecmp(insert_into[k].c_str(), insert[i].c_str()) == 0)
			{
				exists = true;
				break;
			}	
		}
		if (!exists)
			insert_into.push_back(insert[i]);
	}
}

string trimSpaces(string s)
{
	// Remove white space indents
	int lineStart = s.find_first_not_of(" \t\n\r");
	if (lineStart == string::npos)
		return "";

	// Remove spaces after the last character
	int lineEnd = s.find_last_not_of(" \t\n\r");
	if (lineEnd != string::npos && lineEnd < s.length() - 1)
		s = s.substr(lineStart, (lineEnd+1) - lineStart);
	else
		s = s.substr(lineStart);

	return s;
}

vector<string> splitString( string str, const char * delimitters )
{
	vector<string> split;
	if (str.size() == 0)
		return split;
	
	// somehow plain assignment doesn't create a copy and even modifies the parameter that was passed by value (WTF!?!)
	//string copy = str; 
	string copy;
	for (int i = 0; i < str.length(); i++)
		copy += str[i]; 

	char * tok = strtok((char *)copy.c_str(), delimitters);

	while (tok != NULL)
	{
		split.push_back(tok);
		tok = strtok(NULL, delimitters);
	}
	return split;
}

bool dirExists(const string& path)
{
	struct stat info;

	if (stat(path.c_str(), &info) != 0)
		return false;
	else if(info.st_mode & S_IFDIR)
		return true;
	return false;
}
