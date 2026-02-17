
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

int EmployeesManager::GetLocalEmployeeCount()
{
	int local_employees = local_employees_count;
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

int EmployeesManager::GetRemoteEmployeeCount()
{

	try {
				

		QJsonObject query;
		QJsonObject where;
		QJsonObject select;

		(void)where.insert("custId", QString::number(trak_details.cust_id));
		(void)where.insert("kabId", trak_details.trak_id);
		(void)select.insert("count", "total");


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

		remote_employees_count = webportalToolCount.value("total").toInt();

		
	}
	catch (void*)
	{
	}


	return remote_employees_count;
}

QJsonArray EmployeesManager::GetRemoteEmployees(const QJsonArray& columns, const QJsonObject& where)
{

	QJsonArray returnedValues;

	try {

		QJsonObject query;
		QJsonObject select;

		if (where.isEmpty()) {
			QJsonObject where;

			(void)where.insert("custId", QString::number(trak_details.cust_id));
			(void)where.insert("kabId", trak_details.trak_id);

			(void)query.insert("where", where);
		} else {
			(void)query.insert("where", where);
		}

		// QJsonArray({ "custId", "userId", "empId" })
		(void)select.insert("columns", columns);
		

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

QJsonArray EmployeesManager::GetLocalEmployees(const QString& query, const QJsonObject& placeholders)
{
	return queryManager.ExecuteTargetSql(query, placeholders);
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

int EmployeesManager::ClearCloudUpdate()
{
	if (local_employees_count == remote_employees_count)
	{
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
		
		TCHAR buffer[1024] = "\0";
		DWORD size = sizeof(buffer);
		
		if (rtManager.GetVal("employeesCheckedDate", REG_SZ, buffer, size)) {
			QDate currDate = QDate::currentDate();
			QJsonObject params;
			params["date"] = currDate.toString();
			rtManager.SetVal("employeesCheckedDate", REG_SZ, currDate.toString().toStdString().data(), currDate.toString().toStdString().size());
			(void)queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% employees%' AND DatePosted < :date AND (posted = 0 OR posted = 2)", params);
		}
		return 1;
	}
	return 0;
}

