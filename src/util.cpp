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

#if defined(WIN32) || defined(_WIN32)
#include <Windows.h>
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#define GetCurrentDir getcwd

typedef char TCHAR;

void OutputDebugString(const char *str) {}
#endif

vector<string> printlog;

str_map_vector g_tracemap_req;
str_map_vector g_tracemap_opt;

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

	str_map_vector& trace = required ? g_tracemap_req : g_tracemap_opt;

	push_unique(trace[file], reference);
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

vector<string> get_replacement_file_resources(string fname)
{
	vector<string> resources;

	contentExists(fname, true); // fix caps

	ifstream myfile(fname);
	if (myfile.is_open())
	{
		while ( !myfile.eof() )
		{
			string line;
			getline (myfile,line);

			line = trimSpaces(line);
			if (line.find("//") == 0)
				continue;

			line.erase(std::remove(line.begin(), line.end(), '\"'), line.end());

			int ext1 = line.find_first_of(".");
			if (ext1 == string::npos)
				continue;
			line = line.substr(ext1);
			int ext2 = line.find_first_of(" \t");
			if (ext2 == string::npos)
				continue;
			line = trimSpaces(line.substr(ext2));

			push_unique(resources, normalize_path(line));
		}
	}
	//else
	//	cout << "Failed to open: " << fname << endl; // not needed - file will be flagged as missing later
	myfile.close();

	return resources;
}

vector<string> get_sentence_file_resources(string fname)
{
	vector<string> resources;

	contentExists(fname, true); // fix caps

	ifstream myfile(fname);
	if (myfile.is_open())
	{
		while ( !myfile.eof() )
		{
			string line;
			getline (myfile,line);

			line = trimSpaces(line);
			if (line.find("//") == 0)
				continue;

			line = replaceChar(line, '\t', ' ' );

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
			}
		}
	}
	//else
	//	cout << "Failed to open: " << fname << endl; // not needed - file will be flagged as missing later
	myfile.close();

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
			while ( !file.eof() )
			{
				string line;
				getline (file,line);
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
			while ( !file.eof() )
			{
				string line;
				getline (file,line);
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
			while ( !file.eof() )
			{
				string line;
				getline (file,line);
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

vector<string> get_script_dependencies(string fname, vector<string>& searchedScripts)
{
	vector<string> resources;

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
		while ( !myfile.eof() )
		{
			string line;
			getline (myfile,line);
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
				if (push_unique(searchedScripts, include))
				{
					push_unique(resources, include);
					insert_unique(get_script_dependencies(include, searchedScripts), resources);
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
					string sound_val = "sound/" + val;
					if (values.size() > 1 && !contentExists(val, false) && !contentExists(sound_val, false))
					{
						string concatVal = "";
						for (int i = 0; i < values.size(); i++)
							concatVal += values[i];
						if (contentExists(concatVal, false))
						{
							val = concatVal;
							if (toLowerCase(concatVal).find("sound/") == 0) 
								val = val.substr(6); // readded later
						}
					}

					if (ext == "mdl")
					{
						val = normalize_path(val);
						trace_missing_file(val, trace, true);
						push_unique(resources, val);

						Mdl model = Mdl(val);
						if (model.valid)
						{
							vector<string> model_res = model.get_resources();
							for (int k = 0; k < model_res.size(); k++)
							{
								trace_missing_file(model_res[k], trace + " --> " + val, true);
								push_unique(resources, model_res[k]);
							}
						}
					}
					else if (ext == "spr")
					{
						val = normalize_path(val);
						trace_missing_file(val, trace, true);
						push_unique(resources, val);
					}
					else
					{
						// "sound/" only needed if this is PrecacheSound. Hopefully people aren't
						// writing wrapper functions for precaching or else this won't work
						string prefix = line.find("PrecacheGeneric") == string::npos ? "sound/" : "";

						val = normalize_path(prefix + val);
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
						while ( !file.eof() )
						{
							string line;
							getline (file,line);
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

string normalize_path(string s)
{
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

bool push_unique(vector<string>& list, string val)
{
	if (is_unique(list, val))
	{
		list.push_back(val);
		return true;
	}
	return false;
}

bool is_unique(vector<string>& list, string val)
{
	for (int i = 0; i < list.size(); i++)
		if (strcasecmp(list[i].c_str(), val.c_str()) == 0)
			return false;
	return true;
}

string toLowerCase(string str)
{
	transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}

bool fileExists(string& file, bool fix_path, string from_path, int from_skip)
{
	if (FILE *f = fopen(file.c_str(), "r")) {
		fclose(f);
		return true;
	}

#if defined(WIN32) || defined(_WIN32)
	return false;
#else
	if (case_sensitive_mode)
		return false;

	// shorten relative paths
	string safe_file = file;
	size_t iup = safe_file.find("/../");
	while (iup != string::npos)
	{
		string before = safe_file.substr(0, iup);
		string after = safe_file.substr(iup + 3);
		size_t iprev = before.find_last_of("/");
		if (iprev == string::npos)
			before = "";
		else
			before = before.substr(0, iprev);
		
		safe_file = before + after;
		iup = safe_file.find("/../");
	}
	
	// check each folder in the path for correct capitilization
	string path = ".";
	string filename = safe_file;
	if (safe_file.find_first_of("/") != string::npos)
	{
		filename = safe_file.substr(safe_file.find_last_of("/")+1);
		path = safe_file.substr(0, safe_file.find_last_of("/"));
		vector<string> dirs = splitString(path, "/");
		path = ".";
		if (from_skip)
			path = from_path;

		if (from_skip < dirs.size())
		{
			// open first dir in path
			DIR * dir = opendir(path.c_str());
			if (!dir)
				return false; // shouldn't ever happen

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
				if (toLowerCase(name).compare(lowerDir) == 0 && 
					fileExists(file, fix_path, path + "/" + name, from_skip+1))
				{
					closedir(dir);
					return true;
				}
			}
			closedir(dir);
			return false; // next dir in path doesn't exist
		}
	}

	if (path.length() > 2 && path[0] == '.')
		path = path.substr(2); // skip "./"

	// case-insensitive search for the file
	DIR *dir = opendir(path.c_str());
		
	if(!dir)
		return false;
	
	string lowerFile = toLowerCase(filename);
	bool found = false;
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
			if (fix_path)
			{
				string oldFile = safe_file;
				if (path.length() > 1 || path[0] != '.')
					file = path + '/' + name;
				else
					file = name;
				//cout << "Case mismatch: " << oldFile << " -> " << file << endl;
			}
			found = true;
			break;
		}
	}
	closedir(dir);
	return found;
#endif
}

bool contentExists(string& file, bool fix_path)
{
	if (fileExists(file, fix_path))
		return true;

	for (int i = 0; i < numContentDirs; i++)
	{
		string f = "../" + string(contentDirs[i]) + "/" + file;
		if (fileExists(f, fix_path)) {
			if (fix_path) file = f;
			return true;
		}
	}

	return false;
}

string find_content_ext(string fname, string dir)
{
	vector<string> results;

	results = getDirFiles(dir, "*", fname);
	if (results.size()) return get_ext(results[0]);

	for (int i = 0; i < numContentDirs; i++)
	{
		results = getDirFiles("../" + string(contentDirs[i]) + "/" + dir, "*", fname);
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

vector<string> getDirFiles( string path, string extension, string startswith )
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
			results.push_back(FindFileData.cFileName);

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
				results.push_back(name);
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
	s = s.substr(lineStart);

	// Remove spaces after the last character
	int lineEnd = s.find_last_not_of(" \t\n\r");
	if (lineEnd != string::npos && lineEnd < s.length() - 1)
		s = s.substr(0, lineEnd+1);

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
    #if defined(WIN32) || defined(_WIN32)
	DWORD dwAttrib = GetFileAttributesA(path.c_str());

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
			(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    #else
    struct stat data;
    return stat(path.c_str(), &data) == 0 && S_ISDIR(data.st_mode);
    #endif
}
