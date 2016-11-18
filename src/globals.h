#pragma once
#include "util.h"

// all valid sound file extensions (everything FMOD supports)
#define NUM_SOUND_EXTS 22
char * g_sound_exts[NUM_SOUND_EXTS] = {
	"aiff", "asf", "asx", "dls", "flac", 
	"fsb", "it", "m3u", "midi", "mid", 
	"mod", "mp2", "pls", "s3m", "vag", 
	"wax", "wma", "xm", "wav", "ogg", 
	"mp3", "au",
};