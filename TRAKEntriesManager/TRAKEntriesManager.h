#pragma once


#include <QObject>
#include <QString>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <QSqlTableModel>


#include "RegistryManager.h"
#include "SQLiteManager2.h"
#include "QueryManager.h"
#include "NetworkManager.h"
#include "ServiceHelper.h"

#include <WinError.h>



class TRAKEntriesManager : public QObject
{

	Q_OBJECT;

protected:
	QJsonArray Columns;
	const TCHAR* registryEntry = "";
	int local_count = 0;
	int remote_count = 0;
	QString time_zone;

	TrakDetails trak_details;
	WebportalDetails webportal_details;
	s_DATABASE_INFO db_info;

	SQLiteManager2 sqliteManager;
	QueryManager queryManager;
	NetworkManager networkManager;
	QSqlTableModel table;

public:
	enum NEXTSTEP {
		ALL_UPDATED = 0,
		SYNC_PORTAL,
		SYNC_LOCAL,
		UPDATE_OUTDATED,
		CHECK_NEXT_STEP,
	};


public:
	explicit TRAKEntriesManager(QObject* parent = nullptr, const TrakDetails& trakDetails = TrakDetails(), const WebportalDetails& webportalDetails = WebportalDetails(), const s_DATABASE_INFO& database_info = s_DATABASE_INFO());

	~TRAKEntriesManager();

	int GetCurrentState();

	virtual QJsonArray GetColumns(bool reset = false);

protected:
	void Initialize();

	QString ClearCloudUpdate();

	virtual QSqlTableModel* GetTable();

	virtual int GetLocalCount(const QList<QString>& conditions = QList<QString>(), const QJsonObject& placeholders = QJsonObject());

	virtual int GetRemoteCount(const QJsonObject& select= QJsonObject(), const QJsonObject& where = QJsonObject(), QJsonObject* p_returned_data = nullptr);

	virtual QJsonArray GetRemote(const QJsonArray& columns = QJsonArray(), const QJsonObject& where = QJsonObject(), const QJsonObject& p_select = QJsonObject());

	virtual QJsonArray GetLocal(const QString& query = QString(""), const QJsonObject& placeholders = QJsonObject());

	virtual QJsonArray GetGroupedRemote(const QJsonArray& columns = QJsonArray(), const QJsonArray& grouped_columns = QJsonArray(), const QJsonArray& group_by = QJsonArray(), const QString& type = QString("COUNT"), const QString& separator = QString(""), const QJsonObject& p_where = QJsonObject());

	virtual int SendToRemote(const QJsonObject& entry = QJsonObject(), const QJsonObject& data = QJsonObject());
	virtual int CreateLocal(const QJsonObject& entry = QJsonObject());

	virtual int UpdateRemote(const QJsonObject& entry = QJsonObject(), const QJsonObject& data = QJsonObject());

	virtual QJsonArray UpdateLocal(const QList<QString>& update = QList<QString>(), const QJsonObject& placeholders = QJsonObject());
	virtual QList<QVariantMap> UpdateLocal(const QList<QString>& update = QList<QString>(), const QVariantMap& placeholders = QVariantMap());

	virtual void HandleUpdatingEntries(const QJsonObject& local, const QJsonObject& remote);

	virtual int UpdateOutdated();

	virtual int SyncWebportal();

	virtual int SyncLocal();

	virtual void breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values);
	virtual void breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values);

	int UpdateCheckedTime();
};

#define NEXT_STEP TRAKEntriesManager::NEXTSTEP

constexpr auto ENTRIES_ALL_UPDATED(int val) { return val == TRAKEntriesManager::NEXTSTEP::ALL_UPDATED; }
constexpr auto ENTRIES_SYNC_PORTAL(int val) { return val == TRAKEntriesManager::NEXTSTEP::SYNC_PORTAL; }
constexpr auto ENTRIES_SYNC_LOCAL(int val) { return val == TRAKEntriesManager::NEXTSTEP::SYNC_LOCAL; }
constexpr auto ENTRIES_UPDATE_OUTDATED(int val) { return val == TRAKEntriesManager::NEXTSTEP::UPDATE_OUTDATED; }
constexpr auto ENTRIES_(int val, int status) {
	switch (status) {
	case TRAKEntriesManager::NEXTSTEP::ALL_UPDATED: {
		return val == TRAKEntriesManager::NEXTSTEP::ALL_UPDATED;
	}
	case TRAKEntriesManager::NEXTSTEP::SYNC_PORTAL: {
		return val == TRAKEntriesManager::NEXTSTEP::SYNC_PORTAL;
	}
	case TRAKEntriesManager::NEXTSTEP::SYNC_LOCAL: {
		return val == TRAKEntriesManager::NEXTSTEP::SYNC_LOCAL;
	}
	case TRAKEntriesManager::NEXTSTEP::UPDATE_OUTDATED: {
		return val == TRAKEntriesManager::NEXTSTEP::UPDATE_OUTDATED;
	}
	case TRAKEntriesManager::NEXTSTEP::CHECK_NEXT_STEP: {
		return val == TRAKEntriesManager::NEXTSTEP::CHECK_NEXT_STEP;
	}
	default: {
		return false;
	}
	}
}

Q_DECLARE_METATYPE(TRAKEntriesManager);
