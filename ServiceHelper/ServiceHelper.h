#ifndef SERVICE_HELPER_H
#define SERVICE_HELPER_H
#pragma once

//#include <QtCore>

//#include "openssl/crypto.h"
//#include "openssl/err.h"
//#include "openssl/ssl.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <QString>
//#include <WinBase.h>

#include "RegistryManager.h"

long int microseconds();

bool Contain(QString str, QString search);

const char * fileBasename(QString path);

const char * get_file_contents(const char* filename);

const char * GetFileExtension(const QString& FileName);

//char* base64(std::string string);
//
//char* decodeBase64(std::string string);

std::string GetExportsPath(std::string app_path = "");

std::string GetLogsPath(std::string app_path = "");

void WriteToLog(std::string log);

void WriteToError(std::string log);

// String Sanatizer provided by Simple on Stackoverflow
// https://stackoverflow.com/a/34221488
void sanitize(std::string& stringValue);

void removeQuotes(std::string& stringValue);
#endif