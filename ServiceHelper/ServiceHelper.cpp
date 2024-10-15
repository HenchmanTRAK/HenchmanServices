

#include "ServiceHelper.h"

using namespace std;

long int microseconds() 
{
	struct timespec tp;
	timespec_get(&tp, TIME_UTC);
	long int ms = tp.tv_sec * 1000 + tp.tv_nsec / 1000;
	return ms;
}

bool Contain(QString str, QString search)
{
	//cout << "searching: " << str.data() << " for: " << search.data() << endl;
	size_t found = str.toStdString().find(search.toStdString());
	if (found != string::npos) {
		return 1;
	}
	return 0;
}

const char * fileBasename(QString path)
{
	return path.toStdString().substr(path.toStdString().find_last_of("/\\") + 1).data();
	// without extension
	// string::size_type const p(base_filename.find_last_of('.'));
	// string file_without_extension = base_filename.substr(0, p);
}

const char * get_file_contents(const char* filename)
{
	bool result = false;
	if (ifstream is{ filename, ios::binary | ios::ate })
	{
		auto size = is.tellg();
		QString str(size, '\0'); // construct string to stream size
		is.seekg(0);
		is.read(&str.toStdString()[0], size);
		/*if (is)
			cout << str << '\n';*/
		return str.toStdString().data();
	}
	throw(errno);
}

const char * GetFileExtension(const QString& FileName)
{
	if (FileName.toStdString().find_last_of(".") != string::npos)
		return FileName.toStdString().substr(FileName.toStdString().find_last_of(".") + 1).data();
	return "";
}

//char* base64(string string)
//{
//	// Credit to mtrw from Stackoverflow
//	auto pl = 4 * ((string.size() + 2) / 3);
//	calloc(pl + 1, 1); //+1 for the terminating null that EVP_EncodeBlock adds on
//	unsigned char* output;
//	auto ol = EVP_EncodeBlock(output, reinterpret_cast<unsigned char*>(string.data()), string.size());
//	if (pl != ol) { cerr << "Whoops, encode predicted " << pl << " but we got " << ol << "\n"; }
//	return output;
//}
//
//char* decodeBase64(string string)
//{
//	auto pl = (3 * (string.size() / 4));
//	auto output = reinterpret_cast<char*>(calloc(pl + 1, 1)); //+1 for the terminating null that EVP_EncodeBlock adds on
//	auto ol = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(output), reinterpret_cast<unsigned char*>(string.data()), string.size());
//	if (pl != ol) { cerr << "Whoops, decode predicted " << pl << " but we got " << ol << "\n"; }
//	return output;
//}

string GetExportsPath(string app_path) 
{
	string exportsPath;
	int _results = 0;
	char buff[1024];
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
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
	installDir.clear();
	return exportsPath;
}

string GetLogsPath(string app_path) 
{
	string logsPath;
	int _results = 0;
	char buff[1024];
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
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
	installDir.clear();
	return logsPath;
}

void WriteLog(char *targetFile, string log)
{
	fstream fs(targetFile, ios::out | ios_base::app);
	if (fs) {
		time_t timer = time(NULL);
		struct tm currDateTime = *localtime(&timer);
		char dateBuf[120];
		char timeBuf[120];
		strftime(dateBuf, sizeof(dateBuf), "%F", &currDateTime);
		strftime(timeBuf, sizeof(timeBuf), "%T", &currDateTime);
		cout << "---| " << dateBuf << " " << timeBuf << " |--- " << log << endl;
		fs << "---| " << dateBuf << " " << timeBuf << " |--- " << log << "\n";
		fs.close();
	}

}

void WriteToLog(string log)
{
	string logDir = GetLogsPath();
	logDir.append("log.txt");
	WriteLog(logDir.data(), log);
	logDir.clear();
	log.clear();
}

void WriteToError(string log)
{
	string logDir = GetLogsPath().data();
	logDir.append("error.txt");
	WriteLog(logDir.data(), log);
	logDir.clear();
	log.clear();
}

void WriteToCustomLog(string log, string logName)
{
	string logDir = GetLogsPath();
	logDir.append(logName+".txt");
	WriteLog(logDir.data(), log);
	logDir.clear();
	log.clear();
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