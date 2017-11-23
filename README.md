Resguy is similar to RESgen and FLRGen, but can find all content that Sven Co-op maps are allowed to customize, and only includes what is required. It can also find dependencies between files, like T models and model sounds. This is what Resguy will write in your .res files:

- Skybox .tga files
- Wads that are actually used
- Detail textures file and the .tga files listed inside.
- Models/sprites/sounds used in map entities
- Models/sprites/sounds used in map scripts (error prone)
- HUD files and sprites used by custom weapons defined in map scripts (even more error prone)
- HUD files and sprites set in the "CustomSpriteDir" key for weapon_* entities
- External texture models and animation models (e.g. scientistT.mdl + scientist01.mdl)
- Custom muzzle flash configs and sprites used in model animations (event 5005)
- Sounds played in model animations (events 5004/1004/1008)
- Sounds used in entity sentence keys (e.g. "UseSentence" = "+barney/yessir.wav")
- Sounds listed in per-monster sound replacment files (the "soundlist" entity key)
- Sounds listed in custom sentence files
- Sounds listed in Global Sound Replacement files
- Models listed in Global Model Replacement files
- Player models and preview images for the names listed in the "forcepmodels" setting

Resguy will find and process the following files but __NOT__ include them in the .res file. These files are excluded because clients don't need them, and some can't be sent to clients anyway. You can include these with "-extra" if you really want to:

- BSP file (included with -extra2, but not -extra)
- MOTD file
- CFG files (SC does not allow .cfg files to be transfered to clients)
- map_script file with all its #includes (.as files are also not allowed to be transfered to clients)
- Custom sentence files
- Custom materials file
- Global model/sound replacement files
- Per-monster sound replacment files

Sven Co-op uses FMOD which supports a large variety of audio formats. These are the types of sounds resguy will find:

- aiff, asf, asx, au, dls, flac, fsb, it, m3u, mid, midi, mod, mp2, mp3, ogg, pls, s3m, vag, wav, wax, wma, xm

# Usage:

Place resguy and resguy_default_content.txt into a content folder (e.g. svencoop, svencoop_addon, or something you made) and click to run it. This will start an interactive mode where you can select maps and options.

Maps will be searched for in the "maps" folder in the same location as the program, but also in the other content directories if something isn't found.

For example, if you place the program in "svencoop" but the maps are in "svencoop_addon" and the sounds are in "svencoop_downloads", everything should still be found.

If you run resguy from the command line or a script, use this syntax:

### resguy [filename] [options]

__[filename]__ can be the name of a map ("stadium3" or "stadium3.bsp"), or a search string ("stadium&ast;").

### Available Options:

**-test** = Don't write any .res files, just check for problems.

**-allrefs** = List all references for missing files (normally clipped to 3)

**-printskip** = Log content that was skipped because it was invalid, unused, optional, or listed in resguy_default_content.txt

**-extra** = Write server files to .res file (not recommended&ast;)

**-extra2** = Write server files to a separate .res2 file

**-missing** = Write missing files to .res file (not recommended&ast;&ast;)

**-missing3** = Write missing files to a separate .res3 file

**-series** = Write the same files into every .res file (includes BSPs for other maps in the series)

**-log** = Log output to mapname_resguy.log

**-icase** = Case-insensitive mode (Linux only)

**-7z[0-9]** = Create a 7-Zip archive from the selected maps. The compression level is optional (default = 9).

**-zip[0-1]** = Create a Zip archive from the selected maps. The compression level is optional (default = 1).

([7-Zip](http://www.7-zip.org/download.html) is required for **-7z** and **-zip**. If you have a 64-bit version for Windows then you'll need the 64-bit version of Resguy.)

&ast; I know people (me too until recently) include server files in their .res files hoping that clients will have everything needed to host their own server, but that doesn't quite work because .cfg and .as files are blacklisted. Unless your map doesn't use a .cfg (very unlikely), you're just wasting time and disk space for every player that connects to your server.

&ast;&ast; Some missing files may be linked to yet more files, which means your .res file would still be incomplete. Only use this if you know the missing files aren't linked to anything else (sounds, sprites, WADs, and bitmaps are safe).

**Note for Linux Users:**
The wildcard character "&ast;" will be interpreted as a file glob in the shell, which prevents resguy from working. To work around this, escape the character with a slash (e.g. "./resguy stadium\\&ast; -test") or run "set -f" before a resguy command.

## resguy_default_content.txt

This lists all content that is included with a fresh install of Sven Co-op. Maps should exclude these files from their map package, and this list should be updated every time the game updates.*

To update the default content list, place resguy in your "/common/Sven Co-op/svencoop/" folder and run this command:

__resguy -gend__

Maps will need to be repackaged if they rely on default content that was removed from the game.

If for some reason you want to include default content in your .res file, delete resguy_default_content.txt.

&ast;The goal of this file is to prevent people from overwriting default content with their own crap. If you're including default files just to future-proof your map package, there's still a chance that the game will update those files. At that point, you're distributing old versions of files and possibly breaking the game for people.

# How to build the source

### Linux users:
```
git clone https://github.com/wootguy/resguy.git
cd resguy
mkdir build; cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE
make
```

### Windows users:
1. Install CMake and Visual Studio
2. Download and extract the source somewhere
3. Open a command prompt in the source folder (one level above "src") and run these commands:
```
md build & cd build
cmake ..
cmake --build . --config Release
```
