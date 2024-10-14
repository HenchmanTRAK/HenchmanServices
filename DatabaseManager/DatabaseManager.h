#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H
#pragma once


#include <optional>
#include <string>
#include <vector>
//#include <mysql/jdbc.h>
//#include <mysqlx/xdevapi.h>

#include <QObject>
#include <QString>
#include <QSqlQueryModel>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QRestAccessManager>
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
* The DatabaseManager class is responsible for managing database connections and executing SQL queries.
*
* This class provides methods for connecting to both remote and local databases, as well as executing SQL scripts and queries.
*
* @class DatabaseManager
* @author Willem Swanepoel
* @version 1
*/
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    bool requestRunning;
    int queryLimit = 10;

    /**
    * Constructor for the DatabaseManager class.
    *
    * @param parent - The parent object of the DatabaseManager instance. Defaults to nullptr.
    *
    * @throws None
    */
    DatabaseManager(QObject* parent = nullptr);

    /**
    * Destructor for the DatabaseManager class.
    *
    * @throws None
    */
	~DatabaseManager();

    /**
    * Connects to a remote database and retrieves queries to execute.
    *
    * @param target_app - The name of the target application.
    *
    * @return Returns 0 if the connection to the local database is invalid, otherwise returns the number of queries retrieved.
    *
    * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the remote database.
    */
    int connectToRemoteDB(std::string& target_app);

    /**
    * Connects to a local database and performs various operations.
    *
    * @param target_app - The name of the target application.
    *
    * @return Returns 1 if the connection to the local database is successful, otherwise returns 0.
    *
    * @throws Throws an exception if there is an error opening the database connection or if there is an error creating the database.
    */
    int connectToLocalDB(std::string& target_app);

    /**
    * Executes a SQL script file on a local database.
    *
    * @param targetApp - The name of the target application.
    * @param filename - The path to the SQL script file.
    *
    * @return Returns the number of SQL statements successfully executed.
    *
    * @throws None
    */
    int ExecuteTargetSqlScript(std::string& targetApp, std::string& filename);

    /**
    * Executes a SQL query on a local database.
    *
    * @param targetApp - The name of the target application.
    * @param sqlQuery - The SQL query to execute.
    *
    * @return Returns the number of SQL statements successfully executed. Returns 0 if the connection to the local database is invalid.
    *
    * @throws Throws an exception if there is an error executing the query or if there is an error connecting to the local database.
    */
    int ExecuteTargetSql(std::string& targetApp, std::string& sqlQuery);

    bool isInternetConnected();

    void performCleanup();

public slots:
    /**
    * Parses the data received from a network request.
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
    std::string targetApp;
    //QNetworkRequest* request;
    QNetworkAccessManager* netManager = nullptr;
    //QNetworkReply* netReply;
    QRestAccessManager* restManager = nullptr;
    QHttpMultiPart* form = nullptr;
    bool testingDBManager = false;
    QString defaultProtocol = "";
    QString apiUsername = "";
    QString apiPassword = "";

    void makeNetworkRequest(QUrl &url, QMap<QString, QString> &query);
};

#endif