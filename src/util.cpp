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
	LARGE_INTEGER fq, li;
	QueryPerformanceFrequency(&fq);
	QueryPerformanceCounter(&li);

	double freq = (double)fq.QuadPart;
	double time = (double)li.QuadPart*1000.0*1000.0;
	return (uint64)(time/freq);
    #else
    return time(NULL);
    #endif
}

string getSubStr(const string& s, int beginIndex, int endIndex)
{
    string result = "";
    int max = s.length();
    if (beginIndex < 0 || beginIndex >= max)
    {
        cout << "getSubStr: Invalid begin index " << beginIndex << " '" << s << "'\n";
        return "";
    }
    if (endIndex < 0 || endIndex > max)
    {
        cout << "getSubStr: Invalid end index " << endIndex << endl;
        return "";
    }
    if (beginIndex >= endIndex)
    {
        //println("getSubStr: end index must be larger than begin index");
        return "";
    }

    for (int i = beginIndex; i < endIndex; i++)
        result += s[i];
    return result;
}

string getSubStr(const string& s, int beginIndex)
{
    string result = "";
    int max = s.size();
    if (beginIndex < 0 || beginIndex >= max)
        cout << "getSubStr: Invalid begin index " << beginIndex << " '" << s << "'\n";

    for (int i = beginIndex; i < max; i++)
        result += s.at(i);
    return result;
}

string replaceChar(string s, char c, char with)
{
	replace(s.begin(), s.end(), c, with);
	return s;
}

vector<string> get_replacement_file_resources(string fname)
{
	vector<string> resources;

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
	else
		cout << "Failed to open: " << fname << endl;
	myfile.close();

	return resources;
}

vector<string> get_sentence_file_resources(string fname)
{
	vector<string> resources;

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
			vector<string> parts = splitString(line, " ");
			if (parts.size() < 2)
				continue;

			string folder = "";
			for (int i = 1; i < parts.size(); i++)
			{
				string snd = parts[i];
				if (i == 1)
				{
					int idir = snd.find_first_of("/");
					if (idir != string::npos && idir < snd.length()-1)
					{
						folder = snd.substr(0, idir+1);
						snd = snd.substr(idir+1);
					}
				}
				string ext = find_content_ext(snd, "sound/" + folder);
				if (ext.length()) 
				{
					snd = folder + snd + "." + ext;
					push_unique(resources, normalize_path("sound/" + snd));
				}
				else
				{
					cout << "ERROR: Can't find 'sound/" << folder << snd << "' referenced in sentence file.\n";
				}
			}
		}
	}
	else
		cout << "Failed to open: " << fname << endl;
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
			cout << err << "\t Reason: array index couldn't be parsed from '" << arg << "'\n";
			return ret;
		}
		
		bool isNumber = true;
		int idx = -1;
		for (int i = 0; i < index.size(); i++)
			if (!isNumeric(index[i]))
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

		cout << err << "\t Reason: Failed to parse array definition for '" << arg << "\n";
	}
	else if (arg.find("(") != string::npos && arg.find("(") != string::npos)
	{
		// Uh oh. It's a function
		int open = arg.find("(");
		int close = arg.find(")");
		string params = trimSpaces(arg.substr(open, close));
		if (close != open+1 && params.length())
		{
			cout << err << "\t Reason: function call '" << arg << "' has parameters\n";
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
							cout << err << "\t Reason: Failed to parse return value for '" << arg << "' on line " << lineNum << "\n";
							break;
						}
						line = trimSpaces(line.substr(0, semi));
						if (line.find("\"") == string::npos)
						{
							cout << err << "\t Reason: Failed to parse return value for '" << arg << "' on line " << lineNum << "\n";
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
		cout << err << "\t Reason: Failed to parse function definition for '" << arg << "\n";
		return ret;
	}
	else // must be a variable
	{
		bool valid_var_name = true;
		for (int i = 0; i < arg.length(); i++)
		{
			if (!isLetter(arg[i]) && !isNumeric(arg[i]) && arg[i] != '_')
			{
				valid_var_name = false;
				break;
			}
		}
		if (!valid_var_name)
		{
			cout << err << "\t Reason: argument '" << arg << "' does not appear to be a string, variable, or function\n";
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
		cout << err << "\t Reason: Failed to parse variable assignment for '" << arg << "\n";
	}

	return ret;
}

vector<string> get_script_dependencies(string fname)
{
	vector<string> resources;

	string folder = "scripts/maps/";

	int idir = fname.find_last_of("/\\");
	if (idir != string::npos && idir > folder.length())
		folder = fname.substr(0, idir) + '/';

	string trace = fname;

	ifstream myfile(fname);
	if (myfile.is_open())
	{
		int lineNum = 0;
		while ( !myfile.eof() )
		{
			string line;
			getline (myfile,line);
			lineNum++;

			line = trimSpaces(line);
			if (line.find("//") == 0)
				continue;

			line = replaceChar(line, '\t', ' ' );
			
			if (line.find("#include") == 0) 
			{
				string include = normalize_path(folder + readQuote(line) + ".as");
				push_unique(resources, include);
				insert_unique(get_script_dependencies(include), resources);
			}

			{
				string s = line;
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
						val = normalize_path("sound/" + val);
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
			if (line.find("g_ItemRegistry.RegisterWeapon(") != string::npos)
			{
				string err = "ERROR: Failed to parse custom weapon definition in " + fname + " (line " + to_string(lineNum) + ")\n";
				line = line.substr(line.find("g_ItemRegistry.RegisterWeapon(") + string("g_ItemRegistry.RegisterWeapon(").length());
				int comma = line.find_first_of(",");
				if (comma >= line.length()-1)
				{
					cout << err << "\t Reason: couldn't find all arguments on this line\n";
					continue;
				}
				string arg1 = trimSpaces(line.substr(0, comma));

				line = line.substr(comma+1);
				int comma2 = line.find_first_of(",");
				if (comma2 == string::npos)
					comma2 = line.find_last_of(")"); // third arg is optional
				if (comma2 >= line.length()-1)
				{
					cout << err << "\t Reason: couldn't find all arguments on this line\n";
					continue;
				}
				string arg2 = trimSpaces(line.substr(0, comma2));

				vector<string> ret1 = parse_script_arg(arg1, fname, err);
				vector<string> ret2 = parse_script_arg(arg2, fname, err);

				if (ret1.size() == 0 || ret2.size() == 0)
					continue;

				if (ret1.size() > 1 || ret2.size() > 1)
				{
					cout << err << "\t Reason: array argument(s) with variable index not handled\n";
					continue;
				}

				arg1 = ret1[0];
				arg2 = ret2[0];

				string hud_file = "sprites/" + arg2 + "/" + arg1 + ".txt";
				trace_missing_file(hud_file, trace, true);
				push_unique(resources, hud_file);

				string hud_path = hud_file;
				if (contentExists(hud_path))
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

							line = trimSpaces(line);
							if (line.find("//") == 0)
								continue;

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
	else
		cout << "Failed to open: " << fname << endl;
	myfile.close();

	return resources;
}

string normalize_path(string s)
{
	s = replaceChar(s, '\\', '/');
	vector<string> parts = splitString(s, "/");
	for (int i = 0; i < parts.size(); i++)
	{
		if (parts[i] == "..")
		{
			if (i == 0)
			{
				cout << "Failed to normalize path: " << s << endl;
				return s;
			}
			parts.erase(parts.begin() + i);
			parts.erase(parts.begin() + (i-1));
			i -= 2;
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

void push_unique(vector<string>& list, string val)
{
	if (is_unique(list, val))
		list.push_back(val);
}

bool is_unique(vector<string>& list, string val)
{
	string val_lower = toLowerCase(val);
	for (int i = 0; i < list.size(); i++)
		if (toLowerCase(list[i]) == val_lower)
			return false;
	return true;
}

bool matchStr(const string& str, const string& str2)
{
    if (str.length() == str2.length())
    {
        for (int i = 0, len = (int)str.length(); i < len; i++)
        {
            char l1 = str[i];
            char l2 = str2[i];
            if (l1 != l2 && !matchLetter(l1, l2))
                return false;
        }
    }
    else
        return false;
    return true;
}

bool matchLetter(char l1, char l2)
{
    if (isLetter(l1) && isLetter(l2))
    {
        if (l1 == l2 || l1 + 32 == l2 || l1 - 32 == l2)
            return true;
    }
    //println("matchLetter: Both chars aren't letters!");
    return false;
}

bool matchStrCase(const string& str, const string& str2)
{
    if (str.length() == str2.length())
    {
        for (int i = 0; i < (int)str.length(); i++)
        {
            if (str.at(i) != str2.at(i))
                return false;
        }
    }
    else
        return false;
    return true;
}

bool isLetter(char c)
{
    return ( c >= 65 && c <= 90) || (c >= 97 && c <= 122);
}

bool isCapitalLetter(char c)
{
	return c >= 65 && c <= 90;
}

bool isNumeric(char c)
{
    if (c >= 48 && c <= 57)
        return true;
    return false;
}

bool isNumber(const string& str)
{
    for (int i = 0 ; i < (int)str.length(); i++)
    {
        if (i == 0 && str.at(0) == '-')
            continue;
        if (!isNumeric(str.at(i)))
            return false;
    }
    return true;
}

bool contains(const string& str, char c)
{
	for (size_t i = 0, size = str.size(); i < size; i++)
	{
		if (str.at(i) == c)
			return true;
	}
	return false;
}

string toLowerCase(string str)
{
	transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}

bool fileExists(string& file, bool fix_path)
{
	/*
	if (FILE *f = fopen(file.c_str(), "r")) {
		fclose(f);
		return true;
	} else {
		return false;
	}
	*/

	if (FILE *f = fopen(file.c_str(), "r")) {
		fclose(f);
		return true;
	}

#if defined(WIN32) || defined(_WIN32)
	return false;
#else
	if (case_sensitive_mode)
		return false;

	// check each folder in the path for correct capitilization
	string path = ".";
	string filename = file;
	if (file.find_first_of("/") != string::npos)
	{
		filename = file.substr(file.find_last_of("/")+1);
		path = file.substr(0, file.find_last_of("/"));
		vector<string> dirs = splitString(path, "/");
		path = ".";
		for (int i = 0; i < dirs.size(); i++)
		{
			// try exact match
			string targetDir = path + "/" + dirs[i];
			DIR *dir = opendir(targetDir.c_str());
			if (dir)
			{
				path += "/" + dirs[i];
				closedir(dir);
				continue;
			}
			
			// check other capitalizations
			dir = opendir(path.c_str());
			if (!dir)
				return false; // shouldn't ever happen

			string lowerDir = toLowerCase(dirs[i]);
			bool found = false;
			while(true)
			{
				dirent *entry = readdir(dir);
		
				if(!entry)
					break;
		
				if(entry->d_type != DT_DIR)
					continue;
		
				string name = string(entry->d_name);
				string lowerName = toLowerCase(name);
		
				if (lowerName.compare(lowerDir) == 0)
				{
					path += "/" + name;
					found = true;
					break;
				}
			}
			closedir(dir);
			if (!found)
				return false; // containing folder doesn't exist
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
		
		if(!entry)
			break;
		
		if(entry->d_type == DT_DIR)
			continue;
		
		string name = string(entry->d_name);
		string lowerName = toLowerCase(name);
		
		if (lowerName.compare(lowerFile) == 0)
		{
			
			if (fix_path)
			{
				string oldFile = file;
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

	const int numContentDirs = 5;
	const char * contentDirs[numContentDirs] = {"svencoop_hd", "svencoop_addon", "svencoop_downloads", "svencoop", "valve"};
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

	results = getDirFiles(dir, fname + "*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../svencoop_hd/" + dir, fname + "*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../svencoop_addon/" + dir, fname + "*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../svencoop_downloads/" + dir, fname + "*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../svencoop/" + dir, fname + "*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../valve/" + dir, fname + "*");
	if (results.size()) return get_ext(results[0]);

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

string getPath(const string& file)
{
    int index = file.find_last_of('/');

    if (index == -1)
        return "";
    else if ((int) file.length() > index)
        return getSubStr(file, 0, index + 1);
    else
        return file;
}

bool hasUppercaseLetters( const string& str )
{
	for (int i = 0; i < str.length(); i++)
		if (isCapitalLetter(str[i]))
			return true;
	return false;
}

double readDouble(const string& line, int dir)
{
    int numBegin = -1;
    int numEnd = -1;
    bool hasPoint = false;

    for (int i = 0; i < (int)line.length(); i++)
    {
        char c;
        if (dir == FROM_START)
            c = line.at(i);
        else
            c = line.at(line.length() - 1 - i);

        if (numBegin == -1)
        {
            if (isNumeric(c) || (dir == FROM_START && c == '-') )
                numBegin = i;
        }
        else
        {
            if (dir == FROM_START && !hasPoint && c == '.')
                hasPoint = true;
            else if ( !isNumeric(c) )
            {
                numEnd = i;
                break;
            }
        }
    }
    if (numEnd == -1)
        numEnd = line.length();
    if (dir == FROM_END)
    {
        int temp = numEnd;
        numEnd = line.length() - numBegin;
        numBegin = line.length() - temp;
    }

    if (numBegin != -1 && numEnd != -1)
        return atof(getSubStr(line, numBegin, numEnd).c_str());
    else
        cout << "readDouble: No digits were found in '" << line << "'\n";

    return 0;
}

int readInt(const string& line, int dir)
{
    int numBegin = -1;
    int numEnd = -1;

    for (int i = 0; i < (int)line.length(); i++)
    {
        char c;
        if (dir == FROM_START)
            c = line.at(i);
        else
            c = line.at(line.length() - 1 - i);
        if (numBegin == -1)
        {
            if (isNumeric(c) || (dir == FROM_START && c == '-') )
                numBegin = i;
        }
        else
        {
            if (dir == FROM_END && c == '-')
            {
                numEnd = i + 1;
                break;
            }
            if ( !isNumeric(c) )
            {
                numEnd = i;
                break;
            }
        }
    }
    if (numEnd == -1)
        numEnd = line.length();
    if (dir == FROM_END)
    {
        int temp = numEnd;
        numEnd = line.length() - numBegin;
        numBegin = line.length() - temp;
    }

    if (numBegin != -1 && numEnd != -1)
        return atoi(getSubStr(line, numBegin, numEnd).c_str());
    else
        cout << "readInt: No digits were found in '" << line << "'\n";

    return 0;
}

string readQuote(const string& str)
{
    int begin = -1;
    int end = -1;

    for (int i = 0; i < (int)str.length(); i++)
    {
        char c = str.at(i);
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
        return getSubStr(str, begin, end);
   // else
   //     cout << "readQuote: Could not find quote in '" << str << "'\n";

    return "";
}

/*
double toRadians(double deg)
{
    return deg*(PI/180.0);
}

double toDegrees(double rad)
{
    return rad*(180.0/PI);
}
*/

int ceilPow2( int value )
{
    int pow = 1;
    for (int i = 0; i < 32; i++) // 32 bits to shift through
    {
        if (value <= pow)
            return pow;
        pow = (pow << 1);
    }
    cout << "No power of two exists greater than " << value << " as an integer\n";
    return -1;
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
			!matchStr(FindFileData.cFileName, ".") && 
			!matchStr(FindFileData.cFileName, ".."))
			results.push_back(FindFileData.cFileName);

		while (FindNextFile(hFind, &FindFileData) != 0)
		{
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && 
				!matchStr(FindFileData.cFileName, ".") && 
				!matchStr(FindFileData.cFileName, ".."))
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
           !matchStr(name, ".") &&
           !matchStr(name, ".."))
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
		cout << "file does not exist " << file << endl;
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

DateTime DateTime::now()
{
    #if defined(WIN32) || defined(_WIN32)
	SYSTEMTIME t;
	GetLocalTime(&t);
	return DateTime(t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    #else
    time_t now = time(NULL);
    tm *data = localtime(&now);
    return DateTime(data->tm_year, data->tm_mon, data->tm_mday, data->tm_hour, data->tm_min, data->tm_sec);
    #endif
}

string DateTime::str()
{
    #if defined(WIN32) || defined(_WIN32)
	TIME_ZONE_INFORMATION tz;
	GetTimeZoneInformation(&tz);
	string zone = to_string(-tz.Bias / 60);
	if (-tz.Bias >= 0)
		zone = "+" + zone;
	zone = " (UTC" + zone + ")";

	string suffix = "AM";
	int s_hour = hour;
	string s_min = minute < 10 ? ("0" + ::to_string(minute)) : ::to_string(minute);
	string s_sec = second < 10 ? ("0" + ::to_string(second)) : ::to_string(second);
	if (hour > 12)
	{
		s_hour -= 12;
		suffix = "PM";
	}
	return ::to_string(year) + "/" + ::to_string(month) + "/" + ::to_string(day) + " " + ::to_string(s_hour) + ":" + s_min + " " + suffix + " " + zone;
    #else
    char buffer[256];
    time_t now = time(NULL);
    tm *data = localtime(&now);
    data->tm_year = year;
    data->tm_mon = month;
    data->tm_mday = day;
    data->tm_hour = hour;
    data->tm_min = minute;
    data->tm_sec = second;
    
    strftime(buffer, 256, "%Y/%m/%d %I:%M %p (UTC %z)", data);
    
    return string(buffer);
    #endif
}

string DateTime::compact_str()
{
    #if defined(WIN32) || defined(_WIN32)
	string s_min = minute < 10 ? ("0" + ::to_string(minute)) : ::to_string(minute);
	string s_hour = hour < 10 ? ("0" + ::to_string(hour)) : ::to_string(hour);
	string s_day = day < 10 ? ("0" + ::to_string(day)) : ::to_string(day);
	string s_month = month < 10 ? ("0" + ::to_string(month)) : ::to_string(month);
	string s_year = year < 10 ? ("0" + ::to_string(year % 100)) : ::to_string(year % 100);

	return s_year + s_month + s_day + s_hour + s_min;
    #else
    char buffer[256];
    time_t now = time(NULL);
    tm *data = localtime(&now);
    data->tm_year = year;
    data->tm_mon = month;
    data->tm_mday = day;
    data->tm_hour = hour;
    data->tm_min = minute;
    data->tm_sec = second;
    
    strftime(buffer, 256, "%y%m%d%H%M", data);
    
    return string(buffer);
    #endif
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

// no slashes allowed at the end of start_dir
string relative_path_to_absolute(string start_dir, string path)
{
	int up = path.find("../");
	if (up == string::npos)
		return start_dir + "/" + path;
	
	while (up != string::npos)
	{
		int up_dir = start_dir.find_last_of("\\/");
		if (up_dir == string::npos)
		{
			if (start_dir.length())
			{
				up_dir = 0;
				start_dir = "";
			}
			else
			{
				cout << "Could not convert '" << path << "' to absolute path using root dir: " << start_dir << endl; 
				return start_dir + "/" + path;
			}
		}
		if (up > 0) // some crazy person went back down a directory before going up
		{
			start_dir += getSubStr(path, 0, up-1);
			path = getSubStr(path, up);
			up = 0;
		}
		if (up_dir > 0)
			start_dir = getSubStr(start_dir, 0, up_dir);
		path = getSubStr(path, 3);
		up = path.find("../");
	}
	if (start_dir.length())
		return start_dir + "/" + path;	
	else
		return path;
}

void insert_unique(const vector<string>& insert, vector<string>& insert_into)
{
	for (uint i = 0; i < insert.size(); ++i)
	{
		bool exists = false;
		for (uint k = 0; k < insert_into.size(); ++k)
		{
			if (matchStr(insert_into[k], insert[i]))
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
	string copy = str;
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

string base36(int num)
{
	string b36;

	while (num)
	{
		b36 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[num % 36] + b36;
		num /= 36; 
	}
	return b36;
}

void sleepMsecs(int msecs)
{
    #if defined(WIN32) || defined(_WIN32)
        Sleep(msecs);
    #else
        struct timespec spec;
        spec.tv_sec = msecs / 1000;
        spec.tv_nsec = 1000000 * (msecs % 1000);
        
        nanosleep(&spec, NULL);
    #endif
}
