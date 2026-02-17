#ifndef SQLITE_MANAGER_2_H
#define SQLITE_MANAGER_2_H
#pragma once

#ifdef SQLITE_MANAGER_2_LIBRARY_EXPORTS
#define SQLITE_MANAGER_2_LIBRARY_ __declspec(dllexport)
#else
#define SQLITE_MANAGER_2_LIBRARY_ __declspec(dllimport)
#endif

#if defined(SQLITE_MANAGER_2)
#  define SQLITE_MANAGER_2_EXPORT Q_DECL_EXPORT
#else
#  define SQLITE_MANAGER_2_EXPORT Q_DECL_IMPORT
#endif

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <QObject>
#include <QSqlError>

#include <QSettings>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>

//#include <Windows.h>

#include "HenchmanServiceException.h"
#include "RegistryManager.h"
#include "ServiceHelper.h"

/**
 * @class SQLiteManager2
 *
 * @brief The SQLiteManager2 class provides functions for managing SQLite database connections and executing SQL queries.
 *
 * This class allows you to connect to a local SQLite database, execute SQL queries, and retrieve query results.
 * It provides methods for connecting to the database, executing SQL queries, and retrieving query results.
 *
 * @author Willem Swanepoel
 * @version 1.0
 *
 * @details
 * - The SQLiteManager2 class uses the Qt SQLite driver for database operations.
 * - The class handles exceptions for common errors that may occur during database operations.
 * - The class provides a convenient way to manage SQLite database connections and execute SQL queries within your application.
 *
 * @see QSqlDatabase
 * @see QSqlQuery
 */
class SQLITE_MANAGER_2_EXPORT SQLiteManager2 : public QObject
{
	Q_OBJECT

private:
	/**
	 * @brief The database name.
	 *
	 * This is the name of the SQLite database file.
	 */
	std::string databaseName;

	/**
	 * @brief The driver name.
	 *
	 * This is the name of the Qt SQLite driver.
	 */
	QString databaseDriver = "QSQLITE";

	/**
	 * @brief The location of the SQLite database.
	 *
	 * This is the location of the SQLite database file.
	 */
	QString databaseLocation;

	//QSqlDatabase db;
	QMutex mutex;

public:

	/**
	 * @brief Constructs a SQLiteManager2 object.
	 *
	 * This constructor initializes the SQLiteManager2 object with the specified parent object.
	 *
	 * @param parent The parent object.
	 */
	SQLiteManager2(QObject* parent = nullptr);

	/**
	 * @brief Destroys the SQLiteManager2 object and performs cleanup operations.
	 *
	 * This destructor performs cleanup operations by deleting the QSqlDatabase object and setting its pointer to nullptr.
	 */
	~SQLiteManager2();

	/**
	 * @brief Creates a table in the SQLite database.
	 *
	 * This function creates a table in the SQLite database with the specified name and columns.
	 *
	 * @param table The name of the table to create.
	 * @param columns A vector of column names.
	 *
	 * @return Returns the number of rows affected by the query.
	 *
	 * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the SQLite database.
	 */
	int CreateTable(
		const std::string& table,
		const std::vector<std::string>& columns
	);

	/**
	 * @brief Adds an entry to a table in the SQLite database.
	 *
	 * This function adds an entry to a table in the SQLite database with the specified table name and data.
	 *
	 * @param tableName The name of the table to add the entry to.
	 * @param data A map of column names to values.
	 *
	 * @return Returns the number of rows affected by the query.
	 *
	 * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the SQLite database.
	 */
	int AddEntry(
		const std::string& tableName,
		const stringmap& data
	);

	/**
	 * @brief Updates an entry in a table in the SQLite database.
	 *
	 * This function updates an entry in a table in the SQLite database with the specified table name, conditions, and data.
	 *
	 * @param tableName The name of the table to update the entry in.
	 * @param conditions A vector of condition strings.
	 * @param data A map of column names to values.
	 *
	 * @return Returns the number of rows affected by the query.
	 *
	 * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the SQLite database.
	 */
	int UpdateEntry(
		const std::string& tableName,
		const std::vector<std::string>& conditions,
		const stringmap& data
	);

	/**
	 * @brief Removes an entry from a table in the SQLite database.
	 *
	 * This function removes an entry from a table in the SQLite database with the specified table name and conditions.
	 *
	 * @param tableName The name of the table to remove the entry from.
	 * @param conditions A vector of condition strings.
	 *
	 * @return Returns the number of rows affected by the query.
	 *
	 * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the SQLite database.
	 */
	int RemoveEntry(
		const std::string& tableName,
		const std::vector<std::string>& conditions
	);

	/**
	 * @brief Retrieves entries from a table in the SQLite database.
	 *
	 * This function retrieves entries from a table in the SQLite database with the specified table name, selections, and conditions.
	 *
	 * @param tableName The name of the table to retrieve entries from.
	 * @param selections A vector of column names to select.
	 * @param conditions A vector of condition strings.
	 *
	 * @return Returns a vector of maps, where each map represents a row in the query result.
	 *
	 * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the SQLite database.
	 */
	std::vector<stringmap> GetEntry(
		const std::string& tableName,
		const std::vector<std::string>& selections,
		const std::vector<std::string>& conditions = std::vector<std::string>()
	);

	/**
	 * @brief Executes a SQL query on the SQLite database.
	 *
	 * This function executes the specified SQL query on the SQLite database and returns the results as a QSqlQuery object.
	 *
	 * @param query The SQL query to execute.
	 * @param results A pointer to a vector of string maps to store the query results.
	 *
	 * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the SQLite database.
	 */
	void ExecQuery(
		const std::string& query,
		std::vector<stringmap> &results
	);
	void ExecQuery(
		const std::string& query
	);

private:
	void CreateNewDatabase(QSqlDatabase* db, const QString& databaseName);

};

Q_DECLARE_METATYPE(SQLiteManager2);

#endif