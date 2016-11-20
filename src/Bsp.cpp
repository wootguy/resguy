#include "Bsp.h"
#include "util.h"
#include <algorithm>
#include "globals.h"
#include "Mdl.h"

const int NUM_LUMPS_TO_LOAD = 2;
int lumps_to_load[NUM_LUMPS_TO_LOAD] = {LUMP_ENTITIES, LUMP_TEXTURES};

Bsp::Bsp(std::string mapname)
{
	this->name = mapname;
	valid = false;

	bool exists = true;
	string fname = "maps/" + mapname + ".bsp";
	if (!contentExists(fname)) {
		return;
	}

	if (!load_lumps(fname)) {
		return;
	}

	load_ents();

	valid = true;
}

Bsp::~Bsp()
{
	if (!valid)
		return;
	 
	for (int i = 0; i < NUM_LUMPS_TO_LOAD; i++)
		delete [] lumps[lumps_to_load[i]];
	delete [] lumps;

	for (int i = 0; i < ents.size(); i++)
		delete ents[i];
}

vector<string> Bsp::get_resources()
{
	vector<string> resources;

	push_unique(resources, "maps/" + name + ".bsp");

	string bsp_fname = name + ".bsp";

	Entity * worldSpawn = NULL;

	for (int i = 0; i < ents.size(); i++)
	{
		string cname = toLowerCase(ents[i]->keyvalues["classname"]);
		string tname = ents[i]->keyvalues["targetname"];
		string ent_trace = bsp_fname + " --> " +  "\"" + tname + "\" (" + cname + ")";

		if (cname == "game_text") // possible some text is interpreted as a file extension.
			continue;
		if (cname == "worldspawn") // deal with this later. It has weird values too.
		{
			worldSpawn = ents[i];
			continue;
		}
		for (hashmap::iterator it = ents[i]->keyvalues.begin(); it != ents[i]->keyvalues.end(); ++it)
		{
			string key = toLowerCase(it->first);
			string val = replaceChar(it->second, '\\', '/');

			// support for weapon_custom
			vector<string> vals = splitString(val, ";");
			if (vals.size() > 1)
			{
				val = val[0];
			}
			else
			{
				val.erase(std::remove(val.begin(), val.end(), ';'), val.end());
				vals.clear();
				vals.push_back(val);
			}


			int iext = val.find_last_of(".");
			if (iext == string::npos || iext == val.length()-1)
				continue;

			iext++;

			string ext = toLowerCase(val.substr(iext, val.length() - iext));

			if (ext == "mdl") 
			{
				string model_path = normalize_path(val);

				if (is_unique(resources, model_path))
				{
					Mdl model = Mdl(model_path);
					trace_missing_file(model_path, ent_trace, true);
					push_unique(resources, model_path);
					if (model.valid)
					{
						vector<string> model_res = model.get_resources();
						for (int k = 0; k < model_res.size(); k++)
						{
							trace_missing_file(model_res[k], ent_trace + " --> " + model_path, true);
							push_unique(resources, model_res[k]);
						}
					}
				}
			}
			else if (ext == "spr")
			{
				string spr = normalize_path(val);
				trace_missing_file(spr, ent_trace, true);
				push_unique(resources, spr);
			}
			else if (key == "soundlist" && val.length())
			{
				// make sure file has an extension
				int lastdot = val.find_last_of(".");
				int lastslash = val.find_last_of("/");
				if (lastdot == string::npos || (lastslash != string::npos && lastdot < lastslash))
					continue;

				string res = normalize_path("sound/" + name + "/" + val);
				if (is_unique(resources, res))
				{
					trace_missing_file(res, ent_trace, true);

					resources.push_back(res);
					vector<string> replace_res = get_replacement_file_resources(res);
					for (int k = 0; k < replace_res.size(); k++)
					{
						string snd = "sound/" + replace_res[k];
						trace_missing_file(snd, ent_trace + " --> " + res, true);
						push_unique(resources, snd);
					}
				}
			}
			else
			{
				for (int v = 0; v < vals.size(); v++)
				{
					string sval = vals[v];
					string sext = get_ext(sval);
					for (int s = 0; s < NUM_SOUND_EXTS; s++)
					{
						if (sext == g_valid_exts[s])
						{
							string prefix = "sound/";
							if (key == "usesentence" || key == "unusesentence")
								prefix = "";
							if (key == "sentence" && sval[0] == '+') // honestly idk what the + is for but it looks useless
								sval = sval.substr(1);

							string snd = normalize_path(prefix + sval);
							trace_missing_file(snd, ent_trace, true);
							push_unique(resources, snd);
							break;
						}
					}
				}
			}
		}
	}

	if (worldSpawn) 
	{
		//vector<string> map_textures = get_textures();

		// find wads (TODO: Only include them if the textures are actually used)
		string wadList = worldSpawn->keyvalues["wad"];
		string trace = bsp_fname + " --> \"Map Properties\" (worldspawn)";
		vector<string> wads = splitString(wadList, ";");
		for (int i = 0; i < wads.size(); i++)
		{
			string wadname = wads[i];
			int idir = wadname.find_last_of("\\/");
			if (idir != string::npos && idir < wadname.length()-5) { // need at least 5 chars for a WAD name ("a.wad") 
				wadname = wadname.substr(idir+1);
			}

			trace_missing_file(wadname, bsp_fname + " --> worldspawn wadlist", true);
			push_unique(resources, wadname);
		}

		// find sky
		string sky = worldSpawn->keyvalues["skyname"];
		if (sky.length()) 
		{
			trace_missing_file("gfx/env/" + sky + "bk.tga", trace, true);
			trace_missing_file("gfx/env/" + sky + "dn.tga", trace, true);
			trace_missing_file("gfx/env/" + sky + "ft.tga", trace, true);
			trace_missing_file("gfx/env/" + sky + "lf.tga", trace, true);
			trace_missing_file("gfx/env/" + sky + "rt.tga", trace, true);
			trace_missing_file("gfx/env/" + sky + "up.tga", trace, true);
			push_unique(resources, "gfx/env/" + sky + "bk.tga");
			push_unique(resources, "gfx/env/" + sky + "dn.tga");
			push_unique(resources, "gfx/env/" + sky + "ft.tga");
			push_unique(resources, "gfx/env/" + sky + "lf.tga");
			push_unique(resources, "gfx/env/" + sky + "rt.tga");
			push_unique(resources, "gfx/env/" + sky + "up.tga");
		}

		// parse replacement files
		string global_model_list = worldSpawn->keyvalues["globalmodellist"];
		if (global_model_list.length())
		{			
			global_model_list = normalize_path("models/" + name + "/" + global_model_list);

			trace_missing_file(global_model_list, trace, true);
			push_unique(resources, global_model_list);
			vector<string> replace_res = get_replacement_file_resources(global_model_list);
			for (int k = 0; k < replace_res.size(); k++)
			{
				string model_path = normalize_path(replace_res[k]);
				Mdl model = Mdl(model_path);

				trace_missing_file(model_path, trace + " --> " + global_model_list, true);
				push_unique(resources, model_path);
				if (model.valid)
				{
					vector<string> model_res = model.get_resources();
					for (int k = 0; k < model_res.size(); k++)
					{
						trace_missing_file(model_res[k], trace + " --> " + global_model_list + " --> " + model_path , true);
						push_unique(resources, model_res[k]);
					}
				}
			}
		}

		string global_sound_list = worldSpawn->keyvalues["globalsoundlist"];
		if (global_sound_list.length())
		{			
			global_sound_list = normalize_path("sound/" + name + "/" + global_sound_list);

			trace_missing_file(global_sound_list, trace, true);
			push_unique(resources, global_sound_list);
			vector<string> replace_res = get_replacement_file_resources(global_sound_list);
			for (int k = 0; k < replace_res.size(); k++)
			{
				trace_missing_file("sound/" + replace_res[k], trace + " --> " + global_sound_list, true);
				push_unique(resources, "sound/" + replace_res[k]);
			}
		}

		string forced_player_models = worldSpawn->keyvalues["forcepmodels"];
		if (forced_player_models.length()) 
		{
			vector<string> models = splitString(forced_player_models, ";");
			for (int i = 0; i < models.size(); i++)
			{
				string model = models[i];
				if (model.length() == 0)
					continue;
				string path = "models/player/" + model + "/" + model;

				trace_missing_file(path + ".mdl", trace, true);
				trace_missing_file(path + ".bmp", trace, true);
				push_unique(resources, path + ".mdl");
				push_unique(resources, path + ".bmp");

				string model_path = normalize_path(path + ".mdl");
				Mdl mdl = Mdl(model_path);
				if (mdl.valid)
				{
					vector<string> model_res = mdl.get_resources();
					for (int k = 0; k < model_res.size(); k++)
					{
						trace_missing_file(model_res[k], trace + " --> " + model_path, true);
						push_unique(resources, model_res[k]);
					}
				}
			}
		}

		string sentences_file = normalize_path(worldSpawn->keyvalues["sentence_file"]);
		if (sentences_file.length())
		{
			trace_missing_file(sentences_file, trace, true);
			push_unique(resources, sentences_file);
			vector<string> sounds = get_sentence_file_resources(sentences_file);
			for (int i = 0; i < sounds.size(); i++)
			{
				trace_missing_file(sounds[i], trace + " --> " + sentences_file, true);
				push_unique(resources, sounds[i]);
			}
		}

		string materials_file = worldSpawn->keyvalues["materials_file"];
		if (materials_file.length()) 
		{
			materials_file = normalize_path("sound/" + name + "/" + materials_file);
			trace_missing_file(materials_file, trace, true);
			push_unique(resources, materials_file);
		}
	}

	return resources;
}

vector<string> Bsp::get_textures()
{
	vector<string> tex_names;

	byte * textures = lumps[LUMP_TEXTURES];
	int num_textures = ((int*)textures)[0];
	int lump_len = header.lump[LUMP_TEXTURES].nLength;

	if (num_textures > MAX_MAP_TEXTURES)
	{
		cout << "Map has too many textures\n";
		return tex_names;
	}

	for (int i = 0; i < num_textures; i++)
	{
		int offset = ((int*)textures)[i + 1];
		if (offset + sizeof(BSPMIPTEX) > lump_len)
		{
			cout << "ERROR: Invalid texture lump offset: " + offset << endl;
			continue;
		}
		BSPMIPTEX * t = (BSPMIPTEX*)&textures[offset];
		bool inWad = t->nOffsets[0] == 0;

		if (inWad)
		{
			string name = t->szName;
			tex_names.push_back(name);
			//cout << "GOT WAD TEX: " << name << endl;
		}
	}

}

bool Bsp::load_lumps(string fname)
{
	bool valid = true;

	// Read all BSP Data
	ifstream fin(fname, ios::binary);

	fin.read((char*)&header.nVersion, sizeof(int));

	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		fin.read((char*)&header.lump[i], sizeof(BSPLUMP));
	}
	lumps = new byte*[HEADER_LUMPS];
	
	for (int i = 0; i < NUM_LUMPS_TO_LOAD; i++)
	{
		int idx = lumps_to_load[i];
		lumps[idx] = new byte[header.lump[idx].nLength];
		fin.seekg(header.lump[idx].nOffset);
		if (fin.eof()) {
			cout << "FAILED TO READ BSP LUMP " << idx << endl;
			valid = false;
		}
		else
			fin.read((char*)lumps[idx], header.lump[idx].nLength);
	}	
	
	fin.close();

	return valid;
}

void Bsp::load_ents()
{
	bool verbose = true;
	membuf sbuf((char*)lumps[LUMP_ENTITIES], header.lump[LUMP_ENTITIES].nLength);
	istream in(&sbuf);

	int lineNum = 0;
	int lastBracket = -1;
	Entity* ent = NULL;

	string line = "";
	while (!in.eof())
	{
		lineNum++;
		getline (in,line);
		if (line.length() < 1 || line[0] == '\n')
			continue;

		if (line[0] == '{')
		{
			if (lastBracket == 0)
			{
				cout << name << ".bsp ent data (line " << lineNum << "): Unexpected '{'\n";
				continue;
			}
			lastBracket = 0;

			if (ent != NULL)
				delete ent;
			ent = new Entity();
		}
		else if (line[0] == '}')
		{
			if (lastBracket == 1)
				cout << name << ".bsp ent data (line " << lineNum << "): Unexpected '}'\n";
			lastBracket = 1;

			if (ent == NULL)
				continue;

			ents.push_back(ent);
			ent = NULL;
		}
		else if (lastBracket == 0 && ent != NULL) // currently defining an entity
		{
			Keyvalue k(line);
			if (k.key.length() && k.value.length())
				ent->addKeyvalue(k);
		}
	}	
	//cout << "got " << ents.size() <<  " entities\n";

	if (ent != NULL)
		delete ent;
}