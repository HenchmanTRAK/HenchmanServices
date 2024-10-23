#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H
#pragma once


#include <iostream>
#include <optional>
#include <vector>
#include <string>

#include <QObject>
#include <QString>
//#include <QSqlQueryModel>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
//#include <QHttpMultiPart>
//#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QRestAccessManager>
#include <QNetworkReply>
#include <QRestReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QRegularExpression>
#include <QTimer>
#include <QCoreApplication>
#include <QMap>

#include <SimpleIni.h>

#include "HenchmanServiceException.h"
#include "ServiceHelper.h"
#include "RegistryManager.h"


/**
 * @class DatabaseManager
 *
 * @brief The DatabaseManager class provides functions for managing database connections and executing SQL queries.
 *
 * This class allows you to connect to a local MySQL database and a remote MySQL database, execute SQL queries, and retrieve query results.
 * It provides methods for connecting to the databases, executing SQL queries, and retrieving query results.
 *
 * @author Willem Swanepoel
 * @version 2.0
 *
 * @details
 * - The DatabaseManager class uses the MySQL Connector/C++ library for database operations.
 * - The class handles exceptions for common errors that may occur during database operations.
 * - The class provides a convenient way to manage database connections and execute SQL queries within your application.
 *
 * @see QSqlDatabase
 * @see QSqlQuery
 */
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @var bool requestRunning
     *
     * @brief Flag indicating whether a request is currently running.
     *
     * This flag is used to prevent multiple requests from running concurrently.
     */
    bool requestRunning;

    /**
     * @var int queryLimit
     *
     * @brief The maximum number of queries to retrieve from the cloudupdate table.
     *
     * This variable determines the number of queries to fetch from the cloudupdate table and execute on the remote API.
     */
    int queryLimit = 10;

    /**
     * @var std::string targetApp
     *
     * @brief The target application for the database operations.
     *
     * This variable stores the name of the target application for the database operations.
     */
    std::string targetApp;

    /**
     * @brief Constructs a DatabaseManager object.
     *
     * This constructor initializes the DatabaseManager object with the specified database names.
     *
     * @param parent - The parent object of the DatabaseManager instance. Defaults to nullptr.
     * 
     * @throws None
     */
    DatabaseManager(QObject* parent = nullptr);

    /**
     * @brief Destroys the DatabaseManager object and performs cleanup operations.
     *
     * This destructor performs cleanup operations by deleting the QSqlDatabase objects and setting their pointers to nullptr.
     */
	~DatabaseManager();

    /**
     * @brief Connects to the remote database and performs various configuration steps if required.
     *
     * Establishes baseline connection settings to the remote MySQL database and ensures the desired database exists within it.
     *
     * @return Returns 1 if the connection to the remote database is successful, otherwise returns 0.
     *
     * @throws Throws an exception if there is an error opening the database connection or if there is an error creating the database.
     */
    int connectToRemoteDB();

    /**
     * @brief Connects to the local database and performs various configuration steps if required.
     *
     * Establishes baseline connection settings to the local MySQL database and ensures the desired database exists within it.
     *
     * @return Returns 1 if the connection to the local database is successful, otherwise returns 0.
     *
     * @throws Throws an exception if there is an error opening the database connection or if there is an error creating the database.
     */
    int connectToLocalDB();

    /**
    * @brief Executes a target SQL script file on a local database.
    * 
    * Takes a passed SQL script path and executes it query by query on the local database.
    *
    * @param filepath - The path to the SQL script file.
    *
    * @return Returns the number of SQL statements successfully executed.
    *
    * @throws None
    */
    int ExecuteTargetSqlScript(std::string& filepath);

    /**
     * @brief Executes a SQL query on a local database and returns the results as a vector of QMap objects.
     *
     * This function connects to a local database, executes the given SQL query, and returns the results as a vector of QMap objects.
     * Each QMap object represents a row in the query result and contains the column names as keys and the corresponding values as values.
     *
     * @param sqlQuery - The SQL query to execute.
     *
     * @return Returns a vector of QMap objects representing the query results. Each QMap object contains the column names as keys and the corresponding values as values.
     *
     * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the local database.
    */
    std::vector<QMap<QString, QString>> ExecuteTargetSql(std::string sqlQuery);

    /**
     * @brief Checks if the internet connection is available by attempting to connect to www.google.com on port 80.
     *
     * This function creates a QTcpSocket object, connects to www.google.com on port 80, and waits for the connection to be established.
     * If the connection is successful within the specified timeout, the function returns true. Otherwise, it returns false.
     *
     * @return Returns true if the internet connection is available, otherwise returns false.
     *
     * @throws None
    */
    bool isInternetConnected();

    /**
     * @brief Cleans up resources used by the DatabaseManager.
     *
     * This function deletes the QNetworkAccessManager and QRestAccessManager instances used by the DatabaseManager,
     * setting their pointers to nullptr.
     *
     * @throws None.
    */
    void performCleanup();

    /**
     * @brief Adds tools to the database if they do not already exist.
     *
     * This function retrieves a list of tools from the local database and checks if each tool already exists on the remote database.
     * If a tool does not exist, it is inserted into the remote database using an SQL query.
     *
     * @return Returns 0 if the function completes successfully, otherwise returns an error code.
     *
     * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
    */
    int AddToolsIfNotExists();

    /**
     * @brief Adds missing itemkabs to the database if they do not already exist.
     *
     * This function checks the number of itemkabs in the database and if it is less than the specified number of kabs to check,
     * it retrieves the missing itemkabs from the cloudupdate table and adds them to the database.
     *
     * @return Returns 0 if the number of itemkabs in the database is greater than or equal to the number of kabs to check,
     *         otherwise returns 1.
     *
     * @throws None.
     */
    int AddKabsIfNotExists();

    /**
     * @brief Adds missing itemkabdrawers to the database if they do not already exist.
     *
     * This function checks the number of itemkabdrawers in the database and if it is less than the specified number of drawers to check,
     * it retrieves the missing itemkabdrawers from the cloudupdate table and adds them to the database.
     *
     * @return Returns 0 if the number of itemkabdrawers in the database is greater than or equal to the number of drawers to check,
     *         otherwise returns 1.
     *
     * @throws None.
     */
    int AddDrawersIfNotExists();

    /**
     * @brief Adds missing tools in drawers to the database if they do not already exist.
     *
     * This function checks the number of tools in drawers in the database and if it is less than the specified number of tools to check,
     * it retrieves the missing tools in drawers from the cloudupdate table and adds them to the database.
     *
     * @return Returns 0 if the number of tools in drawers in the database is greater than or equal to the number of tools to check,
     *         otherwise returns 1.
     *
     * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
     */
    int AddToolsInDrawersIfNotExists();

    /**
     * @brief Parses a QJsonArray into a string representation.
     *
     * @param array The QJsonArray to be parsed.
     *
     * @return The string representation of the QJsonArray.
     *
     * @throws None.
    */
    static std::string parseArray(QJsonArray array);

    /**
     * @brief Parses a QJsonObject and returns a formatted string.
     *
     * This function takes a QJsonObject and recursively parses its keys and values.
     * If a key's value is a string, the key-value pair is added to the formatted string.
     * If a key's value is an object, the function calls itself to parse the nested object.
     * If a key's value is an array, the function calls another function to parse the array.
     *
     * @param object The QJsonObject to be parsed.
     *
     * @return The formatted string representing the parsed QJsonObject.
     *
     * @throws None.
    */
    static std::string parseObject(QJsonObject object);

public slots:
    /**
    * @brief Parses the data received from a network request.
    *
    * This function takes the network reply and performs various operations on it.
    * It checks for network and HTTP errors, logs the response status, executes a SQL query,
    * reads the JSON response, and prints any errors or data received.
    *
    * @throws Throws an exception if there is a network or HTTP error, or if there is an error
    *         executing the SQL query or parsing the JSON response.
    */
    void parseData(QNetworkReply* netReply);

private:
    //QNetworkRequest* request;
    QNetworkAccessManager* netManager = nullptr;
    //QNetworkReply* netReply;
    QRestAccessManager* restManager = nullptr;
    bool testingDBManager = false;
    QString apiUsername = "";
    QString apiPassword = "";
    QString apiUrl = "";
    int numToolsChecked = 0;
    int numKabsChecked = 0;
    int numDrawersChecked = 0;
    int numToolsInDrawersChecked = 0;

    /**
     * @brief Sends a network request to the specified URL with the provided query data.
     *
     * Ensures the request has appropriate headers and parses the data returned before logging it, then returns the pure json through the results pointer.
     *
     * @param url The URL to send the network request to.
     * @param query The query data to be sent in the request.
     * @param results The QJsonDocument to store the response in.
     *
     * @return Returns 1 if the network request was successful, otherwise returns 0.
     *
     * @throws Throws an exception if there is a network or HTTP error, or if there is an error executing the SQL query or parsing the JSON response.
    */
    int makeNetworkRequest(QString &url, QMap<QString, QString> &query, QJsonDocument& results);
};

#endif