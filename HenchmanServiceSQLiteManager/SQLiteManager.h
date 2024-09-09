#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#pragma once

#include <iostream>
#include <Windows.h>
//#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <sstream>

using namespace std;

class SQLite_Manager
{
	string dbName = "henchmanService.db3";
	bool logToConsole = false;
public:
	SQLite_Manager();
	void ToggleConsoleLogging();
	int InitDB();
	int CreateTable(string table, vector<string> &cols);
};

#endif
