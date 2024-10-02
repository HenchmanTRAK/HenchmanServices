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
	std::string dbName;
	std::string dbDir;
	bool logToConsole;
public:
	/**
	* Constructs a SQLite_Manager object with the given database directory and name.
	*
	* @param db_dir - The directory where the database file will be stored.
	* @param db_name - The name of the database file.
	*
	* @throws exception If there is an error opening or creating the database file.
	*/
	SQLite_Manager(std::string db_dir = "", std::string db_name = "");

	/**
	* Destructor for the SQLite_Manager class.
	*
	* This function is called when an instance of the SQLite_Manager class is destroyed.
	* It clears the dbName and dbDir strings, and sets the logToConsole flag to NULL.
	*
	* @throws None
	*/
	~SQLite_Manager();

	/**
	* Toggles the logging to console for the SQLite_Manager.
	*
	* @throws None
	*/
	void ToggleConsoleLogging();
	/**
	* Initializes the SQLite database by creating a table named "test" with two columns: "id" and "value".
	*
	* @return 0 if the database initialization is successful, otherwise an exception is thrown.
	*
	* @throws exception if there is an error opening or creating the database file.
	*/
	int InitDB();
	
	/**
	* Returns the name of the database as a string.
	*
	* @return The name of the database as a string.
	*/
	std::string GetDBName();

	/**
	* Creates a table in the SQLite database with the given table name and columns.
	*
	* @param tableName - The name of the table to be created.
	* @param cols - A vector of strings representing the columns of the table.
	*
	* @return 0 if the table is successfully created, otherwise an exception is thrown.
	*
	* @throws exception If there is an error opening or creating the database file, or if the query execution fails.
	*/
	int CreateTable(std::string & tableName, std::vector<std::string> &cols);

	/**
	* Adds a row to the specified table in the SQLite database.
	*
	* @param targetTable - The name of the table to add the row to.
	* @param values - A vector of strings representing the values to be inserted into the row.
	*
	* @return 0 if the row is successfully added, otherwise an exception is thrown.
	*
	* @throws exception If there is an error opening or creating the database file, or if the query execution fails.
	*/
	int AddRow(std::string& targetTable, std::vector<std::string>& values);
};

#endif
