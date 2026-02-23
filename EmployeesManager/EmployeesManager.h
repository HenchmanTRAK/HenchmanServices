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
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFuture>
#include <QFutureSynchronizer>
#include <QtConcurrentMap>
#include <QtConcurrentRun>

#include "RegistryManager.h"
#include "SQLiteManager2.h"
#include "QueryManager.h"
#include "NetworkManager.h"

#include <WinError.h>


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
	int query_limit = 0;
};


class EmployeesManager : public QObject
{
	Q_OBJECT
private:
	int local_employees_count = 0;
	int remote_employees_count = 0;
	QString time_zone;

	TrakDetails trak_details;
	WebportalDetails webportal_details;
	s_DATABASE_INFO db_info;

	SQLiteManager2 sqliteManager;
	QueryManager queryManager;
	NetworkManager networkManager;

public:
	enum NEXTSTEP {
		ALL_UPDATED = 0,
		SYNC_PORTAL,
		SYNC_LOCAL,
		UPDATE_OUTDATED,
		CHECK_NEXT_STEP,
	};

public:
	EmployeesManager(QObject* parent = nullptr, const TrakDetails& trakDetails = TrakDetails(), const WebportalDetails& webportalDetails = WebportalDetails(), const s_DATABASE_INFO& database_info = s_DATABASE_INFO());
	~EmployeesManager();

	int GetLocalEmployeeCount(const QList<QString>& conditions = QList<QString>(), const QJsonObject& placeholders = QJsonObject());

	int GetRemoteEmployeeCount(const QJsonObject& select= QJsonObject(), const QJsonObject& where = QJsonObject(), QJsonObject* p_returned_data = nullptr);

	QJsonArray GetRemoteEmployees(const QJsonArray& columns = QJsonArray(), const QJsonObject& where = QJsonObject(), const QJsonObject& p_select = QJsonObject());

	QJsonArray GetLocalEmployees(const QString& query = QString(""), const QJsonObject& placeholders = QJsonObject());

	QJsonArray GetGroupedRemoteEmployees(const QJsonArray& columns = QJsonArray(), const QJsonArray& grouped_columns = QJsonArray(), const QJsonArray& group_by = QJsonArray(), const QString& type = QString("COUNT"), const QString& separator = QString(""), const QJsonObject& p_where = QJsonObject());

	int SendEmployeeToRemote(const QJsonObject& employee = QJsonObject(), const QJsonObject& data = QJsonObject());
	int CreateLocalEmployee(const QJsonObject& employee = QJsonObject());
	
	int UpdateRemoteEmployee(const QJsonObject& employee = QJsonObject(), const QJsonObject& data = QJsonObject());
	
	QJsonArray UpdateLocalEmployee(const QList<QString>& update = QList<QString>(), const QJsonObject& placeholders = QJsonObject());
	QMap<int, QList<QVariantMap>> UpdateLocalEmployee(const QList<QString>& update = QList<QString>(), const QVariantMap& placeholders = QVariantMap());

	int UpdateOutdatedEmployees();

	int SyncWebportalEmployees();

	int SyncLocalEmployees();

	int GetCurrentState();

	int ClearCloudUpdate();

	QJsonArray GetColumns();

private:
	static void breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values);
	static void breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values);
	
	void HandleUpdatingEmployeeEntries(const QJsonObject& local, const QJsonObject& remote);

	int UpdateCheckedTime();
};

#define NEXT_STEP EmployeesManager::NEXTSTEP

constexpr auto EMPLOYEES_ALL_UPDATED(int val) { return val == EmployeesManager::NEXTSTEP::ALL_UPDATED; };
constexpr auto EMPLOYEES_SYNC_PORTAL(int val) { return val == EmployeesManager::NEXTSTEP::SYNC_PORTAL; };
constexpr auto EMPLOYEES_SYNC_LOCAL(int val) { return val == EmployeesManager::NEXTSTEP::SYNC_LOCAL; };
constexpr auto EMPLOYEES_UPDATE_OUTDATED(int val) { return val == EmployeesManager::NEXTSTEP::UPDATE_OUTDATED; };
constexpr auto EMPLOYEES_(int val, int status) {
	switch (status) {
	case EmployeesManager::NEXTSTEP::ALL_UPDATED: {
		return val == EmployeesManager::NEXTSTEP::ALL_UPDATED;
	}
	case EmployeesManager::NEXTSTEP::SYNC_PORTAL: {
		return val == EmployeesManager::NEXTSTEP::SYNC_PORTAL;
	}
	case EmployeesManager::NEXTSTEP::SYNC_LOCAL: {
		return val == EmployeesManager::NEXTSTEP::SYNC_LOCAL;
	}
	case EmployeesManager::NEXTSTEP::UPDATE_OUTDATED: {
		return val == EmployeesManager::NEXTSTEP::UPDATE_OUTDATED;
	}
	case EmployeesManager::NEXTSTEP::CHECK_NEXT_STEP: {
		return val == EmployeesManager::NEXTSTEP::CHECK_NEXT_STEP;
	}
	default: {
		return false;
	}
	}
};

Q_DECLARE_METATYPE(EmployeesManager);

#endif