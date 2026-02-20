
#ifndef QUERY_MANAGER_H
#define QUERY_MANAGER_H
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

#include <QObject>
#include <QString>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>

#include "ServiceHelper.h"
#include "HenchmanServiceException.h"

// QUERY_MANAGER_EXPORT
class QueryManager : public QObject
{
	Q_OBJECT

private:
	QMutex* p_thread_controller = nullptr;
	QString schema;

public:
	QueryManager(QObject* parent = nullptr, const QString &target_schema = "");
	~QueryManager();

	void setSchema(const QString& new_schema = "");
	QString getSchema();

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
	std::vector<QStringMap> ExecuteTargetSql(const std::string& sqlQuery, const std::map<std::string, QVariant>& params = std::map<std::string, QVariant>());

	std::vector<QStringMap> ExecuteTargetSql(const std::wstring& sqlQuery) const;

	/*std::vector<QStringMap> ExecuteTargetSql(const QString& sqlQuery, const QStringMap& params = QStringMap());*/

	std::vector<QStringMap> ExecuteTargetSql(const QString& sqlQuery, const QVariantMap& params = QVariantMap());

	std::vector<QStringMap> ExecuteTargetSql(const TCHAR* sqlQuery, const std::map<const TCHAR*, const TCHAR*>& params = std::map<const TCHAR*, const TCHAR*>());

	QJsonArray ExecuteTargetSql(const std::string& sqlQuery, const QJsonObject& params);
	QJsonArray ExecuteTargetSql(const QString& sqlQuery, const QJsonObject& params);
	QJsonArray ExecuteTargetSql(const TCHAR* sqlQuery, const QJsonObject& params);

	QJsonArray ExecuteTargetSql_Array(const QString& sqlQuery, const QMap<QString, QVariant>& params);

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
	int ExecuteTargetSqlScript(const std::string& filepath) const;
};

Q_DECLARE_METATYPE(QueryManager);

#endif