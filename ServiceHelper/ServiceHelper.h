#ifndef SERVICE_HELPER_H
#define SERVICE_HELPER_H
#pragma once


#include <iostream>
#include <array>

#include <filesystem>
#include <fstream>

#include <QString>

#include "RegistryManager.h"



//char* base64(std::string string);
//
//char* decodeBase64(std::string string);

// String Sanatizer provided by Simple on Stackoverflow
// https://stackoverflow.com/a/34221488

class ServiceHelper
{
public:

	static std::array<std::string, 2> timestamp();

	static std::string GetExportsPath(std::string app_path = "");

	static std::string GetLogsPath(std::string app_path = "");

	static void WriteToLog(std::string log);

	static void WriteToError(std::string log);

	static void WriteToCustomLog(std::string log, std::string logName);

	static long int microseconds();

	static bool Contain(QString str, QString search);

	static const char* fileBasename(QString path);

	static const char* get_file_contents(const char* filename);

	static const char* GetFileExtension(const QString& FileName);

	static void sanitize(std::string& stringValue);

	static void removeQuotes(std::string& stringValue);

private:
	static void WriteLog(char* targetFile, std::string log);

};

#endif