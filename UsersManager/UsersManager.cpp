
#include "UsersManager.h"

using namespace UsersManager;
using namespace TRAKEntriesManager;

CUsersManager::CUsersManager(QObject* parent, const TrakDetails& trakDetails, const WebportalDetails& webportalDetails, const s_DATABASE_INFO& database_info)
	:CTRAKEntriesManager(parent, trakDetails, webportalDetails, database_info)
{

	qDebug() << "Initialized UsersManager";

	m_registryEntry = "usersChecked";

	m_webportal_details.base_route = "users";

	m_db_info.table = "users";

	CTRAKEntriesManager::Initialize();

}

CUsersManager::~CUsersManager()
{
	LOG << "Deconstructing UsersManager";
}

void CUsersManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values)
{
	CTRAKEntriesManager::breakoutValuesToUpdate(older, newer, set_values, updated_values);
}

void CUsersManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values)
{

	CTRAKEntriesManager::breakoutValuesToUpdate(older, newer, set_values, updated_values);
};

QJsonArray CUsersManager::GetColumns(bool reset)
{
	CTRAKEntriesManager::GetColumns(reset);

	if (m_MySQL_Columns.size() <= 0)
		return m_MySQL_Columns;

	QJsonArray tableColumns = m_MySQL_Columns;

	QStringList uniqueIndexCols = { "custId", "userId", "kabId", "cribId", "scaleId" };
	s_UpdateLocalTableOptions update_options;

	(void)m_sqliteManager.ExecQuery(
		"pragma table_info(" + m_db_info.table + ")",
		&m_SQLITE_Columns
	);

	QJsonArray sqliteTableColumns = m_SQLITE_Columns;

	update_options.AddCreatedAt = true;
	update_options.AddUpdatedAt = true;
	update_options.AddEmpId = true;
	update_options.AddDisabledToSQLITE = true;
	update_options.AddDeletedToSQLITE = true;
	update_options.AddDisabledToMySQL = true;
	update_options.AddDeletedToMySQL = true;

	//QMutex mutex;

	QFuture<QJsonArray> mysqlTableColumnsFuture = QtConcurrent::run([&tableColumns]() {
		QJsonArray colummns = tableColumns;
		QJsonArray mysqlTableColumnNames;
		for (const auto& colummn : colummns)
		{
			if (!colummn.isObject())
				continue;
			QJsonObject mysqlColumn = colummn.toObject();
			mysqlTableColumnNames.append(mysqlColumn.value("Field").toString().trimmed());
		}
		return mysqlTableColumnNames;
		});

	QFuture<QJsonArray> sqliteTableColumnsFuture = QtConcurrent::run([&sqliteTableColumns]() {
		QJsonArray colummns = sqliteTableColumns;
		QJsonArray sqliteTableColumnNames;
		for (const auto& colummn : colummns)
		{
			if (!colummn.isObject())
				continue;
			QJsonObject sqliteColumn = colummn.toObject();
			
			qDebug() << sqliteColumn.value("name").toString();

			sqliteTableColumnNames.append(sqliteColumn.value("name").toString().trimmed());
		}
		return sqliteTableColumnNames;
		});

	QFuture<QJsonArray> columnsFuture = QtConcurrent::run([&tableColumns, &update_options, &uniqueIndexCols]() {
		QJsonArray columnsForTable = tableColumns;

		QJsonArray columns;
		QStringList skippedColumns;
		QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
		QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
		QJsonArray mysqlColumnNames;

		for (const auto& tableColumn : columnsForTable)
		{
			if (!tableColumn.isObject())
				throw HenchmanServiceException("Failed to retrieve list of columns for employees table");
			QJsonObject column = tableColumn.toObject();

			QString field = column.value("Field").toString().trimmed();

			if (field == "empId") {
				update_options.AddEmpId = false;

				if (!uniqueIndexCols.contains("empId"))
					uniqueIndexCols.push_back("empId");
			}

			if (field == "createdAt") {
				update_options.AddCreatedAt = false;
			}
			if (field == "updatedAt") {
				update_options.AddUpdatedAt = false;
			}
			if (field == "disabled") {
				update_options.AddDisabledToMySQL = false;
			}
			if (field == "deleted") {
				update_options.AddDeletedToMySQL = false;
			}

			if (skipTargetCols.contains(field)) {
				skippedColumns.push_back(field);
				continue;
			}

			if (dates.contains(column.value("Type").toString().toLower()))
				column["Type"] = "TEXT";

			mysqlColumnNames.append(field);

			columns.append((
				field + " " + column.value("Type").toString().toUpper() + " " +
				(uniqueIndexCols.contains(field)
					? "NOT NULL DEFAULT " + QString(column.value("Type").toString().toUpper() == "INT" ? "0" : "''") + ""
					: (column.value("Null").toString() == "NO"
						? "NOT NULL DEFAULT " + (column.value("Default").toString() == ""
							? "''"
							: column.value("Default").toString())
						: "NULL" + (column.value("Default").toString() == ""
							? ""
							: " DEFAULT " + column.value("Default").toString()
							)
						)
					)
				));
		}
		return columns;
		});


	QJsonArray sqliteTableColumnNames = sqliteTableColumnsFuture.result();
	QJsonArray mysqlTableColumnNames = mysqlTableColumnsFuture.result();

	qInfo() << sqliteTableColumnNames;
	qInfo() << mysqlTableColumnNames;

	int performTableUpdate = mysqlTableColumnNames.size() != sqliteTableColumnNames.size();

	if (!performTableUpdate)
	{
		for (int i = 0; i < mysqlTableColumnNames.size(); ++i)
		{
			if (sqliteTableColumnNames.contains(mysqlTableColumnNames.at(i)))
				continue;
			performTableUpdate = true;
			break;
		}
	}

	update_options.AddDisabledToSQLITE = !sqliteTableColumnNames.contains("disabled");
	update_options.AddDeletedToSQLITE = !sqliteTableColumnNames.contains("deleted");

	if (performTableUpdate)
	{

		(void)m_sqliteManager.ExecQuery("DROP INDEX IF EXISTS unique_custId_userId_kabId_cribId_scaleId");

		(void)m_sqliteManager.CreateTable(
			m_db_info.table.toStdString(),
			columnsFuture.result()
		);

		update_options.CreateUniqueIndex = true;
		update_options.AddEmpIdSqliteOnly = !sqliteTableColumnNames.contains("empId");

	}

	columnsFuture.waitForFinished();

	handleUpdatingLocalDB(m_db_info.table, uniqueIndexCols, &update_options);
	
	return m_MySQL_Columns;

}

int CUsersManager::GetLocalCount(const QList<QString>& p_conditions, const QJsonObject& p_placeholders)
{
	
	int local_users = 0;

	try {
		QList<QString> conditions(p_conditions);
		QJsonObject placeholders(p_placeholders);


		if (conditions.isEmpty()) {
			conditions.append("userId <> ''");
			conditions.append("userId IS NOT NULL");
			conditions.append("userId IN (SELECT userId FROM employees WHERE userId <> '' AND userId IS NOT NULL)");
		}
		
		local_users = CTRAKEntriesManager::GetLocalCount(conditions, placeholders);
		//QJsonArray rowCount = queryManager.execute("SELECT COUNT(*) as total FROM users WHERE " + conditions.join(" AND "), placeholders);

		/*if (rowCount.size() <= 0 || !rowCount.at(0).isObject()) {
			throw std::exception();
		}

		local_users = rowCount.at(0).toObject().value("total").toVariant().toInt();*/
	}
	catch (const std::exception& e) {
		local_users = local_count;
	}
	return local_users;
}

int CUsersManager::GetRemoteCount(const QJsonObject& p_select, const QJsonObject& p_where, QJsonObject* p_returned_data)
{

	int remote_users = 0;

	try {
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if (!select.contains("count"))
			(void)select.insert("count", "total");

		remote_users = CTRAKEntriesManager::GetRemoteCount(select, where, p_returned_data);

	}
	catch (const std::exception& e)
	{
		remote_users = remote_count;
	}


	return remote_users;
}

QJsonArray CUsersManager::GetRemote(const QJsonArray& columns, const QJsonObject& p_where, const QJsonObject& p_select)
{

	QJsonArray returnedValues;

	try {
		QJsonObject where(p_where);
		QJsonObject select(p_select);


		returnedValues = CTRAKEntriesManager::GetRemote(columns, where, select);

	}
	catch (const std::exception& e)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

QJsonArray CUsersManager::GetLocal(const QString& query, const QJsonObject& placeholders)
{
	GetTable();

	if (!placeholders.isEmpty()) {
		QSqlQuery sqlQuery(m_table.database());
	
		QString query_str = query;

		QVariantMap boundValues = m_queryManager.processPlaceholders(placeholders.toVariantMap(), query_str);
	
		sqlQuery.prepare(query_str);
		
		QMapIterator it(boundValues);

		while (it.hasNext()) {
			it.next();
			QString key = it.key();
			QString val = it.value().toString();
			sqlQuery.bindValue(key, val);
		}

		m_table.setQuery(sqlQuery);
	}
	else {
		QSqlQuery sqlQuery(query, m_table.database());

		m_table.setQuery(sqlQuery);
	}
	
	QJsonArray queryResults;

	QString statement = m_table.query().executedQuery();

	for (const auto& value : m_table.query().boundValues())
	{
		QString key = "?";
		QString val = value.toString().trimmed();
		
		QString firstSection = statement.sliced(0, statement.indexOf(key));
		QString secondSection = statement.sliced(statement.indexOf(key) + key.length());
		
		if(!(val.startsWith("'") && val.endsWith("'")))
			val = "'" + val + "'";

		statement = firstSection + val + secondSection;
	}
	QString where = "WHERE";
	QString conditions = statement.sliced(statement.indexOf(where) + where.length()).trimmed();
	m_table.clear();
	
	GetTable();

	qDebug() << "Conditions:" << conditions;

	m_table.setFilter(conditions);
	m_table.select();

	qDebug() << m_table.rowCount();

	for (int i = 0; i < m_table.rowCount(); ++i)
	{
		QVariantMap results = m_queryManager.recordToMap(m_table.record(i));
		queryResults.append(QJsonObject::fromVariantMap(results));
	}

	qDebug() << queryResults;

	CleanTable();

	return queryResults;
	//return queryManager.execute(query, placeholders);
}

QJsonArray CUsersManager::GetGroupedRemote(const QJsonArray& columns, const QJsonArray& grouped_columns, const QJsonArray& group_by, const QString& type, const QString& separator, const QJsonObject& p_where)
{

	QJsonArray returnedValues;

	try {

		QJsonObject query;
		QJsonObject select;
		QJsonObject group;
		QJsonObject where(p_where);

		if (!where.contains("custId"))
			(void)where.insert("custId", QString::number(m_trak_details.cust_id));

		if (!where.contains(m_trak_details.trak_id_type))
			(void)where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);

		returnedValues = CTRAKEntriesManager::GetGroupedRemote(columns, grouped_columns, group_by, type, separator, where);

	}
	catch (const std::exception& e)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

int CUsersManager::SendToRemote(const QJsonObject& entry, const QJsonObject& data)
{

	return CTRAKEntriesManager::SendToRemote(entry, data);
}

int CUsersManager::CreateLocal(const QJsonObject& entry)
{
	QJsonArray results = m_queryManager.execute("INSERT INTO users (" + entry.keys().join(", ") + ") VALUES (:" + entry.keys().join(",:") + ")", entry);

	if (results.size() <= 0)
		return 0;
	return 1;


	//GetTable();
	//int result = 0;
	//try {
	//	//table.database().transaction();
	//	qDebug() << "Table:" << table.tableName();

	//	table.select();

	//	qDebug() << "rows:" << table.selectRow(0);

	//	QSqlRecord newEntry;

	//	for (const auto& key : entry.keys())
	//	{
	//		QSqlField field(table.record(0).field(key));
	//		field.setValue(entry.value(key));
	//		newEntry.append(field);
	//	}

	//	result = table.insertRecord(-1, newEntry);

	//	if(result == 0)
	//		throw std::exception();

	//	result = table.submitAll();

	//	if(result == 0)
	//		throw std::exception();

	//	//table.database().commit();
	//}
	//catch (const std::exception& e) {
	//	//table.database().rollback();
	//	result = 0;
	//}

	//table.clear();

	//table.database().close();

	//return result;

	/*QSqlQuery sqlQuery(table.database());
	sqlQuery.prepare("INSERT INTO users (" + entry.keys().join(", ") + ") VALUES (:" + entry.keys().join(", :") + ")");

	for (const auto& placeholder : entry.keys())
	{
		sqlQuery.bindValue(":" + placeholder, entry.value(placeholder));
	}

	table.setQuery(sqlQuery);

	QJsonArray queryResults;

	for (int i = 0; i < table.rowCount(); ++i)
	{
		QVariantMap results = queryManager.recordToMap(table.record(i));
		queryResults.append(QJsonObject::fromVariantMap(results));
	}*/

	/*table.clear();

	table.database().close();*/

	//QJsonArray results = queryManager.execute("INSERT INTO users (" + entry.keys().join(", ") + ") VALUES (:" + entry.keys().join(", :") + ")", entry);

	/*if (queryResults.size() == 0)
		return 0;

	return 1;*/
}

int CUsersManager::UpdateRemote(const QJsonObject& entry, const QJsonObject& data)
{
	return CTRAKEntriesManager::UpdateRemote(entry, data);
}

QJsonArray CUsersManager::UpdateLocal(const QList<QString>& update, const QJsonObject& placeholders)
{
	QJsonArray queryResults;
	//return queryManager.execute("UPDATE users SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId", placeholders);
	QList<QVariantMap> updateLocal = UpdateLocal(update, placeholders.toVariantMap());

	for (int i = 0; i < updateLocal.size(); ++i)
	{
		QVariantMap results = updateLocal.at(i);
		queryResults.append(QJsonObject::fromVariantMap(results));
	}

	return queryResults;
}

QList<QVariantMap> CUsersManager::UpdateLocal(const QList<QString>& update, const QVariantMap& placeholders)
{
	
	QString queryToExec = "UPDATE users SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND (empId = :empId OR empId = '')";
	QVariantMap newPlaceholders = placeholders;
	return m_queryManager.execute(queryToExec, newPlaceholders);
}

void CUsersManager::HandleUpdatingEntries(const QJsonObject& local, const QJsonObject& remote)
{
	qDebug() << "Local: " << QJsonDocument(local).toJson().toStdString().data();
	qDebug() << "Remote: " << QJsonDocument(remote).toJson().toStdString().data();


	if (remote.value("updatedAt") == local.value("updatedAt")) {
		qInfo() << "Local entry last updated at:" << local.value("updatedAt") << "\nRemote entry last updated at:" << remote.value("updatedAt");
		qInfo() << "Local and Remote are in sync";
		return;
	}

	QDateTime remoteUpdate = QDateTime::fromString(remote.value("updatedAt").toString(), Qt::ISODate);
	if (!remoteUpdate.isValid())
		remoteUpdate = QDateTime::fromString(remote.value("updatedAt").toString(), Qt::ISODateWithMs);

	QDateTime localUpdate = QDateTime::fromString(local.value("updatedAt").toString(), Qt::ISODate);
	if (!localUpdate.isValid())
		localUpdate = QDateTime::fromString(local.value("updatedAt").toString(), Qt::ISODateWithMs);
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
	where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);

	QJsonObject body;
	QList<QString> set;
	QJsonObject update;

	QVariantMap placeholders;

	if (remoteUpdate > localUpdate)
	{
		qInfo() << "Remote is newer that Local";
		qInfo() << "Updating Local entry to match Remote";

		(void)CTRAKEntriesManager::breakoutValuesToUpdate(local, remote, &set, &placeholders);
		//(void)CTRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &update);

		update = QJsonObject::fromVariantMap(placeholders);

		placeholders.insert("ori_custId", remote.value("custId").toInt());
		placeholders.insert("ori_empId", remote.value("empId").toString());
		placeholders.insert("ori_userId", remote.value("userId").toString());
	}

	if (remoteUpdate < localUpdate)
	{
		qInfo() << "Remote is older that Local";
		qInfo() << "Updating Remote entry to match Local";
		(void)CTRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &placeholders);
		//(void)CTRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &update);

		update = QJsonObject::fromVariantMap(placeholders);

		placeholders.insert("ori_custId", local.value("custId").toInt());
		placeholders.insert("ori_empId", local.value("empId").toString());
		placeholders.insert("ori_userId", local.value("userId").toString());
	}
	set.push_back("updatedAt = CONVERT_TZ(:updatedAt, '+00:00', :tz)");
	placeholders.insert("updatedAt", currentDateTime);
	placeholders.insert("tz", m_time_zone);

	qDebug() << set;
	qDebug() << placeholders;
	qDebug() << update;
	qDebug() << where;

	QString queryToExec = "UPDATE users SET " + set.join(", ") + " WHERE custId = :ori_custId AND userId = :ori_userId AND (empId = :ori_empId OR empId = '')";
	(void)m_queryManager.execute(queryToExec, placeholders);

	update.insert("updatedAt", currentDateTime.toString(Qt::ISODateWithMs));
	where.insert("custId", placeholders.value("ori_custId").toJsonValue());
	where.insert("empId", placeholders.value("ori_empId").toJsonValue());
	where.insert("userId", placeholders.value("ori_userId").toJsonValue());

	body.insert("update", update);
	body.insert("where", where);

	(void)UpdateRemote(local, body);

	return;
}

int CUsersManager::SyncWebportal()
{
	(void)GetColumnNames(true);

	QStringList skipTargetCols = { "id", "createdAt", "updatedAt"};

	//QJsonArray webportalResults = GetRemote(QJsonArray({ "custId", "userId", "empId", m_trak_details.trak_id_type, "updatedAt" }));
	QJsonArray webportalResults = GetRemote(QJsonArray({ "custId", "userId", "empId", m_trak_details.trak_id_type, "updatedAt" }));

	if (!webportalResults.isEmpty() && remote_count != webportalResults.size())
		remote_count = webportalResults.size();

	QJsonArray webportalGroupedResults = GetGroupedRemote(QJsonArray({ "custId", "userId", "empId", m_trak_details.trak_id_type }), QJsonArray({ "updatedAt" }), QJsonArray({ "custId", "userId", "empId", m_trak_details.trak_id_type, }), "MIN", nullptr);

	qDebug() << webportalResults;
	qDebug() << webportalGroupedResults;
	qDebug() << webportalResults.empty();
	//qDebug() << webportalEmployeeIds.empty;

	ServiceHelper().WriteToLog("Exporting Users");

	QString select;
	QJsonObject placeholderMap;

	//QVariantMap vMapConditionals = {};

	if (webportalResults.empty())
		select = "SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL ORDER BY id DESC";
	else {
		QJsonArray employeeIds;
		QJsonArray userIds;
		QJsonArray updatedAtDates;

		for (const QJsonValue& entry : webportalResults)
		{
			if (!entry.isObject())
				continue;

			QJsonObject entryObject = entry.toObject();

			if (entryObject.isEmpty())
				continue;

			if (entryObject.value("empId").isNull() || entryObject.value("empId").isUndefined())
				continue;

			QString empId = entryObject.value("empId").toString();

			if (entryObject.value("userId").isNull() || entryObject.value("userId").isUndefined())
				continue;

			QString userId = entryObject.value("userId").toString();

			if(empId != "")
				employeeIds.push_back(entryObject.value("empId"));
			if(userId != "")
				userIds.push_back(entryObject.value("userId"));
			
			updatedAtDates.push_back(entryObject.value("updatedAt"));

		}

		placeholderMap.insert("empIds", employeeIds);
		placeholderMap.insert("userIds", userIds);
		placeholderMap.insert("tz", m_time_zone);

		if (webportalGroupedResults.size() <= 0)
			select = "SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL AND userId NOT IN (:userIds) ORDER BY id ASC";
		else {
			select = "SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL AND (userId NOT IN (:userIds) OR CONVERT_TZ(updatedAt, :tz, '+00:00') NOT IN (:updatedAts)) ORDER BY updatedAt ASC, id ASC";
			placeholderMap.insert("updatedAts", updatedAtDates);
		}

	}

	//std::vector sqlQueryResults = queryManager.execute(employeeSelect, vMapConditionals);
	QJsonArray queryResults = GetLocal(select, placeholderMap);

	qDebug() << QJsonDocument(queryResults).toJson().toStdString().data();


	QJsonArray SQLiteColumns;

	m_sqliteManager.GetTableColumnNames(m_db_info.table.toStdString().data(), &SQLiteColumns);

	
	for (const auto& queryResult : queryResults) {
		if (!queryResult.isObject())
			continue;
		
		QJsonObject result = queryResult.toObject();



		if (!result.value("empId").toString().isEmpty() && (placeholderMap.value("userIds").toArray().contains(result.value("userId")) || placeholderMap.value("empIds").toArray().contains(result.value("empId"))))
		{
			result[m_trak_details.trak_id_type] = m_trak_details.trak_id;

			QJsonObject where;

			if (result.value("custId").toString() != "")
				where.insert("custId", result.value("custId"));
			if (result.value("empId").toString() != "")
				where.insert("empId", result.value("empId"));
			if (result.value("userId").toString() != "")
				where.insert("userId", result.value("userId"));

			where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);

			QJsonObject webportalEntry = GetRemote(QJsonArray(), where).at(0).toObject();

			if (webportalEntry.isEmpty())
				continue;

			qDebug() << "Local: " << QJsonDocument(result).toJson().toStdString().data();
			qDebug() << "Remote: " << QJsonDocument(webportalEntry).toJson().toStdString().data();

			HandleUpdatingEntries(result, webportalEntry);

			continue;
		}

		//qDebug() << result;

		std::map<std::string, std::string> sqliteData;
		QJsonObject data;

		for (auto it = result.constBegin(); it != result.constEnd(); ++it)
		{
			QString key = it.key();
			QVariant value = it.value().toVariant();

			QString val;

			qDebug() << value.typeName();

			if (value.type() != QVariant::String && value.canConvert<int>())
				val = QString::number(value.toInt());
			else
				val = value.toString();

			val = val.trimmed().simplified();

			qDebug() << key << ":" << val;

			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key) && (SQLiteColumns.size() <= 0 || SQLiteColumns.contains(key)))
				sqliteData.insert_or_assign(key.toStdString(), val.toStdString());
			data.insert(key, val);
		}


		if (!data.contains("userId"))
			continue;

		if (!data.contains(m_trak_details.trak_id_type) || data.value(m_trak_details.trak_id_type).toString() != m_trak_details.trak_id)
			data.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);
			//data[m_trak_details.trak_id_type] = m_trak_details.trak_id;

		if (!data.contains("custId"))
			data.insert("custId", m_trak_details.cust_id);
			//data["custId"] = m_trak_details.cust_id;

		QVariant empId;

		if (data.contains("empId")) {
			empId = data.value("empId").toVariant();
		}
		else if (result.contains("empId")) {
			QJsonArray userEmpId = m_queryManager.execute("SELECT empId as uuid FROM employees WHERE custId = :custId AND userId = :userId AND empId IS NOT NULL AND empId <> ''", data);

			if (userEmpId.size() >= 1) {
				empId = userEmpId.at(0).toObject().value("uuid").toVariant();
			}
		}

		qDebug() << "empId" << empId;


		if (empId.isNull())
		{
			/*QJsonArray uuidVector = m_queryManager.execute("SELECT REGEXP_REPLACE(UUID(), '-', '') as uuid", QJsonObject());

			empId = uuidVector.at(0).toObject().value("uuid");

			qDebug() << uuidVector;*/

			UUID uuid;

			(void)UuidCreate(&uuid);

			char* str;

			(void)UuidToString(&uuid, (RPC_CSTR*)&str);

			empId = QString(str).replace("-", "");

			RpcStringFreeA((RPC_CSTR*)&str);
		}

		if (!webportalResults.isEmpty()) {
			QJsonObject webportalEmployee;
			

			for(const auto& result : webportalResults) {
				qDebug() << result;
				if (!result.isObject())
					break;
				QJsonObject resultObject = result.toObject();

				if (resultObject.value("empId").toString() == data.value("empId").toString()) continue;
				if (resultObject.value("userId").toString() != data.value("userId").toString()) continue;

				empId = resultObject.value("empId").toVariant();

			};
		}

		if (empId.toString() != data.value("empId").toString()) {
			QJsonObject placeholder;
			placeholder.insert("empId", empId.toJsonValue());
			placeholder.insert("oldEmpId", data.value("empId"));
			placeholder.insert("userId", data.value("userId"));

			(void)m_queryManager.execute("UPDATE users SET empId = :empId WHERE userId = :userId AND (empId = :oldEmpId OR empId = '')", placeholder);

			data["empId"] = empId.toString();

			sqliteData["empId"] = empId.toString().toStdString();
		}

#if true
		m_sqliteManager.AddEntry(
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

int CUsersManager::SyncLocal()
{
	QStringList skipTargetCols = { "id"};

	//QJsonArray({ "custId", "userId", "empId", "updatedAt" })
	//QJsonArray({ "custId", "userId", "empId", trak_details.trak_id_type, "updatedAt" })
	QJsonArray webportalResults = GetRemote();

	if (!webportalResults.isEmpty() && remote_count != webportalResults.size())
		remote_count = webportalResults.size();

	//QJsonArray localResults = queryManager.execute("SELECT * FROM users WHERE userId LIKE '3600874AB0' OR empId LIKE '5263ccb707a811f1b2de00155d817002' LIMIT 1", placeholders);

	QJsonArray columns = GetColumns();

	qDebug() << columns;

	/*QFuture<QJsonArray>columnFuture(QtConcurrent::run([=, &columns]()
		{*/
	QJsonArray columnNames;

	for (const auto& column : columns)
	{
		if (!column.isObject())
			continue;

		QJsonObject columnObject = column.toObject();

		qDebug() << columnObject;

		columnNames.append(columnObject.value("Field").toString());
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

	for (const auto& webportalResult : webportalResults) {
		if (!webportalResult.isObject())
			continue;

		QJsonObject remoteResult = webportalResult.toObject();
		qDebug() << "result" << remoteResult;

		QJsonArray localResults = GetLocal("SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL AND userId = :userId AND (empId = :empId OR empId = '')", remoteResult);
		
		qDebug() << "local entry arr size" << localResults.size();
		if(localResults.size() > 0)
			qDebug() << "local entry" << localResults.at(0).toObject();
		
		qDebug() << "remote entry" << remoteResult;
		qDebug() << "local entry" << localResults;

		if (localResults.size() > 0)
		{
			QJsonObject localEntry = localResults.at(0).toObject();

			if (localEntry.value("empId").toString().isEmpty()) {
				QJsonArray remoteEmployees = m_queryManager.execute("SELECT empId FROM employees WHERE custId = :custId AND userId = :userId", localEntry);
				if (!remoteEmployees.isEmpty()) {
					localEntry["empId"] = remoteEmployees.at(0).toObject().value("empId");
					m_queryManager.execute("UPDATE users SET empId = :empId WHERE custId = :custId AND userId = :userId AND empId = ''", localEntry);
				}
			}

			qDebug() << "Local: " << QJsonDocument(localEntry).toJson().toStdString().data();
			qDebug() << "Remote: " << QJsonDocument(remoteResult).toJson().toStdString().data();
			localEntry[m_trak_details.trak_id_type] = m_trak_details.trak_id;

			if (remoteResult.value("updatedAt") == localEntry.value("updatedAt"))
				continue;

			HandleUpdatingEntries(localEntry, remoteResult);
			
			continue;
		}

		qDebug() << QJsonDocument(remoteResult).toJson().toStdString().data();

		//if (placeholderMap.value("userIds").toJsonArray().contains(result.value("userId")))
		//{
		//	continue;
		//	/*QJsonObject where;
		//	where.insert("custId", result.value("custId"));
		//	where.insert("empId", result.value("empId"));
		//	where.insert("userId", result.value("userId"));
		//	where.insert(trak_details.trak_id_type, trak_details.trak_id);
		//	QJsonArray localEntries = GetLocal("SELECT * FROM users WHERE empId = :empId OR userId = :userId", where);

		//	if (!localEntries.isEmpty()) {
		//		QJsonObject localEntry = localEntries.at(0).toObject();

		//		qDebug() << "Local: " << QJsonDocument(localEntry).toJson().toStdString().data();
		//		qDebug() << "Remote: " << QJsonDocument(result).toJson().toStdString().data();
		//		localEntry[trak_details.trak_id_type] = trak_details.trak_id;

		//		HandleUpdatingEntries(localEntry, result);
		//		continue;
		//	}*/

		//}

		//qDebug() << result;


		QJsonObject data;

		//QJsonArray tableColumnNames = columnFuture.result();
		QJsonArray tableColumnNames = columnNames;

		QStringList datetimeCols = { "updatedAt", "createdAt" };

		std::map<std::string, std::string> sqliteData;
		
		for (auto it = remoteResult.constBegin(); it != remoteResult.constEnd(); ++it)
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

			if (!skipTargetCols.contains(key)) {
				sqliteData[key.toStdString()] = val.toStdString();
				data[key] = val;
			}
		}


#if true
		m_sqliteManager.AddEntry(
			"users",
			sqliteData
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

int CUsersManager::UpdateOutdated()
{
	

	DWORD size = sizeof(TCHAR);
	std::vector<TCHAR> buffer(size);
	{
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
		
		(void)rtManager.GetValSize("usersCheckedDate", REG_SZ, &size, &buffer);

		if (!size) {
			(void)UpdateCheckedTime();
			(void)rtManager.GetValSize("usersCheckedDate", REG_SZ, &size, &buffer);
		}

		(void)rtManager.GetVal("usersCheckedDate", REG_SZ, buffer.data(), &size);
	}
	QString lastCheckedDateTime(buffer.data());
	buffer.clear();
	
	qDebug() << "lastCheckedDateTime" << lastCheckedDateTime;

	QDateTime last_checked_date(QDateTime::fromString(lastCheckedDateTime, Qt::ISODate));

	QMap<QString, QVariant> placeholder;
	placeholder.insert("last_checked_date", last_checked_date);
	placeholder.insert("tz", m_time_zone);

	qDebug() << "placeholder" << placeholder;

	QList<QVariantMap> outdatedLocals(m_queryManager.execute("SELECT * FROM users WHERE updatedAt >= CONVERT_TZ(:last_checked_date, '+00:00', :tz)", placeholder));

	qDebug() << "outdatedLocals" << outdatedLocals;

	QJsonObject select;
	select.insert("outdated", lastCheckedDateTime);

	QJsonArray outdatedRemotes(GetRemote(QJsonArray(), QJsonObject(), select));

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
			QJsonObject local = QJsonObject::fromVariantMap(outdatedLocal);
			QJsonObject remote;


			if (local.value("empId").toString().isEmpty()) {
				QJsonArray remoteEmployees = m_queryManager.execute("SELECT empId FROM employees WHERE custId = :custId AND userId = :userId", local);
				if (!remoteEmployees.isEmpty()) {
					local["empId"] = remoteEmployees.at(0).toObject().value("empId");
				}
			}

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
				where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);
				QJsonArray targetRemote = GetRemote(QJsonArray(), where);

				if (targetRemote.isEmpty()) {
					//throw HenchmanServiceException("Needed user instance from remote but failed to find one");
					ServiceHelper().WriteToError("Needed user instance from remote but failed to find one");
					continue;
				}

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
				
				QJsonObject outdatedLocalObject = QJsonObject::fromVariantMap(outdatedLocal);
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

				if (targetLocal.isEmpty()) {
					//throw HenchmanServiceException("Needed user instance from local but failed to find one");
					ServiceHelper().WriteToError("Needed user instance from local but failed to find one");
					continue;
				}

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

int CUsersManager::ClearCloudUpdate()
{
	QString date = CTRAKEntriesManager::ClearCloudUpdate();
	
	QJsonObject params;
	params["date"] = date;
	params["tz"] = m_time_zone;
	qDebug() << "params" << params;

	(void)m_queryManager.execute("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% users%' AND CONVERT_TZ(DatePosted,:tz, '+00:00') < :date AND (posted = 0 OR posted = 2)", params);

	return NEXTSTEP::ALL_UPDATED;
};

//#include "moc_UsersManager.cpp"