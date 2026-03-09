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

struct s_UpdateLocalTableOptions {
	bool AddEmpId = 0;
	bool AddEmpIdSqliteOnly = 0;
	bool AddCreatedAt = 0;
	bool UpdateCreatedAt = 0;
	bool AddUpdatedAt = 0;
	bool UpdateUpatedAt = 0;
	bool CreateUniqueIndex = 0;
};

namespace TRAKEntriesManager {
	
	class CTRAKEntriesManager : public QObject
	{

		Q_OBJECT;

	protected:
		QJsonArray m_MySQL_Columns{};
		QJsonArray m_SQLITE_Columns{};
		QStringList m_TableColumns{};
		const TCHAR* m_registryEntry = "";
		QString m_time_zone{};

		TrakDetails m_trak_details{};
		WebportalDetails m_webportal_details{};
		s_DATABASE_INFO m_db_info{};

		SQLiteManager2 m_sqliteManager;
		QueryManager m_queryManager;
		NetworkManager m_networkManager;

		QSqlTableModel m_table{};
		QSqlQueryModel* m_query = nullptr;

	public:
		int local_count = 0;
		int remote_count = 0;

		enum NEXTSTEP {
			ALL_UPDATED = 0,
			SYNC_PORTAL,
			SYNC_LOCAL,
			UPDATE_OUTDATED,
			CHECK_NEXT_STEP,
		};

	public:
		explicit CTRAKEntriesManager(QObject* parent = nullptr, const TrakDetails& trakDetails = TrakDetails(), const WebportalDetails& webportalDetails = WebportalDetails(), const s_DATABASE_INFO& database_info = s_DATABASE_INFO());

		~CTRAKEntriesManager();

		QString ClearCloudUpdate();

		virtual int GetLocalCount(const QList<QString>& conditions = QList<QString>(), const QJsonObject& placeholders = QJsonObject());

		virtual int GetRemoteCount(const QJsonObject& select = QJsonObject(), const QJsonObject& where = QJsonObject(), QJsonObject* p_returned_data = nullptr);

		virtual QJsonArray GetRemote(const QJsonArray& columns = QJsonArray(), const QJsonObject& where = QJsonObject(), const QJsonObject& p_select = QJsonObject());

		virtual QJsonArray GetLocal(const QString& query = QString(""), const QJsonObject& placeholders = QJsonObject());
		virtual QList<QVariantMap> GetLocal(const QStringList& t_columns = QStringList({ "*" }), const QStringList& t_conditions = QStringList(), const QVariantMap& t_placeholders = QVariantMap());

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

		int GetCurrentState();

		virtual QJsonArray GetColumns(bool reset = false);

		virtual QStringList GetColumnNames(bool reset = false);

	protected:
		void Initialize();

		virtual QSqlTableModel* GetTable();
		virtual QSqlTableModel* CleanTable();

		virtual QSqlQueryModel* GetQuery();
		virtual void CleanQuery();

		void handleUpdatingLocalDB(const QString& table, const QStringList& unique_columns, s_UpdateLocalTableOptions* options);

		virtual void breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values);
		virtual void breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values);

		int UpdateCheckedTime();
	};
};
#define NEXT_STEP TRAKEntriesManager::CTRAKEntriesManager::NEXTSTEP

constexpr auto ENTRIES_ALL_UPDATED(int val) { return val == NEXT_STEP::ALL_UPDATED; }
constexpr auto ENTRIES_SYNC_PORTAL(int val) { return val == NEXT_STEP::SYNC_PORTAL; }
constexpr auto ENTRIES_SYNC_LOCAL(int val) { return val == NEXT_STEP::SYNC_LOCAL; }
constexpr auto ENTRIES_UPDATE_OUTDATED(int val) { return val == NEXT_STEP::UPDATE_OUTDATED; }
constexpr auto ENTRIES_(int val, int status) {
	switch (status) {
	case NEXT_STEP::ALL_UPDATED: {
		return val == NEXT_STEP::ALL_UPDATED;
	}
	case NEXT_STEP::SYNC_PORTAL: {
		return val == NEXT_STEP::SYNC_PORTAL;
	}
	case NEXT_STEP::SYNC_LOCAL: {
		return val == NEXT_STEP::SYNC_LOCAL;
	}
	case NEXT_STEP::UPDATE_OUTDATED: {
		return val == NEXT_STEP::UPDATE_OUTDATED;
	}
	case NEXT_STEP::CHECK_NEXT_STEP: {
		return val == NEXT_STEP::CHECK_NEXT_STEP;
	}
	default: {
		return false;
	}
	}
}

Q_DECLARE_METATYPE(TRAKEntriesManager::CTRAKEntriesManager);
