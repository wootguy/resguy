#include "Mdl.h"
#include <algorithm>
#include <string.h>

Mdl::Mdl(string fname)
{
	valid = false;

	if (!contentExists(fname, true))
		return;

	if (get_ext(fname) != "mdl")
		return;

	this->fname = fname;
	name = fname;
	int idir = fname.find_last_of("/\\");
	if (idir != string::npos && idir < fname.length() - 1)
		name = name.substr(idir+1);

	int iext = name.find_last_of(".");
	if (iext != string::npos && iext < name.length() - 1)
		name = name.substr(0, iext);

	valid = true;
}

Mdl::~Mdl()
{

}


vector<string> Mdl::get_resources()
{
	vector<string> resources;

	string model_path = fname;
	int mdir = fname.find("models/");
	if (mdir != string::npos)
		model_path = fname.substr(mdir);

	mdir = model_path.find_last_of("\\/");
	if (mdir != string::npos)
		model_path = model_path.substr(0, mdir) + "/";

	//cout << "SEARCH NAME " << name << endl;

	if (name.length() == 0)
		return resources;

	// Add player model preview
	if (normalize_path(model_path).find("models/player/") != string::npos)
		resources.push_back(model_path + name + ".bmp");
	
	ifstream fin (fname, ios::binary);

	//
	// Verify the model is valid
	//

	studiohdr_t mdlHead;
	fin.read((char*)&mdlHead, sizeof(studiohdr_t));

	if (string(mdlHead.name).length() <= 0)
	{
		cout << "ERROR: Can't get dependencies from T model: " << fname << endl;
		fin.close();
		return resources;
	}

	if (mdlHead.id != 1414743113)
	{
		cout << "ERROR: Invalid ID in model header: " << fname << endl;
		fin.close();
		return resources;
	}

	if (mdlHead.version != 10)
	{
		cout << "ERROR: Invalid version in model header: " << fname << endl;
		fin.close();
		return resources;
	}

	push_unique(resources, model_path + name + ".mdl");

	// animation models (01/02/03.mdl)
	bool missing_animations = false;
	if (mdlHead.numseqgroups > 1)
	{
		for (int m = 1; m < mdlHead.numseqgroups; m++)
		{
			string suffix = m < 10 ? "0" + to_string(m) : to_string(m);
			string apath = model_path + name + suffix + ".mdl";
			string apath2 = apath;
			push_unique(resources, apath);
		}
	}


	// T model
	int numTextures = mdlHead.numtextures;
	bool uses_t_model = numTextures == 0;
	if (numTextures == 0)
	{
		string t_path = model_path + name + "t.mdl";
		string t_path2 = t_path;
		push_unique(resources, t_path);
	}


	// sounds and sprites attached to events
	for (int i = 0; i < mdlHead.numseq; i++)
	{
		fin.seekg(mdlHead.seqindex + i*sizeof(mstudioseqdesc_t));

		mstudioseqdesc_t seq;
		fin.read((char*)&seq, sizeof(mstudioseqdesc_t));
	
		for (int k = 0; k < seq.numevents; k++)
		{
			fin.seekg(seq.eventindex + k*sizeof(mstudioevent_t));
			mstudioevent_t evt;
			fin.read((char*)&evt, sizeof(mstudioevent_t));

			string opt = evt.options;
			if (get_ext(opt).length() == 0)
				continue;

			if (evt.event == 1004 || evt.event == 1008 || evt.event == 5004) // play sound
			{
				string snd = evt.options;
				if (snd[0] == '*')
					snd = snd.substr(1); // not sure why some models do this but it looks pointless.
				snd = normalize_path("sound/" + snd);
				push_unique(resources, snd);
			}
			if (evt.event == 5001 || evt.event == 5011 || evt.event == 5021 || evt.event == 5032) // muzzleflash sprite
			{
				string snd = normalize_path(evt.options);
				push_unique(resources, snd);
			}
			if (evt.event == 5005) // custom muzzleflash
			{
				string muzzle_txt = normalize_path(string("events/") + evt.options);
				push_unique(resources, muzzle_txt);
				
				string muzzle_txt_path = muzzle_txt;
				if (!contentExists(muzzle_txt_path, true))
					continue;

				// parse muzzleflash config for sprite name
				ifstream file(muzzle_txt_path);
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

						if (line.find("spritename") == 0)
						{
							string val = trimSpaces(line.substr(line.find("spritename")+strlen("spritename")));
							val.erase(std::remove(val.begin(), val.end(), '\"'), val.end());
							push_unique(resources, val);
						}
					}
				}
				file.close();
			}
		}
	}
	

	fin.close();
	

	return resources;
}