#include "util.h"
#include "studio.h"

class Mdl
{
public:
	string name, fname;
	bool valid;

	studiohdr_t header;

	Mdl(string fname);
	~Mdl();

	vector<string> get_resources();
};