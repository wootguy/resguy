# resguy
Resguy is similar to RESgen, but can find all content that Sven Co-op maps are allowed to customize. It can also find dependencies between files, like T models and model sounds. Here is what Resguy will write in your .res files:

- Skybox .tga files
- Wads that are actually used
- Detail textures file and the .tga files listed inside.
- Models/sprites/sounds used in map entities
- Models/sprites/sounds used in map scripts (error prone)
- HUD files and sprites used by custom weapons defined in map scripts (even more error prone)
- External texture models and animation models (e.g. scientistT.mdl + scientist01.mdl)
- Custom muzzle flash configs and sprites used in model animations (event 5005)
- Sounds played in model animations (events 5004/1004/1008)
- Sounds used in entity sentence keys (e.g. "UseSentence" = "+barney/yessir.wav")
- Sounds listed in per-monster sound replacment files (the "soundlist" entity key)
- Sounds listed in custom sentence files
- Sounds listed in Global Sound Replacement files
- Models listed in Global Model Replacement files
- Player models and preview images for the names listed in the "forcepmodels" setting

Resguy will find and process the following files but __NOT__ include them in the .res file. The reason for this is because clients don't need them (only the server uses these files). You can include these with "-extra" if you really want to:

- BSP file (client already knows about this)
- MOTD file
- CFG files (Note: SC doesn't even allow these to be transfered to clients)
- Custom sentence files
- Custom materials file
- map_script file and all other scripts that it #includes
- Global model/sound replacement files
- Per-monster sound replacment files

With RESgen, only .wav sounds were found in maps. Sven Co-op uses FMOD which supports a large variety of file type. These are the types of sounds Resguy will find:

- aiff, asf, asx, au, dls, flac, fsb, it, m3u, mid, midi, mod, mp2, mp3, ogg, pls, s3m, vag, wav, wax, wma, xm

# Usage:

Place resguy.exe and default_content.txt into a content folder (e.g. svencoop, svencoop_addon, or something you made) and run it from the command line:

### resguy.exe [filename] [options]

__[filename]__ can be the name of a map ("stadium3" or "stadium3.bsp"), or a search string ("stadium*").

Maps will be searched for in the "maps" folder in the same location as the .exe, but it will also search the other content directories if it isn't found. The same goes for models/sounds/etc. 

For example, if you place the program in "svencoop" but the maps are in "svencoop_addon" and the sounds are in "svencoop_downloads", everything should still be found.

### Available Options:

**-test** = Don't write the .res file, just check if there are any missing files

**-allrefs** = List all references for missing files (normally clipped to 3)

**-printdefault** = Print content that was skipped because it was listed in default_content.txt

**-extra** = Write server-specific files to .res

**-extra2** = Write server-specific files to a separate .res2 file

## default_content.txt

This lists all content that is included with a fresh install of Sven Co-op. Usually maps should omit these files from their map package, but content that was once considered default may be removed from the game in the future. This file should be updated every time the game updates, unless no content was added/removed.

To update the default content list, place Resguy in your "/common/Sven Co-op/svencoop/" folder and run this command:

__resguy.exe !gend__

Maps should be repackaged if they rely on default content that was removed from the game.
