#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSqlTableModel>
#include <QSqlField>

#include "RegistryManager.h"
#include "SQLiteManager2.h"
#include "QueryManager.h"
#include "NetworkManager.h"
#include "ServiceHelper.h"
#include "TRAKEntriesManager.h"

#include <WinError.h>


class UsersManager : public TRAKEntriesManager
{
	Q_OBJECT

public:
	using TRAKEntriesManager::TRAKEntriesManager;
	explicit UsersManager(QObject* parent = nullptr, const TrakDetails& trakDetails = TrakDetails(), const WebportalDetails& webportalDetails = WebportalDetails(), const s_DATABASE_INFO& database_info = s_DATABASE_INFO());
	~UsersManager();

	int GetLocalCount(const QList<QString>& conditions = QList<QString>(), const QJsonObject& placeholders = QJsonObject());

	int GetRemoteCount(const QJsonObject& select = QJsonObject(), const QJsonObject& where = QJsonObject(), QJsonObject* p_returned_data = nullptr);

	QJsonArray GetRemote(const QJsonArray& columns = QJsonArray(), const QJsonObject& where = QJsonObject(), const QJsonObject& p_select = QJsonObject());

	QJsonArray GetLocal(const QString& query = QString(""), const QJsonObject& placeholders = QJsonObject());

	QJsonArray GetGroupedRemote(const QJsonArray& columns = QJsonArray(), const QJsonArray& grouped_columns = QJsonArray(), const QJsonArray& group_by = QJsonArray(), const QString& type = QString("COUNT"), const QString& separator = QString(""), const QJsonObject& p_where = QJsonObject());

	int SendToRemote(const QJsonObject& entry = QJsonObject(), const QJsonObject& data = QJsonObject());
	int CreateLocal(const QJsonObject& entry = QJsonObject());

	int UpdateRemote(const QJsonObject& entry = QJsonObject(), const QJsonObject& data = QJsonObject());

	QJsonArray UpdateLocal(const QList<QString>& update = QList<QString>(), const QJsonObject& placeholders = QJsonObject());
	QList<QVariantMap> UpdateLocal(const QList<QString>& update = QList<QString>(), const QVariantMap& placeholders = QVariantMap());

	int UpdateOutdated();

	int SyncWebportal();

	int SyncLocal();

	int ClearCloudUpdate();

private:

	void breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values);
	void breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values);

	void HandleUpdatingEntries(const QJsonObject& local, const QJsonObject& remote);
	
};

Q_DECLARE_METATYPE(UsersManager);
