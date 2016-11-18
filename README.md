# resguy
Like RESGen, but accounts for all content that Sven Co-op maps can customize, as well as some linked files that the old program didn't find:

- External texture models and animation models (e.g. scientistT.mdl + scientist01.mdl)
- Sounds played in model animations (events 5004/1004/1008)
- Sound files with the following extensions:
	aiff, asf, asx, dls, flac, fsb, it, m3u, midi, mid, mod, 
	mp2, pls, s3m, vag, wax, wma, xm, wav, ogg, mp3, au
- Sound replacment files and their listed content for specific monsters ("soundlist" entity key)
- Monster/sequence sentence keys (e.g. "+barney/yessir.wav")
- Global model/sound replacement files and their listed content (specified in the BSP or CFG)
- Custom sentences and their listed sounds (specified in the BSP or CFG)
- Detail textures file and the .tga files listed inside.
- map_script file and all scripts it #include's
- Player models w/ preview images specified in the "Force Player Models" map settings
- Custom materials file
- MOTD file

# Usage:

Place resguy.exe and default_content.txt in a content folder (svencoop_addon, svencoop_downloads, svencoop_hd, or svencoop) and run it from the command line:

### resguy.exe [filename] [options]

[filename] can be the name of a map ("stadium3" or "stadium3.bsp"), or a search string ("stadium*").

The map will be searched for in the "maps" folder in the same location as the .exe, but it will also search the other content directories when something isn't found. 

For example, if you place the program in "svencoop" but the maps are in "svencoop_addon" and the sounds are in "svencoop_downloads", everything will still be found.

### Available Options:

**-test** = Don't write the .res file, just check if there are any missing files

**-allrefs** = List all references for missing files (normally clipped to 5)

**-printdefault** = Print all content that was skipped because it was included in default_content.txt

## default_content.txt

This lists all content that is included with a fresh install of Sven Co-op. Usually maps should omit these files from their map package, but content that was once considered default may be removed from the mod in the future. Maps that relied on content that was removed will need to be repacked with an updated list.

This file should be updated every time the game updates, unless no content was added/removed.
