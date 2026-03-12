
#include "EmployeesManager.h"

using namespace EmployeesManager;
using namespace TRAKEntriesManager;

CEmployeesManager::CEmployeesManager(QObject* parent, const TrakDetails& trakDetails, const WebportalDetails& webportalDetails, const s_DATABASE_INFO& database_info)
	:CTRAKEntriesManager(parent, trakDetails, webportalDetails, database_info)
{	
	ServiceHelper().WriteToLog("Initialized EmployeesManager");

	m_registryEntry = "employeesChecked";

	m_webportal_details.base_route = "employees";

	m_db_info.table = "employees";

	Initialize();
}

CEmployeesManager::~CEmployeesManager()
{
	LOG << "Deconstructing EmployeesManager";
}

void CEmployeesManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values)
{
	CTRAKEntriesManager::breakoutValuesToUpdate(older, newer, set_values, updated_values);
}

void CEmployeesManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values)
{

	CTRAKEntriesManager::breakoutValuesToUpdate(older, newer, set_values, updated_values);
};

QJsonArray CEmployeesManager::GetColumns(bool reset)
{
	CTRAKEntriesManager::GetColumns(reset);

	if (m_MySQL_Columns.size() <= 0)
		return m_MySQL_Columns;

	QJsonArray tableColumns = m_MySQL_Columns;

	QStringList uniqueIndexCols = { "custId", "userId" };
	s_UpdateLocalTableOptions update_options;

	QJsonArray sqliteTableColumns;

	(void)m_sqliteManager.ExecQuery(
		"pragma table_info(" + m_db_info.table + ")",
		&sqliteTableColumns
	);


	update_options.AddCreatedAt = true;
	update_options.AddUpdatedAt = true;
	update_options.AddEmpId = true;
	update_options.AddDisabledToSQLITE = true;
	update_options.AddDisabledToMySQL = true;

	QFuture<QJsonArray> mysqlTableColumnsFuture = QtConcurrent::run([&tableColumns]() {
		QJsonArray colummns = tableColumns;
		QJsonArray mysqlTableColumnNames;
		for (const auto& colummn : colummns)
		{
			if (!colummn.isObject())
				continue;
			QJsonObject mysqlColumn = colummn.toObject();
			mysqlTableColumnNames.append(mysqlColumn.value("Field").toString());
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
			sqliteTableColumnNames.append(sqliteColumn.value("name").toString());
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

			QString field = column.value("Field").toString();

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


	if (performTableUpdate)
	{

		(void)m_sqliteManager.ExecQuery("DROP INDEX IF EXISTS unique_custId_userId");

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

int CEmployeesManager::GetLocalCount(const QList<QString> & p_conditions, const QJsonObject& p_placeholders)
{
	int local_employees = 0;


	try {
		QList<QString> conditions(p_conditions);
		QJsonObject placeholders(p_placeholders);

		if (conditions.isEmpty()) {
			conditions.append("userId <> ''");
			conditions.append("userId IS NOT NULL");
		}
		
		local_employees = CTRAKEntriesManager::GetLocalCount(conditions, placeholders);
	}catch (const std::exception& e) {
		local_employees = local_count;
	}
	
	m_table.database().close();

	return local_employees;
}

int CEmployeesManager::GetRemoteCount(const QJsonObject& p_select, const QJsonObject& p_where, QJsonObject * p_returned_data)
{

	int remote_employees = 0;

	try {
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if (!where.contains("custId"))
			(void)where.insert("custId", QString::number(m_trak_details.cust_id));

		if (!where.contains(m_trak_details.trak_id_type))
			(void)where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);
		
		if (!select.contains("count"))
			(void)select.insert("count", "total");

		remote_employees = CTRAKEntriesManager::GetRemoteCount(select, where, p_returned_data);
		
	}
	catch (void*)
	{
		remote_employees = remote_count;
	}


	return remote_employees;
}

QJsonArray CEmployeesManager::GetRemote(const QJsonArray& columns, const QJsonObject& p_where, const QJsonObject& p_select)
{

	QJsonArray returnedValues;

	try {

		QJsonObject where(p_where);
		QJsonObject select(p_select);

		returnedValues = CTRAKEntriesManager::GetRemote(columns, where, select);

	}
	catch (void*)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

QJsonArray CEmployeesManager::GetLocal(const QString& query, const QJsonObject& placeholders)
{
	return m_queryManager.execute(query, placeholders);
}

QJsonArray CEmployeesManager::GetGroupedRemote(const QJsonArray& columns, const QJsonArray& grouped_columns, const QJsonArray& group_by, const QString& type, const QString& separator, const QJsonObject& p_where)
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
	catch (void*)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

int CEmployeesManager::SendToRemote(const QJsonObject& entry, const QJsonObject& data)
{

	return CTRAKEntriesManager::SendToRemote(entry, data);
}

int CEmployeesManager::CreateLocal(const QJsonObject& entry)
{
	/*QJsonArray results = queryManager.execute("INSERT INTO employees (" + entry.keys().join(", ") + ") VALUES (:" + entry.keys().join(", :") + ")", entry);

	if (results.size() == 0)
		return 0;

	return 1;*/
	return CTRAKEntriesManager::CreateLocal(entry);
}

int CEmployeesManager::UpdateRemote(const QJsonObject& entry, const QJsonObject& data)
{
	return CTRAKEntriesManager::UpdateRemote(entry, data);
}

QJsonArray CEmployeesManager::UpdateLocal(const QList<QString>& update, const QJsonObject& placeholders)
{
	return m_queryManager.execute("UPDATE employees SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId", placeholders);
}

QList<QVariantMap> CEmployeesManager::UpdateLocal(const QList<QString>& update, const QVariantMap& placeholders)
{
	QString queryToExec = "UPDATE employees SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId";
	QVariantMap newPlaceholders = placeholders;
	return m_queryManager.execute(queryToExec, newPlaceholders);
}

void CEmployeesManager::HandleUpdatingEntries(const QJsonObject& local, const QJsonObject& remote)
{

	qDebug() << "Local: " << QJsonDocument(local).toJson().toStdString().data();
	qDebug() << "Remote: " << QJsonDocument(remote).toJson().toStdString().data();


	if (remote.value("updatedAt") == local.value("updatedAt")) {
		qInfo() << "Local entry last updated at:" << local.value("updatedAt") << "\nRemote entry last updated at:" << remote.value("updatedAt");
		qInfo() << "Local and Remote are in sync";
		return;
	}

	QDateTime remoteUpdate = QDateTime::fromString(remote.value("updatedAt").toString(), Qt::ISODate);

	if(!remoteUpdate.isValid())
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
		qInfo() << "Remote Employee is newer that Local";
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
		qInfo() << "Remote Employee is older that Local";
		qInfo() << "Updating Remote entry to match Local";
		(void)CTRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &placeholders);
		//(void)CTRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &update);

		update = QJsonObject::fromVariantMap(placeholders);

		placeholders.insert("ori_custId", local.value("custId").toInt());
		placeholders.insert("ori_empId", local.value("empId").toString());
		placeholders.insert("ori_userId", local.value("userId").toString());

	}
	update.remove("kabId");
	update.remove("cribId");
	update.remove("scaleId");

	set.push_back("updatedAt = CONVERT_TZ(:updatedAt, '+00:00', :tz)");
	placeholders.insert("updatedAt", currentDateTime);
	placeholders.insert("tz", m_time_zone);

	qDebug() << set;
	qDebug() << placeholders;
	qDebug() << update;
	qDebug() << where;

	QString queryToExec = "UPDATE employees SET " + set.join(", ") + " WHERE custId = :ori_custId AND ((empId = '' AND userId = :ori_userId) OR empId = :ori_empId)";
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

int CEmployeesManager::SyncWebportal()
{
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	(void)GetColumnNames(true);

	QJsonArray webportalResults = GetRemote(QJsonArray({ "custId", "userId", "empId", "updatedAt" }));

	if(!webportalResults.isEmpty() && remote_count != webportalResults.size())
		remote_count = webportalResults.size();

	QJsonArray webportalGroupedResults = GetGroupedRemote(QJsonArray({ "custId", "userId", "empId" }), QJsonArray({ "updatedAt" }), QJsonArray({ "custId", "userId", "empId" }), "MIN", nullptr);

	qDebug() << webportalResults;
	qDebug() << webportalGroupedResults;
	qDebug() << webportalResults.empty();
	//qDebug() << webportalEmployeeIds.empty;

	ServiceHelper().WriteToLog("Exporting Employees to Webportal");

	QString employeeSelect;
	QJsonObject placeholderMap;

	//QVariantMap vMapConditionals = {};

	if (webportalResults.empty())
		employeeSelect = "SELECT * FROM employees WHERE userId <> '' AND userId IS NOT NULL ORDER BY id DESC";
	else {
		QJsonArray employeeIds;
		QJsonArray updatedAtDates;
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
			updatedAtDates.push_back(employeeObject.value("updatedAt"));

		}

		placeholderMap.insert("empIds", employeeIds);
		placeholderMap.insert("userIds", userIds);
		placeholderMap.insert("tz", m_time_zone);

		if (webportalGroupedResults.size() <= 0)
			employeeSelect = "SELECT * FROM employees WHERE userId <> '' AND userId IS NOT NULL AND empId NOT IN (:empIds) ORDER BY id ASC";
		else {
			employeeSelect = "SELECT * FROM employees WHERE userId <> '' AND userId IS NOT NULL AND (empId NOT IN (:empIds) OR CONVERT_TZ(updatedAt, :tz, '+00:00') NOT IN (:updatedAts)) ORDER BY updatedAt ASC, id ASC";
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

		

		if (!result.value("empId").toString().isEmpty() && (placeholderMap.value("empIds").toArray().contains(result.value("empId")) || placeholderMap.value("userIds").toArray().contains(result.value("userId"))))
		{
			result[m_trak_details.trak_id_type] = m_trak_details.trak_id;

			QJsonObject where;
			if(result.value("custId").toString() != "")
				where.insert("custId", result.value("custId"));
			if (result.value("empId").toString() != "")
				where.insert("empId", result.value("empId"));
			if (result.value("userId").toString() != "")
				where.insert("userId", result.value("userId"));
			where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);
			QJsonObject webportalEntry = GetRemote(QJsonArray(), where).at(0).toObject();

			qDebug() << "Local: " << QJsonDocument(result).toJson().toStdString().data();
			qDebug() << "Remote: " << QJsonDocument(webportalEntry).toJson().toStdString().data();

			HandleUpdatingEntries(result, webportalEntry);

			continue;
		}

		//qDebug() << result;

		if(!result.contains("custId") || result.value("custId").toInt() != m_trak_details.cust_id)
			result["custId"] = m_trak_details.cust_id;

		std::map<std::string, std::string> sqliteData;
		QJsonObject data;

		for (auto it = result.constBegin(); it != result.constEnd(); ++it)
		{
			QString key = it.key();
			QVariant val = it.value().toVariant();

			QString value;

			qDebug() << val.typeName();

			if (val.type() != QVariant::String && val.canConvert<int>())
				value = QString::number(val.toInt());
			else
				value = val.toString();

			value = value.trimmed().simplified();

			qDebug() << value;

			if (value.isEmpty() || value == "''")
				continue;
			if (!skipTargetCols.contains(key))
				sqliteData[key.toStdString()] = value.toStdString();
			data[key] = value;
		}

		if (!data.contains("userId"))
			continue;

		if (!data.contains(m_trak_details.trak_id_type) || data.value(m_trak_details.trak_id_type).toString() != m_trak_details.trak_id)
			data[m_trak_details.trak_id_type] = m_trak_details.trak_id;
		

		QVariant empId;

		if (data.contains("empId")) {
			empId = data.value("empId").toVariant();
		}
		else if (result.contains("empId")) {
			QJsonArray userEmpId = m_queryManager.execute("SELECT empId FROM users WHERE custId = :custId AND userId = :userId AND empId <> '' AND empId IS NOT NULL", data);

			if (userEmpId.size() >= 1) {
				empId = userEmpId.at(0).toObject().value("empId").toVariant();
			}
		}


		if (empId.isNull())
		{
			//std::vector uuidVector = m_queryManager.execute("SELECT REGEXP_REPLACE(UUID(), '-', '') as empId");
			UUID uuid;

			(void)UuidCreate(&uuid);
			
			char* str;
			
			(void)UuidToString(&uuid, (RPC_CSTR*)&str);

			empId = QString(str).replace("-", "");

			RpcStringFreeA((RPC_CSTR*)&str);

		}

		qDebug() << empId;

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

			(void)m_queryManager.execute("UPDATE employees SET empId = :empId WHERE userId = :userId", placeholder);

			data["empId"] = empId.toString();

			sqliteData["empId"] = empId.toString().toStdString();
		}

#if true
		m_sqliteManager.AddEntry(
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

int CEmployeesManager::SyncLocal()
{
	QStringList skipTargetCols = { "id", m_trak_details.trak_id_type};

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

	ServiceHelper().WriteToLog("Importing Employees from Webportal");

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

	//QJsonArray queryResults = GetLocal(employeeSelect, placeholderMap);

	QStringList conditions;
	conditions << "userId <> ''" << "userId IS NOT NULL" << "(empId IN (:empIds) OR userId IN (:userIds))";

	QList<QVariantMap> queryResults = CTRAKEntriesManager::GetLocal(QStringList({"*"}), conditions, placeholderMap.toVariantMap());
	//else {

	qDebug() << "queryResults" << queryResults;

	employeeIds = QJsonArray();
	userIds = QJsonArray();

	placeholderMap = QJsonObject();
	//placeholderMap.erase(placeholderMap.find("empIds"));

	for (const auto& employee : queryResults)
	{
		/*if (!employee.isObject())
			continue;

		QJsonObject employeeObject = employee.toObject();

		if (employeeObject.isEmpty() || employeeObject.value("empId").isNull() || employeeObject.value("empId").isUndefined())
			continue;

		employeeIds.push_back(employeeObject.value("empId"));*/

		if (employee.isEmpty() || employee.value("empId").isNull() || !employee.value("empId").isValid())
			continue;

		employeeIds.push_back(employee.value("empId").toJsonValue());

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
			where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);
			/*QJsonObject webportalEntry = GetRemoteEmployees(QJsonArray(), where).at(0).toObject();*/
			QJsonArray localEntries = GetLocal("SELECT * FROM employees WHERE empId = :empId OR userId = :userId", where);

			if (!localEntries.isEmpty()) {
				QJsonObject localEntry = localEntries.at(0).toObject();
				
				qDebug() << "Local: " << QJsonDocument(localEntry).toJson().toStdString().data();
				qDebug() << "Remote: " << QJsonDocument(result).toJson().toStdString().data();
				localEntry[m_trak_details.trak_id_type] = m_trak_details.trak_id;
				
				HandleUpdatingEntries(localEntry, result);
				continue;
			}

		}

		//qDebug() << result;

		
		QJsonObject data;

		QJsonArray employeeTableColumnNames = employeeColumnFuture.result();

		QStringList datetimeCols = { "updatedAt", "createdAt" };

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
			if (!skipTargetCols.contains(key)) {
				sqliteData[key.toStdString()] = val.toStdString();
				data[key] = val;
			}
		}
		

#if true
		m_sqliteManager.AddEntry(
			"employees",
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

	conditions.clear();
	conditions.append("userId <> ''");
	conditions.append("userId IS NOT NULL");
	local_count = GetLocalCount(conditions);

	return NEXTSTEP::CHECK_NEXT_STEP;
}

int CEmployeesManager::UpdateOutdated()
{

	DWORD size = sizeof(TCHAR);
	std::vector<TCHAR> buffer(size);
	{
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
		(void)rtManager.GetValSize("employeesCheckedDate", REG_SZ, &size, &buffer);

		if (!size) {
			(void)UpdateCheckedTime();
			(void)rtManager.GetValSize("employeesCheckedDate", REG_SZ, &size, &buffer);
		}

		(void)rtManager.GetVal("employeesCheckedDate", REG_SZ, buffer.data(), &size);
	}
	QString lastCheckedDateTime(buffer.data());
	buffer.clear();

	qDebug() << "lastCheckedDateTime" << lastCheckedDateTime;
	
	QDateTime last_checked_date(QDateTime::fromString(lastCheckedDateTime, Qt::ISODate));

	QMap<QString, QVariant> placeholder;
	//placeholder.insert("lastCheckedDate", last_checked_date.date());
	//placeholder.insert("lastCheckedTime", last_checked_date.time());
	placeholder.insert("last_checked_date", last_checked_date);
	placeholder.insert("tz", m_time_zone);

	qDebug() << "placeholder" << placeholder;

	QList<QVariantMap> outdatedLocalEmployees = m_queryManager.execute("SELECT * FROM employees WHERE CONVERT_TZ(updatedAt, :tz, '+00:00') >= :last_checked_date", placeholder);

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

int CEmployeesManager::ClearCloudUpdate()
{
	QString date = CTRAKEntriesManager::ClearCloudUpdate();

	QJsonObject params;
	params["date"] = date;
	params["tz"] = m_time_zone;
	qDebug() << "params" << params;

	(void)m_queryManager.execute("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% employees%' AND CONVERT_TZ(DatePosted, :tz, '+00:00') < :date AND (posted = 0 OR posted = 2)", params);

	return NEXTSTEP::ALL_UPDATED;
};

//#include "moc_EmployeesManager.cpp"