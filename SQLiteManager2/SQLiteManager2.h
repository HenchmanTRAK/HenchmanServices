#ifndef SQLITE_MANAGER_2_H
#define SQLITE_MANAGER_2_H

#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <thread>

#include <QObject>
#include <QString>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlQuery>

#include <Windows.h>

#include "HenchmanServiceException.h"
#include "RegistryManager.h"
#include "DatabaseManager.h"
#include "ServiceHelper.h"

typedef std::map<std::string, std::string> stringmap;

class SQLiteManager2 : public QObject
{
	Q_OBJECT

public:
	SQLiteManager2(QObject* parent = nullptr);

	~SQLiteManager2();

	int CreateTable(
		const std::string& table,
		const std::vector<std::string>& columns
	);

	int AddEntry(
		const std::string& tableName,
		const stringmap& data
	);

	int UpdateEntry(
		const std::string& tableName,
		const std::vector<std::string>& conditions,
		const stringmap& data
	);

	int RemoveEntry(
		const std::string& tableName,
		const std::vector<std::string>& conditions
	);

	std::vector<stringmap> GetEntry(
		const std::string& tableName,
		const std::vector<std::string>& selections,
		const std::vector<std::string>& conditions
	);

private:
	QString databaseName;
	QString dbDriver = "QSQLITE";

	void ExecQuery(
		const std::string& query,
		std::vector<stringmap>* results = nullptr
	);
};

#endif