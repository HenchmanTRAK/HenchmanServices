

#include "ServiceHelper.h"

ServiceHelper::ServiceHelper(const std::source_location& caller)
{
	
	int substringStart = std::string(caller.function_name()).find_first_of(" ") + 1;
	int substringEnd = std::string(caller.function_name()).find_first_of("(") - substringStart;
	
	std::vector<std::string> exploded = ExplodeString((
		substringStart >= substringEnd 
		? __FUNCTION__
		: std::string(caller.function_name())
		.substr(
			substringStart,
			substringEnd
		)), " ");
	
	exploded = ExplodeString(exploded[exploded.size() - 1], "::", 2);
	functionName = "";
	for (int i = 0; i < exploded.size(); i++)
	{
		functionName.append(exploded[i]);
		if(i < exploded.size() - 1)
			functionName.append("::");
	}
}

std::vector<std::string> ServiceHelper::ExplodeString(std::string targetString, const char *seperator, int maxLen)
{
	std::vector<std::string> results;
	try {
		if (targetString == "")
			throw ("No String was provided");
		if (seperator == "")
			throw ("Invalid Seperator provided");

		size_t pos = 0;
		while ((pos = targetString.find(seperator)) != std::string::npos) {
			std::string token = targetString.substr(0, pos);
			targetString.erase(0, pos + std::string(seperator).length());
			if(token != "")
				results.push_back(token);
		}
		if (targetString != "")
			results.push_back(targetString);
		if (maxLen > 0 && results.size() > maxLen)
		{
			results.resize(maxLen);
		}
			
	}
	catch (std::exception& e)
	{
		ServiceHelper().WriteToError(e.what());
	}
	return results;
}

QList<QString> ServiceHelper::ExplodeString(QString targetString, const char *seperator, int maxLen)
{
	QList<QString> results;
	try {
		if (targetString == "")
			throw "No String was provided";
		if (seperator == "")
			throw "Invalid Seperator provided";

		size_t pos = 0;
		while ((pos = targetString.indexOf(seperator)) != std::string::npos) {
			QString token = targetString.mid(0, pos);
			targetString.remove(0, pos + QString(seperator).length());
			if (token != "")
				results.push_back(token);
		}
		if (targetString != "")
			results.push_back(targetString);
		if (maxLen > 0 && results.size() > maxLen)
		{
			results.resize(maxLen);
		}

	}
	catch (std::exception& e)
	{
		ServiceHelper().WriteToError(e.what());
	}
	return results;
}

long int ServiceHelper::microseconds()
{
	struct timespec tp;
	timespec_get(&tp, TIME_UTC);
	long int ms = tp.tv_sec * 1000 + tp.tv_nsec / 1000;
	return ms;
}

bool ServiceHelper::Contain(QString str, QString search)
{
	
	size_t found = str.toStdString().find(search.toStdString());
	if (found != std::string::npos) {
		return 1;
	}
	return 0;
}

std::string ServiceHelper::fileBasename(const std::string& path)
{
	return path.substr(path.find_last_of("/\\") + 1);
	// without extension
	// string::size_type const p(base_filename.find_last_of('.'));
	// string file_without_extension = base_filename.substr(0, p);
}

char * ServiceHelper::get_file_contents(const char* filename)
{
	bool result = false;
	if (std::ifstream is{ filename, std::ios::binary | std::ios::ate })
	{
		auto size = is.tellg();
		std::string str(size, '\0'); // construct string to stream size
		is.seekg(0);
		is.read(&str[0], size);
		
		return (char *)str.data();
	}
	throw(errno);
}

char * ServiceHelper::GetFileExtension(std::string& FileName)
{
	if (FileName.find_last_of(".") != std::string::npos)
		return (char *)FileName.substr(FileName.find_last_of(".") + 1).data();
	return (char *)"";
}

std::string ServiceHelper::GetServicePath(std::string app_path)
{

	std::string installDir;
	TCHAR buffer[MAX_PATH];
	DWORD size = MAX_PATH;
	DWORD _results;

	if (app_path.empty())
	{
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\HenchmanService").c_str());
		rtManager.GetVal("INSTALL_DIR", REG_SZ, (char*)buffer, size);
		installDir = buffer;
		//return app_path.ends_with("\\") ? app_path.substr(0, app_path.find_last_of("/\\")) : app_path;
	}
	else
	{
		installDir = app_path.ends_with("\\") ? app_path.substr(0, app_path.find_last_of("/\\")) : app_path;
	}

	if (installDir.empty())
	{
		do {
			_results = GetCurrentDirectory(size, buffer);
			installDir = buffer;
		} while (_results > installDir.length() && !installDir.empty());
	}

	return installDir;

}

std::string ServiceHelper::GetExportsPath(std::string app_path)
{
	std::string exportsPath = GetServicePath().append("\\exports\\");

	if (!std::filesystem::is_directory(exportsPath.c_str())) {
		std::filesystem::create_directory(exportsPath.c_str());
	}

	return exportsPath;
}

std::string ServiceHelper::GetLogsPath(std::string app_path)
{
	std::string exportsPath = GetServicePath().append("\\logs\\");

	if (!std::filesystem::is_directory(exportsPath.c_str())) {
		std::filesystem::create_directory(exportsPath.c_str());
	}

	return exportsPath;
}

std::array<std::string, 2> ServiceHelper::timestamp()
{
	time_t timer = time(NULL);
	struct tm currDateTime = *localtime(&timer);
	char dateBuf[120];
	char timeBuf[120];
	strftime(dateBuf, sizeof(dateBuf), "%F", &currDateTime);
	strftime(timeBuf, sizeof(timeBuf), "%T", &currDateTime);
	std::string date(dateBuf);
	std::string time(timeBuf);
	return { date, time };
}

void ServiceHelper::WriteLog(char *targetFile, const std::string& log)
{
	std::fstream fs(targetFile, std::ios::out | std::ios_base::app);
	if (fs) {
		std::array<std::string, 2> dateTime = timestamp();
		fs << "---| " << dateTime[0] << " " << dateTime[1] << " |--- " << functionName << ": " << log << std::endl;
		fs.close();
	}

}

void ServiceHelper::WriteToLog(std::string log)
{
	std::string logDir = GetLogsPath();
	logDir.append(timestamp()[0] + "-log.txt");
	LOG << logDir << log.data();
	WriteLog((char *)logDir.data(), log);
	logDir.clear();
	log.clear();
}

void ServiceHelper::WriteToError(std::string log)
{
	std::string logDir = GetLogsPath();
	logDir.append(timestamp()[0] + "-error.txt");
	LOG << logDir << log.data();
	WriteLog((char *)logDir.data(), log);
	WriteToLog("Logged an error");
	logDir.clear();
	log.clear();
}

void ServiceHelper::WriteToCustomLog(std::string log, std::string logName)
{
	std::string logDir = GetLogsPath();
	logDir.append(logName + ".txt");
	LOG << logDir << log.data();
	WriteLog((char *)logDir.data(), log);
	WriteToLog("Logged to " + logName);
	logDir.clear();
	log.clear();
}

void ServiceHelper::ConsoleLog(const char* log)
{
	std::array<std::string, 2> dateTime = timestamp();
	std::cout << "|-- " << dateTime[0] << " " << dateTime[1] << " --| <" << functionName << "> |" << log << std::endl;
	//qDebug() << "|-- " << dateTime[0] << " " << dateTime[1] << " --| <" << functionName << "> |" << log << std::endl;
}

void ServiceHelper::sanitize(std::string& stringValue)
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

void ServiceHelper::removeQuotes(std::string& stringValue)
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

int ServiceHelper::ShellExecuteApp(std::string appName, std::string params)
{
	SHELLEXECUTEINFO SEInfo;
	DWORD ExitCode;
	std::string exeFile = appName;
	std::string paramStr = params;
	std::string StartInString;

	if (!std::filesystem::exists(appName)) {
		ServiceHelper().WriteToError("Could not find Target EXE: " + appName);
		return 0;
	}


	// fine the windows handle using https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/obtain-console-window-handle

	ZeroMemory(&SEInfo, sizeof(SEInfo));
	SEInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	SEInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	//SEInfo.hwnd = NULL;
	//SEInfo.lpVerb = NULL;
	//SEInfo.lpDirectory = NULL;
	SEInfo.lpFile = exeFile.data();
	SEInfo.lpParameters = paramStr.data();
	//: SW_HIDE
	SEInfo.nShow = paramStr == "" && SW_NORMAL;
	ServiceHelper().WriteToLog("Executing target: " + appName + params);
	if (ShellExecuteEx(&SEInfo)) {
		std::stringstream exitCodeMessage;
		//do {
		GetExitCodeProcess(SEInfo.hProcess, &ExitCode);
		exitCodeMessage << "Target: " << exeFile << " return exit code : " << std::to_string(ExitCode);
		ServiceHelper().WriteToLog(exitCodeMessage.str());
		exitCodeMessage.clear();
		//} while (ExitCode != STILL_ACTIVE);
		return 1;
	}

	return 0;
}

std::wstring ServiceHelper::s2ws(const std::wstring& str)
{
	return str;
}

std::wstring ServiceHelper::s2ws(const std::string& str)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.from_bytes(str);
}

std::string ServiceHelper::ws2s(const std::string& wstr)
{
	return wstr;
}

std::string ServiceHelper::ws2s(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

ServiceHelper& ServiceHelper::operator<<(const char* s) 
{
#ifdef DEBUG
	ConsoleLog(s);
#endif
	return *this;
}

ServiceHelper& ServiceHelper::operator<<(const std::string& s)
{
#ifdef DEBUG
	ConsoleLog(s.data());
#endif
	return *this;
}

ServiceHelper& ServiceHelper::operator<<(const QString& s)
{
#ifdef DEBUG
	ConsoleLog(s.toStdString().data());
#endif
	return *this;
}

ServiceHelper& ServiceHelper::operator<<(const QByteArray& s)
{
#ifdef DEBUG
	ConsoleLog(s.toStdString().data());
#endif
	return *this;
}

ServiceHelper& ServiceHelper::operator<<(const int& s)
{
#ifdef DEBUG
	ConsoleLog(std::to_string(s).data());
#endif
	return *this;
}

//template<typename T>
ServiceHelper& ServiceHelper::operator<<(const std::vector<std::string>& s)
{
#ifdef DEBUG
	qDebug() << s;
#endif
	return *this;
}