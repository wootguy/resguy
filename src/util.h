#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unordered_map>

#ifdef _WIN32
#include <string.h> 
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

using namespace std;

typedef unsigned int uint;
typedef unsigned long long uint64;
typedef unsigned char byte;

extern vector<string> printlog;

struct vec3 {
	float x, y, z;
};

enum read_dir
{
    FROM_START, // parse the string from beginning to end (normal)
    FROM_END	// parse the string end to beginning (backwards)
};

typedef unordered_map< string, vector<string> > str_map_vector;

extern str_map_vector g_tracemap_req;
extern str_map_vector g_tracemap_opt;

// if file is missing, log where it was used
void trace_missing_file(string file, string reference, bool required);

string replaceChar(string s, char c, char with);

void trace_missing_file(str_map_vector trace_type, string file, string reference);

// prints to screen and logs to file
void log(string s, bool print_to_screen=true);

// creates a new log file
void log_init(string log_file_path);

void log_close();

// parses a sound/model replacement file and returns all unique resources on the right-hand side
vector<string> get_replacement_file_resources(string fname);

vector<string> get_sentence_file_resources(string fname);

// parses script for includes of other scripts
vector<string> get_script_dependencies(string fname, vector<string>& searchedScripts);

// removes '..' from relative paths and replaces all \ slashes with /
string normalize_path(string s);

// pushes a string into the vector only if it's not already in the list (case insensitive)
bool push_unique(vector<string>& list, string val);

// check if val is in the list (case insensitive)
bool is_unique(vector<string>& list, string val);

string toLowerCase(string str);

/*
	Returns true if the file could be found.
	file - absolute path to the file (Ex: "C:\Project\thing.png")
	fixPath - Update file param if the incorrect case was used (Linux only)
*/
bool fileExists(string& file, bool fix_path=false, string from_path=".", int from_skip=0);

// searches all content folders (current dir + ../svencoop + ../svencoop_downloads + ../svencoop_hd etc.)
// file is set to the first path where the file is found
bool contentExists(string& file, bool fix_path);

// given a filename without an extension, search in a sound dir (for all content dirs) for any files with the same name
// (used with custom sentences since extensions are omitted but are important)
string find_content_ext(string fname, string dir);

// get extension for filename
string get_ext(string fname);

/*
	Extract a message surrounded by quotes ("") from a string
	str - string to read
*/
string readQuote(const string& str);

// convert / to \ in the pathname
void winPath(string& path);

vector<string> getDirFiles(string path, string searchStr, string startswith="");

vector<string> getSubdirs(string path);

char * loadFile( string file );

void recurseSubdirs(std::string path, vector<std::string>& dirs);

vector<std::string> getAllSubdirs(std::string path);

void insert_unique(const vector<string>& insert, vector<string>& insert_into);

vector<string> splitString( string str, const char * delimitters);

string trimSpaces(string str); // remove spaces at the beginning and end of the string

bool dirExists(const string& path);

uint64 getSystemTime();
