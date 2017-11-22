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
#include <Windows.h>
#include <Shlobj.h>
#define CLEAR_COMMAND "cls"
#else
#include <termios.h>
#include <unistd.h>
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
vector<string> archive_files;
vector<string> series_client_files;
int unused_wads = 0;
int max_reference_prints = 3;
int res_files_generated = 0;

bool just_testing = false;
bool print_all_references = false;
bool print_skip = false;
bool quiet_mode = false;
bool client_files_only = true; // don't include files not needed by clients (e.g. motd, .res file, scripts)
bool write_separate_server_files = false; // if client_files_only is on, the server files will be written to mapname.res2
bool write_separate_missing = false;
bool include_missing = false;
bool case_sensitive_mode = true;
bool log_enabled = false;
bool series_mode = false;
string archive_arg;

// interactive mode vars
bool interactive = false;
bool chose_opts = false;
string target_maps = "";
bool map_not_found = false;

string version_string = "resguy v6 (November 2017)";
string resguy_header = "// Created with " + version_string + "\n// https://github.com/wootguy/resguy\n\n";

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
	cout << "Loading default content...\n";
	default_content.clear();
	default_wads.clear();

	bool parsingTexNames = false;
	string wad_name;
	string default_content_path = "resguy_default_content.txt";
	fileExists(default_content_path, true); // cheack caps

	ifstream myfile(default_content_path);
	if (myfile.is_open())
	{
		while ( !myfile.eof() )
		{
			string line;
			getline (myfile,line);

			line = trimSpaces(line);
			if (line.find("//") == 0 || line.length() == 0)
				continue;
			if (line[0] == '[')
			{
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
					wad_name = line.substr(1);
					wad_name = wad_name.substr(0, wad_name.find_last_of("]"));
					continue;
				}
			}
			

			if (parsingTexNames)
				default_wads[wad_name].push_back(toLowerCase(line));
			else
				default_content.push_back(toLowerCase(line));
		}
		system(CLEAR_COMMAND);
	}
	else
	{
		system(CLEAR_COMMAND);
		cout << "WARNING:\nresguy_default_content.txt is missing! Files from the base game may be included.\n\n";
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

			all_files.push_back(normalize_path(dir + files[k]));

			if (get_ext(files[k]) == "wad")
				wad_files.push_back(dir + files[k]);
		}
	}

	sort( all_files.begin(), all_files.end(), stringCompare );

	ofstream fout;
	fout.open("resguy_default_content.txt", ios::out | ios::trunc);

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

	if (!contentExists(cfg_path, true))
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

			// Note: These can also be set in worldspawn entity
			if (line.find("globalmodellist") == 0)
			{
				string val = trimSpaces(line.substr(line.find("globalmodellist")+strlen("globalmodellist")));
				val.erase(std::remove(val.begin(), val.end(), '\"'), val.end());
				string global_model_list = normalize_path("models/" + map + "/" + val);

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
				val.erase(std::remove(val.begin(), val.end(), '\"'), val.end());
				string global_sound_list = normalize_path("sound/" + map + "/" + val);

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
				vector<string> sounds = get_sentence_file_resources(sentences_file, cfg + " --> " + sentences_file);
				for (int i = 0; i < sounds.size(); i++)
					push_unique(cfg_res, sounds[i]);
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
				bool needsExt = toLowerCase(val).find(".as") != val.length() - 3;
				string map_script = normalize_path("scripts/maps/" + val + (needsExt ? ".as" : ""));
				map_script.erase(std::remove(map_script.begin(), map_script.end(), '\"'), map_script.end());
				
				trace_missing_file(map_script, cfg, true);
				push_unique(server_files, map_script);
				push_unique(cfg_res, map_script);

				vector<string> searchedScripts;
				vector<string> scripts = get_script_dependencies(map_script, searchedScripts);
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

	if (!contentExists(detail_path, true))
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
				log("Warning: Invalid line in detail file ( " + to_string(line_num) + "): " + detail_path);
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

// returns 1 if user wants to choose a different map
// returns -1 if user wants to quit
int ask_options() 
{
	bool opts[] = {just_testing, print_all_references, print_skip, !client_files_only, 
					   write_separate_server_files, include_missing, write_separate_missing, 
					   series_mode, log_enabled, !case_sensitive_mode};

	while(true) 
	{
		system(CLEAR_COMMAND);

		cout << endl << target_maps << "\n\n";

		cout << "Select options with number keys. Confirm with Enter:\n\n";
		cout << " 1. [" + string(opts[0] ? "X" : " ") +  "]  Don't write any .res files, just check for problems (-test)\n";
		cout << " 2. [" + string(opts[1] ? "X" : " ") +  "]  List all references for missing files (-allrefs)\n";
		cout << " 3. [" + string(opts[2] ? "X" : " ") +  "]  Print content that was skipped (-printskip)\n";
		cout << " 4. [" + string(opts[3] ? "X" : " ") +  "]  Write server files (-extra)\n";
		cout << " 5. [" + string(opts[4] ? "X" : " ") +  "]  Write server files to a separate .res2 file (-extra2)\n";
		cout << " 6. [" + string(opts[5] ? "X" : " ") +  "]  Write missing files (-missing)\n";
		cout << " 7. [" + string(opts[6] ? "X" : " ") +  "]  Write missing files to a separate .res3 file (-missing3)\n";
		cout << " 8. [" + string(opts[7] ? "X" : " ") +  "]  Write the same files into every .res file (-series)\n";
		cout << " 9. [" + string(opts[8] ? "X" : " ") +  "]  Log output to mapname_resguy.log (-log)\n";
		#ifndef WIN32
			cout << " 0. [" + string(opts[9] ? "X" : " ") +  "]  Disable case sensitivity (-icase)\n";
		#endif
		cout << "\nPress B to go back or Q to quit\n";

		char choice = _getch();
		if (choice == '1') opts[0] = !opts[0];
		if (choice == '2') opts[1] = !opts[1];
		if (choice == '3') opts[2] = !opts[2];
		if (choice == '4') opts[3] = !opts[3];
		if (choice == '5') opts[4] = !opts[4];
		if (choice == '6') opts[5] = !opts[5];
		if (choice == '7') opts[6] = !opts[6];
		if (choice == '8') opts[7] = !opts[7];
		if (choice == '9') opts[8] = !opts[8];
		if (choice == '0') opts[9] = !opts[9];
		if (choice == 'Q' || choice == 'q') return -1;
		if (choice == 'B' || choice == 'b') return 1;
			
		if (choice == '\r' || choice == '\n') break;
	}

	just_testing = opts[0];
	print_all_references = opts[1];
	print_skip = opts[2];
	client_files_only = !opts[3];
	write_separate_server_files = opts[4];
	include_missing = opts[5];
	write_separate_missing = opts[6];
	series_mode = opts[7];
	log_enabled = opts[8];
	case_sensitive_mode = !opts[9];

	system(CLEAR_COMMAND);
	cout << endl;

	return 0;
}

bool isServerFile(string file)
{
	if (find(server_files.begin(), server_files.end(), file) != server_files.end())
		return true;

	if (file.find("..") == 0)
	{
		file = file.substr(file.find_first_of("\\/")+1);
		file = file.substr(file.find_first_of("\\/")+1);
		if (find(server_files.begin(), server_files.end(), file) != server_files.end())
			return true;
	}
	return false;
}

// 2 = error
// 1 = go back
// 0 = success
// -1 = quit
int write_map_resources(string map)
{
	vector<string> all_resources;

	Bsp bsp(map);

	if (!bsp.valid) {
		map_not_found = true;
		return 2;
	}

	// wait until now to choose opts because here we know at least one map exists
	if (interactive && !chose_opts) 
	{
		int ret = ask_options();
		if (ret)
			return ret;
		chose_opts = true;
	}

	res_files_generated++;

	string map_path = bsp.path;
	log_init(map_path + map + "_resguy.log");
	archive_files.push_back(bsp.full_path);
	if (series_mode)
		series_client_files.push_back(bsp.path + map + ".bsp");
	
	string opt_string;
	opt_string += just_testing ? " -test" : "";
	opt_string += print_all_references ? " -allrefs" : "";
	opt_string += print_skip ? " -printskip" : "";
	opt_string += !client_files_only ? " -extra" : "";
	opt_string += write_separate_server_files ? " -extra2" : "";
	opt_string += include_missing ? " -missing" : "";
	opt_string += write_separate_missing ? " -missing3" : "";
	opt_string += log_enabled ? " -log" : "";
	opt_string += !case_sensitive_mode ? " -icase" : "";
	opt_string += archive_arg.length() ? " " + archive_arg : "";

	log("Options Used:" + opt_string + "\n\n", false);

	log("Generating .res file for " + bsp.path + map + ".bsp\n");

	//cout << "Parsing " << bsp.name << ".bsp...\n\n";

	insert_unique(bsp.get_resources(), all_resources);
	insert_unique(get_cfg_resources(map), all_resources);
	insert_unique(get_detail_resources(map), all_resources);

	string skl = "maps/" + map + "_skl.cfg";
	if (contentExists(skl, true))
	{
		//trace_missing_file("maps/" + map + "_skl.cfg", "(optional file)", false);
		//push_unique(all_resources, "maps/" + map + "_skl.cfg");
		push_unique(server_files, "maps/" + map + "_skl.cfg");
	}
	string motd = "maps/" + map + "_motd.txt";
	if (contentExists(motd, true))
	{
		trace_missing_file("maps/" + map + "_motd.txt", "(optional file)", false);
		push_unique(server_files, "maps/" + map + "_motd.txt");
		push_unique(all_resources, "maps/" + map + "_motd.txt");
	}
	string save = "maps/" + map + ".save";
	if (contentExists(save, true))
	{
		trace_missing_file("maps/" + map + ".save", "(optional file)", false);
		push_unique(server_files, "maps/" + map + ".save");
		push_unique(all_resources, "maps/" + map + ".save");
	}

	push_unique(all_resources, "maps/" + map + ".res");
	push_unique(server_files, "maps/" + map + ".res");

	sort( all_resources.begin(), all_resources.end(), stringCompare );
	sort( server_files.begin(), server_files.end(), stringCompare );

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
			log("'" + oldPath + "' should not be restricted to a specific content folder. Referenced in:\n");
			if (g_tracemap_req[oldPath].size())
			{
				vector<string>& refs = g_tracemap_req[oldPath];
				for (int i = 0; i < refs.size(); i++)
				{
					int left_to_print = refs.size() - i;
					if (!print_all_references && i == (max_reference_prints - 1) && left_to_print > 1)
					{
						log("\t" + to_string(left_to_print) + " more...\n");
						break;
					}
					log("\t" + refs[i] + "\n");
				}
				log("\n");
			}
			else
			{
				log("(usage unknown)\n\n");
			}
		}
	}

	// remove all referenced files included in the base game
	int numskips = 0;
	for (int i = 0; i < all_resources.size(); i++)
	{
		if (find(default_content.begin(), default_content.end(), toLowerCase(all_resources[i])) != default_content.end())
		{
			if (print_skip)
			{
				if (unused_wads == 0 && numskips == 0)
					log("\n");
				log("Skip default: " + all_resources[i] + "\n");
			}
			all_resources.erase(all_resources.begin() + i);
			i--;
			numskips++;
		}
	}

	// remove all referenced files with invalid extensions
	for (int i = 0; i < all_resources.size(); i++)
	{
		bool ext_ok = false;
		string ext = get_ext(all_resources[i]);
		for (int k = 0; k < NUM_VALID_EXTS; k++)
		{
			if (ext == g_valid_exts[k])
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
					log("\n");
				log("Skip invalid: " + all_resources[i] + "\n");
			}
			all_resources.erase(all_resources.begin() + i);
			i--;
			numskips++;
		}
	}

	// count missing files and create list of archive files
	int missing = 0;
	for (int i = 0; i < all_resources.size(); i++)
	{
		bool is_res = get_ext(all_resources[i]) == "res";
		string tmp_path = all_resources[i];
		string full_path;
		if (!contentExists(tmp_path, true, full_path) && !is_res)
			missing++;
		else if (!is_res)
			push_unique(archive_files, full_path);
	}

	// add server files to archive list
	for (int i = 0; i < server_files.size(); i++)
	{
		string tmp_path = server_files[i];
		string full_path;
		if (contentExists(tmp_path, true, full_path))
			push_unique(archive_files, full_path);
	}

	// write server files 
	if (write_separate_server_files && !just_testing && server_files.size())
	{
		bool will_write_res = false;
		int client_file_count = 0;
		for (int i = 0; i < all_resources.size(); i++)
		{
			if (!isServerFile(all_resources[i]))
			{
				if (++client_file_count > missing)
				{
					will_write_res = true;
					break;
				}
			}
		}

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
		log("No .res file needed. Map uses default content only.\n");
		log_close();
		return 0;
	}

	// remove+write missing files
	missing = 0;
	ofstream fmiss;
	for (int i = 0; i < all_resources.size(); i++)
	{
		string file = all_resources[i];
		if (!contentExists(file, false) && get_ext(file) != "res")
		{
			if (g_tracemap_req[file].size())
			{
				if (missing == 0) log("\n");
				vector<string>& refs = g_tracemap_req[file];
				log("Missing file \"" + file + "\" referenced in:\n");
				for (int i = 0; i < refs.size(); i++)
				{
					int left_to_print = refs.size() - i;
					if (!print_all_references && i == (max_reference_prints - 1) && left_to_print > 1)
					{
						log("\t" + to_string(left_to_print) + " more...\n");
						break;
					}
					log("\t" + refs[i] + "\n");
				}
				log("\n");
			}
			else
			{
				if (unused_wads == 0 && numskips == 0 && missing == 0) log("\n");
				log("Missing file \"" + file + "\" (usage unknown)\n\n");
			}
			if (write_separate_missing) {
				if (!fmiss.is_open())
					fmiss.open(map_path + map + ".res3", ios::out | ios::trunc);
				fmiss << file << endl;
			}

			if (!include_missing)
			{
				all_resources.erase(all_resources.begin() + i);
				i--;
			}
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
			if (isServerFile(all_resources[i]))
			{
				if (print_skip)
					log("Skip optional: " + all_resources[i] + "\n");
				numskips++;
				if (unused_wads == 0 && numskips == 0)
					log("\n");

				all_resources.erase(all_resources.begin() + i);
				i--;
			}
		}
	}

	if ((!just_testing && all_resources.size() == 0) || (all_resources.size() == 1 && get_ext(all_resources[0]) == "res"))
	{
		if (missing)
			log("No .res file generated. All required files are missing!\n");
		else
			log("No .res file generated. Server-related files skipped.\n");
		log_close();
		return 0;
	}

	// TODO: 
	// ignore missing files if they're only referenced in weird keyvalues that don't make sense for the entity
	// (this can happen when you copy paste entities and change their types)
	// ignore files in entities that are never triggered? Impossible due to AS.
	// detect file paths that are constructed at run-time? (no maps do this afaik)

	ofstream fout;
	if (!just_testing && !series_mode)
		fout.open(map_path + map + ".res", ios::out | ios::trunc);

	fout << resguy_header;

	int numEntries = 0;
	for (int i = 0; i < all_resources.size(); i++)
	{
		string file = all_resources[i];
		if (!just_testing || series_mode)
		{
			contentExists(file, true);
			if (file.find("..") == 0) // strip "../svencoop_downloads/" or similar
			{
				file = file.substr(file.find_first_of("\\/")+1);
				file = file.substr(file.find_first_of("\\/")+1);
			}
			if (series_mode)
				push_unique(series_client_files, file);
			else
				fout << file << endl;
		}
		numEntries++;
	}

	if (!missing || (numskips && print_skip))
		log("\n");

	if (!just_testing)
	{
		string action = series_mode ? "Found " : "Wrote ";
		log(action + to_string(numEntries) + " entries. " + to_string(missing) + " files missing. " + to_string(numskips) + " files skipped.\n\n");
		fout.close(); 
	}
	else
	{
		log("Test finished. " + to_string(numEntries) + " files found. " + to_string(missing) + " files missing. " + to_string(numskips) + " files skipped.\n\n");
	}

	push_unique(archive_files, map_path + map + ".res");

	log_close();
	return 0;
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

bool archive_output(string archive_name)
{
	string archiver = "";

	// Attempt to find the 7z or 7za program
	#if defined(WIN32) || defined(_WIN32)
		string suppress_output = " > nul 2>&1";
		TCHAR x64[MAX_PATH];
		TCHAR x86[MAX_PATH];
		SHGetSpecialFolderPath(0, x64, CSIDL_PROGRAM_FILES, FALSE ); 
		SHGetSpecialFolderPath(0, x86, CSIDL_PROGRAM_FILESX86, FALSE ); 
		string program_files = x64;
		string program_files_x86 = x86;
		string zip64 = "\"" + program_files + "\\7-Zip\\7z.exe\"";
		string zip32 = "\"" + program_files_x86 + "\\7-Zip\\7z.exe\"";

		if (system("7z > nul 2>&1") == 0)
			archiver = "7z";
		else if (system(string(zip64 + suppress_output).c_str()) == 0)
			archiver = zip64;
		else if (system(string(zip32 + suppress_output).c_str()) == 0)
			archiver = zip32;
		else if (system("7za > nul 2>&1") == 0)
			archiver = "7za";
	#else
		string suppress_output = " > /dev/null 2> /dev/null";
		if (system(("7z" + suppress_output).c_str()) == 0)
			archiver = "7z";
		else if (system(("7za" + suppress_output).c_str()) == 0)
			archiver = "7za";
	#endif

	if (res_files_generated == 0)
		return false;

	if (archiver.length() == 0 && interactive)
		return false;

	string level = "-mx1";
	string ext = ".7z";
	bool make_archive = true;

	if (interactive)
	{
		if (res_files_generated > 1)
			cout << "Archive these " << res_files_generated << " maps?\n\n";
		else
			cout << "Archive this map?\n\n";
		cout 
		<< " 1. 7-Zip  Ultra   (best compression)\n"
		<< " 2. 7-Zip  Normal\n"
		<< " 3. Zip    Fast\n"
		<< " 4. Zip    Store   (no compression)\n"
		<< "\n 0. No\n\n";
	
		while (true)
		{
			char choice = _getch();
			if (choice == '0') make_archive = false;
			else if (choice == '1') {level = "-mx9 -mmt -m0=lzma2 -t7z"; ext = ".7z";}
			else if (choice == '2') {level = "-mx5 -mmt -m0=lzma2 -t7z"; ext = ".7z";}
			else if (choice == '3') {level = "-mx1 -mmt -tzip"; ext = ".zip";}
			else if (choice == '4') {level = "-mx0 -mmt -tzip"; ext = ".zip";}
			else continue;
			break;
		}
	}
	else if (archive_arg.length() > 0)
	{
		if (archive_arg.find("-7z") == 0) {
			string ilevel = "9";
			if (archive_arg.length() > 3)
				ilevel = to_string(atoi(archive_arg.substr(3,1).c_str()));
			level = "-mx" + ilevel + " -mmt -m0=lzma2 -t7z";
			ext = ".7z";
		}
		else if (archive_arg.find("-zip") == 0)
		{
			string ilevel = "1";
			if (archive_arg.length() > 4)
				ilevel = to_string(atoi(archive_arg.substr(4,1).c_str()));
			level = "-mx" + ilevel + " -mmt -tzip";
			ext = ".zip";
		}
		else
			return false;
		if (!archiver.length())
		{
			cout << "7-Zip program (7z or 7za) not found. Unable to create archive.\n";
			return false;
		}
	}
	else
		return false;

	if (make_archive)
	{
		vector<string> temp_files; // files that are temporarily copied to the current dir
		vector<string> temp_dirs; // directories that are temporarily created in the current dir
		string list_file = "resguy_7zip_file_list.txt";
		ofstream fout;
		fout.open (list_file, ios::out | ios::trunc);
		for (int i = 0; i < archive_files.size(); i++)
		{
			string path = archive_files[i];

			if (path[0] == '.' && path[1] == '.' && path[2] == '/')
			{
				// Any file path beginning with "../" is placed in the root of the archive.
				// The only way to correct this is to move those files into the current dir.
				// The list file doesn't allow you to choose where files go in the archive :<
				string new_path = path.substr(3);
				new_path = new_path.substr(new_path.find_first_of("/")+1);
						
				if (new_path.find_first_of("/") != string::npos)
				{
					string dir = new_path.substr(0, new_path.find_last_of("/"));
					string tdir = dir;
					while (tdir.length())
					{
						if (dirExists(tdir))
							break;
						push_unique(temp_dirs, tdir);
						size_t idir = tdir.find_last_of("/");
						if (idir == string::npos)
							break;
						tdir = tdir.substr(0, idir);
					}
					#if defined(WIN32) || defined(_WIN32)	
						string cmd = "mkdir \"" + dir + "\"" + suppress_output;
					#else
						string cmd = "mkdir -p \"" + dir + "\"" + suppress_output;
					#endif
					system(cmd.c_str());
				}

				#if defined(WIN32) || defined(_WIN32)	
					string old_path_win = replaceChar(path, '/', '\\');
					string new_path_win = replaceChar(new_path, '/', '\\');
					string cmd = "copy /Y \"" + old_path_win + "\" \"" + new_path_win + "\"" + suppress_output;
				#else
					string old_path_win = path;
					string new_path_win = new_path;
					string cmd = "cp \"" + old_path_win + "\" \"" + new_path_win + "\"" + suppress_output;
				#endif
				
				//cout << cmd << endl;
				cout << "Copying " << old_path_win << "\n";
				if (system(cmd.c_str()) == 0)
				{
					temp_files.push_back(new_path_win);
					path = new_path;
				}
			}
			#if defined(WIN32) || defined(_WIN32)	
				std::replace( path.begin(), path.end(), '/', '\\'); // convert to windows slashes
			#endif
			fout << path << endl;
		}
		fout.close();

		// choose a unique archive name
		string base_name = archive_name;
		for (int i = 2; i < 100; i++)
		{
			string fname = archive_name + ext;
			if (fileExists(fname))
				archive_name = base_name + "_" + to_string(i);
			else
				break;
		}
		string fname = archive_name + ext;
		if (fileExists(fname))
			remove((archive_name + ext).c_str());

		string cmd = archiver + " a " + level + " " + archive_name + " @" + list_file;
		bool success = system(cmd.c_str()) == 0;
		cout << endl;

		cout << "Cleaning up... ";
		remove("resguy_7zip_file_list.txt");
		for (int i = 0; i < temp_files.size(); i++)
			remove(temp_files[i].c_str());
		for (int i = 0; i < temp_dirs.size(); i++)
		{
			#if defined(WIN32) || defined(_WIN32)
				RemoveDirectory(temp_dirs[i].c_str());
			#else
				rmdir(temp_dirs[i].c_str());
			#endif
		}
		cout << "Done\n";

		cin.sync();

		if (!success)
		{
			cout << endl;
			if (interactive)
			{
				cout << "Archive creation failed! Try a different option.\n\n";
				return archive_output(archive_name);
			}
			else
			{
				cout << "Archive creation failed!\n\n";
				return true;
			}
		}

		cout << "\n\nCreated archive: " + archive_name + ext + "\n";

		if (interactive)
		{
			cout << "\nPress any key to continue...\n";
			_getch();
		}
	}
	return true;
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
			//if (arg == "-quiet")
			//	quiet_mode = true; // TODO
			if (arg == "-missing")
				include_missing = true;
			if (arg == "-missing3")
				write_separate_missing = true;
			if (arg == "-extra")
				client_files_only = false;
			if (arg == "-extra2")
				write_separate_server_files = true;
			if (arg == "-icase")
				case_sensitive_mode = false;
			if (arg == "-log")
				log_enabled = true;
			if (arg.find("-7z") == 0 || arg.find("-zip") == 0)
				archive_arg = arg.substr(0,4);
			if (arg == "-series")
				series_mode = true;
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
		cout << version_string << "\n\nWhat do you want to generate a .res file for?\n\nExamples:\n  stadium3 = target stadium3.bsp\n  stadium* = target all maps with a name that starts with \"stadium\"\n  *        = target all maps\n\nTarget: ";
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

	bool search_maps = map.find_first_of("*") != string::npos;

	string seperator = "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n\n";

	ask_for_opts:
	int ret = 0;
	vector<string> files;
	if (search_maps)
	{
		files = getDirFiles("maps/", "bsp", map);
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

				int ret = write_map_resources(f);

				if (ret == 1) goto ask_for_map;
				if (ret == -1) return 0;

				if (ret)
				{
					ret = 1;
					#ifdef _DEBUG
						system("pause");
					#endif
				}

				cout << seperator;
			}
		}
	}
	else
	{
		target_maps = "Generating .res file for:\n  " + map;
		files.push_back(map);
		int ret = write_map_resources(map);

		if (ret == 1) goto ask_for_map;
		if (ret == -1) return 0;

		if (ret)
			ret = 1;
	}
	
	if (series_mode && !just_testing && res_files_generated > 0)
	{
		cout << "Writing series .res files for " << files.size() << " maps (" << 
				series_client_files.size() << " entries)\n\n";
		sort( series_client_files.begin(), series_client_files.end(), stringCompare );
		for (int i = 0; i < files.size(); i++)
		{
			string f = bsp_name(files[i]);
			Bsp bsp(f);
			if (!bsp.valid)
				continue;
			
			ofstream fout;
			fout.open(bsp.path + f + ".res", ios::out | ios::trunc);
			fout << resguy_header;
			for (int k = 0; k < series_client_files.size(); k++)
			{
				if (series_client_files[k] == bsp.path + f + ".bsp")
					continue; // only the other bsps should be listed
				fout << series_client_files[k] << endl;
			}
			fout.close();
		}
	}
	else if (series_mode && just_testing)
	{
		cout << "Series .res files would contain " << series_client_files.size() << " entries\n\n";
	}

#ifdef _DEBUG
	system("pause");
#endif

	if (!archive_output(search_maps ? "resguy_output" : "resguy_" + map) && interactive)
	{
		cout << "Press any key to continue...\n";
		_getch();
	}

	if (interactive)
	{
		archive_files.clear();
		series_client_files.clear();
		res_files_generated = 0;

		if (map_not_found)
			goto ask_for_map;
		else
		{
			chose_opts = false;
			goto ask_for_opts;
		}
	}

	return ret;
}