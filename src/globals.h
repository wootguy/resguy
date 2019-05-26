#pragma once
#include "util.h"

// all valid file extensions (should include everything FMOD supports)
#define NUM_SOUND_EXTS 22
#define NUM_VALID_EXTS (11 + NUM_SOUND_EXTS)
static const char * g_valid_exts[NUM_VALID_EXTS] = {
	// sound files
	"asf", "asx", "dls", "fsb",  "m3u", "mid", "mod", "mp2", "pls", "s3m", 
	"vag", "wax", "wma", "wav", "ogg", "mp3", "au",
	"aiff", "flac", "it", "midi", "xm",

	// everything else
	"mdl", "spr", "res", "txt", "tga", 
	"bmp", "conf", "gmr", "gsr", "wad", 
	"as"
};

extern str_map_set default_wads;
extern set_icase server_files; // files used by server but not needed by client
extern set_icase script_files; // files precached by scripts aren't needed in .res files.
extern set<string, InsensitiveCompare> default_content; // files included with the base game
extern set<string> parsed_scripts;
extern int unused_wads;
extern bool client_files_only;
extern bool ignore_script_files; // ignore files precached in scripts
extern bool print_skip;
extern bool case_sensitive_mode;
extern bool log_enabled;