#ifndef SERVICE_HELPER_H
#define SERVICE_HELPER_H
#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <iostream>
#include <time.h>
#include <sstream>
#include <fstream>
#include <filesystem>

using namespace std;

long int microseconds();
bool Contain(string str, string search);
string fileBasename(string path);
string get_file_contents(const char* filename);
string GetFileExtension(const string& FileName);
char* base64(string string);
char* decodeBase64(string string);
string GetExportsPath(string app_path = "");
string GetLogsPath(string app_path = "");
void WriteToLog(string log);
void WriteToError(string log);
#endif