#ifndef SERVICE_HELPER_H
#define SERVICE_HELPER_H
#pragma once

#include <iostream>
#include <time.h>
#include <sstream>
#include <fstream>

using namespace std;

long int microseconds();
bool Contain(string str, string search);
string fileBasename(string path);
string get_file_contents(const char* filename);
string GetFileExtension(const string& FileName);

#endif