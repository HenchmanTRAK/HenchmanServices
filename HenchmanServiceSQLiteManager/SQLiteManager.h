#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#pragma once

#include <Windows.h>
//#include <sqlite3.h>
#include <sstream>

#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>

#include "ServiceHelper.h"

using namespace std;

// Manages the sqlite database associated with the HenchmanTRAK Service
class SQLite_Manager
{
	static string dbName;
	static string dbDir;
	static bool logToConsole;
public:
	// Creates the database file and marks it as hidden.
	SQLite_Manager(string db_dir = "", string db_name = "");
	~SQLite_Manager();
	// Toggles whether functions should output debug text or not.
	void ToggleConsoleLogging();
	// Ensures the sqlite database was successfully created. 
	int InitDB();
	// Returns the name of the database.
	string GetDBName();
	// Creates a table with the passed name and coloumns array in the sql database.
	int CreateTable(string &table, vector<string> &cols);
	// Add a row to the target table with the passed values.
	int AddRow(string& targetName, vector<string>& values);
};

#endif
