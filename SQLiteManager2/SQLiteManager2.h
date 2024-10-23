#ifndef SQLITE_MANAGER_2_H
#define SQLITE_MANAGER_2_H

#pragma once

#include <iostream>
#include <string>

#include <QObject>
#include <QString>
#include <QSettings>
#include <QSqlDatabase>

#include <Windows.h>

#include "RegistryManager.h"
#include "DatabaseManager.h"
#include "ServiceHelper.h"

class SQLiteManager2 : public QObject
{
	Q_OBJECT

public:
	SQLiteManager2(QObject* parent = nullptr);

	~SQLiteManager2();
private:
	QString databaseName;
	QString dbDriver = "QSQLITE";
};

#endif