#include "Mdl.h"

Mdl::Mdl(string fname)
{
	valid = false;

	if (!contentExists(fname))
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


	// sounds attached to events
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
			if (evt.event == 1004 || evt.event == 1008 || evt.event == 5004)
			{
				string snd = normalize_path(string("sound/") + evt.options);
				push_unique(resources, snd);
			}
		}
	}
	

	fin.close();
	

	return resources;
}