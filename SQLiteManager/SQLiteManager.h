#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#pragma once

#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

//#include <Windows.h>

#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>


#include "HenchmanServiceException.h"
#include "ServiceHelper.h"


/**
 * @class SQLite_Manager
 *
 * @brief The SQLite_Manager class provides functions for managing an SQLite database.
 *
 * This class allows you to interact with an SQLite database, creating tables, inserting rows, and retrieving data.
 * It provides methods for initializing the database, creating tables, inserting rows, and retrieving all rows from a table.
 * The class also provides methods for getting the name of the database and toggling console logging.
 *
 * @author Willem Swanepoel
 * @version 1.0
 *
 * @details
 * - The SQLite_Manager class uses the SQLite C++ library for database operations.
 * - The class handles exceptions for common errors that may occur during database operations.
 * - The class provides a convenient way to manage an SQLite database within your application.
 *
 * @see SQLite
 * @see SQLite::Exception
 */
class SQLite_Manager
{
	std::string dbName;
	std::string dbDir;
	bool logToConsole;
public:
	/**
	 * @brief Constructs a SQLite_Manager object with the given database directory and name.
	 *
	 * @param db_dir - The directory where the database file will be stored.
	 * @param db_name - The name of the database file.
	 *
	 * @throws exception If there is an error opening or creating the database file.
	 */
	SQLite_Manager(std::string db_dir = "", std::string db_name = "");

	/**
	 * @brief Destructor for the SQLite_Manager class.
	 *
	 * This function is called when an instance of the SQLite_Manager class is destroyed.
	 * It clears the dbName and dbDir strings, and sets the logToConsole flag to NULL.
	 *
	 * @throws None
	 */
	~SQLite_Manager();

	/**
	 * @brief Toggles the logging to console for the SQLite_Manager.
	 *
	 * @throws None
	 */
	void ToggleConsoleLogging();

	/**
	 * @brief Initializes the SQLite database by creating a table named "test" with two columns: "id" and "value".
	 *
	 * @return 0 if the database initialization is successful, otherwise an exception is thrown.
	 *
	 * @throws exception if there is an error opening or creating the database file.
	 */
	int InitDB();
	
	/**
	 * @brief Returns the name of the database as a string.
	 *
	 * @return The name of the database as a string.
	 */
	std::string GetDBName();

	/**
	 * @brief Creates a table in the SQLite database with the given table name and columns.
	 *
	 * @param tableName - The name of the table to be created.
	 * @param cols - A vector of strings representing the columns of the table.
	 *
	 * @throws exception If there is an error creating the table.
	 */
	int CreateTable(std::string & tableName, std::vector<std::string> &cols);

	/**
	 * @brief Inserts a row into the specified table with the given values.
	 *
	 * @param tableName - The name of the table to insert the row into.
	 * @param values - A vector of strings representing the values to insert.
	 *
	 * @throws exception If there is an error inserting the row.
	 */

	int AddRow(std::string& tableName, std::vector<std::string>& values);
};

/**
 * @brief Retrieves all rows from the specified table.
 *
 * @param tableName - The name of the table to retrieve rows from.
 *
 * @return A vector of vectors of strings, where each inner vector represents a row and contains the values of each column.
 *
 * @throws exception If there is an error retrieving the rows.
std::vector<std::vector<std::string>> GetAllRows(const std::string& tableName);
 */

#endif
