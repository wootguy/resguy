Resguy is similar to RESgen and FLRGen, but can find all content that Sven Co-op maps are allowed to customize, and only includes what is required. It can also find dependencies between files, like T models and model sounds. This is what Resguy will write in your .res files:

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
- CFG files (Note: SC does not even allow these to be transfered to clients)
- Custom sentence files
- Custom materials file
- map_script file and all other scripts that it #includes
- Global model/sound replacement files
- Per-monster sound replacment files

With RESgen, only .wav sounds were found in maps. Sven Co-op uses FMOD which supports a large variety of file types. These are the types of sounds Resguy will find:

- aiff, asf, asx, au, dls, flac, fsb, it, m3u, mid, midi, mod, mp2, mp3, ogg, pls, s3m, vag, wav, wax, wma, xm

# Usage:

Place resguy and default_content.txt into a content folder (e.g. svencoop, svencoop_addon, or something you made) and click to run it. It will ask you for a map name to generate the .res for (Note: You can type "&ast;" to target all maps or "stadium&ast;" to target all maps with a name that starts with "stadium").

Maps will be searched for in the "maps" folder in the same location as the program, but it will also search the other content directories if it isn't found. The same goes for models/sounds/etc. 

For example, if you place the program in "svencoop" but the maps are in "svencoop_addon" and the sounds are in "svencoop_downloads", everything should still be found.

If you run resguy from the command line or a script, use the following syntax:

### resguy [filename] [options]

__[filename]__ can be the name of a map ("stadium3" or "stadium3.bsp"), or a search string ("stadium&ast;").

### Available Options:

**-test** = Don't write any .res files, just check for problems.

**-allrefs** = List all references for missing files (normally clipped to 3)

**-printskip** = Print content that was skipped because it was invalid, unused, optional, or listed in default_content.txt

**-extra** = Include server-specific files in .res file

**-extra2** = Write server-specific files to a separate .res2 file

**-missing3** = Write missing files to a separate .res3 file

## default_content.txt

This lists all content that is included with a fresh install of Sven Co-op. Maps should exclude these files from their map package, and this list should be updated every time the game updates.*

To update the default content list, place resguy in your "/common/Sven Co-op/svencoop/" folder and run this command:

__resguy -gend__

Maps will need to be repackaged if they rely on default content that was removed from the game.

If for some reason you want to include default content in your .res file, delete default_content.txt.

&ast;The goal of this file is to prevent people from overwriting default content with their own crap. If you're including default files just to future-proof your map package, there's still a chance that the game will update those files. At that point, you're distributing old versions of files and possibly breaking the game for people.
