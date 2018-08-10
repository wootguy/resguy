#pragma once
#include "util.h"

// all valid file extensions (should include everything FMOD supports)
#define NUM_SOUND_EXTS 17
#define NUM_VALID_EXTS (11 + NUM_SOUND_EXTS)
static const char * g_valid_exts[NUM_VALID_EXTS] = {
	// sound files
	"asf", "asx", "dls", "fsb",  "m3u", "mid", "mod", "mp2", "pls", "s3m", 
	"vag", "wax", "wma", "wav", "ogg", "mp3", "au",

	// Blacklisted in sven (but shouldn't be):
	// "aiff", "flac", "it", "midi", "xm",

	// everything else
	"mdl", "spr", "res", "txt", "tga", 
	"bmp", "conf", "gmr", "gsr", "wad", 
	"as"
};

extern str_map_vector default_wads;
extern vector<string> server_files; // files used by server but not needed by client
extern vector<string> default_content; // files included with the base game
extern vector<string> parsed_scripts;
extern int unused_wads;
extern bool client_files_only;
extern bool print_skip;
extern bool case_sensitive_mode;
extern bool log_enabled;