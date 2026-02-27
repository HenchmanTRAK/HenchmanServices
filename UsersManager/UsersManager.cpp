
#include "UsersManager.h"

//auto EMPLOYEES_ALL_UPDATED = [](int val) { return val == EmployeesManager::NEXTSTEP::ALL_UPDATED ? 1 : 0; };

UsersManager::UsersManager(QObject* parent, const TrakDetails& trakDetails, const WebportalDetails& webportalDetails, const s_DATABASE_INFO& database_info)
	:TRAKEntriesManager(parent, trakDetails, webportalDetails, database_info)
{
	registryEntry = "usersChecked";

	try {
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

		(void)rtManager.GetVal(registryEntry, REG_SZ, (DWORD*)&local_count, sizeof(DWORD));

		QJsonObject placeholders;

		placeholders.insert(trak_details.trak_id_type, trak_details.trak_id);

		QJsonArray rowCheck = queryManager.ExecuteTargetSql(QString("SELECT COUNT(*) FROM users WHERE userId <> '' AND userId IS NOT NULL"), placeholders);
		int local_emp_count = rowCheck.at(0).toObject().value("COUNT(*)").toInt();
		if (!local_count || local_count != local_emp_count) {
			local_count = local_emp_count;
			(void)rtManager.SetVal(registryEntry, REG_DWORD, (DWORD*)&local_count, sizeof(DWORD));
		}

	}
	catch (void*)
	{

	}
}

UsersManager::~UsersManager()
{
	//TRAKEntriesManager::~TRAKEntriesManager();
}

void UsersManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values)
{
	TRAKEntriesManager::breakoutValuesToUpdate(older, newer, set_values, updated_values);
}

void UsersManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values)
{

	TRAKEntriesManager::breakoutValuesToUpdate(older, newer, set_values, updated_values);
};


QJsonArray UsersManager::GetColumns()
{
	QJsonObject overloader;
	std::string colQuery =
		"SHOW COLUMNS from users";
	QJsonArray colQueryResults = queryManager.ExecuteTargetSql(colQuery, overloader);

	return colQueryResults;
}

int UsersManager::GetLocalCount(const QList<QString>& p_conditions, const QJsonObject& p_placeholders)
{
	int local_users = 0;

	try {
		QList<QString> conditions(p_conditions);
		QJsonObject placeholders(p_placeholders);


		if (conditions.isEmpty() && local_count == 0) {
			conditions.append("userId <> ''");
			conditions.append("userId IS NOT NULL");
			//conditions.append(trak_details.trak_id_type +  " = :" + trak_details.trak_id_type);
			//placeholders.insert(trak_details.trak_id_type, trak_details.trak_id);
		}
		else if (conditions.isEmpty())
			throw std::exception();

		QJsonArray rowCount = queryManager.ExecuteTargetSql("SELECT COUNT(*) as total FROM users WHERE " + conditions.join(" AND "), placeholders);

		if (rowCount.size() <= 0 || !rowCount.at(0).isObject()) {
			throw std::exception();
		}

		local_users = rowCount.at(0).toObject().value("total").toVariant().toInt();
	}
	catch (const std::exception& e) {
		local_users = local_count;
	}
	return local_users;
}

int UsersManager::GetRemoteCount(const QJsonObject& p_select, const QJsonObject& p_where, QJsonObject* p_returned_data)
{

	int remote_users = 0;

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

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/users", query, &reply))
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

		remote_users = webportalToolCount.value("total").toInt();

		if (p_returned_data)
			webportalToolCount.swap(*p_returned_data);

	}
	catch (const std::exception& e)
	{
		remote_users = remote_count;
	}


	return remote_users;
}

QJsonArray UsersManager::GetRemote(const QJsonArray& columns, const QJsonObject& p_where, const QJsonObject& p_select)
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

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/users", query, &reply))
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

QJsonArray UsersManager::GetLocal(const QString& query, const QJsonObject& placeholders)
{
	QJsonArray retVal;
	QJsonArray returnedResults = queryManager.ExecuteTargetSql(query, placeholders);
	returnedResults.swap(retVal);
	return retVal;
}

QJsonArray UsersManager::GetGroupedRemote(const QJsonArray& columns, const QJsonArray& grouped_columns, const QJsonArray& group_by, const QString& type, const QString& separator, const QJsonObject& p_where)
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

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/users", query, &reply))
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

int UsersManager::SendToRemote(const QJsonObject& entry, const QJsonObject& data)
{

	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (networkManager.makePostRequest(webportal_details.api_url + "/users", entry, body, &reply)) {
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

int UsersManager::CreateLocal(const QJsonObject& entry)
{
	QJsonArray results = queryManager.ExecuteTargetSql("INSERT INTO users (" + entry.keys().join(", ") + ") VALUES (:" + entry.keys().join(",:") + ")", entry);

	if (results.size() == 0)
		return 0;

	return 1;
}

int UsersManager::UpdateRemote(const QJsonObject& entry, const QJsonObject& data)
{
	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (networkManager.makePatchRequest(webportal_details.api_url + "/users", entry, body, &reply)) {
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

QJsonArray UsersManager::UpdateLocal(const QList<QString>& update, const QJsonObject& placeholders)
{
	return queryManager.ExecuteTargetSql("UPDATE users SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId", placeholders);
}

QMap<int, QList<QVariantMap>> UsersManager::UpdateLocal(const QList<QString>& update, const QVariantMap& placeholders)
{
	QString queryToExec = "UPDATE users SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId";
	QVariantMap newPlaceholders = placeholders;
	return queryManager.ExecuteTargetSql_Map(queryToExec, newPlaceholders);
}

void UsersManager::HandleUpdatingEntries(const QJsonObject& local, const QJsonObject& remote)
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
		//placeholders.insert("tz", time_zone);
		placeholders.insert("custId", remote.value("custId").toInt());
		placeholders.insert("empId", remote.value("empId").toString());
		placeholders.insert("userId", remote.value("userId").toString());

		qDebug() << set;
		qDebug() << placeholders;
		qDebug() << update;
		qDebug() << where;

		QString queryToExec = "UPDATE users SET " + set.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId";
		(void)queryManager.ExecuteTargetSql_Map(queryToExec, placeholders);

		update.insert("updatedAt", currentDateTime.toString(Qt::ISODate));
		where.insert("custId", placeholders.value("custId").toJsonValue());
		where.insert("empId", placeholders.value("empId").toJsonValue());
		where.insert("userId", placeholders.value("userId").toJsonValue());

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
		placeholders.insert("empId", local.value("empId").toString());
		placeholders.insert("userId", local.value("userId").toString());

		qDebug() << set;
		qDebug() << placeholders;
		qDebug() << update;
		qDebug() << where;

		QString queryToExec = "UPDATE users SET " + set.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId";
		(void)queryManager.ExecuteTargetSql_Map(queryToExec, placeholders);

		update.insert("updatedAt", currentDateTime.toString(Qt::ISODate));
		where.insert("custId", placeholders.value("custId").toJsonValue());
		where.insert("empId", placeholders.value("empId").toJsonValue());
		where.insert("userId", placeholders.value("userId").toJsonValue());

		body.insert("update", update);
		body.insert("where", where);

		(void)UpdateRemote(local, body);

		return;
	}
}

int UsersManager::SyncWebportal()
{
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };

	QJsonArray webportalResults = GetRemote(QJsonArray({ "custId", "userId", "empId", trak_details.trak_id_type, "updatedAt" }));

	if (!webportalResults.isEmpty() && remote_count != webportalResults.size())
		remote_count = webportalResults.size();

	QJsonArray webportalGroupedResults = GetGroupedRemote(QJsonArray({ "custId", "userId", "empId", trak_details.trak_id_type }), QJsonArray({ "updatedAt" }), QJsonArray({ "custId", "userId", "empId", trak_details.trak_id_type, }), "MIN", nullptr);

	qDebug() << webportalResults;
	qDebug() << webportalGroupedResults;
	qDebug() << webportalResults.empty();
	//qDebug() << webportalEmployeeIds.empty;

	ServiceHelper().WriteToLog("Exporting Users");

	QString select;
	QJsonObject placeholderMap;

	//QVariantMap vMapConditionals = {};

	if (webportalResults.empty())
		select = "SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL ORDER BY id DESC LIMIT " + QString::number(webportal_details.query_limit);
	else {
		QJsonArray employeeIds;
		QJsonArray userIds;
		QJsonArray updatedAtDates;

		for (const QJsonValue& entry : webportalResults)
		{
			if (!entry.isObject())
				continue;

			QJsonObject entryObject = entry.toObject();

			if (entryObject.isEmpty() || entryObject.value("empId").isNull() || entryObject.value("empId").isUndefined())
				continue;

			employeeIds.push_back(entryObject.value("empId"));
			userIds.push_back(entryObject.value("userId"));
			updatedAtDates.push_back(entryObject.value("updatedAt"));

		}

		placeholderMap.insert("empIds", employeeIds);
		placeholderMap.insert("userIds", userIds);
		placeholderMap.insert("tz", time_zone);

		if (webportalGroupedResults.size() <= 0)
			select = "SELECT * FROM users WHERE empId NOT IN (:empIds) AND userId NOT IN (:userIds) ORDER BY id ASC LIMIT " + QString::number(webportal_details.query_limit);
		else {
			select = "SELECT * FROM users WHERE (empId NOT IN (:empIds) AND userId NOT IN (:userIds)) OR CONVERT_TZ(updatedAt, :tz, '+00:00') NOT IN (:updatedAts) ORDER BY updatedAt ASC, id ASC LIMIT " + QString::number(webportal_details.query_limit);
			placeholderMap.insert("updatedAts", updatedAtDates);
		}

	}

	//std::vector sqlQueryResults = queryManager.ExecuteTargetSql(employeeSelect, vMapConditionals);
	QJsonArray queryResults = GetLocal(select, placeholderMap);

	qDebug() << QJsonDocument(queryResults).toJson().toStdString().data();

	for (const auto& queryResult : queryResults) {
		if (!queryResult.isObject())
			continue;
		
		QJsonObject result = queryResult.toObject();


		if (placeholderMap.value("userIds").toArray().contains(result.value("userId")) && placeholderMap.value("empIds").toArray().contains(result.value("empId")))
		{
			result[trak_details.trak_id_type] = trak_details.trak_id;

			QJsonObject where;
			where.insert("custId", result.value("custId"));
			where.insert("empId", result.value("empId"));
			where.insert("userId", result.value("userId"));
			where.insert(trak_details.trak_id_type, trak_details.trak_id);
			QJsonObject webportalEntry = GetRemote(QJsonArray(), where).at(0).toObject();

			qDebug() << "Local: " << QJsonDocument(result).toJson().toStdString().data();
			qDebug() << "Remote: " << QJsonDocument(webportalEntry).toJson().toStdString().data();

			HandleUpdatingEntries(result, webportalEntry);

			continue;
		}

		//qDebug() << result;

		std::map<std::string, std::string> sqliteData;
		QJsonObject data;

		QFuture<void> parserFuture = QtConcurrent::run([=, &result, &sqliteData, &data]() {

			for (auto it = result.constBegin(); it != result.constEnd(); ++it)
			{
				QString key = it.key();
				QString val;

				//if(it.value().isString())
				val = it.value().toString().trimmed().simplified();

				if (val.isEmpty() || val == "''")
					continue;
				if (!skipTargetCols.contains(key))
					sqliteData[key.toStdString()] = val.toStdString();
				data[key] = val;
			}


			});

		parserFuture.waitForFinished();


		if (!data.contains("userId"))
			continue;

		if (!data.contains(trak_details.trak_id_type) || data.value(trak_details.trak_id_type).toString() != trak_details.trak_id)
			data[trak_details.trak_id_type] = trak_details.trak_id;

		QVariant empId;

		if (data.contains("empId")) {
			empId = data.value("empId").toVariant();
		}
		else if (result.contains("empId")) {
			QJsonArray userEmpId = queryManager.ExecuteTargetSql("SELECT empId as uuid FROM employees WHERE custId = :custId AND userId = :userId", data);

			if (userEmpId.size() >= 1) {
				empId = userEmpId.at(0).toObject().value("uuid").toVariant();
			}
		}


		if (empId.isNull())
		{
			QJsonArray uuidVector = queryManager.ExecuteTargetSql("SELECT REGEXP_REPLACE(UUID(), '-', '') as uuid", QJsonObject());

			empId = uuidVector.at(1).toObject().value("uuid");

			qDebug() << uuidVector;
		}

		if (!webportalResults.isEmpty()) {
			QJsonObject webportalEmployee;
			QFutureSynchronizer<void> synchronizer;

			synchronizer.addFuture(QtConcurrent::map(webportalResults, [=, &empId, &data](const QJsonValue& result) {
				qDebug() << result;
				if (!result.isObject())
					return;
				QJsonObject resultObject = result.toObject();

				if (resultObject.value("empId").toString() == data.value("empId").toString()) return;
				if (resultObject.value("userId").toString() != data.value("userId").toString()) return;

				empId = resultObject.value("empId").toVariant();

				}));

			synchronizer.waitForFinished();
		}

		if (empId.toString() != data.value("empId").toString()) {
			QJsonObject placeholder;
			placeholder.insert("empId", empId.toJsonValue());
			placeholder.insert("oldEmpId", data.value("empId"));
			placeholder.insert("userId", data.value("userId"));

			(void)queryManager.ExecuteTargetSql("UPDATE users SET empId = :empId WHERE userId = :userId AND empId = :oldEmpId", placeholder);

			data["empId"] = empId.toString();

			sqliteData["empId"] = empId.toString().toStdString();
		}

#if true
		sqliteManager.AddEntry(
			"users",
			sqliteData
		);
#endif

		sqliteData.clear();

		qDebug() << data;

		if (SendToRemote(result, data))
			++remote_count;
	}

	remote_count = GetRemoteCount();

	return NEXTSTEP::CHECK_NEXT_STEP;
}

int UsersManager::SyncLocal()
{
	QStringList skipTargetCols = { "id"};

	//QJsonArray({ "custId", "userId", "empId", "updatedAt" })
	QJsonArray webportalResults = GetRemote();

	if (!webportalResults.isEmpty() && remote_count != webportalResults.size())
		remote_count = webportalResults.size();

	QJsonArray columns = GetColumns();

	/*QFuture<QJsonArray>columnFuture(QtConcurrent::run([=, &columns]()
		{*/
	QJsonArray columnNames;

	for (const auto& column : columns)
	{
		columnNames.append(column.toObject().value("Field"));
	}
			/*return columnNames;
		})
	);*/

	//QJsonArray webportalGroupedResults = GetGroupedRemoteEmployees(QJsonArray({ "custId", "userId", "empId" }), QJsonArray({ "updatedAt" }), QJsonArray({ "custId", "userId", "empId" }), "MIN", nullptr);

	qDebug() << QJsonDocument(webportalResults).toJson().toStdString().data();
	//qDebug() << webportalGroupedResults;
	qDebug() << webportalResults.empty();
	//qDebug() << webportalEmployeeIds.empty;

	ServiceHelper().WriteToLog("Importing Users");

	QString querySelect;
	QVariantMap placeholderMap;

	//QVariantMap vMapConditionals = {};

	//if (webportalResults.empty())
	QJsonArray employeeIds;
	QJsonArray userIds;
	for (const QJsonValue& result : webportalResults)
	{
		if (!result.isObject())
			continue;

		QJsonObject resultObject = result.toObject();

		if (resultObject.isEmpty() || resultObject.value("empId").isNull() || resultObject.value("empId").isUndefined() || resultObject.value("userId").isNull() || resultObject.value("userId").isUndefined())
			continue;

		employeeIds.append(resultObject.value("empId"));
		userIds.append(resultObject.value("userId"));

	}

	placeholderMap.insert("empIds", employeeIds);
	placeholderMap.insert("userIds", userIds);

	qDebug() << placeholderMap;

	QJsonArray queryResults; //= GetLocal("SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL AND (empId IN (:empIds) OR userId IN (:userIds))", placeholderMap);
	//else {

	queryManager.ExecuteTargetSql("SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL AND (empId IN (:empIds) OR userId IN (:userIds))", placeholderMap, &queryResults);

	qDebug() << "queryResults" << queryResults;

	employeeIds = QJsonArray();
	userIds = QJsonArray();

	placeholderMap = QVariantMap();
	//placeholderMap.erase(placeholderMap.find("empIds"));

	for (const QJsonValue& result : queryResults)
	{
		if (!result.isObject())
			continue;

		QJsonObject resultObject = result.toObject();

		if (resultObject.isEmpty() || resultObject.value("empId").isNull() || resultObject.value("empId").isUndefined())
			continue;

		employeeIds.push_back(resultObject.value("empId"));
		userIds.push_back(resultObject.value("userId"));

	}

	placeholderMap.insert("empIds", employeeIds);
	placeholderMap.insert("userIds", userIds);

	qDebug() << placeholderMap;

	//	//employeeSelect = "SELECT * FROM employees WHERE empId NOT IN (:empIds) ORDER BY id ASC";

	//	/*if (webportalGroupedResults.size() <= 0)
	//employeeSelect = "SELECT empId FROM employees WHERE empId IN (:empIds) ORDER BY id ASC";
	//	else {
	//		employeeSelect = "SELECT * FROM employees WHERE empId NOT IN (:empIds) OR updatedAt NOT IN (:updatedAts) ORDER BY updatedAt ASC, id ASC LIMIT " + QString::number(webportal_details.query_limit);
	//		placeholderMap.insert("updatedAts", updatedAtDates);
	//	}*/

	//}

	//std::vector sqlQueryResults = queryManager.ExecuteTargetSql(employeeSelect, vMapConditionals);




	//qDebug() << QJsonDocument(queryResults).toJson().toStdString().data();

	QMutex mutex;

	for (const QJsonValueRef& webportalResult : webportalResults) {
		if (!webportalResult.isObject())
			continue;

		QJsonObject result = webportalResult.toObject();

		qDebug() << QJsonDocument(result).toJson().toStdString().data();

		if (placeholderMap.value("userIds").toJsonArray().contains(result.value("userId")))
		{
			QJsonObject where;
			where.insert("custId", result.value("custId"));
			where.insert("empId", result.value("empId"));
			where.insert("userId", result.value("userId"));
			where.insert(trak_details.trak_id_type, trak_details.trak_id);
			/*QJsonObject webportalEntry = GetRemoteEmployees(QJsonArray(), where).at(0).toObject();*/
			QJsonArray localEntries = GetLocal("SELECT * FROM users WHERE empId = :empId OR userId = :userId", where);

			if (!localEntries.isEmpty()) {
				QJsonObject localEntry = localEntries.at(0).toObject();

				qDebug() << "Local: " << QJsonDocument(localEntry).toJson().toStdString().data();
				qDebug() << "Remote: " << QJsonDocument(result).toJson().toStdString().data();
				localEntry[trak_details.trak_id_type] = trak_details.trak_id;

				HandleUpdatingEntries(localEntry, result);
				continue;
			}

		}

		//qDebug() << result;


		QJsonObject data;

		//QJsonArray tableColumnNames = columnFuture.result();
		QJsonArray tableColumnNames = columnNames;

		QStringList datetimeCols = { "updatedAt", "createdAt" };

		QFuture<std::map<std::string, std::string>> parserFuture(QtConcurrent::run([=, &result, &data, &mutex, &tableColumnNames]() {
			std::map<std::string, std::string> sqliteData;
			for (auto it = result.constBegin(); it != result.constEnd(); ++it)
			{
				QString key = it.key();
				QString val;

				//if(it.value().isString())
				val = it.value().toVariant().toString().trimmed().simplified();

				qDebug() << key << ": " << val;

				if (val.isEmpty() || val == "''")
					continue;

				if (!tableColumnNames.contains(key))
					continue;

				if (datetimeCols.contains(key))
				{
					qDebug() << val;
					val = QDateTime::fromString(val, Qt::ISODateWithMs).toLocalTime().toString(Qt::ISODateWithMs);
					qDebug() << val;
				}

				qDebug() << "skipTargetCols contains" << key << skipTargetCols.contains(key);
				QMutexLocker locker(&mutex);
				if (!skipTargetCols.contains(key)) {
					sqliteData[key.toStdString()] = val.toStdString();
					data[key] = val;
				}
			}

			return sqliteData;
			})
		);


#if true
		sqliteManager.AddEntry(
			"users",
			parserFuture.result()
		);
#endif

		if (!data.contains("userId"))
			continue;

		/*if (!data.contains(trak_details.trak_id_type) || data.value(trak_details.trak_id_type).toString() != trak_details.trak_id)
			data[trak_details.trak_id_type] = trak_details.trak_id;*/

			//qDebug() << data;

			/*if (SendEmployeeToRemote(result, data))
			{
				++remote_employees_count;
			}*/
		if (CreateLocal(data))
			++local_count;
	}

	QList<QString> conditions;
	conditions.append("userId <> ''");
	conditions.append("userId IS NOT NULL");
	local_count = GetLocalCount(conditions);

	return NEXTSTEP::CHECK_NEXT_STEP;
}

int UsersManager::UpdateOutdated()
{
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	DWORD size = sizeof(TCHAR);
	(void)rtManager.GetValSize("usersCheckedDate", REG_SZ, &size);

	if (!size) {
		(void)UpdateCheckedTime();
		(void)rtManager.GetValSize("usersCheckedDate", REG_SZ, &size);
	}

	TCHAR* buffer = new TCHAR[size];

	(void)rtManager.GetVal("usersCheckedDate", REG_SZ, buffer, size);

	QString lastCheckedDateTime(buffer);
	delete[] buffer;

	qDebug() << "lastCheckedDateTime" << lastCheckedDateTime;

	QDateTime last_checked_date(QDateTime::fromString(lastCheckedDateTime, Qt::ISODate));

	QMap<QString, QVariant> placeholder;
	placeholder.insert("lastCheckedDate", last_checked_date.date());
	placeholder.insert("lastCheckedTime", last_checked_date.time());
	placeholder.insert("last_checked_date", last_checked_date);
	placeholder.insert("tz", time_zone);

	qDebug() << "placeholder" << placeholder;

	QJsonArray outdatedLocals = queryManager.ExecuteTargetSql_Array("SELECT * FROM users WHERE CONVERT_TZ(updatedAt, :tz, '+00:00') >= :last_checked_date", placeholder);

	qDebug() << "outdatedLocals" << outdatedLocals;

	QJsonObject select;
	select.insert("outdated", lastCheckedDateTime);

	QJsonArray outdatedRemotes = GetRemote(QJsonArray(), QJsonObject(), select);

	qDebug() << "local_that_need_updating: " << outdatedLocals.size();
	qDebug() << "remote_that_need_updating: " << outdatedRemotes.size();

	if (!outdatedLocals.size() && !outdatedRemotes.size())
		return UpdateCheckedTime();

	if (outdatedLocals.size() >= outdatedRemotes.size())
	{
		//qDebug() << "outdatedLocalEmployees: " << QJsonDocument(outdatedLocalEmployees).toJson().toStdString().data();
		//qDebug() << "outdatedRemoteEmployees: " << QJsonDocument(outdatedRemoteEmployees).toJson().toStdString().data();

		for (const auto& outdatedLocal : outdatedLocals)
		{
			if (!outdatedLocal.isObject())
				continue;
			QJsonObject local = outdatedLocal.toObject();
			QJsonObject remote;

			for (const auto& outdatedRemote : outdatedRemotes) {
				if (!outdatedRemote.isObject())
					continue;

				QJsonObject outdatedRemoteObject = outdatedRemote.toObject();
				if (outdatedRemoteObject.value("empId") != local.value("empId") && outdatedRemoteObject.value("userId") != local.value("userId"))
					continue;

				remote = outdatedRemoteObject;
				break;
			}

			qDebug() << "local: " << local;
			qDebug() << "remote: " << remote;

			if (remote.isEmpty()) {
				//select.erase(select.find("outdated"));
				QJsonObject where;
				where.insert("empId", local.value("empId"));
				where.insert("userId", local.value("userId"));
				where.insert(trak_details.trak_id_type, trak_details.trak_id);
				QJsonArray targetRemote = GetRemote(QJsonArray(), where);

				if (targetRemote.isEmpty())
					throw HenchmanServiceException("Needed user instance from remote but failed to find one");

				remote = targetRemote.at(0).toObject();
			}

			qDebug() << "local: " << local;
			qDebug() << "remote: " << remote;

			HandleUpdatingEntries(local, remote);
		}
	}
	else {
		for (const auto& outdatedRemote : outdatedRemotes)
		{
			if (!outdatedRemote.isObject())
				continue;
			QJsonObject local;
			QJsonObject remote = outdatedRemote.toObject();

			for (const auto& outdatedLocal : outdatedLocals) {
				if (!outdatedLocal.isObject())
					continue;

				QJsonObject outdatedLocalObject = outdatedLocal.toObject();
				if (outdatedLocalObject.value("empId") != outdatedLocalObject.value("empId") && outdatedLocalObject.value("userId") != outdatedLocalObject.value("userId"))
					continue;

				local = outdatedLocalObject;
				break;
			}

			qDebug() << "local: " << local;
			qDebug() << "remote: " << remote;

			if (local.isEmpty()) {
				//select.erase(select.find("outdated"));
				QJsonObject where;
				where.insert("empId", remote.value("empId"));
				where.insert("userId", remote.value("userId"));
				QJsonArray targetLocal = GetLocal("SELECT * FROM users WHERE empId = :empId AND userId = :userId", where);

				if (targetLocal.isEmpty())
					throw HenchmanServiceException("Needed user instance from local but failed to find one");

				local = targetLocal.at(0).toObject();
			}

			qDebug() << "local: " << local;
			qDebug() << "remote: " << remote;

			HandleUpdatingEntries(local, remote);
		}
	}

	UpdateCheckedTime();

	return NEXTSTEP::CHECK_NEXT_STEP;
}


int UsersManager::ClearCloudUpdate()
{
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	DWORD size = sizeof(TCHAR);

	(void)rtManager.GetValSize(std::string(registryEntry).append("Date").data(), REG_SZ, &size);

	if (size == 0) {
		QString currDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

		if (rtManager.SetVal(std::string(registryEntry).append("Date").data(), REG_SZ, currDate.toStdString().data(), currDate.toStdString().size()))
			throw HenchmanServiceException("Failed to store checked date in registery");

		(void)rtManager.GetValSize(std::string(registryEntry).append("Date").data(), REG_SZ, &size);
	}

	TCHAR* buffer = new TCHAR[size];

	LONG err = rtManager.GetVal(std::string(registryEntry).append("Date").data(), REG_SZ, buffer, size);

	if (err) {
		const TCHAR* errMsg = "%d";
		TCHAR buffer[2048] = "\0";
		rtManager.GetSystemError(errMsg, err, buffer, 2048);
		throw HenchmanServiceException("Failed to fetch target stored value from registry. Error: " + std::to_string(err) + " - " + std::string(buffer));
	}

	QJsonObject params;
	params["date"] = QString(buffer);
	params["tz"] = time_zone;

	(void)queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% users%' AND CONVERT_TZ(DatePosted,:tz, '+00:00') < :date AND (posted = 0 OR posted = 2)", params);

	delete[] buffer;

	return NEXTSTEP::ALL_UPDATED;
};