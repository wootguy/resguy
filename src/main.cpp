#include <iostream>
#include <string>
#include <vector>
#include "Bsp.h"
#include "Mdl.h"
#include "util.h"
#include <fstream>
#include <algorithm>
#include "globals.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(WIN32) || defined(_WIN32)
#include <conio.h>
#define CLEAR_COMMAND "cls"
#else
#include <termios.h>
#define CLEAR_COMMAND "clear"

char _getch()
{
	termios old, unbuffered;
	char result;
	tcgetattr(0, &old);
	
	unbuffered = old;
	unbuffered.c_lflag &= ~ICANON;
	unbuffered.c_lflag &= ~ECHO;
	
	tcsetattr(0, TCSANOW, &unbuffered);
	
	result = getchar();
	
	tcsetattr(0, TCSANOW, &old);
	
	return result;
}
#endif

using namespace std;

vector<string> default_content;
str_map_vector default_wads; // texture names in the default wads
vector<string> server_files;
int unused_wads = 0;
int max_reference_prints = 3;

bool just_testing = false;
bool print_all_references = false;
bool print_skip = false;
bool quiet_mode = false;
bool client_files_only = true; // don't include files not needed by clients (e.g. motd, .res file, scripts)
bool write_separate_server_files = false; // if client_files_only is on, the server files will be written to mapname.res2
bool write_missing = false;

// interactive mode vars
bool interactive = false;
bool chose_opts = false;
string target_maps = "";
bool map_not_found = false;

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

void load_default_content()
{
	default_content.clear();
	default_wads.clear();

	bool parsingTexNames = false;
	string wad_name;
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
			if (line.find("[DEFAULT FILES]") != string::npos)
			{
				parsingTexNames = false;
				continue;
			}
			if (line.find("[DEFAULT TEXTURES]") != string::npos)
			{
				parsingTexNames = true;
				continue;
			}

			if (parsingTexNames)
			{
				if (line[0] == '[')
				{
					wad_name = line.substr(1);
					wad_name = wad_name.substr(0, wad_name.find_last_of("]"));
					continue;
				}
				default_wads[wad_name].push_back(toLowerCase(line));
			}
			else
				default_content.push_back(toLowerCase(normalize_path(line)));
		}
	}
	else
	{
		cout << "WARNING:\ndefault_content.txt is missing! Files from the base game may be included.\n\n";
	}
}

void generate_default_content_file()
{
	cout << "Generating default content file...\n";

	vector<string> all_dirs = getAllSubdirs("./");
	vector<string> all_files;
	vector<string> wad_files;

	for (int i = 0; i < all_dirs.size(); i++)
	{
		string dir = all_dirs[i].substr(2); // skip "./"

		vector<string> files = getDirFiles(all_dirs[i], "*");
		for (int k = 0; k < files.size(); k++)
		{
			if (files[k] == "." || files[k] == ".." || files[k].size() == 0)
				continue;

			if (files[k].find_last_of(".") == string::npos)
				continue;

			if (toLowerCase(files[k]).find("resguy") == 0)
				continue;

			all_files.push_back(dir + files[k]);

			if (get_ext(files[k]) == "wad")
				wad_files.push_back(dir + files[k]);
		}
	}

	sort( all_files.begin(), all_files.end(), stringCompare );

	ofstream fout;
	fout.open("default_content.txt", ios::out | ios::trunc);

	fout << "[DEFAULT FILES]\n";
	for (int i = 0; i < all_files.size(); i++)
		fout << all_files[i] << endl;

	// obsolete file
	fout << "pldecal.wad" << endl;

	// Lots of map use these but I think it was just a default value in previous versions
	fout << "sound/sentence.txt" << endl;
	fout << "sound/sentences.txt" << endl;

	// Write default textures
	fout << "\n\n[DEFAULT TEXTURES]\n";
	int num_tex = 0;
	for (int i = 0; i < wad_files.size(); i++) 
	{
		Wad wad(wad_files[i]);
		wad.readInfo();
		if (!wad.dirEntries)
			continue;

		fout << "[" << toLowerCase(wad_files[i]) << "]\n";
		for (int k = 0; k < wad.header.nDir; k++)
		{
			fout << wad.dirEntries[k].szName << endl;
			num_tex++;
		}
		fout << endl;
	}

	fout.close();

	cout << "Wrote " << all_files.size() << " files and " << num_tex << " textures\n";
}

// search for referenced files here that may include other files (replacement files, scripts)
vector<string> get_cfg_resources(string map)
{
	vector<string> cfg_res;

	string cfg = map + ".cfg";
	string cfg_path = "maps/" + cfg;

	//trace_missing_file(cfg, "(optional file)", false);
	push_unique(server_files, cfg_path);

	if (!contentExists(cfg_path))
		return cfg_res;

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
				global_model_list.erase(std::remove(global_model_list.begin(), global_model_list.end(), '\"'), global_model_list.end());

				trace_missing_file(global_model_list, cfg, true);
				push_unique(server_files, global_model_list);
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
				global_sound_list.erase(std::remove(global_sound_list.begin(), global_sound_list.end(), '\"'), global_sound_list.end());

				trace_missing_file(global_sound_list, cfg, true);
				push_unique(server_files, global_sound_list);
				push_unique(cfg_res, global_sound_list);
				vector<string> replace_res = get_replacement_file_resources(global_sound_list);
				for (int k = 0; k < replace_res.size(); k++)
				{
					string snd = "sound/" + replace_res[k];
					trace_missing_file(snd, cfg + " --> " + global_sound_list, true);
					push_unique(cfg_res, snd);
				}
			}

			if (line.find("forcepmodels") == 0)
			{
				string force_pmodels = trimSpaces(line.substr(line.find("forcepmodels")+strlen("forcepmodels")));
				force_pmodels.erase(std::remove(force_pmodels.begin(), force_pmodels.end(), '\"'), force_pmodels.end());

				vector<string> models = splitString(force_pmodels, ";");
				for (int i = 0; i < models.size(); i++)
				{
					string model = models[i];
					if (model.length() == 0)
						continue;
					string path = "models/player/" + model + "/" + model;

					trace_missing_file(path + ".mdl", cfg, true);
					trace_missing_file(path + ".bmp", cfg, true);
					push_unique(cfg_res, path + ".mdl");
					push_unique(cfg_res, path + ".bmp");

					string model_path = normalize_path(path + ".mdl");
					Mdl mdl = Mdl(model_path);
					if (mdl.valid)
					{
						vector<string> model_res = mdl.get_resources();
						for (int k = 0; k < model_res.size(); k++)
						{
							trace_missing_file(model_res[k], cfg + " --> " + model_path, true);
							push_unique(cfg_res, model_res[k]);
						}
					}
				}
			}

			if (line.find("sentence_file") == 0)
			{
				string val = trimSpaces(line.substr(line.find("sentence_file")+strlen("sentence_file")));
				string sentences_file = normalize_path(val);
				sentences_file.erase(std::remove(sentences_file.begin(), sentences_file.end(), '\"'), sentences_file.end());

				trace_missing_file(sentences_file, cfg, true);
				push_unique(server_files, sentences_file);
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
				materials_file.erase(std::remove(materials_file.begin(), materials_file.end(), '\"'), materials_file.end());
				trace_missing_file(materials_file, cfg, true);
				push_unique(server_files, materials_file);
				push_unique(cfg_res, materials_file);
			}

			if (line.find("map_script") == 0)
			{
				string val = trimSpaces(line.substr(line.find("map_script")+strlen("map_script")));
				string map_script = normalize_path("scripts/maps/" + val + ".as");
				map_script.erase(std::remove(map_script.begin(), map_script.end(), '\"'), map_script.end());
				
				trace_missing_file(map_script, cfg, true);
				push_unique(server_files, map_script);
				push_unique(cfg_res, map_script);

				vector<string> scripts = get_script_dependencies(map_script);
				for (int i = 0; i < scripts.size(); i++)
				{
					bool isScript = get_ext(scripts[i]) == "as";
					if (isScript) {
						trace_missing_file(scripts[i], cfg + " --> " + map_script, true);
						push_unique(server_files, scripts[i]);
						push_unique(cfg_res, scripts[i]);
					}
					else // file is a sound/model and was traced in the dependency function
					{
						push_unique(cfg_res, scripts[i]);
					}
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

	push_unique(resources, detail); // required by clients

	ifstream file(detail_path);
	if (file.is_open())
	{
		int line_num = 0;
		while ( !file.eof() )
		{
			string line;
			getline (file,line);
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
	file.close();

	return resources;
}

void ask_options() 
{
	bool opts[] = {just_testing, print_all_references, print_skip, !client_files_only, 
					   write_separate_server_files, write_missing};

	while(true) 
	{
		system(CLEAR_COMMAND);

		cout << endl << target_maps << "\n\n";

		cout << "Select options with number keys. Confirm with Enter:\n\n";
		cout << "\n 1. [" + string(opts[0] ? "X" : " ") +  "]  Don't write any .res files, just check for problems (-test)\n";
		cout << "\n 2. [" + string(opts[1] ? "X" : " ") +  "]  List all references for missing files (-allrefs)\n";
		cout << "\n 3. [" + string(opts[2] ? "X" : " ") +  "]  Print content that was skipped (-printskip)\n";
		cout << "\n 4. [" + string(opts[3] ? "X" : " ") +  "]  Include server files in .res file (-extra)\n";
		cout << "\n 5. [" + string(opts[4] ? "X" : " ") +  "]  Write server files to a separate .res2 file (-extra2)\n";
		cout << "\n 6. [" + string(opts[5] ? "X" : " ") +  "]  Write missing files to a separate .res3 file (-missing3)\n";

		char choice = _getch();
		if (choice == '1') opts[0] = !opts[0];
		if (choice == '2') opts[1] = !opts[1];
		if (choice == '3') opts[2] = !opts[2];
		if (choice == '4') opts[3] = !opts[3];
		if (choice == '5') opts[4] = !opts[4];
		if (choice == '6') opts[5] = !opts[5];
			
		if (choice == '\r' || choice == '\n') break;
	}

	just_testing = opts[0];
	print_all_references = opts[1];
	print_skip = opts[2];
	client_files_only = !opts[3];
	write_separate_server_files = opts[4];
	write_missing = opts[5];

	system(CLEAR_COMMAND);
	cout << endl;
}

bool write_map_resources(string map)
{
	vector<string> all_resources;

	Bsp bsp(map);

	if (!bsp.valid) {
		cout << "ERROR: " << map << ".bsp not found\n";
		map_not_found = true;
		return false;
	}

	// wait until now to choose opts because here we know at least one map exists
	if (interactive && !chose_opts) 
	{
		ask_options();
		chose_opts = true;
	}

	string map_path = bsp.path;
	cout << "Generating .res file for " << bsp.path + map << "\n";

	//cout << "Parsing " << bsp.name << ".bsp...\n\n";

	insert_unique(bsp.get_resources(), all_resources);
	insert_unique(get_cfg_resources(map), all_resources);
	insert_unique(get_detail_resources(map), all_resources);

	string skl = "maps/" + map + "_skl.cfg";
	if (contentExists(skl))
	{
		//trace_missing_file("maps/" + map + "_skl.cfg", "(optional file)", false);
		//push_unique(all_resources, "maps/" + map + "_skl.cfg");
		push_unique(server_files, "maps/" + map + "_skl.cfg");
	}
	string motd = "maps/" + map + "_motd.txt";
	if (contentExists(motd))
	{
		trace_missing_file("maps/" + map + "_motd.txt", "(optional file)", false);
		push_unique(server_files, "maps/" + map + "_motd.txt");
		push_unique(all_resources, "maps/" + map + "_motd.txt");
	}
	string save = "maps/" + map + ".save";
	if (contentExists(save))
	{
		trace_missing_file("maps/" + map + ".save", "(optional file)", false);
		push_unique(server_files, "maps/" + map + ".save");
		push_unique(all_resources, "maps/" + map + ".save");
	}

	push_unique(all_resources, "maps/" + map + ".res");
	push_unique(server_files, "maps/" + map + ".res");

	sort( all_resources.begin(), all_resources.end(), stringCompare );

	// fix bad paths (they shouldn't be legal, but they are)
	for (int i = 0; i < all_resources.size(); i++)
	{
		string oldPath = all_resources[i];
		string f = replaceChar(toLowerCase(oldPath), '\\', '/');

		bool bad_path = true;
		if (f.find("svencoop_hd/") == 0)
			all_resources[i] = all_resources[i].substr(string("svencoop_hd/").length());
		else if (f.find("svencoop_addon/") == 0)
			all_resources[i] = all_resources[i].substr(string("svencoop_addon/").length());
		else if (f.find("svencoop_downloads/") == 0)
			all_resources[i] = all_resources[i].substr(string("svencoop_downloads/").length());
		else if (f.find("svencoop/") == 0)
			all_resources[i] = all_resources[i].substr(string("svencoop/").length());
		else if (f.find("valve/") == 0)
			all_resources[i] = all_resources[i].substr(string("valve/").length());
		else
			bad_path = false;

		if (bad_path)
		{
			cout << "'" << oldPath << "' should not be restricted to a specific content folder. Referenced in:\n";
			if (g_tracemap_req[oldPath].size())
			{
				vector<string>& refs = g_tracemap_req[oldPath];
				for (int i = 0; i < refs.size(); i++)
				{
					int left_to_print = refs.size() - i;
					if (!print_all_references && i == (max_reference_prints - 1) && left_to_print > 1)
					{
						cout << "\t" << left_to_print << " more...\n";
						break;
					}
					cout << "\t" << refs[i] << endl;
				}
				cout << endl;
			}
			else
			{
				cout << "(usage unknown)\n\n";
			}
		}
	}

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
					if (unused_wads == 0 && numskips == 0)
						cout << endl;
					cout << "Skip default: " << all_resources[i] << "\n";
				}
				all_resources.erase(all_resources.begin() + i);
				i--;
				numskips++;
				break;
			}
		}
	}

	// remove all referenced files with invalid extensions
	for (int i = 0; i < all_resources.size(); i++)
	{
		bool ext_ok = false;
		for (int k = 0; k < NUM_VALID_EXTS; k++)
		{
			if (get_ext(all_resources[i]) == g_valid_exts[k])
			{
				ext_ok = true;
				break;
			}
		}
		if (!ext_ok)
		{
			if (print_skip)
			{
				if (unused_wads == 0 && numskips == 0)
					cout << endl;
				cout << "Skip invalid: " << all_resources[i] << "\n";
			}
			all_resources.erase(all_resources.begin() + i);
			i--;
			numskips++;
		}
	}

	// count missing files
	int missing = 0;
	for (int i = 0; i < all_resources.size(); i++)
	{
		if (!contentExists(all_resources[i]) && get_ext(all_resources[i]) != "res")
			missing++;
	}

	// write server files 
	if (write_separate_server_files && !just_testing && server_files.size())
	{
		bool will_write_res = (all_resources.size() - missing) > server_files.size();

		ofstream fout2;
		fout2.open(map_path + map + ".res2", ios::out | ios::trunc);
		for (int i = 0; i < server_files.size(); i++)
		{
			string file = server_files[i];
			if (get_ext(file) != "res")
			{
				if (!contentExists(file, true))
					continue;
				if (file.find("..") == 0) // strip "../svencoop_downloads/" or similar
				{
					file = file.substr(file.find_first_of("\\/")+1);
					file = file.substr(file.find_first_of("\\/")+1);
				}
			}
			else if (!will_write_res)
				continue;
			fout2 << file << endl;
		}
		fout2.close();
	}

	if (all_resources.size() == 0 || (all_resources.size() == 1 && get_ext(all_resources[0]) == "res"))
	{
		cout << "No .res file needed. Map uses default content only.\n";
		return true;
	}

	// remove+write missing files
	missing = 0;
	ofstream fmiss;
	for (int i = 0; i < all_resources.size(); i++)
	{
		string file = all_resources[i];
		if (!contentExists(file) && get_ext(file) != "res")
		{
			if (g_tracemap_req[file].size())
			{
				if (missing == 0) cout << endl;
				vector<string>& refs = g_tracemap_req[file];
				cout << "Missing file \"" << file << "\" referenced in:\n";
				for (int i = 0; i < refs.size(); i++)
				{
					int left_to_print = refs.size() - i;
					if (!print_all_references && i == (max_reference_prints - 1) && left_to_print > 1)
					{
						cout << "\t" << left_to_print << " more...\n";
						break;
					}
					cout << "\t" << refs[i] << endl;
				}
				cout << endl;
			}
			else
			{
				if (unused_wads == 0 && numskips == 0 && missing == 0) cout << endl;
				cout << "Missing file \"" << file << "\" (usage unknown)\n\n";
			}
			if (write_missing) {
				if (!fmiss.is_open())
					fmiss.open(map_path + map + ".res3", ios::out | ios::trunc);
				fmiss << file << endl;
			}

			all_resources.erase(all_resources.begin() + i);
			i--;
			missing++;
		}
	}
	if (fmiss.is_open())
		fmiss.close();

	// remove optional server files
	if (client_files_only)
	{
		for (int i = 0; i < all_resources.size(); i++)
		{
			if (find(server_files.begin(), server_files.end(), all_resources[i]) != server_files.end())
			{
				if (print_skip)
					cout << "Skip optional: " << all_resources[i] << "\n";
				numskips++;
				if (unused_wads == 0 && numskips == 0)
					cout << endl;

				all_resources.erase(all_resources.begin() + i);
				i--;
			}
		}
	}

	if (all_resources.size() == 0 || (all_resources.size() == 1 && get_ext(all_resources[0]) == "res"))
	{
		if (missing)
			cout << "No .res file generated. All required files are missing!\n";
		else
			cout << "No .res file generated. Server-related files skipped.\n";
		return true;
	}

	// TODO: 
	// Don't include WADs that aren't really used
	// ignore missing files if they're only referenced in weird keyvalues that don't make sense for the entity
	// (this can happen when you copy paste entities and change their types)
	// ignore files in entities that are never triggered?
	// detect file paths that are constructed at run-time? (no maps do this afaik)

	ofstream fout;
	if (!just_testing)
		fout.open(map_path + map + ".res", ios::out | ios::trunc);

	fout << "// Created with resguy v12\n";
	fout << "// https://github.com/wootguy/resguy\n\n";


	int numEntries = 0;
	for (int i = 0; i < all_resources.size(); i++)
	{
		string file = all_resources[i];
		if (!just_testing)
		{
			contentExists(file, true);
			if (file.find("..") == 0) // strip "../svencoop_downloads/" or similar
			{
				file = file.substr(file.find_first_of("\\/")+1);
				file = file.substr(file.find_first_of("\\/")+1);
			}

			fout << file << endl;
		}
		numEntries++;
	}

	if (!just_testing)
	{
		cout << "Wrote " << numEntries << " entries. " << missing << " files missing. " << numskips << " files skipped.\n\n";
		fout.close(); 
	}
	else
	{
		cout << "Test finished. " << numEntries << " files found. " << missing << " files missing. " << numskips << " files skipped.\n\n";
	}

	return missing == 0;
}

string bsp_name(string fname)
{
	string f = fname;
	int iname = f.find_last_of("\\/");
	if (iname != string::npos && iname < f.length()-1)
		f = f.substr(iname+1);

	int bidx = toLowerCase(f).find(".bsp");
	if (bidx == f.length() - 4)
		f = f.substr(0, f.length()-4);

	return f;
}

int main(int argc, char* argv[])
{
	string map = "";
	if (argc > 1)
	{
		map = argv[1];
		if (map == "-gend") {
			generate_default_content_file();
#ifdef _DEBUG
			system("pause");
#endif
			return 0;
		}
	}
	
	if (argc > 2) 
	{
		for (int i = 2; i < argc; i++)
		{
			string arg = toLowerCase(argv[i]);
			if (arg == "-test")
				just_testing = true;
			if (arg == "-allrefs")
				print_all_references = true;
			if (arg == "-printskip")
				print_skip = true;
			if (arg == "-quiet")
				quiet_mode = true; // TODO
			if (arg == "-missing3")
				write_missing = true;
			if (arg == "-extra")
				client_files_only = false;
			if (arg == "-extra2")
				write_separate_server_files = true;
		}
	}
	
	interactive = map.length() == 0;
	ask_for_map:	
	if (interactive)
	{
		system(CLEAR_COMMAND);
		cout << endl;
		load_default_content();
		chose_opts = false;
		map_not_found = false;
		cout << "What do you want to generate a .res file for?\n\nExamples:\n  stadium3 = target stadium3.bsp\n  stadium* = target all maps with a name that starts with \"stadium\"\n  *        = target all maps\n\nTarget: ";
		cin >> map;
		system(CLEAR_COMMAND);

		#if !defined(WIN32) || !defined(_WIN32)
			_getch(); // ignore newline
		#endif
	}
	else
		load_default_content();

	// strip ".bsp" if it was added
	int bidx = toLowerCase(map).find(".bsp");
	if (bidx == map.length() - 4)
		map = map.substr(0, map.length()-4);

	bool all_maps = map.find_first_of("*") != string::npos;

	string seperator = "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n\n";

	int ret = 0;
	if (all_maps)
	{
		vector<string> files = getDirFiles("maps/", "bsp", map);
		insert_unique(getDirFiles("../svencoop_hd/maps/", "bsp", map), files);
		insert_unique(getDirFiles("../svencoop_addons/maps/", "bsp", map), files);
		insert_unique(getDirFiles("../svencoop_downloads/maps/", "bsp", map), files);
		insert_unique(getDirFiles("../svencoop/maps/", "bsp", map), files);
		insert_unique(getDirFiles("../valve/maps/", "bsp", map), files);
		sort( files.begin(), files.end(), stringCompare );

		if (!files.size()) 
		{
			cout << "No maps matching the search string were found.\n";
			ret = 2;
			map_not_found = true;
		}
		else 
		{
			if (interactive)
			{
				target_maps = "Generating .res files for:\n  ";
				const int max_target = 4;
				for (int i = 0; i < files.size(); i++) 
				{
					if (i > 0)
						target_maps += ", ";
					target_maps += bsp_name(files[i]);
					if (i == max_target-1 && (files.size() - max_target > 1)) 
					{
						target_maps += string(" (") + to_string(files.size() - max_target) + " more...)";
						break;
					}
				}
			}
			else
				cout << "Generating .res files for " << files.size() << " maps...\n\n" << seperator;

			for (int i = 0; i < files.size(); i++)
			{
				// reset globals
				server_files.clear();
				unused_wads = 0;
				g_tracemap_req.clear();
				g_tracemap_opt.clear();

				string f = bsp_name(files[i]);

				if (!write_map_resources(f))
				{
					ret = 2;
					#ifdef _DEBUG
						system("pause");
					#endif
				}

				if (i != files.size()-1)
					cout << seperator;
			}
		}
	}
	else
	{
		target_maps = "Generating .res file for:\n  " + map;
		if (!write_map_resources(map))
			ret = 1;
	}

#ifdef _DEBUG
	system("pause");
#endif


	if (interactive)
	{
		cout << endl;
		cout << "Press any key to continue...\n";
		_getch();
		if (map_not_found)
			goto ask_for_map;
	}

	return ret;
}