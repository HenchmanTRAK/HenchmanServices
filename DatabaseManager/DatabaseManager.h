#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H
#pragma once

#include <string>
//#include <mysql/jdbc.h>
//#include <mysqlx/xdevapi.h>

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "ServiceHelper.h"
#include "RegistryManager.h"

int connectToRemoteDB(std::string & targetApp);
int connectToLocalDB(std::string& targetApp);
int ExecuteTargetSqlScript(std::string& targetApp, std::string& filename);

#endif