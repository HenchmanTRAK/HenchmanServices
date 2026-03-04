
#include "EmployeesManager.h"

EmployeesManager::EmployeesManager(QObject* parent, const TrakDetails& trakDetails, const WebportalDetails& webportalDetails, const s_DATABASE_INFO& database_info)
	:TRAKEntriesManager(parent, trakDetails, webportalDetails, database_info)
{	

	qDebug() << "Initialized EmployeesManager";

	registryEntry = "employeesChecked";

	TRAKEntriesManager::Initialize();
}

EmployeesManager::~EmployeesManager()
{
}

void EmployeesManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values)
{
	TRAKEntriesManager::breakoutValuesToUpdate(older, newer, set_values, updated_values);
}

void EmployeesManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values)
{

	TRAKEntriesManager::breakoutValuesToUpdate(older, newer, set_values, updated_values);
};


QJsonArray EmployeesManager::GetColumns()
{
	QJsonObject overloader;
	std::string colQuery =
		"SHOW COLUMNS from employees";
	QJsonArray colQueryResults = queryManager.execute(colQuery, overloader);

	return colQueryResults;
}

int EmployeesManager::GetLocalCount(const QList<QString> & p_conditions, const QJsonObject& placeholders)
{
	int local_employees = 0;

	try {
		QList<QString> conditions(p_conditions);


		if (conditions.isEmpty() && local_count == 0) {
			conditions.append("userId <> ''");
			conditions.append("userId IS NOT NULL");
		} else if(conditions.isEmpty())
			throw std::exception();

		QJsonArray rowCount = queryManager.execute("SELECT COUNT(*) as total FROM employees WHERE " + conditions.join(" AND "), placeholders);

		if (rowCount.size() <= 0 || !rowCount.at(0).isObject()) {
			throw std::exception();
		}

		local_employees = rowCount.at(0).toObject().value("total").toVariant().toInt();
	}catch (const std::exception& e) {
		local_employees = local_count;
	}
	return local_employees;
}

int EmployeesManager::GetRemoteCount(const QJsonObject& p_select, const QJsonObject& p_where, QJsonObject * p_returned_data)
{

	int remote_employees = 0;

	try {
		QJsonObject query;
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if(!where.contains("custId"))
			(void)where.insert("custId", QString::number(trak_details.cust_id));
		
		if (!where.contains(trak_details.trak_id_type))
			(void)where.insert(trak_details.trak_id_type, trak_details.trak_id);
		
		if (!select.contains("count"))
			(void)select.insert("count", "total");

		(void)query.insert("where", where);
		(void)query.insert("select", select);
		
		QJsonDocument reply;

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/employees", query, &reply))
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

		remote_employees = webportalToolCount.value("total").toInt();
		
		if (p_returned_data)
			webportalToolCount.swap(*p_returned_data);
		
	}
	catch (void*)
	{
		remote_employees = remote_count;
	}


	return remote_employees;
}

QJsonArray EmployeesManager::GetRemote(const QJsonArray& columns, const QJsonObject& p_where, const QJsonObject& p_select)
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
		
		if(!select.isEmpty())
			(void)query.insert("select", select);
		


		qDebug() << query;

		QJsonDocument reply;

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/employees", query, &reply))
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
	catch (void*)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

QJsonArray EmployeesManager::GetLocal(const QString& query, const QJsonObject& placeholders)
{
	return queryManager.execute(query, placeholders);
}

QJsonArray EmployeesManager::GetGroupedRemote(const QJsonArray& columns, const QJsonArray& grouped_columns, const QJsonArray& group_by, const QString& type, const QString& separator, const QJsonObject& p_where)
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
		if(!separator.isEmpty())
			(void)group.insert("separator", separator);

		(void)select.insert("group", group);

		(void)query.insert("select", select);

		qDebug() << query;

		QJsonDocument reply;

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/employees", query, &reply))
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
	catch (void*)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

int EmployeesManager::SendToRemote(const QJsonObject& entry, const QJsonObject& data)
{

	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (networkManager.makePostRequest(webportal_details.api_url + "/employees", entry, body, &reply)) {
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

int EmployeesManager::CreateLocal(const QJsonObject& entry)
{
	QJsonArray results = queryManager.execute("INSERT INTO employees (" + entry.keys().join(", ") + ") VALUES (:" + entry.keys().join(", :") + ")", entry);

	if (results.size() == 0)
		return 0;

	return 1;
}

int EmployeesManager::UpdateRemote(const QJsonObject& entry, const QJsonObject& data)
{
	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (networkManager.makePatchRequest(webportal_details.api_url + "/employees", entry, body, &reply)) {
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

QJsonArray EmployeesManager::UpdateLocal(const QList<QString>& update, const QJsonObject& placeholders)
{
	return queryManager.execute("UPDATE employees SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId", placeholders);
}

QList<QVariantMap> EmployeesManager::UpdateLocal(const QList<QString>& update, const QVariantMap& placeholders)
{
	QString queryToExec = "UPDATE employees SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId";
	QVariantMap newPlaceholders = placeholders;
	return queryManager.execute(queryToExec, newPlaceholders);
}

void EmployeesManager::HandleUpdatingEntries(const QJsonObject& local, const QJsonObject& remote)
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
		qInfo() << "Remote Employee is newer that Local";
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

		QString queryToExec = "UPDATE employees SET " + set.join(", ") + " WHERE custId = :custId AND empId = :empId";
		(void)queryManager.execute(queryToExec, placeholders);
		
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
		qInfo() << "Remote Employee is older that Local";
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

		QString queryToExec = "UPDATE employees SET " + set.join(", ") + " WHERE custId = :custId AND empId = :empId";
		(void)queryManager.execute(queryToExec, placeholders);

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

int EmployeesManager::SyncWebportal()
{
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };

	QJsonArray webportalResults = GetRemote(QJsonArray({ "custId", "userId", "empId", "updatedAt" }));

	if(!webportalResults.isEmpty() && remote_count != webportalResults.size())
		remote_count = webportalResults.size();

	QJsonArray webportalGroupedResults = GetGroupedRemote(QJsonArray({ "custId", "userId", "empId" }), QJsonArray({ "updatedAt" }), QJsonArray({ "custId", "userId", "empId" }), "MIN", nullptr);

	qDebug() << webportalResults;
	qDebug() << webportalGroupedResults;
	qDebug() << webportalResults.empty();
	//qDebug() << webportalEmployeeIds.empty;

	ServiceHelper().WriteToLog("Exporting Employees");

	QString employeeSelect;
	QJsonObject placeholderMap;

	//QVariantMap vMapConditionals = {};

	if (webportalResults.empty())
		employeeSelect = "SELECT * FROM employees WHERE userId <> '' AND userId IS NOT NULL ORDER BY id DESC LIMIT " + QString::number(webportal_details.query_limit);
	else {
		QJsonArray employeeIds;
		QJsonArray updatedAtDates;

		for (const QJsonValue& employee : webportalResults)
		{
			if (!employee.isObject())
				continue;

			QJsonObject employeeObject = employee.toObject();

			if (employeeObject.isEmpty() || employeeObject.value("empId").isNull() || employeeObject.value("empId").isUndefined())
				continue;

			employeeIds.push_back(employeeObject.value("empId"));
			updatedAtDates.push_back(employeeObject.value("updatedAt"));

		}

		placeholderMap.insert("empIds", employeeIds);
		placeholderMap.insert("tz", time_zone);

		if (webportalGroupedResults.size() <= 0)
			employeeSelect = "SELECT * FROM employees WHERE empId NOT IN (:empIds) ORDER BY id ASC LIMIT " + QString::number(webportal_details.query_limit);
		else {
			employeeSelect = "SELECT * FROM employees WHERE empId NOT IN (:empIds) OR CONVERT_TZ(updatedAt, :tz, '+00:00') NOT IN (:updatedAts) ORDER BY updatedAt ASC, id ASC LIMIT " + QString::number(webportal_details.query_limit);
			placeholderMap.insert("updatedAts", updatedAtDates);
		}

	}

	//std::vector sqlQueryResults = queryManager.execute(employeeSelect, vMapConditionals);
	QJsonArray queryResults = GetLocal(employeeSelect, placeholderMap);

	qDebug() << QJsonDocument(queryResults).toJson().toStdString().data();

	for (const auto& queryResult : queryResults) {
		if (!queryResult.isObject())
			continue;
		QJsonObject result = queryResult.toObject();

		

		if (placeholderMap.value("empIds").toArray().contains(result.value("empId")))
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

		if(!data.contains(trak_details.trak_id_type) || data.value(trak_details.trak_id_type).toString() != trak_details.trak_id)
			data[trak_details.trak_id_type] = trak_details.trak_id;

		QVariant empId;

		if (data.contains("empId")) {
			empId = data.value("empId").toVariant();
		}
		else if (result.contains("empId")) {
			QJsonArray userEmpId = queryManager.execute("SELECT empId as uuid FROM users WHERE custId = :custId AND userId = :userId", data);

			if (userEmpId.size() >= 1) {
				empId = userEmpId.at(0).toObject().value("uuid").toVariant();
			}
		}


		if (empId.isNull())
		{
			QJsonArray uuidVector = queryManager.execute("SELECT REGEXP_REPLACE(UUID(), '-', '') as uuid", QJsonObject());

			empId = uuidVector.at(1).toObject().value("uuid");

			qDebug() << uuidVector;
		}

		if (!webportalResults.isEmpty()) {
			QJsonObject webportalEmployee;
			QFutureSynchronizer<void> synchronizer;

			synchronizer.addFuture(QtConcurrent::map(webportalResults, [=, &empId, &data](const QJsonValue& employee) {
				qDebug() << employee;
				if (!employee.isObject())
					return;
				QJsonObject employeeObject = employee.toObject();

				if (employeeObject.value("empId").toString() == data.value("empId").toString()) return;
				if (employeeObject.value("userId").toString() != data.value("userId").toString()) return;

				empId = employeeObject.value("empId").toVariant();

				}));

			synchronizer.waitForFinished();
		}

		if (empId.toString() != data.value("empId").toString()) {
			QJsonObject placeholder;
			placeholder.insert("empId", empId.toJsonValue());
			placeholder.insert("userId", data.value("userId"));

			(void)queryManager.execute("UPDATE employees SET empId = :empId WHERE userId = :userId", placeholder);

			data["empId"] = empId.toString();

			sqliteData["empId"] = empId.toString().toStdString();
		}

#if true
		sqliteManager.AddEntry(
			"employees",
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

int EmployeesManager::SyncLocal()
{
	QStringList skipTargetCols = { "id", trak_details.trak_id_type};

	//QJsonArray({ "custId", "userId", "empId", "updatedAt" })
	QJsonArray webportalResults = GetRemote();

	if (!webportalResults.isEmpty() && remote_count != webportalResults.size())
		remote_count = webportalResults.size();

	QJsonArray employeeColumns = GetColumns();

	QFuture<QJsonArray>employeeColumnFuture(QtConcurrent::run([=, &employeeColumns]()
		{
			QJsonArray columnNames;

			for (const auto& employeeColumn : employeeColumns)
			{
				columnNames.append(employeeColumn.toObject().value("Field"));
			}
			return columnNames;
		})
	);

	//QJsonArray webportalGroupedResults = GetGroupedRemoteEmployees(QJsonArray({ "custId", "userId", "empId" }), QJsonArray({ "updatedAt" }), QJsonArray({ "custId", "userId", "empId" }), "MIN", nullptr);

	qDebug() << QJsonDocument(webportalResults).toJson().toStdString().data();
	//qDebug() << webportalGroupedResults;
	qDebug() << webportalResults.empty();
	//qDebug() << webportalEmployeeIds.empty;

	ServiceHelper().WriteToLog("Exporting Employees");

	QString employeeSelect;
	QJsonObject placeholderMap;

	//QVariantMap vMapConditionals = {};

	//if (webportalResults.empty())
	QJsonArray employeeIds;
	QJsonArray userIds;
	for (const QJsonValue& employee : webportalResults)
	{
		if (!employee.isObject())
			continue;

		QJsonObject employeeObject = employee.toObject();

		if (employeeObject.isEmpty() || employeeObject.value("empId").isNull() || employeeObject.value("empId").isUndefined())
			continue;

		employeeIds.push_back(employeeObject.value("empId"));
		userIds.push_back(employeeObject.value("userId"));

	}

	placeholderMap.insert("empIds", employeeIds);
	placeholderMap.insert("userIds", userIds);

	qDebug() << placeholderMap;

	employeeSelect = "SELECT * FROM employees WHERE userId <> '' AND userId IS NOT NULL AND (empId IN (:empIds) OR userId IN (:userIds)) ORDER BY id DESC";

	QJsonArray queryResults = GetLocal(employeeSelect, placeholderMap);
	//else {

	qDebug() << "queryResults" << queryResults;

	employeeIds = QJsonArray();
	userIds = QJsonArray();

	placeholderMap = QJsonObject();
	//placeholderMap.erase(placeholderMap.find("empIds"));

	for (const QJsonValue& employee : queryResults)
	{
		if (!employee.isObject())
			continue;

		QJsonObject employeeObject = employee.toObject();

		if (employeeObject.isEmpty() || employeeObject.value("empId").isNull() || employeeObject.value("empId").isUndefined())
			continue;

		employeeIds.push_back(employeeObject.value("empId"));

	}

	placeholderMap.insert("empIds", employeeIds);

	qDebug() << placeholderMap;

	//	//employeeSelect = "SELECT * FROM employees WHERE empId NOT IN (:empIds) ORDER BY id ASC";

	//	/*if (webportalGroupedResults.size() <= 0)
	//employeeSelect = "SELECT empId FROM employees WHERE empId IN (:empIds) ORDER BY id ASC";
	//	else {
	//		employeeSelect = "SELECT * FROM employees WHERE empId NOT IN (:empIds) OR updatedAt NOT IN (:updatedAts) ORDER BY updatedAt ASC, id ASC LIMIT " + QString::number(webportal_details.query_limit);
	//		placeholderMap.insert("updatedAts", updatedAtDates);
	//	}*/

	//}

	//std::vector sqlQueryResults = queryManager.execute(employeeSelect, vMapConditionals);
	



	//qDebug() << QJsonDocument(queryResults).toJson().toStdString().data();

	QMutex mutex;

	for (const QJsonValueRef& webportalResult : webportalResults) {
		if (!webportalResult.isObject())
			continue;
		
		QJsonObject result = webportalResult.toObject();

		qDebug() << QJsonDocument(result).toJson().toStdString().data();

		if (placeholderMap.value("empIds").toArray().contains(result.value("empId")))
		{
			QJsonObject where;
			where.insert("custId", result.value("custId"));
			where.insert("empId", result.value("empId"));
			where.insert("userId", result.value("userId"));
			where.insert(trak_details.trak_id_type, trak_details.trak_id);
			/*QJsonObject webportalEntry = GetRemoteEmployees(QJsonArray(), where).at(0).toObject();*/
			QJsonArray localEntries = GetLocal("SELECT * FROM employees WHERE empId = :empId OR userId = :userId", where);

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

		QJsonArray employeeTableColumnNames = employeeColumnFuture.result();

		QStringList datetimeCols = { "updatedAt", "createdAt" };

		QFuture<std::map<std::string, std::string>> parserFuture(QtConcurrent::run([=, &result, &data, &mutex, &employeeTableColumnNames]() {
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

				if (!employeeTableColumnNames.contains(key))
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
			"employees",
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

int EmployeesManager::UpdateOutdated()
{
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	DWORD size = sizeof(TCHAR);
	(void)rtManager.GetValSize("employeesCheckedDate", REG_SZ, &size);
	
	if (!size) {
		(void)UpdateCheckedTime();
		(void)rtManager.GetValSize("employeesCheckedDate", REG_SZ, &size);
	}
	
	std::vector<TCHAR> buffer;
	buffer.reserve(size);

	(void)rtManager.GetVal("employeesCheckedDate", REG_SZ, buffer.data(), &size);

	QString lastCheckedDateTime(buffer.data());

	qDebug() << "lastCheckedDateTime" << lastCheckedDateTime;
	
	QDateTime last_checked_date(QDateTime::fromString(lastCheckedDateTime, Qt::ISODate));

	QMap<QString, QVariant> placeholder;
	placeholder.insert("lastCheckedDate", last_checked_date.date());
	placeholder.insert("lastCheckedTime", last_checked_date.time());
	placeholder.insert("last_checked_date", last_checked_date);
	placeholder.insert("tz", time_zone);

	qDebug() << "placeholder" << placeholder;

	QList<QVariantMap> outdatedLocalEmployees = queryManager.execute("SELECT * FROM employees WHERE CONVERT_TZ(updatedAt, :tz, '+00:00') >= :last_checked_date", placeholder);

	qDebug() << "outdatedLocalEmployees" << outdatedLocalEmployees;

	QJsonObject select;
	select.insert("outdated", lastCheckedDateTime);

	QJsonArray outdatedRemoteEmployees = GetRemote(QJsonArray(), QJsonObject(), select);

	qDebug() << "local_employeees_that_need_updating: " << outdatedLocalEmployees.size();
	qDebug() << "remote_employeees_that_need_updating: " << outdatedRemoteEmployees.size();

	if (!outdatedLocalEmployees.size() && !outdatedRemoteEmployees.size())
		return UpdateCheckedTime();
	
	if (outdatedLocalEmployees.size() >= outdatedRemoteEmployees.size())
	{
		//qDebug() << "outdatedLocalEmployees: " << QJsonDocument(outdatedLocalEmployees).toJson().toStdString().data();
		//qDebug() << "outdatedRemoteEmployees: " << QJsonDocument(outdatedRemoteEmployees).toJson().toStdString().data();

		for (const auto& outdatedLocalEmployee : outdatedLocalEmployees)
		{
			QJsonObject localEmployee = QJsonObject::fromVariantMap(outdatedLocalEmployee);
			QJsonObject remoteEmployee;

			for (const auto& outdatedRemoteEmployee : outdatedRemoteEmployees) {
				if (!outdatedRemoteEmployee.isObject())
					continue;

				QJsonObject outdatedRemoteEmployeeObject = outdatedRemoteEmployee.toObject();
				if (outdatedRemoteEmployeeObject.value("empId") != localEmployee.value("empId"))
					continue;

				remoteEmployee = outdatedRemoteEmployeeObject;
				break;
			}

			qDebug() << "localEmployee: " << localEmployee;
			qDebug() << "remoteEmployee: " << remoteEmployee;

			if (remoteEmployee.isEmpty()) {
				//select.erase(select.find("outdated"));
				QJsonObject where;
				where.insert("empId", localEmployee.value("empId"));
				QJsonArray targetRemoteEmployee = GetRemote(QJsonArray(), where);

				if (targetRemoteEmployee.isEmpty())
					throw HenchmanServiceException("Needed employee instance from remote but failed to find one");

				remoteEmployee = targetRemoteEmployee.at(0).toObject();
			}

			qDebug() << "localEmployee: " << localEmployee;
			qDebug() << "remoteEmployee: " << remoteEmployee;

			HandleUpdatingEntries(localEmployee, remoteEmployee);
		}
	}
	else {
		for (const auto& outdatedRemoteEmployee : outdatedRemoteEmployees)
		{
			if (!outdatedRemoteEmployee.isObject())
				continue;
			QJsonObject localEmployee;
			QJsonObject remoteEmployee = outdatedRemoteEmployee.toObject();

			for (const auto& outdatedLocalEmployee : outdatedLocalEmployees) {
				QJsonObject outdatedLocalEmployeeObject = QJsonObject::fromVariantMap(outdatedLocalEmployee);
				if (outdatedLocalEmployeeObject.value("empId") != localEmployee.value("empId"))
					continue;

				localEmployee = outdatedLocalEmployeeObject;
				break;
			}

			qDebug() << "localEmployee: " << localEmployee;
			qDebug() << "remoteEmployee: " << remoteEmployee;

			if (localEmployee.isEmpty()) {
				//select.erase(select.find("outdated"));
				QJsonObject where;
				where.insert("empId", remoteEmployee.value("empId"));
				QJsonArray targetLocalEmployee = GetLocal("SELECT * FROM employees WHERE empId = :empId", where);

				if (targetLocalEmployee.isEmpty())
					throw HenchmanServiceException("Needed employee instance from local but failed to find one");

				localEmployee = targetLocalEmployee.at(0).toObject();
			}

			qDebug() << "localEmployee: " << localEmployee;
			qDebug() << "remoteEmployee: " << remoteEmployee;

			HandleUpdatingEntries(localEmployee, remoteEmployee);
		}
	}

	UpdateCheckedTime();

	return NEXTSTEP::CHECK_NEXT_STEP;
}

int EmployeesManager::ClearCloudUpdate()
{
	QString date = TRAKEntriesManager::ClearCloudUpdate();

	QJsonObject params;
	params["date"] = date;
	params["tz"] = time_zone;
	qDebug() << "params" << params;

	(void)queryManager.execute("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% employees%' AND CONVERT_TZ(DatePosted, :tz, '+00:00') < :date AND (posted = 0 OR posted = 2)", params);

	return NEXTSTEP::ALL_UPDATED;
};