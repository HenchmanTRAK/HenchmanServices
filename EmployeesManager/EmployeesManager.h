#ifndef EMPLOYEES_MANAGER_H
#define EMPLOYEES_MANAGER_H
#pragma once

//#ifdef EMPLOYEES_MANAGER_EXPORTS
//#define EMPLOYEES_MANAGER_ __declspec(dllexport)
//#else
//#define EMPLOYEES_MANAGER_ __declspec(dllimport)
//#endif

//#if defined(EMPLOYEES_MANAGER)
//#  define EMPLOYEES_MANAGER_EXPORT Q_DECL_EXPORT
//#else
//#  define EMPLOYEES_MANAGER_EXPORT Q_DECL_IMPORT
//#endif

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include "RegistryManager.h"
#include "QueryManager.h"
#include "NetworkManager.h"


struct TrakDetails {
	int cust_id = 0;
	QString schema = "";
	QString trak_type = "";
	QString trak_id_type = "";
	QString trak_id = "";
};

struct WebportalDetails {
	int cust_id = 0;
	QString api_url = "";
	QString api_key = "";
};

class EmployeesManager : public QObject
{
	Q_OBJECT
private:
	int local_employees_count = 0;
	int remote_employees_count = 0;

	TrakDetails trak_details;
	WebportalDetails webportal_details;

	QueryManager queryManager;
	NetworkManager networkManager;

public:

public:
	EmployeesManager(QObject* parent = nullptr, const TrakDetails& trakDetails = TrakDetails(), const WebportalDetails& webportalDetails = WebportalDetails());
	~EmployeesManager();

	int GetLocalEmployeeCount(const QList<QString>& conditions = QList<QString>(), const QJsonObject& placeholders = QJsonObject());

	int GetRemoteEmployeeCount(const QJsonObject& select= QJsonObject(), const QJsonObject& where = QJsonObject());

	QJsonArray GetRemoteEmployees(const QJsonArray& columns = QJsonArray(), const QJsonObject& where = QJsonObject(), const QJsonObject& p_select = QJsonObject());

	QJsonArray GetLocalEmployees(const QString& query = QString(""), const QJsonObject& placeholders = QJsonObject());

	QJsonArray GetGroupedRemoteEmployees(const QJsonArray& columns = QJsonArray(), const QJsonArray& grouped_columns = QJsonArray(), const QString& type = QString("COUNT"), const QString& separator = QString(""), const QJsonObject& where = QJsonObject());

	int SendEmployeeToRemote(const QJsonObject& employee = QJsonObject(), const QJsonObject& data = QJsonObject());
	
	int UpdateRemoteEmployee(const QJsonObject& employee = QJsonObject(), const QJsonObject& data = QJsonObject());
	
	QJsonArray UpdateLocalEmployee(const QList<QString>& update = QList<QString>(), const QJsonObject& placeholders = QJsonObject());

	
	int ClearCloudUpdate();

	QJsonArray GetColumns();

	static void breakoutValuesToUpdate(const QString& currentDateTime, const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values);
};

Q_DECLARE_METATYPE(EmployeesManager);

#endif