#pragma once

//#ifdef QUERY_MANAGER_EXPORTS
//#define QUERY_MANAGER_ __declspec(dllexport)
//#else
//#define QUERY_MANAGER_ __declspec(dllimport)
//#endif

//#if defined(QUERY_MANAGER)
//#  define QUERY_MANAGER_EXPORT Q_DECL_EXPORT
//#else
//#  define QUERY_MANAGER_EXPORT Q_DECL_IMPORT
//#endif

#include <vector>
#include <variant>
#include <exception>

#include <QObject>
#include <QString>
#include <QMap>
#include <QThread>

#include <QMutex>
#include <QMutexLocker>

#include <QJsonArray>
#include <QJsonObject>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlDriver>
#include <QSqlResult>
#include <QSqlQueryModel>

#include "ServiceHelper.h"
#include "HenchmanServiceException.h"


struct s_TZ_INFO {
	QString time_zone = "";
	int time_zone_offset = 0;
};

struct s_DATABASE_INFO {
	QString driver = "";
	QString schema = "";
	QString username = "";
	QString password = "";
	QString server = "";
	int port = 0;
	QStringList conn_options = QStringList();
	QString table = "";
};

// QUERY_MANAGER_EXPORT
class QueryManager : public QObject
{
	Q_OBJECT

//public:

private:
	//QMutex p_thread_controller;

	s_TZ_INFO tz_info;
	s_DATABASE_INFO db_info;


public:
	QueryManager(QObject* parent = nullptr, const s_DATABASE_INFO& database_info = s_DATABASE_INFO());

	~QueryManager();

	void setSchema(const QString& new_schema);
	void set_database_details(const s_DATABASE_INFO& database_info);

	QString getSchema();

	QList<QVariantMap> execute(const TCHAR* sql, const QVariantMap& placeholders);
	QList<QVariantMap> execute(const QString& sql, const QVariantMap& placeholders);
	QList<QVariantMap> execute(const std::string& sql, const QVariantMap& placeholders);

	QJsonArray execute(const TCHAR* sql, const QJsonObject& placeholders);
	QJsonArray execute(const QString& sql, const QJsonObject& placeholders);
	QJsonArray execute(const std::string& sql, const QJsonObject& placeholders);

	QList<QStringMap> execute(const TCHAR* sql, const QStringMap& placeholders);
	QList<QStringMap> execute(const QString& sql, const QStringMap& placeholders);
	QList<QStringMap> execute(const std::string& sql, const QStringMap& placeholders);

	std::vector<QStringMap> execute(const TCHAR* sql);
	std::vector<QStringMap> execute(const QString& sql);
	std::vector<QStringMap> execute(const std::string& sql);

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


	QVariantMap recordToMap(const QSqlRecord& record);
	QVariantMap processPlaceholders(const QVariantMap& placeholders, QString* statement);
	void BindValues(const QVariantMap& values, QSqlQuery* query);

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

	s_TZ_INFO GetTimezone();
private:

	QSqlDatabase GetDatabaseConnection();

};

Q_DECLARE_METATYPE(QueryManager)
