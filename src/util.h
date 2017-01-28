#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unordered_map>

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

/*
	Extract a section from a string
	str - string to extract from
	beginIndex - index to start at (inclusive)
	endIndex - index to stop at (exclusive)
*/
string getSubStr(const string& str, int beginIndex, int endIndex);

string getSubStr(const string& str, int beginIndex);

string replaceChar(string s, char c, char with);

void trace_missing_file(str_map_vector trace_type, string file, string reference);

// parses a sound/model replacement file and returns all unique resources on the right-hand side
vector<string> get_replacement_file_resources(string fname);

vector<string> get_sentence_file_resources(string fname);

// parses script for includes of other scripts
vector<string> get_script_dependencies(string fname);

// removes '..' from relative paths and replaces all \ slashes with /
string normalize_path(string s);

// pushes a string into the vector only if it's not already in the list (case insensitive)
void push_unique(vector<string>& list, string val);

// check if val is in the list (case insensitive)
bool is_unique(vector<string>& list, string val);

/*
	Check if two strings are idential (case insensitive)
	str - a string
	str2 - another string
*/
bool matchStr(const string& str, const string& str2);

/*
	True if two letters match (case insensitive). If non-letters are used, then
	this function will retrun false.
	l1 - a letter
	l2 - another letter
*/
bool matchLetter(char l1, char l2);

/*
	Check if two strings are identical (case sensitive)
	str - a string
	str2 - another string
*/
bool matchStrCase(const string& str, const string& str2);


// True if the character passed is a letter (a-z, A-Z)
bool isLetter(char c);

bool isCapitalLetter(char c);

// True if the character passed is a number (0-9)
bool isNumeric(char c);

// True if the string passed is a number. Negative values are allowed (Ex: "-9")
bool isNumber(const string& str);

// Whether or not the string contains the specified character
bool contains(const string& str, char c);

string toLowerCase(string str);

/*
	Returns true if the file could be found.
	file - absolute path to the file (Ex: "C:\Project\thing.png")
	fixPath - Update file param if the incorrect case was used (Linux only)
*/
bool fileExists(string& file, bool fix_path=false);

// searches all content folders (current dir + ../svencoop + ../svencoop_downloads + ../svencoop_hd etc.)
// file is set to the first path where the file is found
bool contentExists(string& file, bool fix_path=true);

// given a filename without an extension, search in a sound dir (for all content dirs) for any files with the same name
// (used with custom sentences since extensions are omitted but are important)
string find_content_ext(string fname, string dir);

// get extension for filename
string get_ext(string fname);

// Return the absoulte path the file, excluding the file's name
// Ex: "C:\Project\thing.jpg" -> "C:\Project"
string getPath(const string& file);

// returns true if any characters in the string are uppercase letters
bool hasUppercaseLetters(const string& str);

/*
	Extract a double value in the string. If none can be found, 0 is returned.
	str - string to read
	dir - which direction to read (FROM_START, FROM_END)
*/
double readDouble(const string& str, int dir);

/*
	Extract an integer value in the string. If none can be found, 0 is returned.
	str - string to read
	dir - which direction to read (FROM_START, FROM_END)
*/
int readInt(const string& str, int dir);


/*
	Extract a message surrounded by quotes ("") from a string
	str - string to read
*/
string readQuote(const string& str);

// convert a value in degrees to radians.
double toRadians(double deg);

// convert a value in radians to degrees.
double toDegrees(double rad);

// returns the value rounded up to the nearest power of two
int ceilPow2(int value);

// convert / to \ in the pathname
void winPath(string& path);

vector<string> getDirFiles(string path, string searchStr, string startswith="");

vector<string> getSubdirs(string path);

char * loadFile( string file );

void recurseSubdirs(std::string path, vector<std::string>& dirs);

vector<std::string> getAllSubdirs(std::string path);

string relative_path_to_absolute(string start_dir, string path);

void insert_unique(const vector<string>& insert, vector<string>& insert_into);

vector<string> splitString( string str, const char * delimitters);

string trimSpaces(string str); // remove spaces at the beginning and end of the string

bool dirExists(const string& path);

uint64 getSystemTime();

string base36(int num);

void sleepMsecs(int msecs);

struct DateTime
{
	int year, month, day;
	int hour, minute, second;

	DateTime() : year(0),  month(0), day(0), hour(0), minute(0), second(0) {}
	DateTime(int year, int month, int day, int hour, int minute, int second) 
	: year(year),  month(month), day(day), hour(hour), minute(minute), second(second) {}
	
	static DateTime now();
	string str();
	string compact_str();
};
