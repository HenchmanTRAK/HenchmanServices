
#include "TRAKEntriesManager.h"

//auto EMPLOYEES_ALL_UPDATED = [](int val) { return val == EmployeesManager::NEXTSTEP::ALL_UPDATED ? 1 : 0; };

TRAKEntriesManager::TRAKEntriesManager(QObject* parent, const TrakDetails& trakDetails, const WebportalDetails& webportalDetails, const s_DATABASE_INFO& database_info)
	: QObject(parent), sqliteManager(parent), queryManager(parent, database_info), networkManager(parent), table(this, QSqlDatabase::database(database_info.schema))
{
	qDebug() << "Initialized TRAKEntriesManager";

	registryEntry = "customersChecked";

	trak_details = trakDetails;
	webportal_details = webportalDetails;
	db_info = database_info;	

	queryManager.set_database_details(db_info);

	networkManager.setApiUrl(webportal_details.api_url);
	networkManager.setApiKey(webportal_details.api_key);
	networkManager.toggleSecureTransport(DEBUG);

	try {
		if (!db_info.schema.isEmpty())
			throw std::exception();

		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, ("SOFTWARE\\HenchmanTRAK\\" + trak_details.trak_type + "\\Database").toStdString().c_str());

		DWORD size = sizeof(TCHAR);

		rtManager.GetValSize("Schema", REG_SZ, &size);

		//TCHAR* buffer = new TCHAR[size];
		std::vector<TCHAR> buffer;
		buffer.reserve(size);

		rtManager.GetVal("Schema", REG_SZ, buffer.data(), &size);

		trak_details.schema = QString(buffer.data());
		db_info.schema = trak_details.schema;

		queryManager.setSchema(db_info.schema);
	}
	catch (const std::exception& e)
	{
		LOG << "database_info.schema was not empty, skipping setting schema manually";
	}

	s_TZ_INFO tzMap = queryManager.GetTimezone();

	time_zone = tzMap.time_zone;

	try {
		if (networkManager.isInternetConnected())
			(void)networkManager.authenticateSession();
	}
	catch (const std::exception& e) {
		throw e;
	}
}

TRAKEntriesManager::~TRAKEntriesManager()
{

}


void TRAKEntriesManager::Initialize()
{
	try {
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

		DWORD size = sizeof(local_count);

		(void)rtManager.GetValSize(registryEntry, REG_DWORD, &size);

		(void)rtManager.GetVal(registryEntry, REG_DWORD, &local_count, &size);

		QJsonObject placeholders;

		(void)placeholders.insert(trak_details.trak_id_type, trak_details.trak_id);

		QJsonArray rowCheck = queryManager.execute("SELECT COUNT(*) as " + db_info.table + " FROM users WHERE userId <> '' AND userId IS NOT NULL", placeholders);
		int local_emp_count = rowCheck.at(0).toObject().value("total").toInt();
		if (!local_count || local_count != local_emp_count) {
			local_count = local_emp_count;
			size = sizeof(local_count);
			(void)rtManager.SetVal(registryEntry, REG_DWORD, &local_count, size);
		}


	}
	catch (const std::exception& e) {
		throw e;
	}

	if (table.database().isOpen())
		table.database().close();
}

QSqlTableModel* TRAKEntriesManager::GetTable()
{
	if (!table.database().isOpen())
		table.database().open();

	if (table.tableName() != db_info.table)
		table.setTable(db_info.table);

	return &table;
}

void TRAKEntriesManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values)
{
	QVariantMap temp_variantMap;

	breakoutValuesToUpdate(older, newer, set_values, &temp_variantMap);

	updated_values->toVariantMap().swap(temp_variantMap);
}

void TRAKEntriesManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values)
{

	QList<QString> excludeCols({ "id", "createdAt", "updatedAt" });
	QList<QString> dateCols({ "createDate", "createTime", "createdAt", "lastvisit", "updatedAt" });

	QList<QString> set;
	QVariantMap update;

	for (const QString& key : newer.keys()) {
		if (!older.keys().contains(key))
			continue;

		if (excludeCols.contains(key))
			continue;

		QVariant webportalValue = newer.value(key).toVariant();
		QVariant localValue = older.value(key).toVariant();

		if (webportalValue.isNull())
			webportalValue = "";

		if (localValue.isNull())
			localValue = "";

		if (!webportalValue.canConvert<QString>()) {
			continue;
		}


		if (webportalValue.toString() == localValue.toString())
			continue;


		if (dateCols.contains(key)) {
			QDateTime webportalDate = QDateTime::fromString(webportalValue.toString());
			QDateTime localDate = QDateTime::fromString(localValue.toString());

			if (webportalDate == localDate)
				continue;
		}

		qDebug() << key;
		qDebug() << webportalValue;
		qDebug() << localValue;

		set.push_back(key + " = :" + key);
		update.insert(key, newer.value(key));
	}

	qDebug() << "seting: " << set;
	qDebug() << "updating: " << update;

	set.swap(*set_values);
	update.swap(*updated_values);
};

QJsonArray TRAKEntriesManager::GetColumns(bool reset)
{
	if (Columns.size() > 0 && !reset)
		return Columns;


	QJsonObject overloader;
	QString colQuery =
		"SHOW COLUMNS from " + db_info.table;
	//QJsonArray colQueryResults = queryManager.execute(colQuery, overloader);

	GetTable();

	QSqlQuery query(colQuery, table.database());

	table.setQuery(query);

	for (int i = 0; i < table.rowCount(); ++i)
	{
		QVariantMap results = queryManager.recordToMap(table.record(i));
		Columns.append(QJsonObject::fromVariantMap(results));
	}

	table.clear();

	table.database().close();

	return Columns;
}

int TRAKEntriesManager::GetLocalCount(const QList<QString>& p_conditions, const QJsonObject& p_placeholders)
{
	int local = 0;

	try {
		QList<QString> conditions(p_conditions);
		QJsonObject placeholders(p_placeholders);


		if (conditions.isEmpty())
			throw std::exception();

		GetTable();

		if (placeholders.isEmpty()) {
			table.setFilter(conditions.join(" AND "));
			table.select();
		}
		else {
			QSqlQuery query(table.database());
			query.prepare("SELECT id FROM "+db_info.table+" WHERE " + conditions.join(" AND "));

			for (const auto& placeholder : placeholders.keys())
			{
				query.bindValue(":" + placeholder, placeholders.value(placeholder));
			}
			table.setQuery(query);
		}

		local = table.rowCount();

		table.clear();
		table.database().close();

		qDebug() << local;

		//QJsonArray rowCount = queryManager.execute("SELECT COUNT(*) as total FROM users WHERE " + conditions.join(" AND "), placeholders);

		/*if (rowCount.size() <= 0 || !rowCount.at(0).isObject()) {
			throw std::exception();
		}

		local_users = rowCount.at(0).toObject().value("total").toVariant().toInt();*/
	}
	catch (const std::exception& e) {
		local = local_count;
	}
	return local;
}

int TRAKEntriesManager::GetRemoteCount(const QJsonObject& p_select, const QJsonObject& p_where, QJsonObject* p_returned_data)
{
	int remote = 0;

	try {
		QJsonObject query;
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if (!where.contains("custId"))
			(void)where.insert("custId", QString::number(trak_details.cust_id));

		if (!where.contains(trak_details.trak_id_type))
			(void)where.insert(trak_details.trak_id_type, trak_details.trak_id);

		if (!select.contains("count"))
			(void)select.insert("count", "total");

		(void)query.insert("where", where);
		(void)query.insert("select", select);

		QJsonDocument reply;

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/" + webportal_details.base_route, query, &reply))
		{
			throw std::exception();
		}

		if (!reply.isObject())
			throw std::exception();

		QJsonObject resultObject = reply.object();
		if (resultObject["data"].isNull() || resultObject["data"].isUndefined())
			throw std::exception();

		QJsonArray webportalResults = resultObject["data"].toArray();
		if (!webportalResults.at(0).isObject())
			throw std::exception();

		QJsonObject webportalToolCount = webportalResults.at(0).toObject();

		remote = webportalToolCount.value("total").toInt();

		if (p_returned_data)
			webportalToolCount.swap(*p_returned_data);

	}
	catch (const std::exception& e)
	{
		remote = remote_count;
	}


	return remote;
}

QJsonArray TRAKEntriesManager::GetRemote(const QJsonArray& columns, const QJsonObject& p_where, const QJsonObject& p_select)
{

	QJsonArray returnedValues;

	try {

		QJsonObject query;
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if (!where.contains("custId"))
			(void)where.insert("custId", QString::number(trak_details.cust_id));

		if (!where.contains(trak_details.trak_id_type))
			(void)where.insert(trak_details.trak_id_type, trak_details.trak_id);

		(void)query.insert("where", where);

		// QJsonArray({ "custId", "userId", "empId" })

		if (!columns.isEmpty()) {
			(void)select.insert("columns", columns);
		}

		if (!select.isEmpty())
			(void)query.insert("select", select);



		qDebug() << query;

		QJsonDocument reply;

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/" + webportal_details.base_route, query, &reply))
		{
			throw std::exception();
		}

		if (!reply.isObject())
			throw std::exception();

		QJsonObject resultObject = reply.object();
		if (resultObject.value("data").isNull() || resultObject.value("data").isUndefined() || !resultObject.value("data").isArray())
			throw std::exception();

		returnedValues = resultObject["data"].toArray();

	}
	catch (const std::exception& e)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

QJsonArray TRAKEntriesManager::GetLocal(const QString& query, const QJsonObject& placeholders)
{
	GetTable();

	if (!placeholders.isEmpty()) {
		QSqlQuery sqlQuery(table.database());

		QString query_str = query;

		QVariantMap boundValues = queryManager.processPlaceholders(placeholders.toVariantMap(), query_str);

		sqlQuery.prepare(query_str);

		QMapIterator it(boundValues);

		while (it.hasNext()) {
			it.next();
			QString key = it.key();
			QString val = it.value().toString();
			sqlQuery.bindValue(key, val);
		}

		table.setQuery(sqlQuery);
	}
	else {
		QSqlQuery sqlQuery(query, table.database());

		table.setQuery(sqlQuery);
	}

	QJsonArray queryResults;

	QString statement = table.query().executedQuery();

	for (const auto& value : table.query().boundValues())
	{
		QString key = "?";
		QString val = value.toString().trimmed();

		QString firstSection = statement.sliced(0, statement.indexOf(key));
		QString secondSection = statement.sliced(statement.indexOf(key) + key.length());

		if (!(val.startsWith("'") && val.endsWith("'")))
			val = "'" + val + "'";

		statement = firstSection + val + secondSection;
	}
	QString where = "WHERE";
	QString conditions = statement.sliced(statement.indexOf(where) + where.length()).trimmed();
	table.clear();

	GetTable();

	qDebug() << "Conditions:" << conditions;

	table.setFilter(conditions);
	table.select();

	qDebug() << table.rowCount();

	for (int i = 0; i < table.rowCount(); ++i)
	{
		QVariantMap results = queryManager.recordToMap(table.record(i));
		queryResults.append(QJsonObject::fromVariantMap(results));
	}

	qDebug() << queryResults;

	table.clear();

	table.database().close();

	return queryResults;
	//return queryManager.execute(query, placeholders);
}

QJsonArray TRAKEntriesManager::GetGroupedRemote(const QJsonArray& columns, const QJsonArray& grouped_columns, const QJsonArray& group_by, const QString& type, const QString& separator, const QJsonObject& p_where)
{

	QJsonArray returnedValues;

	try {

		QJsonObject query;
		QJsonObject select;
		QJsonObject group;
		QJsonObject where(p_where);

		if (!where.contains("custId"))
			(void)where.insert("custId", QString::number(trak_details.cust_id));

		if (!where.contains(trak_details.trak_id_type))
			(void)where.insert(trak_details.trak_id_type, trak_details.trak_id);

		(void)query.insert("where", where);


		// QJsonArray({ "custId", "userId", "empId" })
		(void)select.insert("columns", columns);

		if (!group_by.isEmpty())
			(void)group.insert("group_columns", group_by);

		(void)group.insert("columns", grouped_columns);
		(void)group.insert("group_type", type);
		if (!separator.isEmpty())
			(void)group.insert("separator", separator);

		(void)select.insert("group", group);

		(void)query.insert("select", select);

		qDebug() << query;

		QJsonDocument reply;

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/" + webportal_details.base_route, query, &reply))
		{
			throw std::exception();
		}

		if (!reply.isObject())
			throw std::exception();

		QJsonObject resultObject = reply.object();
		if (resultObject.value("data").isNull() || resultObject.value("data").isUndefined() || !resultObject.value("data").isArray())
			throw std::exception();

		returnedValues = resultObject["data"].toArray();

	}
	catch (const std::exception& e)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

int TRAKEntriesManager::SendToRemote(const QJsonObject& entry, const QJsonObject& data)
{

	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (networkManager.makePostRequest(webportal_details.api_url + "/" + webportal_details.base_route, entry, body, &reply)) {
		qDebug() << reply;

		if (!reply.isObject()) {
			LOG << "Reply was not an Object";
			//databaseTablesChecked[targetKey]++;
			return 0;
		}
		LOG << reply.toJson().toStdString();
		QJsonObject result = reply.object();
		if (result["status"].toDouble() == 200) {
			//databaseTablesChecked[targetKey]++;
			//LOG << result["status"].toDouble();
			return 1;
		}
	}

	LOG << "No rows were altered on db";
	//databaseTablesChecked[targetKey]++;
	return 0;
}

int TRAKEntriesManager::CreateLocal(const QJsonObject& entry)
{
	QJsonArray results = queryManager.execute("INSERT INTO "+ db_info.table +" (" + entry.keys().join(", ") + ") VALUES (:" + entry.keys().join(",:") + ")", entry);

	if (results.size() <= 0)
		return 0;
	return 1;

}

int TRAKEntriesManager::UpdateRemote(const QJsonObject& entry, const QJsonObject& data)
{
	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (networkManager.makePatchRequest(webportal_details.api_url + "/" + webportal_details.base_route, entry, body, &reply)) {
		qDebug() << reply;

		if (!reply.isObject()) {
			LOG << "Reply was not an Object";
			
			return 0;
		}
		LOG << reply.toJson().toStdString();
		QJsonObject result = reply.object();
		if (result["status"].toDouble() == 200) {
			
			return 1;
		}
	}

	LOG << "No rows were altered on db";

	return 0;
}

QJsonArray TRAKEntriesManager::UpdateLocal(const QList<QString>& update, const QJsonObject& placeholders)
{
	QJsonArray queryResults;

	QList<QVariantMap> updateLocal = UpdateLocal(update, placeholders.toVariantMap());

	for (int i = 0; i < updateLocal.size(); ++i)
	{
		QVariantMap results = updateLocal.at(i);
		queryResults.append(QJsonObject::fromVariantMap(results));
	}

	return queryResults;
}

QList<QVariantMap> TRAKEntriesManager::UpdateLocal(const QList<QString>& update, const QVariantMap& placeholders)
{
	QStringList conditions;
	
	if (placeholders.contains("custId"))
		conditions.append("custId = :custId");
	if (placeholders.contains("userId"))
		conditions.append("userId = :userId");
	if (placeholders.contains("empId"))
		conditions.append("empId = :empId");


	QString queryToExec = "UPDATE "+ db_info.table +" SET " + update.join(", ") + " WHERE " + conditions.join(" AND ");

	return queryManager.execute(queryToExec, placeholders);
}

void TRAKEntriesManager::HandleUpdatingEntries(const QJsonObject& local, const QJsonObject& remote)
{
	qDebug() << "Local: " << QJsonDocument(local).toJson().toStdString().data();
	qDebug() << "Remote: " << QJsonDocument(remote).toJson().toStdString().data();


	if (remote.value("updatedAt") == local.value("updatedAt")) {
		qInfo() << "Local entry last updated at:" << local.value("updatedAt") << "\nRemote entry last updated at:" << remote.value("updatedAt");
		qInfo() << "Local and Remote are in sync";
		return;
	}

	QDateTime remoteUpdate = QDateTime::fromString(remote.value("updatedAt").toString(), Qt::ISODate);
	QDateTime localUpdate = QDateTime::fromString(local.value("updatedAt").toString(), Qt::ISODate);
	//localUpdate.setOffsetFromUtc(local_timezone_offset);

	if (remoteUpdate == localUpdate) {
		qInfo() << "Local entry last updated at:" << localUpdate << "\nRemote entry last updated at:" << remoteUpdate;
		qInfo() << "Local and Remote did not share raw values but are in sync";
		return;
	}

	QDateTime currentDateTime = QDateTime::currentDateTimeUtc();

	qDebug() << "remoteUpdate" << remoteUpdate;
	qDebug() << "localUpdate" << localUpdate;
	qDebug() << "currentDateTime" << currentDateTime;

	QJsonObject where;
	where.insert(trak_details.trak_id_type, trak_details.trak_id);

	QJsonObject body;
	QList<QString> set;
	QJsonObject update;

	QVariantMap placeholders;

	if (remoteUpdate > localUpdate)
	{
		qInfo() << "Remote is newer that Local";
		qInfo() << "Updating Local entry to match Remote";

		(void)TRAKEntriesManager::breakoutValuesToUpdate(local, remote, &set, &placeholders);
		(void)TRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &update);

		set.push_back("updatedAt = :updatedAt");
		placeholders.insert("updatedAt", currentDateTime.toLocalTime());
		placeholders.insert("tz", time_zone);
		placeholders.insert("custId", remote.value("custId").toInt());

		qDebug() << set;
		qDebug() << placeholders;
		qDebug() << update;
		qDebug() << where;

		//QString queryToExec = "UPDATE users SET " + set.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId";
		//(void)queryManager.execute(queryToExec, placeholders);
		(void)UpdateLocal(set, placeholders);

		update.insert("updatedAt", currentDateTime.toString(Qt::ISODate));
		where.insert("custId", placeholders.value("custId").toJsonValue());

		body.insert("update", update);
		body.insert("where", where);

		(void)UpdateRemote(local, body);

		return;
	}

	if (remoteUpdate < localUpdate)
	{
		qInfo() << "Remote is older that Local";
		qInfo() << "Updating Remote entry to match Local";
		(void)TRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &placeholders);
		(void)TRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &update);

		set.push_back("updatedAt = :updatedAt");
		placeholders.insert("updatedAt", currentDateTime.toLocalTime());
		placeholders.insert("tz", time_zone);
		placeholders.insert("custId", local.value("custId").toInt());

		qDebug() << set;
		qDebug() << placeholders;
		qDebug() << update;
		qDebug() << where;

		//QString queryToExec = "UPDATE users SET " + set.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId";
		//(void)queryManager.execute(queryToExec, placeholders);
		(void)UpdateLocal(set, placeholders);

		update.insert("updatedAt", currentDateTime.toString(Qt::ISODate));
		where.insert("custId", placeholders.value("custId").toJsonValue());

		body.insert("update", update);
		body.insert("where", where);

		(void)UpdateRemote(local, body);

		return;
	}
}

int TRAKEntriesManager::SyncWebportal()
{
	return 1;
}

int TRAKEntriesManager::SyncLocal()
{
	return 1;
}

int TRAKEntriesManager::UpdateOutdated()
{

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	DWORD size = sizeof(TCHAR);
	(void)rtManager.GetValSize(std::string(registryEntry).append("Date").data(), REG_SZ, &size);

	if (!size) {
		(void)UpdateCheckedTime();
		(void)rtManager.GetValSize(std::string(registryEntry).append("Date").data(), REG_SZ, &size);
	}

	std::vector<TCHAR> buffer(size);

	(void)rtManager.GetVal(std::string(registryEntry).append("Date").data(), REG_SZ, buffer.data(), &size);

	QString lastCheckedDateTime(buffer.data());

	UpdateCheckedTime();

	return 1;
}


int TRAKEntriesManager::GetCurrentState()
{

	if (local_count == 0)
		local_count = GetLocalCount();

	if (remote_count == 0)
		remote_count = GetRemoteCount();

	qDebug() << "local_count: " << local_count;
	qDebug() << "remote_count: " << remote_count;

	if (local_count > remote_count)
		return NEXTSTEP::SYNC_PORTAL;

	if (local_count < remote_count)
		return NEXTSTEP::SYNC_LOCAL;

	return NEXTSTEP::UPDATE_OUTDATED;
}

int TRAKEntriesManager::UpdateCheckedTime()
{
	QString currDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	
	(void)rtManager.SetVal(std::string(registryEntry).append("Date").data(), REG_SZ, currDate.toStdString().data(), currDate.toStdString().size());
	
	return NEXTSTEP::ALL_UPDATED;
}

QString TRAKEntriesManager::ClearCloudUpdate()
{
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	DWORD size = sizeof(TCHAR);

	(void)rtManager.GetValSize(std::string(registryEntry).append("Date").data(), REG_SZ, &size);

	if (size == 0) {
		QString currDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
		
		if (rtManager.SetVal(std::string(registryEntry).append("Date").data(), REG_SZ, currDate.toStdString().data(), currDate.toStdString().size()))
			throw HenchmanServiceException("Failed to store checked date in registery");
		
		(void)rtManager.GetValSize(std::string(registryEntry).append("Date").data(), REG_SZ, &size);
	}

	std::vector<TCHAR> buffer;
	buffer.reserve(size);

	LONG err = rtManager.GetVal(std::string(registryEntry).append("Date").data(), REG_SZ, buffer.data(), &size);
	
	QString date(buffer.data());
	
	if (err) {
		const TCHAR* errMsg = "%d";
		TCHAR buffer[2048] = "\0";
		rtManager.GetSystemError(errMsg, err, buffer, 2048);
		throw HenchmanServiceException("Failed to fetch target stored value from registry. Error: " + std::to_string(err) + " - " + std::string(buffer));
	}
	
	return date;
};

