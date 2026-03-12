
#include "TRAKEntriesManager.h"

//auto EMPLOYEES_ALL_UPDATED = [](int val) { return val == EmployeesManager::NEXTSTEP::ALL_UPDATED ? 1 : 0; };
using namespace TRAKEntriesManager;
CTRAKEntriesManager::CTRAKEntriesManager(QObject* parent, const TrakDetails& trakDetails, const WebportalDetails& webportalDetails, const s_DATABASE_INFO& database_info)
	: QObject(parent), m_sqliteManager(parent), m_queryManager(parent, database_info), m_networkManager(new NetworkManager(parent)), m_table(parent, QSqlDatabase::database(database_info.schema))
{
	qDebug() << "Initialized TRAKEntriesManager";

	m_registryEntry = "customersChecked";

	m_trak_details = trakDetails;
	m_webportal_details = webportalDetails;
	m_db_info = database_info;

	BindNewNetworkManager();

	m_queryManager.set_database_details(m_db_info);

	try {
		if (!m_db_info.schema.isEmpty())
			throw std::exception();

		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, ("SOFTWARE\\HenchmanTRAK\\" + m_trak_details.trak_type + "\\Database").toStdString().c_str());

		DWORD size = sizeof(TCHAR);
		std::vector<TCHAR> buffer(size);

		rtManager.GetValSize("Schema", REG_SZ, &size, &buffer);

		rtManager.GetVal("Schema", REG_SZ, buffer.data(), &size);

		m_trak_details.schema = QString(buffer.data());
		buffer.clear();
		m_db_info.schema = m_trak_details.schema;

		m_queryManager.setSchema(m_db_info.schema);
	}
	catch (const std::exception& e)
	{
		LOG << "database_info.schema was not empty, skipping setting schema manually";
	}

	s_TZ_INFO tzMap = m_queryManager.GetTimezone();

	m_time_zone = tzMap.time_zone;

	return;
}

CTRAKEntriesManager::~CTRAKEntriesManager()
{
	LOG << "Deconstructing TRAKEntriesManager";

	CleanTable();

	QJsonObject placeholder;
	placeholder.insert("schema", m_db_info.schema);
	placeholder.insert("host", m_db_info.server);
	placeholder.insert("username", m_db_info.username);

	QJsonArray processes = m_queryManager.execute("SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST WHERE DB = :schema AND HOST = :host AND USER = :username AND COMMAND LIKE 'Sleep' ORDER BY TIME DESC", placeholder);

	for (int i = 0; i < processes.size() - 1; ++i)
	{
		QJsonValue process = processes.at(i);
		QJsonObject processId = process.toObject();

		(void)m_queryManager.execute("KILL :ID", processId);
	}

	m_sqliteManager.deleteLater();
	m_queryManager.deleteLater();
	
	if (!m_replaced_network_manager) {
		m_networkManager->deleteLater();
		m_networkManager = nullptr;
	}

	m_MySQL_Columns = QJsonArray();
	m_MySQL_Column_Names = QJsonArray();
	m_SQLITE_Columns = QJsonArray();
	m_SQLITE_Column_Names = QJsonArray();

	m_TableColumns.clear();

	return;
}


void CTRAKEntriesManager::Initialize()
{

	CleanTable();

	m_queryManager.set_database_details(m_db_info);

	try {
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

		DWORD size = sizeof(local_count);

		(void)rtManager.GetValSize(m_registryEntry, REG_DWORD, &size);

		(void)rtManager.GetVal(m_registryEntry, REG_DWORD, &local_count, &size);

		QJsonObject placeholders;

		(void)placeholders.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);

		int rowCheck = GetLocalCount();
		int local_emp_count = rowCheck;
		if (!local_count || local_count != local_emp_count) {
			local_count = local_emp_count;
			size = sizeof(local_count);
			(void)rtManager.SetVal(m_registryEntry, REG_DWORD, &local_count, size);
		}


		(void)GetColumns();

	}
	catch (const std::exception& e) {
		throw e;
	}
}

void CTRAKEntriesManager::BindNewNetworkManager(NetworkManager* t_network_manager)
{
	if (m_networkManager && t_network_manager) {
		m_networkManager->deleteLater();
		m_networkManager = nullptr;
	}

	if (t_network_manager) {
		m_networkManager = t_network_manager;
		m_replaced_network_manager = true;
	}

	if (m_networkManager) {
		m_networkManager->setApiUrl(m_webportal_details.api_url);
		m_networkManager->setApiKey(m_webportal_details.api_key);
#ifdef DEBUG
		m_networkManager->toggleSecureTransport(false);
#else // DEBUG
		m_networkManager->toggleSecureTransport(m_webportal_details.api_url.contains("https"));
#endif // DEBUG

	}
	
}

void CTRAKEntriesManager::handleUpdatingLocalDB(const QString& table, const QStringList& unique_columns, s_UpdateLocalTableOptions* options)
{
	//QueryManager queryManager(this, db_info);
	qDebug() << "modifying table" << table;
	qDebug() << "passed options" << options;

	if (!options)
		return;

	if (options->AddCreatedAt) {
		qDebug() << "Adding created at";
		(void)m_queryManager.execute("ALTER TABLE " + table + " ADD createdAt timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP");
		options->AddCreatedAt = false;
	}

	if (options->UpdateCreatedAt) {
		qDebug() << "updating created at";
		(void)m_queryManager.execute("ALTER TABLE " + table + " ALTER createdAt SET DEFAULT CURRENT_TIMESTAMP");
		options->UpdateCreatedAt = false;
	}

	if (options->AddUpdatedAt) {
		qDebug() << "Adding updated at";
		(void)m_queryManager.execute("ALTER TABLE " + table + " ADD updatedAt timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP");
		options->AddUpdatedAt = false;
	}

	if (options->UpdateUpatedAt) {
		qDebug() << "updating updated at";
		(void)m_queryManager.execute("ALTER TABLE " + table + " ALTER updatedAt SET DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP");
		options->UpdateUpatedAt = false;
	}

	if (options->AddEmpIdSqliteOnly && !options->AddEmpId) {
		qDebug() << "adding empid at sqlite db only";
		(void)m_sqliteManager.ExecQuery("ALTER TABLE " + table + " ADD empId VARCHAR(32) NOT NULL DEFAULT ''");
		options->AddEmpIdSqliteOnly = false;
	}
	else {
		options->AddEmpIdSqliteOnly = false;
	}

	if (options->AddEmpId) {
		qDebug() << "adding empid";
		(void)m_sqliteManager.ExecQuery("ALTER TABLE " + table + " ADD empId VARCHAR(32) NOT NULL DEFAULT ''");
		(void)m_queryManager.execute("ALTER TABLE " + table + " ADD empId VARCHAR(32) NOT NULL DEFAULT ''");
		options->AddEmpId = false;
	}

	if (options->AddDisabledToSQLITE) {
		qDebug() << "Adding disabled to sqlite db";
		(void)m_sqliteManager.ExecQuery("ALTER TABLE " + table + " ADD disabled INT NOT NULL DEFAULT 0");
		options->AddDisabledToSQLITE = false;
	}

	if (options->AddDisabledToMySQL) {
		qDebug() << "Adding disabled to sqlite db";
		(void)m_queryManager.execute("ALTER TABLE " + table + " ADD disabled INT NOT NULL DEFAULT 0");
		options->AddDisabledToMySQL = false;
	}

	if (options->AddDeletedToSQLITE) {
		qDebug() << "Adding deleted to sqlite db";
		(void)m_sqliteManager.ExecQuery("ALTER TABLE " + table + " ADD deleted INT NOT NULL DEFAULT 0");
		options->AddDeletedToSQLITE = false;
	}
	
	if (options->AddDeletedToMySQL) {
		qDebug() << "Adding deleted to sqlite db";
		(void)m_queryManager.execute("ALTER TABLE " + table + " ADD deleted INT NOT NULL DEFAULT 0");
		options->AddDeletedToMySQL = false;
	}

	if (options->CreateUniqueIndex) {
		qDebug() << "Adding unique index to sqlite db";
		if (unique_columns.isEmpty())
			throw HenchmanServiceException("Cannot add unique index without providing columns");
		(void)m_sqliteManager.ExecQuery("CREATE UNIQUE INDEX IF NOT EXISTS unique_" + unique_columns.join("_") + " ON " + table + "(" + unique_columns.join(",") + ")");
		options->CreateUniqueIndex = false;
	}
}

QSqlTableModel* CTRAKEntriesManager::GetTable()
{
	if (!m_table.database().isOpen())
		m_table.database().open();

	if (m_table.tableName() != m_db_info.table)
		m_table.setTable(m_db_info.table);

	if (m_table.editStrategy() != QSqlTableModel::OnManualSubmit)
		m_table.setEditStrategy(QSqlTableModel::OnManualSubmit);

	m_table.select();

	return &m_table;
}

QSqlTableModel* CTRAKEntriesManager::CleanTable()
{

	if (!m_table.database().isOpen())
		m_table.database().open();

	if (m_table.tableName() != m_db_info.table)
		m_table.setTable(m_db_info.table);

	m_table.clear();

	m_table.database().close();

	return &m_table;
}

QSqlQueryModel* CTRAKEntriesManager::GetQuery()
{
	m_query = new QSqlQueryModel(this);

	return m_query;
}

void CTRAKEntriesManager::CleanQuery()
{
	if (m_query) {
		m_query->clear();
		m_query->deleteLater();
		m_query = nullptr;
	}

}

void CTRAKEntriesManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values)
{
	QVariantMap temp_variantMap;

	breakoutValuesToUpdate(older, newer, set_values, &temp_variantMap);

	QJsonObject temp_jsonObject = QJsonObject::fromVariantMap(temp_variantMap);
	updated_values->swap(temp_jsonObject);
}

void CTRAKEntriesManager::breakoutValuesToUpdate(const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QVariantMap* updated_values)
{

	QList<QString> excludeCols({ "id", "createdAt", "updatedAt" });
	QList<QString> dateCols({ "createDate", "createTime", "createdAt", "lastvisit", "updatedAt" });

	QStringList mysqlCols = GetColumnNames();

	QList<QString> set;
	QVariantMap update;

	for (const QString& key : newer.keys()) {
		if (!older.keys().contains(key))
			continue;

		if (excludeCols.contains(key))
			continue;

		if (!mysqlCols.contains(key))
			continue;

		QVariant newerValue = newer.value(key).toVariant();
		QVariant olderValue = older.value(key).toVariant();

		if (newerValue.isNull())
			newerValue = "";

		qDebug() << key;
		qDebug() << "New" << newerValue;
		qDebug() << "Old" << olderValue;

		if (!newerValue.canConvert<QString>()) {
			continue;
		}


		if (newerValue == olderValue)
			continue;


		if (dateCols.contains(key)) {
			QDateTime webportalDate = QDateTime::fromString(newerValue.toString(), Qt::ISODate);
			QDateTime localDate = QDateTime::fromString(olderValue.toString(), Qt::ISODate);

			if (webportalDate == localDate)
				continue;
		}

		

		set.push_back(key + " = :" + key);

		if (key == "userRole" && newerValue.toInt() > 1) {
			newerValue = 0;
		}

		if (newerValue.typeId() == QVariant::String) {
			newerValue = newerValue.toString().trimmed();
		}

		update.insert(key, newerValue);

	}

	qDebug() << "seting: " << set;
	qDebug() << "updating: " << update;

	set.swap(*set_values);
	update.swap(*updated_values);
};

QJsonArray CTRAKEntriesManager::GetColumns(bool reset)
{
	if (reset) {
		m_MySQL_Columns = QJsonArray();
	}

	if(m_MySQL_Columns.size() > 0)
		return m_MySQL_Columns;


	GetTable();

	QJsonObject overloader;
	QString colQuery =
		"SHOW COLUMNS from " + m_db_info.table;
	//QJsonArray colQueryResults = queryManager.execute(colQuery, overloader);

	m_table.setQuery(colQuery, m_table.database());

	qDebug() << "Returned Count" << m_table.rowCount();

	for (int i = 0; i < m_table.rowCount(); ++i)
	{
		QVariantMap results = m_queryManager.recordToMap(m_table.record(i));
		m_MySQL_Columns.append(QJsonObject::fromVariantMap(results));
	}

	CleanTable();

	return m_MySQL_Columns;
}

QStringList CTRAKEntriesManager::GetColumnNames(bool reset)
{
	if (reset)
	{
		m_TableColumns.clear();
	}

	if(m_TableColumns.size() > 0 && m_TableColumns.size() == m_MySQL_Columns.size())
		return m_TableColumns;

	QJsonArray columns = GetColumns(reset);

	for (int i = 0; i < columns.size(); ++i)
	{
		m_TableColumns.append(columns.at(i).toObject().value("Field").toString());
	}

	return m_TableColumns;
}

int CTRAKEntriesManager::GetLocalCount(const QList<QString>& p_conditions, const QJsonObject& p_placeholders)
{
	int local = 0;

	GetTable();

	try {
		QList<QString> conditions(p_conditions);
		QJsonObject placeholders(p_placeholders);

		if (conditions.isEmpty())
			throw std::exception();

		if (placeholders.isEmpty()) {
			m_table.setFilter(conditions.join(" AND "));
			m_table.select();
		}
		else {
			QSqlQuery query(m_table.database());
			query.prepare("SELECT id FROM "+ m_db_info.table+" WHERE " + conditions.join(" AND "));

			for (const auto& placeholder : placeholders.keys())
			{
				query.bindValue(":" + placeholder, placeholders.value(placeholder));
			}
			m_table.setQuery(query);
		}

		local = m_table.rowCount();

		CleanTable();

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

int CTRAKEntriesManager::GetRemoteCount(const QJsonObject& p_select, const QJsonObject& p_where, QJsonObject* p_returned_data)
{
	int remote = 0;

	try {
		QJsonObject query;
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if (!where.contains("custId"))
			(void)where.insert("custId", QString::number(m_trak_details.cust_id));

		if (!where.contains(m_trak_details.trak_id_type))
			(void)where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);

		if (!select.contains("count"))
			(void)select.insert("count", "total");

		QJsonObject options;
		(void)options.insert("useEmpId", true);

		(void)query.insert("options", options);
		(void)query.insert("where", where);
		(void)query.insert("select", select);

		QJsonDocument reply;

		if (m_networkManager->isInternetConnected())
			(void)m_networkManager->authenticateSession();

		if (!m_networkManager->makeGetRequest(m_webportal_details.api_url + "/" + m_webportal_details.base_route, query, &reply))
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

QJsonArray CTRAKEntriesManager::GetRemote(const QJsonArray& columns, const QJsonObject& p_where, const QJsonObject& p_select)
{

	QJsonArray returnedValues;

	try {

		QJsonObject query;
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if (!where.contains("custId"))
			(void)where.insert("custId", QString::number(m_trak_details.cust_id));

		if (!where.contains(m_trak_details.trak_id_type))
			(void)where.insert(m_trak_details.trak_id_type, m_trak_details.trak_id);

		(void)query.insert("where", where);

		// QJsonArray({ "custId", "userId", "empId" })

		if (!columns.isEmpty()) {
			(void)select.insert("columns", columns);
		}

		if (!select.isEmpty())
			(void)query.insert("select", select);

		QJsonObject options;
		(void)options.insert("useEmpId", true);

		(void)query.insert("options", options);

		QJsonDocument reply;

		if (m_networkManager->isInternetConnected())
			(void)m_networkManager->authenticateSession();

		if (!m_networkManager->makeGetRequest(m_webportal_details.api_url + "/" + m_webportal_details.base_route, query, &reply))
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

QJsonArray CTRAKEntriesManager::GetLocal(const QString& query, const QJsonObject& placeholders)
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

		if (!(val.startsWith("'") && val.endsWith("'")))
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

QList<QVariantMap> CTRAKEntriesManager::GetLocal(const QStringList& t_columns, const QStringList& t_conditions, const QVariantMap& t_placeholders)
{

	GetTable();

	QList<QVariantMap> returnedMap;

	try {

		QStringList columns(t_columns);
		QStringList conditions(t_conditions);

		if (t_placeholders.isEmpty()) {
			m_table.setFilter(conditions.join(" AND "));
			m_table.select();
		}
		else {
			QStringList queryStrList;
			queryStrList << "SELECT" << columns.join(", ") << "FROM" << m_db_info.table;

			if (!conditions.isEmpty())
				queryStrList << "WHERE" << conditions.join(" AND ");

			QString queryStr(queryStrList.join(" "));

			QVariantMap valuesToBind = m_queryManager.processPlaceholders(t_placeholders, queryStr);

			if (valuesToBind.isEmpty()) {
				QString where = "WHERE";
				m_table.setFilter(queryStr.sliced(queryStr.indexOf(where) + where.length()).trimmed());
				m_table.select();
			}
			else {
				QSqlQuery query(m_table.database());
				query.prepare(queryStr);

				m_queryManager.BindValues(valuesToBind, &query);

				m_table.setQuery(query);

				qDebug() << "Query To Execute:" << m_table.query().executedQuery();
				qDebug() << "Values bound:" << m_table.query().boundValues();
			}
		}

		qDebug() << "Fetched rows:" << m_table.rowCount();

		for(int i = 0; i < m_table.rowCount(); ++i)
		{
			QVariantMap results = m_queryManager.recordToMap(m_table.record(i));

			qDebug() << "Entry number" << i << ":" << results;

			returnedMap.append(results);
		}

	}
	catch (const std::exception* e) {

	}

	CleanTable();

	return returnedMap;

}

QJsonArray CTRAKEntriesManager::GetGroupedRemote(const QJsonArray& columns, const QJsonArray& grouped_columns, const QJsonArray& group_by, const QString& type, const QString& separator, const QJsonObject& p_where)
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

		QJsonObject options;
		(void)options.insert("useEmpId", true);

		(void)query.insert("options", options);

		QJsonDocument reply;

		if (m_networkManager->isInternetConnected())
			(void)m_networkManager->authenticateSession();

		if (!m_networkManager->makeGetRequest(m_webportal_details.api_url + "/" + m_webportal_details.base_route, query, &reply))
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

int CTRAKEntriesManager::SendToRemote(const QJsonObject& entry, const QJsonObject& data)
{

	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (m_networkManager->isInternetConnected())
		(void)m_networkManager->authenticateSession();

	if (m_networkManager->makePostRequest(m_webportal_details.api_url + "/" + m_webportal_details.base_route, entry, body, &reply)) {
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

int CTRAKEntriesManager::CreateLocal(const QJsonObject& entry)
{
	/*QJsonArray results = queryManager.execute("INSERT INTO "+ db_info.table +" (" + entry.keys().join(", ") + ") VALUES (:" + entry.keys().join(",:") + ")", entry);

	if (results.size() <= 0)
		return 0;
	return 1;*/

	GetTable();
	int returnVal = 0;

	try {
		int row = m_table.rowCount();

		if(!m_table.insertRows(row, 1))
			throw std::exception();

		for (const auto& key : entry.keys()) {
			m_table.setData(m_table.index(row, m_table.fieldIndex(key)), entry.value(key));
		}

		qDebug() << "Table is dirty" << m_table.isDirty();

		returnVal = m_table.submitAll();

		qDebug() << "Table is dirty" << m_table.isDirty();

	}
	catch (const std::exception& e) {
		returnVal = 0;
	}

	CleanTable();

	return returnVal;

}

int CTRAKEntriesManager::UpdateRemote(const QJsonObject& entry, const QJsonObject& data)
{
	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (m_networkManager->isInternetConnected())
		(void)m_networkManager->authenticateSession();

	if (m_networkManager->makePatchRequest(m_webportal_details.api_url + "/" + m_webportal_details.base_route, entry, body, &reply)) {
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

QJsonArray CTRAKEntriesManager::UpdateLocal(const QList<QString>& update, const QJsonObject& placeholders)
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

QList<QVariantMap> CTRAKEntriesManager::UpdateLocal(const QList<QString>& update, const QVariantMap& placeholders)
{
	QStringList conditions;
	
	if (placeholders.contains("custId"))
		conditions.append("custId = :custId");
	if (placeholders.contains("userId"))
		conditions.append("userId = :userId");
	if (placeholders.contains("empId"))
		conditions.append("empId = :empId");


	QString queryToExec = "UPDATE "+ m_db_info.table +" SET " + update.join(", ") + " WHERE " + conditions.join(" AND ");

	return m_queryManager.execute(queryToExec, placeholders);
}

void CTRAKEntriesManager::HandleUpdatingEntries(const QJsonObject& local, const QJsonObject& remote)
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
		(void)CTRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &update);

		set.push_back("updatedAt = :updatedAt");
		placeholders.insert("updatedAt", currentDateTime.toLocalTime());
		placeholders.insert("tz", m_time_zone);
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
		(void)CTRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &placeholders);
		(void)CTRAKEntriesManager::breakoutValuesToUpdate(remote, local, &set, &update);

		set.push_back("updatedAt = :updatedAt");
		placeholders.insert("updatedAt", currentDateTime.toLocalTime());
		placeholders.insert("tz", m_time_zone);
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

int CTRAKEntriesManager::SyncWebportal()
{
	return 1;
}

int CTRAKEntriesManager::SyncLocal()
{
	return 1;
}

int CTRAKEntriesManager::UpdateOutdated()
{

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	DWORD size = sizeof(TCHAR);
	(void)rtManager.GetValSize(std::string(m_registryEntry).append("Date").data(), REG_SZ, &size);

	if (!size) {
		(void)UpdateCheckedTime();
		(void)rtManager.GetValSize(std::string(m_registryEntry).append("Date").data(), REG_SZ, &size);
	}

	std::vector<TCHAR> buffer(size);

	(void)rtManager.GetVal(std::string(m_registryEntry).append("Date").data(), REG_SZ, buffer.data(), &size);

	QString lastCheckedDateTime(buffer.data());

	UpdateCheckedTime();

	return 1;
}


int CTRAKEntriesManager::GetCurrentState()
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

int CTRAKEntriesManager::UpdateCheckedTime()
{
	QString currDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	
	(void)rtManager.SetVal(std::string(m_registryEntry).append("Date").data(), REG_SZ, currDate.toStdString().data(), currDate.toStdString().size());
	
	return NEXTSTEP::ALL_UPDATED;
}

QString CTRAKEntriesManager::ClearCloudUpdate()
{
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	DWORD size = sizeof(TCHAR);

	(void)rtManager.GetValSize(std::string(m_registryEntry).append("Date").data(), REG_SZ, &size);

	if (size == 0) {
		QString currDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
		
		if (rtManager.SetVal(std::string(m_registryEntry).append("Date").data(), REG_SZ, currDate.toStdString().data(), currDate.toStdString().size()))
			throw HenchmanServiceException("Failed to store checked date in registery");
		
		(void)rtManager.GetValSize(std::string(m_registryEntry).append("Date").data(), REG_SZ, &size);
	}

	std::vector<TCHAR> buffer;
	buffer.reserve(size);

	LONG err = rtManager.GetVal(std::string(m_registryEntry).append("Date").data(), REG_SZ, buffer.data(), &size);
	
	QString date(buffer.data());
	
	if (err) {
		const TCHAR* errMsg = "%d";
		TCHAR buffer[2048] = "\0";
		rtManager.GetSystemError(errMsg, err, buffer, 2048);
		throw HenchmanServiceException("Failed to fetch target stored value from registry. Error: " + std::to_string(err) + " - " + std::string(buffer));
	}

	rtManager.SetVal(m_registryEntry, REG_DWORD, &local_count, sizeof(local_count));
	
	return date;
};

