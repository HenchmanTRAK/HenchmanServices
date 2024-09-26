

#include "ServiceHelper.h"

using namespace std;

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
	string targetFile = filename;
	if (ifstream is{ targetFile, ios::binary | ios::ate })
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

char* base64(string string)
{
	// Credit to mtrw from Stackoverflow
	const auto pl = 4 * ((string.size() + 2) / 3);
	auto output = reinterpret_cast<char*>(calloc(pl + 1, 1)); //+1 for the terminating null that EVP_EncodeBlock adds on
	const auto ol = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(output), reinterpret_cast<unsigned char*>(string.data()), string.size());
	if (pl != ol) { cerr << "Whoops, encode predicted " << pl << " but we got " << ol << "\n"; }
	return output;
}

char* decodeBase64(string string)
{
	const auto pl = (3 * (string.size() / 4));
	auto output = reinterpret_cast<char*>(calloc(pl + 1, 1)); //+1 for the terminating null that EVP_EncodeBlock adds on
	const auto ol = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(output), reinterpret_cast<unsigned char*>(string.data()), string.size());
	if (pl != ol) { cerr << "Whoops, decode predicted " << pl << " but we got " << ol << "\n"; }
	return output;
}

string GetExportsPath(string app_path) 
{
	string exportsPath;
	int _results = 0;
	char buff[1024];
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	string installDir = GetStrVal(hKey, "Install_DIR", REG_SZ);
	if (app_path == "" && installDir == "") {
		do {
			_results = GetCurrentDirectoryA(sizeof(buff), buff);
			exportsPath = buff;
		} while (_results > exportsPath.length() && exportsPath.data());
	}
	else if (installDir != "") {
		exportsPath = installDir;
	}
	else {
		exportsPath = app_path.substr(0, app_path.find_last_of("/\\"));
	}
	RegCloseKey(hKey);
	exportsPath.append("\\exports\\");
	if (!filesystem::is_directory(exportsPath.c_str())) {
		filesystem::create_directory(exportsPath.c_str());
	}
	return exportsPath;
}

string GetLogsPath(string app_path) 
{
	string logsPath;
	int _results = 0;
	char buff[1024];
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	string installDir = GetStrVal(hKey, "Install_DIR", REG_SZ);
	if (app_path == "" && installDir == "") {
		do {
			_results = GetCurrentDirectoryA(sizeof(buff), buff);
			logsPath = buff;
		} while (_results > logsPath.length() && logsPath.data());
	}
	else if (installDir != "") {
		logsPath = installDir;
	}
	else {
		logsPath = app_path.substr(0, app_path.find_last_of("/\\"));
	}
	RegCloseKey(hKey);
	logsPath.append("\\logs\\");
	if (!filesystem::is_directory(logsPath.c_str())) {
		filesystem::create_directory(logsPath.c_str());
	}
	return logsPath;
}


void WriteToLog(string log) 
{
	string logDir = GetLogsPath();
	logDir.append("log.txt");
	fstream fs(logDir.c_str(), ios::out | ios_base::app);
	if (fs) {
		time_t timer = time(NULL);
		struct tm currDateTime = *localtime(&timer);
		char dateBuf[120];
		char timeBuf[120];
		strftime(dateBuf, sizeof(dateBuf), "%F", &currDateTime);
		strftime(timeBuf, sizeof(timeBuf), "%T", &currDateTime);
		cout << "---| " << dateBuf << " " << timeBuf << " |--- " << log << endl;
		fs << "---| " << dateBuf << " " << timeBuf << " |--- ";
		fs << log << '\n';
		fs.close();
	}
}

void WriteToError(string log) 
{
	string logDir = GetLogsPath();
	logDir.append("error.txt");
	fstream fs(logDir.c_str(), ios::out | ios_base::app);
	if (fs) {
		time_t timer = time(NULL);
		struct tm currDateTime = *localtime(&timer);
		char dateBuf[120];
		char timeBuf[120];
		strftime(dateBuf, sizeof(dateBuf), "%F", &currDateTime);
		strftime(timeBuf, sizeof(timeBuf), "%T", &currDateTime);
		cerr << "---| " << dateBuf << " " << timeBuf << " |--- " << log << endl;
		fs << "---| " << dateBuf << " " << timeBuf << " |--- ";
		fs << log << '\n';
		fs.close();
	}
}

void sanitize(string& stringValue)
{
	// Add backslashes.
	for (auto i = stringValue.begin();;) {
		auto const pos = std::find_if(
			i, stringValue.end(),
			[](char const c) { return '\\' == c || '\'' == c || '"' == c; }
		);
		if (pos == stringValue.end()) {
			break;
		}
		i = std::next(stringValue.insert(pos, '\\'), 2);
	}

	// Removes others.
	stringValue.erase(
		std::remove_if(
			stringValue.begin(), stringValue.end(), [](char const c) {
				return '\n' == c || '\r' == c || '\0' == c || '\x1A' == c;
			}
		),
		stringValue.end()
	);
}

void removeQuotes(string& stringValue)
{
	// Removes others.
	stringValue.erase(
		std::remove_if(
			stringValue.begin(), stringValue.end(), [](char const c) {
				return '\'' == c || '"' == c;
			}
		),
		stringValue.end()
	);
}