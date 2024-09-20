#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#pragma once

#include <Windows.h>

#include <vector>
#include <sstream>
#include <string>

#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>

#include "ServiceHelper.h"


// Manages the sqlite database associated with the HenchmanTRAK Service
class SQLite_Manager
{
	static std::string dbName;
	static std::string dbDir;
	static bool logToConsole;
public:
	// Creates the database file and marks it as hidden.
	SQLite_Manager(std::string db_dir = "", std::string db_name = "");
	~SQLite_Manager();
	// Toggles whether functions should output debug text or not.
	void ToggleConsoleLogging();
	// Ensures the sqlite database was successfully created. 
	int InitDB();
	// Returns the name of the database.
	std::string GetDBName();
	// Creates a table with the passed name and coloumns array in the sql database.
	int CreateTable(std::string &table, std::vector<std::string> &cols);
	// Add a row to the target table with the passed values.
	int AddRow(std::string& targetName, std::vector<std::string>& values);
};

#endif
