#include "ServiceHelper.h"

long int microseconds() 
{
	struct timespec tp;
	timespec_get(&tp, TIME_UTC);
	long int ms = tp.tv_sec * 1000 + tp.tv_nsec / 1000;
	return ms;
}


bool Contain(string str, string search) 
{
	//cout << "searching: " << str.data() << " for: " << search.data() << endl;
	size_t found = str.find(search);
	if (found != string::npos) {
		return 1;
	}
	return 0;
}


string fileBasename(string path) 
{
	string filename = path.substr(path.find_last_of("/\\") + 1);
	return filename;
	// without extension
	// string::size_type const p(base_filename.find_last_of('.'));
	// string file_without_extension = base_filename.substr(0, p);
}

string get_file_contents(const char* filename)
{

	if (ifstream is{ filename, ios::binary | ios::ate })
	{
		auto size = is.tellg();
		string str(size, '\0'); // construct string to stream size
		is.seekg(0);
		is.read(&str[0], size);
		/*if (is)
			cout << str << '\n';*/
		return str.c_str();
	}
	throw(errno);
}

string GetFileExtension(const string& FileName)
{
	if (FileName.find_last_of(".") != string::npos)
		return FileName.substr(FileName.find_last_of(".") + 1);
	return "";
}
