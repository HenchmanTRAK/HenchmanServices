#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H
#pragma once

#ifdef DATABASE_MANAGER_EXPORTS
#define DATABASE_MANAGER_ __declspec(dllexport)
#else
#define DATABASE_MANAGER_ __declspec(dllimport)
#endif

#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QObject>
#include <QRegularExpression>
#include <QRestAccessManager>
#include <QRestReply>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QList>
#include <QThread>

#include "HenchmanServiceException.h"
#include "RegistryManager.h"
#include "ServiceHelper.h"
#include "SQLiteManager2.h"


#define QT_NO_DEBUG_OUTPUT

typedef QMap<QString, QString> QStringMap;

enum table_enums{
	tools,
	users,
	employees,
	customer,
	jobs,
	kabs,
	drawers,
	toolbins,
	cribs,
	cribconsumables,
	cribtoollocation,
	cribtoollockers,
	cribtools,
	kittools,
	tooltransfer,
	itemscale,
	itemkits,
	kitcategory,
	kitlocation,
	kabemployeeitemtransactions,
	cribemployeeitemtransactions,
	portaemployeeitemtransactions,
	lokkaemployeeitemtransactions,
	tblcounterid
};

enum query_types_enum {
	SELECT,
	INSERT,
	UPDATE,
	REMOVE
};

static QMap<QString, query_types_enum> query_types = {
	{"insert", INSERT},
	{"select", SELECT},
	{"update", UPDATE},
	{"delete", REMOVE},
};

static QMap<QString, table_enums> table_map = {
	{"tools", tools},
	{"users", users},
	{"customer", customer},
	{"employees", employees},
	{"jobs", jobs},
	{"itemkabs", kabs},
	{"itemkabdrawers", drawers},
	{"itemkabdrawerbins", toolbins},
	{"cribs", cribs},
	{"cribconsumables", cribconsumables},
	{"cribtoollocation", cribtoollocation},
	{"cribtoollocations", cribtoollocation},
	{"cribtoollockers", cribtoollockers},
	{"cribtools", cribtools},
	{"kittools", kittools},
	{"tooltransfer", tooltransfer},
	{"itemscale", itemscale},
	{"itemkits", itemkits},
	{"kitcategory", kitcategory},
	{"kitlocation", kitlocation},
	{"kabemployeeitemtransactions", kabemployeeitemtransactions},
	{"cribemployeeitemtransactions", cribemployeeitemtransactions},
	{"portaemployeeitemtransactions", portaemployeeitemtransactions},
	{"lokkaemployeeitemtransactions", lokkaemployeeitemtransactions},
	{"tblcounterid", tblcounterid}
};

/**
 * @class DatabaseManager
 *
 * @brief The DatabaseManager class represents a manager for database operations in the HenchmanService application.
 *
 * This class contains the necessary data members and member functions to manage database operations, including connecting to the database, executing queries, and retrieving results.
 *
 * @author Willem Swanepoel
 * @version 2.0
 *
 * @details
 * The class has the following data members:
 * - `QObject* parent`: The parent object of the DatabaseManager instance.
 * - `netManager`: The network manager for database connections.
 * - `restManager`: The REST manager for database operations.
 *
 * The class has the following member functions:
 * - `DatabaseManager(QObject* parent = nullptr)`: The constructor for the DatabaseManager class.
 * - `~DatabaseManager()`: The destructor for the DatabaseManager class.
 * - `void performCleanup()`: Performs cleanup operations for the DatabaseManager object.
 */
class DatabaseManager : public QObject
{
	Q_OBJECT

private:

	/**
	 * @var netManager
	 *
	 * @brief The network manager for database connections.
	 *
	 * This object is used to make network requests to the database server.
	 */
	QNetworkAccessManager* netManager = nullptr;

	QTcpSocket* sock = nullptr;

	/**
	 * @var restManager
	 *
	 * @brief The REST manager for database operations.
	 *
	 * This object is used to make RESTful network requests to the database server.
	 */
	QRestAccessManager* restManager = nullptr;

	QNetworkCookieJar* cookieJar = nullptr;

	/**
	 * @var testingDBManager
	 *
	 * @brief A flag indicating whether the database manager is in testing mode.
	 *
	 * This flag is used to determine whether the database manager should use test data or not.
	 */
	bool testingDBManager = false;

	bool shouldIgnoreDatabaseCustId = false;
	bool shouldIgnoreDatabaseTrakId = false;

	/**
	 * @brief The API username.
	 *
	 * This is the username used to authenticate with the database server.
	 */
	QString apiUsername = "";

	/**
	 * @brief The API password.
	 *
	 * This is the password used to authenticate with the database server.
	 */
	QString apiPassword = "";

	/**
	 * @brief The API URL.
	 *
	 * This is the URL of the database server.
	 */
	QString apiUrl = "";

	/**
	 * @brief The API Key.
	 *
	 * This is used to authenticate the service connection with the api server.
	 */
	QString apiKey = "";

	/**
	 * @brief A map of database tables and their corresponding check status.
	 *
	 * This map is used to keep track of the status of each database table.
	 */
	QMap<QString, int> databaseTablesChecked = {
		// General
		{"tools", 0},
		{"users", 0},
		{"employees", 0},
		{"jobs", 0},
		// Kabtraks
		{"kabs", 0},
		{"kabDrawers", 0},
		{"kabDrawerBins", 0},
		// Cribtraks
		{"cribs", 0},
		{"toolLocation", 0},
		{"cribtools", 0},
		{"tooltransfer", 0},
		{"cribconsumables", 0},
		{"kittools", 0},
		// Portatracks
		{"scales", 0},
		{"itemkits", 0},
		{"kitCategory", 0},
		{"kitLocation", 0},
	};

	std::string trakType;
	std::string trakId;
	int custId;
	QString trakIdNum;

	//QNetworkRequest request;

	SQLiteManager2 sqliteManager;

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

	QString databaseDriver;

public:
	/**
	 * @brief Constructs a DatabaseManager object.
	 *
	 * This constructor initializes the DatabaseManager object with the specified parent object.
	 *
	 * @param parent The parent object of the DatabaseManager instance.
	 */
	DatabaseManager(QObject* parent = nullptr);

	/**
	 * @brief Destroys the DatabaseManager object.
	 *
	 * This destructor performs cleanup operations for the DatabaseManager object.
	 */
	~DatabaseManager();

	void loadTrakDetailsFromRegistry();

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
	int ExecuteTargetSqlScript(const std::string& filepath);

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
	std::vector<QStringMap> ExecuteTargetSql(const std::string& sqlQuery, const stringmap& params = stringmap());

	std::vector<QStringMap> ExecuteTargetSql(const std::wstring &sqlQuery);
	
	std::vector<QStringMap> ExecuteTargetSql(const QString& sqlQuery, const QStringMap& params = QStringMap());

	std::vector<QStringMap> ExecuteTargetSql(const TCHAR* sqlQuery, const std::map<const TCHAR*, const TCHAR*>& params = std::map<const TCHAR*, const TCHAR*>());

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
	 * @brief Performs cleanup operations for the DatabaseManager object.
	 *
	 * This member function performs cleanup operations for the DatabaseManager object,
	 * including deleting the network manager and REST manager.
	 */
	void performCleanup();

	//General Table Upload

	/**
	 * @brief Adds tools to the database if they do not already exist.
	 *
	 * This function retrieves a list of tools from the local database and checks if each tool already exists on the remote database.
	 * If a tool does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of tools in the database is lesser than or equal to the number of tools checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	*/
	int addToolsIfNotExists();

	/**
	 * @brief Adds users to the database if they do not already exist.
	 *
	 * This function retrieves a list of users from the local database and checks if each user already exists on the remote database.
	 * If an user does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of users in the database is lesser than or equal to the number of users checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addUsersIfNotExists();

	/**
	 * @brief Adds employees to the database if they do not already exist.
	 *
	 * This function retrieves a list of employees from the local database and checks if each employee already exists on the remote database.
	 * If an employee does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of employees in the database is lesser than or equal to the number of employees checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addEmployeesIfNotExists();

	/**
	 * @brief Adds jobs to the database if they do not already exist.
	 *
	 * This function retrieves a list of jobs from the local database and checks if each job already exists on the remote database.
	 * If a job does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of jobs in the database is lesser than or equal to the number of jobs checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addJobsIfNotExists();

	// Upload Kabtrak Tables

	/**
	 * @brief Adds missing itemkabs to the database if they do not already exist.
	 *
	 * This function checks the number of itemkabs in the database and if it is less than the specified number of kabs to check,
	 * it retrieves the missing itemkabs from the cloudupdate table and adds them to the database.
	 *
	 * @return Returns 0 if the number of kabs in the database is lesser than or equal to the number of kabs checked, otherwise returns 1.
	 *
	 * @throws None.
	 */
	int addKabsIfNotExists();

	/**
	 * @brief Adds missing itemkabdrawers to the database if they do not already exist.
	 *
	 * This function checks the number of itemkabdrawers in the database and if it is less than the specified number of drawers to check,
	 * it retrieves the missing itemkabdrawers from the cloudupdate table and adds them to the database.
	 *
	 * @return Returns 0 if the number of drawers in the database is lesser than or equal to the number of drawers checked, otherwise returns 1.
	 *
	 * @throws None.
	 */
	int addDrawersIfNotExists();

	/**
	 * @brief Adds missing tools in drawers to the database if they do not already exist.
	 *
	 * This function checks the number of tools in drawers in the database and if it is less than the specified number of tools to check,
	 * it retrieves the missing tools in drawers from the cloudupdate table and adds them to the database.
	 *
	 * @return Returns 0 if the number of tools in drawers in the database is lesser than or equal to the number of drawers tools in drawers checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addToolsInDrawersIfNotExists();

	int createKabtrakTransactionsTable();

	// Upload Cribtrak Tables

	/**
	 * @brief Adds cribTRAKs to the database if they do not already exist.
	 *
	 * This function retrieves a list of cribTRAKs from the local database and checks if each cribTRAK already exists on the remote database.
	 * If a cribTRAK does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of cribTRAKs in the database is lesser than or equal to the number of cribTRAKs checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addCribsIfNotExists();

	/**
	 * @brief Adds missing cribTRAK tool locations to the database if they do not already exist.
	 *
	 * This function retrieves a list of locations from the local database and checks if each location already exists on the remote database.
	 * If a location does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of tool locations in the database is lesser than or equal to the number of tool locations checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addCribToolLocationIfNotExists();

	/**
	 * @brief Adds missing cribTRAK tools to the database if they do not already exist.
	 *
	 * This function retrieves a list of tools from the local database and checks if each tool already exists on the remote database.
	 * If a tool does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of cribTRAK tools in the database is lesser than or equal to the number of cribTRAK tools checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addCribToolsIfNotExists();

	/**
	 * @brief Adds missing tool transactions to the database if they do not already exist.
	 *
	 * This function retrieves a list of tool transactions from the local database and checks if each transaction already exists on the remote database.
	 * If a tool transaction does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of tool transactions in the database is lesser than or equal to the number of tool transactions checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addCribToolTransferIfNotExists();

	/**
	 * @brief Adds missing cribtrak consumables to the database if they do not already exist.
	 *
	 * This function retrieves a list of cribtrak consumables from the local database and checks if each consumable already exists on the remote database.
	 * If a cribtrak consumable does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of cribtrak consumable in the database is lesser than or equal to the number of cribtrak consumables checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addCribConsumablesIfNotExists();

	/**
	 * @brief Adds missing cribtrak kits to the database if they do not already exist.
	 *
	 * This function retrieves a list of cribtrak kits from the local database and checks if each consumable already exists on the remote database.
	 * If a cribtrak kit does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of cribtrak kits in the database is lesser than or equal to the number of cribtrak kits checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addCribKitsIfNotExists();

	int createCribtrakTransactionsTable();

	// Upload Portatrak Tables

	/**
	 * @brief Adds portaTRAKs to the database if they do not already exist.
	 *
	 * This function retrieves a list of portaTRAKs from the local database and checks if each portaTRAK already exists on the remote database.
	 * If a portaTRAK does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of cribTRAKs in the database is lesser than or equal to the number of portaTRAKs checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addPortasIfNotExists();

	/**
	 * @brief Adds missing tool transactions to the database if they do not already exist.
	 *
	 * This function retrieves a list of tool transactions from the local database and checks if each transaction already exists on the remote database.
	 * If a tool transaction does not exist, it is inserted into the remote database using an SQL query.
	 *
	 * @return Returns 0 if the number of tool transactions in the database is lesser than or equal to the number of tool transactions checked, otherwise returns 1.
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addItemKitsIfNotExists();

	/**
	 * @brief Adds a kit category to the database if it does not already exist.
	 *
	 * This function checks if a kit category name from the local database already exists in the remote database.
	 * If it does not exist, the function adds the kit category to the remote database.
	 *
	 * @return Returns 0 if the number of kit categories in the local database is lesser than or equal to the number of kit categories checked, otherwise returns 1
	 * 
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addKitCategoryIfNotExists();

	/**
	 * @brief Adds a kit location to the database if it does not already exist.
	 *
	 * This function checks if a kit location from the local database already exists in the remote database.
	 * If it does not exist, the function adds the kit location to the remote database.
	 *
	 * @return Returns 0 if the number of kit locations in the local database is lesser than or equal to the number of kit locations checked, otherwise returns 1
	 *
	 * @throws Throws an exception if there is an error executing the SQL query or if there is an error connecting to the target database.
	 */
	int addKitLocationIfNotExists();

	int createPortatrakTransactionsTable();

	/**
	 * @brief Parses a QJsonArray into a string representation.
	 *
	 * @param array The QJsonArray to be parsed.
	 *
	 * @return The string representation of the QJsonArray.
	 *
	 * @throws None.
	*/
	static std::string parseData(QJsonArray array);

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
	static std::string parseData(QJsonObject object);

public slots:

	/**
	 * @brief Parses the data received from a network request.
	 *
	 * This function takes the network reply and performs various operations on it.
	 * It checks for network and HTTP errors, logs the response status, executes a SQL query,
	 * reads the JSON response, and prints any errors or data received.
	 *
	 * @param netReply The QNetworkReply object containing the response from the network request.
	 *
	 * @throws Throws an exception if there is a network or HTTP error, or if there is an error
	 *         executing the SQL query or parsing the JSON response.
	 */
	void parseData(QNetworkReply* netReply);

private:

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
	int makeNetworkRequest(const QString &url, const QStringMap &query, QJsonDocument* results);

	int authenticateSession(const QString& url = "");
	
	int makeGetRequest(const QString& url, const QStringMap &queryMap, QJsonDocument* results);
	
	int makePostRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results);

	int makePatchRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results);

	int makeDeleteRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results);

	/**
	 * @brief Processes the keys and values in the provided map and stores the results in the provided results array.
	 *
	 * This function iterates over the keys in the provided map and performs the following operations:
	 * - If the key is not "id", it adds quotes around the corresponding value.
	 * - It appends the key and the quoted value to the queryKeys and queryValues strings, respectively.
	 *
	 * After processing all the keys and values, the function stores the queryKeys and queryValues strings in the provided results array.
	 *
	 * @param map The map containing the keys and values to process.
	 * @param results The array to store the processed queryKeys and queryValues strings.
	 */
	void processKeysAndValues(const QStringMap& map, QString(&results)[]);

	void processInsertStatement( QString& query,  QJsonObject& data, bool& skipQuery);

	void processUpdateStatement( QString& query,  QJsonObject& data, bool& skipQuery);

	void processDeleteStatement( QString& query, QJsonObject& data, bool& skipQuery);

};

std::string getValidDrivers();

#endif