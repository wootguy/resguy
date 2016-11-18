#include <iostream>
#include <string>
#include <vector>
#include "Bsp.h"
#include "Mdl.h"
#include "util.h"
#include <fstream>
#include <algorithm>

using namespace std;

vector<string> default_content;
int max_reference_prints = 5;

bool just_testing = false;
bool print_all_references = false;
bool print_skip = false;

void load_default_content()
{
	ifstream myfile("default_content.txt");
	if (myfile.is_open())
	{
		while ( !myfile.eof() )
		{
			string line;
			getline (myfile,line);

			line = trimSpaces(line);
			if (line.find("//") == 0 || line.length() == 0)
				continue;

			default_content.push_back(toLowerCase(normalize_path(line)));
		}
	}
	else
	{
		cout << "ERROR: default_content.txt is missing! Files from the base game may be included.\n";
	}
}

// search for referenced files here that may include other files (replacement files, scripts)
vector<string> get_cfg_resources(string map)
{
	vector<string> cfg_res;

	string cfg = "maps/" + map + ".cfg";
	string cfg_path = cfg;

	trace_missing_file(cfg, "(optional file)", false);

	if (!contentExists(cfg_path))
		return cfg_res;

	push_unique(cfg_res, cfg);

	ifstream myfile(cfg_path);
	if (myfile.is_open())
	{
		while ( !myfile.eof() )
		{
			string line;
			getline (myfile,line);

			line = trimSpaces(line);
			if (line.find("//") == 0 || line.length() == 0)
				continue;

			line = replaceChar(line, '\t', ' ' );

			if (line.find("globalmodellist") == 0)
			{
				string val = trimSpaces(line.substr(line.find("globalmodellist")+strlen("globalmodellist")));
				string global_model_list = normalize_path("models/" + map + "/" + val);

				trace_missing_file(global_model_list, cfg, true);
				push_unique(cfg_res, global_model_list);
				vector<string> replace_res = get_replacement_file_resources(global_model_list);
				for (int k = 0; k < replace_res.size(); k++)
				{
					string model_path = normalize_path(replace_res[k]);
					Mdl model = Mdl(model_path);

					trace_missing_file(model_path, cfg + " --> " + global_model_list, true);
					push_unique(cfg_res, model_path);
					if (model.valid)
					{
						vector<string> model_res = model.get_resources();
						for (int k = 0; k < model_res.size(); k++)
						{
							trace_missing_file(model_res[k], cfg + " --> " + global_model_list + " --> " + model_path, true);
							push_unique(cfg_res, model_res[k]);
						}
					}
				}
			}

			if (line.find("globalsoundlist") == 0)
			{
				string val = trimSpaces(line.substr(line.find("globalsoundlist")+strlen("globalsoundlist")));
				string global_sound_list = normalize_path("sound/" + map + "/" + val);

				trace_missing_file(global_sound_list, cfg, true);
				push_unique(cfg_res, global_sound_list);
				vector<string> replace_res = get_replacement_file_resources(global_sound_list);
				for (int k = 0; k < replace_res.size(); k++)
				{
					trace_missing_file(replace_res[k], cfg + " --> " + global_sound_list, true);
					push_unique(cfg_res, "sound/" + replace_res[k]);
				}
			}

			if (line.find("sentence_file") == 0)
			{
				string val = trimSpaces(line.substr(line.find("sentence_file")+strlen("sentence_file")));
				string sentences_file = normalize_path(val);

				trace_missing_file(sentences_file, cfg, true);
				push_unique(cfg_res, sentences_file);
				vector<string> sounds = get_sentence_file_resources(sentences_file);
				for (int i = 0; i < sounds.size(); i++)
				{
					trace_missing_file(sounds[i], cfg + " --> " + sentences_file, true);
					push_unique(cfg_res, sounds[i]);
				}
			}

			if (line.find("materials_file") == 0)
			{
				string val = trimSpaces(line.substr(line.find("materials_file")+strlen("materials_file")));
				string materials_file = normalize_path("sound/" + map + "/" + val);
				trace_missing_file(materials_file, cfg, true);
				push_unique(cfg_res, materials_file);
			}

			if (line.find("map_script") == 0)
			{
				string val = trimSpaces(line.substr(line.find("map_script")+strlen("map_script")));
				string map_script = normalize_path("scripts/maps/" + val + ".as");
				trace_missing_file(map_script, cfg, true);
				push_unique(cfg_res, map_script);

				vector<string> scripts = get_script_dependencies(map_script);
				for (int i = 0; i < scripts.size(); i++)
				{
					trace_missing_file(scripts[i], cfg + " --> " + map_script, true);
					push_unique(cfg_res, scripts[i]);
				}
			}
		}
	}
	myfile.close();

	return cfg_res;
}

vector<string> get_detail_resources(string map)
{
	vector<string> resources;

	string detail = "maps/" + map + "_detail.txt";
	string detail_path = detail;

	trace_missing_file(detail_path, "(optional file)", false);

	if (!contentExists(detail_path))
		return resources;

	push_unique(resources, detail);

	ifstream myfile(detail_path);
	if (myfile.is_open())
	{
		int line_num = 0;
		while ( !myfile.eof() )
		{
			string line;
			getline (myfile,line);
			line_num++;

			line = trimSpaces(line);
			if (line.find("//") == 0 || line.length() == 0)
				continue;

			line = replaceChar(line, '\t', ' ');
			vector<string> parts = splitString(line, " ");

			if (parts.size() != 4)
			{
				cout << "Warning: Invalid line in detail file ( " << line_num << "): " << detail_path;
				continue;
			}

			string tga = "gfx/" + parts[1] + ".tga";
			trace_missing_file(tga, detail, true);
			push_unique(resources, tga);
		}
	}

	return resources;
}

bool stringCompare( const string &left, const string &right )
{
	int sz = left.size() > right.size() ? left.size() : right.size();

	for (int i = 0; i < sz; i++)
	{
		if (left[i] != right[i])
			return left[i] < right[i];
	}
	return left.size() < right.size();
}

bool write_map_resources(string map)
{
	vector<string> all_resources;

	Bsp bsp(map);

	if (!bsp.valid) {
		cout << "ERROR: " << map << ".bsp not found\n";
		return false;
	}

	//cout << "Parsing " << bsp.name << ".bsp...\n\n";

	insert_unique(bsp.get_resources(), all_resources);
	insert_unique(get_cfg_resources(map), all_resources);
	insert_unique(get_detail_resources(map), all_resources);

	if (contentExists("maps/" + map + "_skl.cfg"))
	{
		trace_missing_file("maps/" + map + "_skl.cfg", "(optional file)", false);
		push_unique(all_resources, "maps/" + map + "_skl.cfg");
	}
	if (contentExists("maps/" + map + "_motd.txt"))
	{
		trace_missing_file("maps/" + map + "_motd.txt", "(optional file)", false);
		push_unique(all_resources, "maps/" + map + "_motd.txt");
	}

	push_unique(all_resources, "maps/" + map + ".res");

	// remove all referenced files included in the base game
	int numskips = 0;
	for (int i = 0; i < all_resources.size(); i++)
	{
		for (int k = 0; k < default_content.size(); k++)
		{
			if (toLowerCase(all_resources[i]) == default_content[k])
			{
				if (print_skip)
				{
					if (numskips == 0)
						cout << endl;
					cout << "Skipping default content: " << all_resources[i] << "\n";
				}
				all_resources.erase(all_resources.begin() + i);
				i--;
				numskips++;
			}
		}
	}

	sort( all_resources.begin(), all_resources.end(), stringCompare );

	// TODO: 
	// Don't include WADs that aren't really used
	// ignore missing files if they're only referenced in weird keyvalues that don't make sense for the entity
	// (this can happen when you copy paste entities and change their types)

	ofstream fout;
	if (!just_testing)
		fout.open("maps/" + map + ".res", ios::out | ios::trunc);

	fout << "// Created with resguy v1\n\n";

	int numEntries = 0;
	int missing = 0;
	for (int i = 0; i < all_resources.size(); i++)
	{
		string file = all_resources[i];
		if (contentExists(file))
		{
			if (!just_testing)
			{
				string actual_file_name = getFilename(file);
				fout << actual_file_name << endl;
			}
			numEntries++;
		}
		else if (true && g_tracemap_req[file].size())
		{
			if (missing == 0) cout << endl;
			vector<string>& refs = g_tracemap_req[file];
			cout << "Missing file \"" << file << "\" referenced in:\n";
			for (int i = 0; i <	refs.size(); i++)
			{
				int left_to_print = refs.size() - i;
				if (!print_all_references && i == (max_reference_prints-1) && left_to_print > 1)
				{
					cout << "\t" << left_to_print << " more...\n";
					break;
				}
				cout << "\t" << refs[i] << endl;
			}
			cout << endl;
			missing++;
		}
		else if (true)
		{
			if (missing == 0) cout << endl;
			cout << "Missing file \"" << file << "\" (usage unknown)\n\n";
			missing++;
		}
	}

	if (!just_testing)
	{
		string out_file = "maps/" + map + ".res";
		cout << "Wrote " << out_file << " - " <<  numEntries << " entries\n\n";
		fout.close(); 
	}
	else
	{
		cout << "Test finished. " << numEntries << " files found. " << missing << " files missing. " << numskips << " files skipped.\n\n";
	}

	return missing > 0;
}

int main(int argc, char* argv[])
{
	load_default_content();

	string map = "";
	if (argc > 1)
		map = argv[1];
	
	if (argc > 2) 
	{
		for (int i = 2; i < argc; i++)
		{
			string arg = toLowerCase(argv[i]);
			if (arg == "-test")
				just_testing = true;
			if (arg == "-allrefs")
				print_all_references = true;
			if (arg == "-printdefault")
				print_skip = true;
		}
	}
	
	if (map.length() == 0)
	{
		cout << "Enter map name to generate .res file for: ";
		cin >> map;
	}

	// strip ".bsp" if it was added
	int bidx = toLowerCase(map).find(".bsp");
	if (bidx == map.length() - 4)
		map = map.substr(0, map.length()-4);

	cout << "Generating .res file for " << map << "\n";

	int ret = write_map_resources(map);

	system("pause");

	return ret;
}