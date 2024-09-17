#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H
#pragma once

#include <iostream>
#include <string>
//#include <mysql/jdbc.h>
//#include <mysqlx/xdevapi.h>

#include <QString>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QTextStream>


#include "ServiceHelper.h"
#include "RegistryManager.h"

void connection(std::string& targetApp);
int ExecuteTargetSqlScript(std::string& targetApp, std::string& filename);

#endif