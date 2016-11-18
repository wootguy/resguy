#include "Util.h"
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <string.h>

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

string getWorkDir()
{
    char cCurrentPath[FILENAME_MAX];

    #ifdef OS_WIN
        if (!GetCurrentDir(cCurrentPath, sizeof(cCurrentPath) / sizeof(TCHAR)))
        {
            return "ERROR: Could not determine working directory";
        }
    #else
        getcwd(cCurrentPath, sizeof(cCurrentPath));
    #endif

    cCurrentPath[sizeof(cCurrentPath) - 1] = '\0'; /* not really required */
    string path = cCurrentPath;
    int index = -1;
    while (( index = path.find('\\')) != -1)
        path[index] = '/';
    return path + "/";
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
			if (line.find("//") == 0 || line.find_first_of("\"") == string::npos)
				continue;

			vector<string> quotes = splitString(line, "\"");
			if (quotes.size() < 3)
				continue;

			push_unique(resources, normalize_path(quotes[2]));
		}
	}
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
	myfile.close();

	return resources;
}

vector<string> get_script_dependencies(string fname)
{
	vector<string> resources;

	string folder = "scripts/maps/";
	int idir = fname.find_last_of("/\\");
	if (idir != string::npos)
		folder = folder.substr(0, idir) + '/';

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
			
			if (line.find("#include") == 0) 
			{
				string include = folder + readQuote(line) + ".as";
				push_unique(resources, include);
				get_script_dependencies(include);
			}
		}
	}
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
			i--;
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

bool fileExists(const string& file)
{
    if (FILE *f = fopen(file.c_str(), "r")) {
        fclose(f);
        return true;
    } else {
        return false;
    }   
}

bool contentExists(string& file, bool fix_path)
{
	if (fileExists(file))
		return true;
	if (fileExists("../svencoop_hd/" + file)) {
		if (fix_path) file = "../svencoop_hd/" + file;
		return true;
	}
	if (fileExists("../svencoop_addon/" + file)) {
		if (fix_path) file = "../svencoop_addon/" + file;
		return true;
	}
	if (fileExists("../svencoop_downloads/" + file)) {
		if (fix_path) file = "../svencoop_downloads/" + file;
		return true;
	}
	if (fileExists("../svencoop/" + file)) {
		if (fix_path) file = "../svencoop/" + file;
		return true;
	}
	if (fileExists("../valve/" + file)) {
		if (fix_path) file = "../valve/" + file;
		return true;
	}
	return false;
}

string find_content_ext(string fname, string dir)
{
	vector<string> results;

	results = getDirFiles(dir, fname + ".*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../svencoop_hd/" + dir, fname + ".*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../svencoop_addon/" + dir, fname + ".*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../svencoop_downloads/" + dir, fname + ".*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../svencoop/" + dir, fname + ".*");
	if (results.size()) return get_ext(results[0]);

	results = getDirFiles("../valve/" + dir, fname + ".*");
	if (results.size()) return get_ext(results[0]);

	return "";
}

string get_ext(string fname)
{
	int iext = fname.find_first_of(".");
	if (iext != string::npos && iext < fname.length() - 1)
	{
		return fname.substr(iext+1);
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
    else
        cout << "readQuote: Could not find quote in '" << str << "'\n";

    return 0;
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


vector<string> getDirFiles( string path, string searchStr )
{
    vector<string> results;
    
    #if defined(WIN32) || defined(_WIN32)
	path = path + searchStr;
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
        
        if(extension.size() > name.size())
            continue;
        
        if(std::equal(extension.rbegin(), extension.rend(), lowerName.rbegin()))
            results.push_back(name);
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

string getFilename(string filename)
{
    #if defined(WIN32) || defined(_WIN32)
	vector<string> components;
	string old_filename = filename;
	std::replace( filename.begin(), filename.end(), '/', '\\'); // windows slashes required

	// SHGetFileInfoA only fixes the last part of the path. So we have to make sure each folder is correct, too
	while (filename.length())
	{
		SHFILEINFO info;
		// nuke the path separator so that we get real name of current path component

		int dir = filename.find_last_of("\\");
		string testing = filename;
		if (dir != string::npos)
			testing = getSubStr(filename, dir);

		if (!matchStr(testing, ".."))
		{
			info.szDisplayName[0] = 0;
			// TODO: So uh, this doesn't work if the user has file extensions hidden (which is on by default). That sucks.
			SHGetFileInfoA( filename.c_str(), 0, &info, sizeof(info), SHGFI_DISPLAYNAME );
			components.push_back(info.szDisplayName);  
		}
		else
			components.push_back("..");
		if (dir != string::npos)
			filename = getSubStr(filename, 0, dir);
		else
			break;
	}

	string result;
	for (int i = (int)components.size() - 1; i >= 0; --i)
	{
		result += components[i];
		if (i > 0)
			result += "/";
	}

    return result;
    #else
    //shouldn't do anything on *nix, if I understood the intention of this function correctly
	// w00tguy: Yeah, this was meant to handle the case where mappers used the incorrect case and windows was ok with that.
	//          On *nix, the map will just crash or be missing resources.
    return filename;
    #endif
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
