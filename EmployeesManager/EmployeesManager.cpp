
#include "EmployeesManager.h"

EmployeesManager::EmployeesManager(QObject* parent, const TrakDetails& trakDetails, const WebportalDetails& webportalDetails)
	:QObject(parent), queryManager(parent), networkManager(parent)
{

	trak_details = trakDetails;
	webportal_details = webportalDetails;

	networkManager.setApiUrl(webportal_details.api_url);
	networkManager.setApiKey(webportal_details.api_key);
	networkManager.toggleSecureTransport(DEBUG);

	try {

		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, ("SOFTWARE\\HenchmanTRAK\\" + trak_details.trak_type + "\\Database").toStdString().c_str());

		TCHAR buffer[1024] = "\0";
		DWORD size = sizeof(buffer);

		rtManager.GetVal("Schema", REG_SZ, buffer, size);
		trak_details.schema = QString(buffer);

		queryManager.setSchema(trak_details.schema);
	}
	catch (void*)
	{

	}

	try {
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

		TCHAR buffer[1024] = "\0";
		DWORD size = sizeof(buffer);

		(void)rtManager.GetVal("employeesChecked", REG_SZ, (DWORD*)&local_employees_count, sizeof(DWORD));

		std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM employees WHERE userId <> '' AND userId IS NOT NULL");

		if (!local_employees_count || local_employees_count != rowCheck[1][rowCheck[1].firstKey()].toInt()) {
			local_employees_count = rowCheck[1][rowCheck[1].firstKey()].toInt();
			(void)rtManager.SetVal("employeesChecked", REG_DWORD, (DWORD*)&local_employees_count, sizeof(DWORD));
		}

	}
	catch (void*)
	{

	}

	if (networkManager.isInternetConnected())
		(void)networkManager.authenticateSession();
}

EmployeesManager::~EmployeesManager()
{
	networkManager.cleanManager();
}

void EmployeesManager::breakoutValuesToUpdate(const QString& currentDateTime, const QJsonObject& older, const QJsonObject& newer, QList<QString>* set_values, QJsonObject* updated_values)
{

	QList<QString> excludeCols({ "id", "createdAt", "updatedAt" });
	QList<QString> dateCols({ "createDate", "createTime", "createdAt", "lastvisit", "updatedAt" });

	QList<QString> set;
	QJsonObject update;

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

	set.push_back("updatedAt = :updatedAt");
	update.insert("updatedAt", currentDateTime);

	set.swap(*set_values);
	update.swap(*updated_values);
};

int EmployeesManager::GetLocalEmployeeCount(const QList<QString> & conditions, const QJsonObject& placeholders)
{
	int local_employees = 0;

	try {
		if (conditions.isEmpty()) {
			throw std::exception();
		}

		QJsonArray rowCount = queryManager.ExecuteTargetSql("SELECT COUNT(*) as total FROM employees WHERE " + conditions.join(" AND "), placeholders);

		if (rowCount.size() <= 0 || !rowCount.at(0).isObject()) {
			throw std::exception();
		}

		qDebug() << rowCount.at(0).toObject();

		local_employees = rowCount.at(0).toObject().value("total").toVariant().toInt();
	}catch (const std::exception& e) {
		local_employees = local_employees_count;
	}
	return local_employees;
}

QJsonArray EmployeesManager::GetColumns()
{
	QJsonObject overloader;
	std::string colQuery =
		"SHOW COLUMNS from employees";
	QJsonArray colQueryResults = queryManager.ExecuteTargetSql(colQuery, overloader);

	qDebug() << colQueryResults;

	return colQueryResults;
}

int EmployeesManager::GetRemoteEmployeeCount(const QJsonObject& p_select, const QJsonObject& p_where)
{

	try {
				

		QJsonObject query;
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if(!where.contains("custId"))
			(void)where.insert("custId", QString::number(trak_details.cust_id));
		
		if (!where.contains(trak_details.trak_id_type))
			(void)where.insert(trak_details.trak_id_type, trak_details.trak_id);
		
		if (!select.contains("count"))
			(void)select.insert("count", "TOTAL");

		(void)query.insert("where", where);
		(void)query.insert("select", select);
		
		QJsonDocument reply;

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/employees", query, &reply))
		{
			throw;
		}
		
		if (!reply.isObject())
			throw;
		
		QJsonObject resultObject = reply.object();
		if (resultObject["data"].isNull() || resultObject["data"].isUndefined())
			throw;
		
		QJsonArray webportalResults = resultObject["data"].toArray();
		if (!webportalResults.at(0).isObject())
			throw;
		
		QJsonObject webportalToolCount = webportalResults.at(0).toObject();

		remote_employees_count = webportalToolCount.value("TOTAL").toInt();

		
	}
	catch (void*)
	{
	}


	return remote_employees_count;
}

QJsonArray EmployeesManager::GetRemoteEmployees(const QJsonArray& columns, const QJsonObject& p_where, const QJsonObject& p_select)
{

	QJsonArray returnedValues;

	try {

		QJsonObject query;
		QJsonObject where(p_where);
		QJsonObject select(p_select);

		if (where.isEmpty()) {
			(void)where.insert("custId", QString::number(trak_details.cust_id));
			(void)where.insert("kabId", trak_details.trak_id);
		}

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
			throw;
		}

		if (!reply.isObject())
			throw;

		QJsonObject resultObject = reply.object();
		if (resultObject.value("data").isNull() || resultObject.value("data").isUndefined() || !resultObject.value("data").isArray())
			throw;
		
		returnedValues = resultObject["data"].toArray();

	}
	catch (void*)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

QJsonArray EmployeesManager::GetLocalEmployees(const QString& query, const QJsonObject& placeholders)
{
	return queryManager.ExecuteTargetSql(query, placeholders);
}

QJsonArray EmployeesManager::GetGroupedRemoteEmployees(const QJsonArray& columns, const QJsonArray& grouped_columns, const QString& type, const QString& separator, const QJsonObject& where)
{

	QJsonArray returnedValues;

	try {

		QJsonObject query;
		QJsonObject select;
		QJsonObject group;

		if (where.isEmpty()) {
			QJsonObject where;

			(void)where.insert("custId", QString::number(trak_details.cust_id));
			(void)where.insert("kabId", trak_details.trak_id);

			(void)query.insert("where", where);
		}
		else {
			(void)query.insert("where", where);
		}

		// QJsonArray({ "custId", "userId", "empId" })
		(void)select.insert("columns", columns);
		
		(void)group.insert("columns", grouped_columns);
		(void)group.insert("group_type", type);
		if(!separator.isEmpty())
			(void)group.insert("separator", separator);

		select.insert("group", group);

		query.insert("select", select);

		qDebug() << query;

		QJsonDocument reply;

		if (!networkManager.makeGetRequest(webportal_details.api_url + "/employees", query, &reply))
		{
			throw;
		}

		if (!reply.isObject())
			throw;

		QJsonObject resultObject = reply.object();
		if (resultObject.value("data").isNull() || resultObject.value("data").isUndefined() || !resultObject.value("data").isArray())
			throw;

		returnedValues = resultObject["data"].toArray();

	}
	catch (void*)
	{
		throw HenchmanServiceException("Failed to make get request to webportal");
	}

	return returnedValues;
}

int EmployeesManager::SendEmployeeToRemote(const QJsonObject& employee, const QJsonObject& data)
{

	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (networkManager.makePostRequest(webportal_details.api_url + "/employees", employee, body, &reply)) {
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

int EmployeesManager::UpdateRemoteEmployee(const QJsonObject& employee, const QJsonObject& data)
{
	QJsonObject body;
	body["data"] = data;

	//networkManager.makePostRequest(apiUrl + "/employees", result, body);

	QJsonDocument reply;

	if (networkManager.makePatchRequest(webportal_details.api_url + "/employees", employee, body, &reply)) {
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

QJsonArray EmployeesManager::UpdateLocalEmployee(const QList<QString>& update, const QJsonObject& placeholders)
{
	return queryManager.ExecuteTargetSql("UPDATE employees SET " + update.join(", ") + " WHERE custId = :custId AND userId = :userId AND empId = :empId", placeholders);
}

int EmployeesManager::ClearCloudUpdate()
{
	if (local_employees_count == remote_employees_count)
	{
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
		
		TCHAR buffer[1024] = "\0";
		DWORD size = sizeof(buffer);
		
		if (rtManager.GetVal("employeesCheckedDate", REG_SZ, buffer, size)) {
			QString currDate = QDateTime::currentDateTime().toString(Qt::ISODate).replace("T", " ");
			QJsonObject params;
			params["date"] = currDate;
			rtManager.SetVal("employeesCheckedDate", REG_SZ, currDate.toStdString().data(), currDate.toStdString().size());
			(void)queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% employees%' AND DatePosted < :date AND (posted = 0 OR posted = 2)", params);

			for (int i = 0; i <= currDate.toStdString().size(); ++i)
			{
				buffer[i] = currDate.toStdString()[i];
			}
		}

		QString lastCheckedDateTime(buffer);
		QList<QString> lastCheckedDateTimeSplit = lastCheckedDateTime.split(" ");

		qDebug() << "lastCheckedDateTime: " << lastCheckedDateTime;
		QJsonObject placeholder;
		placeholder.insert("lastCheckedDate", lastCheckedDateTimeSplit[0]);
		placeholder.insert("lastCheckedTime", lastCheckedDateTimeSplit[1]);

		QJsonObject select;
		select.insert("outdated", lastCheckedDateTime);

		int local_employeees_that_need_updating = GetLocalEmployeeCount({"updatedAt >= TIMESTAMP(:lastCheckedDate,:lastCheckedTime)"}, placeholder);
		
		int remote_employeees_that_need_updating = GetRemoteEmployeeCount(select);

		qDebug() << "local_employeees_that_need_updating: " << local_employeees_that_need_updating;
		qDebug() << "remote_employeees_that_need_updating: " << remote_employeees_that_need_updating;

		if (!local_employeees_that_need_updating && !remote_employeees_that_need_updating)
			return 1;

		QJsonArray outdatedLocalEmployees = GetLocalEmployees("SELECT * FROM employees WHERE updatedAt >= TIMESTAMP(:lastCheckedDate,:lastCheckedTime)", placeholder);
		
		QJsonArray outdatedRemoteEmployees = GetRemoteEmployees(QJsonArray(), QJsonObject(), select);

		qDebug() << "outdatedLocalEmployees: " << QJsonDocument(outdatedLocalEmployees).toJson().toStdString().data();
		qDebug() << "outdatedRemoteEmployees: " << QJsonDocument(outdatedRemoteEmployees).toJson().toStdString().data();

		return 1;
	}
	return 0;
}


