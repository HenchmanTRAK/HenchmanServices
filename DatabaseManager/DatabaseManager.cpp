
#include "DatabaseManager.h"

static std::array<std::string, 2> timeStamp;

static bool doNotRunCloudUpdate = 0;
static bool parseCloudUpdate = 1;
static bool pushCloudUpdate = 1;

std::string getValidDrivers()
{
	std::stringstream results;
	for (const auto& str : QSqlDatabase::drivers())
	{
		results << " - " << str.toStdString() << "\n";
	}
	return results.str();
}
//
//static int checkValidConnections(QString &targetConnection)
//{
//	for (auto& str : QSqlDatabase::connectionNames())
//	{
//		std::cout << " - " << str.toUtf8().data() << endl;
//		if (str == targetConnection)
//			return TRUE;
//	}
//
//	return FALSE;
//}

static std::array<QString, 2> GetTrakDirAndIni(RegistryManager::CRegistryManager & rtManager)
{
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	QString trakDir;
	QString iniFile;
	try {
		rtManager.GetVal("TRAK_DIR", REG_SZ, (TCHAR*)buffer, 1024);
		trakDir = buffer;
		rtManager.GetVal("INI_FILE", REG_SZ, (TCHAR*)buffer, 1024);
		iniFile = buffer;
	}
	//catch (std::exception& e)
	catch(const HenchmanServiceException& e)
	{
		ServiceHelper().WriteToError(e.what());
	}

	return { trakDir, iniFile };
}

DatabaseManager::DatabaseManager(QObject* parent) 
: QObject(parent), sqliteManager(parent), networkManager(parent), queryManager(parent)
{
	timeStamp = ServiceHelper().timestamp();
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);

	rtManager.GetVal("INSTALL_DIR", REG_SZ, (char*)buffer, size);
	std::string installDir(buffer);

	QSettings ini(installDir.append("\\service.ini").data(), QSettings::IniFormat, this);
	ini.sync();
	testingDBManager = ini.value("DEVELOPMENT/testingDBManager", 0).toBool();

	ini.beginGroup("API");
	queryLimit = ini.value("NumberOfQueries", 10).toInt();
	apiUsername = ini.value("Username", "").toString();
	apiPassword = ini.value("Password", "").toString();
	apiKey = ini.value("apiKey", "").toString();
	ini.endGroup();

	ini.beginGroup("SYSTEM");
	databaseDriver = ini.value("databaseDriver", "").toString();
	ini.endGroup();

	if (testingDBManager)
		apiUrl = ini.value("DEVELOPMENT/URL", "http://localhost:3000/api/service").toString();
	else
		apiUrl = ini.value("API/URL", "https://webportal.henchmantrak.com/api/service").toString();

	shouldIgnoreDatabaseCustId = ini.value("SYSTEM/IgnoreCustId", 0).toBool();
	shouldIgnoreDatabaseTrakId = ini.value("SYSTEM/IgnoreTrakId", 0).toBool();

	LOG << apiUrl;

	LOG << "init db manager";
	
	targetApp = "";
	requestRunning = false;
	for (auto i = databaseTablesChecked.cbegin(); i != databaseTablesChecked.cend(); ++i)
	{
		try {
			if(rtManager.GetVal(i.key().toStdString().append("Checked").c_str(), REG_DWORD, (DWORD *)&databaseTablesChecked[i.key()], sizeof(DWORD)))
				rtManager.SetVal(i.key().toStdString().append("Checked").c_str(), REG_DWORD, (DWORD *)&databaseTablesChecked[i.key()], sizeof(DWORD));
		}
		//catch (std::exception& e)
		catch(const HenchmanServiceException& e)
		{
			ServiceHelper().WriteToError(e.what());

		}
	}

	networkManager.setApiKey(apiKey);
	networkManager.setApiUrl(apiUrl);
	networkManager.toggleSecureTransport(!testingDBManager);
}

DatabaseManager::~DatabaseManager() 
{
	LOG << "Deleting DatabaseManager";
	ServiceHelper().WriteToLog("DatabaseManager is being unitialised");

	performCleanup();
}

void DatabaseManager::loadTrakDetailsFromRegistry()
{
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal("APP_NAME", REG_SZ, (TCHAR*)buffer, size);
	trakType = std::string(buffer);

	RegistryManager::CRegistryManager rtManagerCustomer(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer").data());

	size = 1024;
	rtManagerCustomer.GetVal("trakID", REG_SZ, (TCHAR*)buffer, size);
	//string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
	trakId = std::string(buffer);
	if (trakId.ends_with("ID")) {
		trakId.pop_back();
		trakId.pop_back();
		trakId += "Id";
	}

	size = 1024;
	rtManagerCustomer.GetVal("ID", REG_SZ, (TCHAR*)buffer, size);
	//string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
	std::string strCustId(buffer);
	custId = std::stoi(strCustId);

	size = 1024;
	rtManagerCustomer.GetVal(trakId.data(), REG_SZ, (TCHAR*)buffer, size);
	//string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);
	std::string idNum(buffer);
	trakIdNum = QString::fromStdString(idNum);

	trakId.resize(trakId.size() - 2);
	trakId.append("Id");
}

void DatabaseManager::attachThreadController(QMutex* threadController)
{
	p_thread_controller = threadController;
}

// Misc Syncs
int DatabaseManager::addToolsIfNotExists()
{
	QLoggingCategory category("databasemanager");
	LOG << "Adding Tools to Webportal";
	QString targetKey = "tools";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM tools");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE  '% tools%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tools");
	std::string query =
		"SELECT * from tools ORDER BY id DESC LIMIT " + 
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);


	std::string colQuery =
		"SHOW COLUMNS from tools";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qCDebug(category) << colQueryResults;

	std::string tableName = "tools";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year"};
	QStringList uniqueIndexCols = { "custId", "toolId", "stockcode", "PartNo" };

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_"+ uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(", ").toStdString() + ")",
		result
	);

	columns.clear();

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();
	
	//connect(&networkManager, &NetworkManager::requestFinished, this, [=](const QJsonDocument& result) {
	//	/*qCDebug(category) << result.toJson().toStdString().data();*/
	//	QString targetKey = "tools";
	//	if (!result.isObject()) {
	//		LOG << "Reply was not an Object";
	//		databaseTablesChecked[targetKey]++;
	//		return;
	//	}
	//	LOG << result.toJson().toStdString();
	//	QJsonObject resultObject = result.object();
	//	if (resultObject["status"].toDouble() == 200 || resultObject["status"].toDouble() == 500) {
	//		LOG << resultObject["status"].toDouble();
	//		databaseTablesChecked[targetKey]++;
	//	}

	//	//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//});

	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["toolId"] = result["id"];

		//QString results[2];

		//processKeysAndValues(result, results);

		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;

		body["data"] = data;
		
		QJsonDocument reply;

		networkManager.makePostRequest(apiUrl + "/tools", result, body, &reply);

		QString targetKey = "tools";
		if (!reply.isObject()) {
			LOG << "Reply was not an Object";
			databaseTablesChecked[targetKey]++;
			continue;
		}
		LOG << reply.toJson().toStdString();
		QJsonObject resultObject = reply.object();
		if (resultObject["status"].toDouble() == 200 || resultObject["status"].toDouble() == 500) {
			LOG << resultObject["status"].toDouble();
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}
	//networkManager.execRequests();
	qCDebug(category) << "Setting" << targetKey + "Checked to" << databaseTablesChecked[targetKey];
	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);
	//performCleanup();
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE  '% tools%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}
	networkManager.cleanManager();

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addUsersIfNotExists()
{
	LOG << "Adding Users to Webportal";
	QString targetKey = "users";
	timeStamp = ServiceHelper().timestamp();
	
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	(void)rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	if (!databaseTablesChecked[targetKey]) {
		std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM users WHERE userId <> '' AND userId IS NOT NULL");
		databaseTablesChecked[targetKey] = rowCheck[1][rowCheck[1].firstKey()].toInt();
		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	}

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	QJsonDocument reply;

	QJsonObject query;
	QJsonObject where;
	QJsonObject select;

	QJsonArray webportalResults;
	QJsonObject webportalToolCount;

	try {

		where.insert("custId", QString::number(custId));
		where.insert("kabId", trakIdNum);
		select.insert("count", "total");

		query.insert("where", where);
		query.insert("select", select);

		if (!networkManager.makeGetRequest(apiUrl + "/users", query, &reply))
		{
			throw std::exception();
		}

		if (reply.isObject()) {
			LOG << reply.toJson().toStdString();
			QJsonObject resultObject = reply.object();
			if (!resultObject["data"].isNull() && !resultObject["data"].isUndefined()) {
				//webportalResults = resultObject["data"].toArray();
				if (resultObject["data"].toArray().at(0).isObject())
					webportalToolCount = resultObject["data"].toArray().at(0).toObject();
			}
		}
	}
	catch (void*)
	{
		networkManager.cleanManager();
		return 1;
	}


	if (databaseTablesChecked[targetKey] <= webportalToolCount["total"].toInt())
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			(void)rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			(void)queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% users%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}


	std::string colQuery =
		"SHOW COLUMNS from users";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	int hadEmpId = 0;

	//qDebug() << colQueryResults;

	std::string tableName = "users";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "userId", "kabId", "cribId", "scaleId"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "empId")
			hadEmpId = 1;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	//std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_"+ uniqueIndexCols.join("_").toStdString() +" ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")"
	);

	columns.clear();

	if (!hadEmpId)
		(void)queryManager.ExecuteTargetSql("ALTER TABLE users ADD empId VARCHAR(32) NOT NULL DEFAULT ''");

	try{
		select.erase(select.find("count"));

		select.insert("columns", QJsonArray({ "custId", "userId", "empId" }));

		query.insert("where", where);
		query.insert("select", select);

		qDebug() << query;

		if (!networkManager.makeGetRequest(apiUrl + "/users", query, &reply)) {
			throw std::exception();
		}

		if (reply.isObject()) {
			//LOG << reply.toJson().toStdString();
			QJsonObject resultObject = reply.object();
			if (!resultObject["data"].isNull() && !resultObject["data"].isUndefined()) {
				webportalResults = resultObject["data"].toArray();
				/*if (webportalResults.at(0).isObject()) {
					webportalEmployeeIds = webportalResults.at(0).toObject();
				}*/
			}
		}
	}
	catch (void*)
	{
		networkManager.cleanManager();
		return 1;
	}

	qDebug() << webportalResults;
	qDebug() << webportalResults.empty();

	ServiceHelper().WriteToLog("Exporting Users");

	QString userSelect;
	QJsonArray userIds;

	QVariantMap vMapConditionals = {};


	if (webportalResults.empty())
		userSelect = "SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL ORDER BY id DESC LIMIT " + QString::number(queryLimit);
	else {
		for (const QJsonValue& user : webportalResults)
		{
			if (!user.isObject())
				continue;
			userIds.push_back(user.toObject().value("userId"));
		}
		userSelect = "SELECT * FROM users WHERE userId <> '' AND userId IS NOT NULL AND userId NOT IN (:userIds) ORDER BY id DESC LIMIT " + QString::number(queryLimit);
		vMapConditionals.insert("userIds", userIds);
	}


	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(userSelect, vMapConditionals);

	qDebug() << sqlQueryResults;

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;

		QStringMap res;
		res["id"] = result["id"];
		//result.remove("id");

		//qDebug() << result;

		if (trakId != "kabId") {
			result["kabId"] = "";
			result["accessCountKab"] = 0;
		}
		if (trakId != "cribId") {
			result["cribId"] = "";
			result["accessCountCrib"] = 0;
		}
		if (trakId != "scaleId") {
			result["scaleId"] = "";
			result["accessCountPorta"] = 0;
		}
		
		if (result.value(trakId.data()).isEmpty())
			result[trakId.data()] = trakIdNum;


		QJsonObject data;
		std::map<std::string, std::string> sqliteData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			if (QStringList({"createdAt", "updatedAt"}).contains(key))
				continue;

			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				sqliteData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

		data[QString::fromStdString(trakId)] = trakIdNum;

		if (!data.contains("userId") || data.value("userId").toString().isEmpty() || data.value("userId").toString() == "''")
			continue;


		if (!data.contains("empId")) {
			try {
				std::vector uuidVector = queryManager.ExecuteTargetSql(
					"SELECT empId as uuid FROM employees WHERE userId = :userId",
					std::map<std::string, QVariant>({
						{"userId", data.value("userId").toVariant()},
					})
				);
				qDebug() << uuidVector;

				if (uuidVector.size() <= 1) {
					qDebug() << data;
					uuidVector = queryManager.ExecuteTargetSql("SELECT REGEXP_REPLACE(UUID(), '-', '') as uuid");

					QVariant empId(uuidVector.at(1).value("uuid"));

					(void)queryManager.ExecuteTargetSql(
						"INSERT INTO employees (custId, userId, empId, firstName, createDate, createTime) VALUES (:custId, :userId, :empId, :firstName, :createdDate, :createdTime)",
						std::map<std::string, QVariant>({
							{"userId", data.value("userId").toVariant()},
							{"custId", data.value("custId").toVariant()},
							{"empId", empId},
							{"firstName", QVariant(data.value("fullName").toString().split(" ").at(0))},
							{"createdDate", data.value("createdDate").toVariant()},
							{"createdTime", data.value("createdTime").toVariant()},
						})
					);
				}

				if (uuidVector.at(1).value("uuid").isEmpty())
					continue;

				QVariant empId(uuidVector.at(1).value("uuid"));

				qDebug() << uuidVector;

				/*if (!webportalResults.isEmpty()) {
					QJsonObject webportalEmployee;
					for (const QJsonValue& user : webportalResults)
					{
						qDebug() << user;
						if (!user.isObject())
							continue;
						QJsonObject empObj = user.toObject();
						qDebug() << "webportal userid:" << empObj.value("userId").toString() << "localdb userid: " << data.value("userId").toString();
						qDebug() << "webportal is same as localdb: " << (empObj.value("userId") == data.value("userId") ? "True" : "False");
						qDebug() << "webportal is same as localdb2: " << (empObj.value("userId").toString() == data.value("userId").toString() ? "True" : "False");
						qDebug() << "webportal is same as localdb3: " << (empObj.value("userId").toString().compare(data.value("userId").toString()) ? "True" : "False");
						if (empObj.value("userId").toString() != data.value("userId").toString()) continue;

						empId = empObj.value("empId").toVariant();
					}
				}*/


				(void)queryManager.ExecuteTargetSql("UPDATE users SET empId = :empId WHERE id = :id", std::map<std::string, QVariant>({
					{"empId", empId},
					{"id", data.value("id").toVariant()},
					}));

				data["empId"] = empId.toString();
			}
			catch (std::exception& e) {
				LOG << e.what();
				continue;
			}
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			sqliteData
		);
#endif

		sqliteData.clear();

		QJsonObject body;

		body["data"] = data;

		//networkManager.makePostRequest(apiUrl + "/users", result, body);

		QJsonDocument reply;
		
		if (networkManager.makePostRequest(apiUrl + "/users", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				//databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				//databaseTablesChecked[targetKey]++;
			}
		}
		/*else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}*/

		//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	/*if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% users%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}*/

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	networkManager.cleanManager();

	return 1;
}




int DatabaseManager::addEmployeesIfNotExists()
{
	LOG << "Adding Employees to Webportal";
	QString targetKey = "employees";
	timeStamp = ServiceHelper().timestamp();

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM employees WHERE userId <> '' AND userId IS NOT NULL");
	
	if (!databaseTablesChecked[targetKey] || databaseTablesChecked[targetKey] != rowCheck[1][rowCheck[1].firstKey()].toInt()) {
		databaseTablesChecked[targetKey] = rowCheck[1][rowCheck[1].firstKey()].toInt();
		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	}

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	QJsonDocument reply;

	QJsonObject query;
	QJsonObject where;
	QJsonObject select;

	QJsonArray webportalResults;
	QJsonObject webportalToolCount;

	try{

		where.insert("custId", QString::number(custId));
		where.insert("kabId", trakIdNum);
		select.insert("count", "total");

		query.insert("where", where);
		query.insert("select", select);

		if (!networkManager.makeGetRequest(apiUrl + "/employees", query, &reply))
		{
			throw std::exception();
		}

		if (reply.isObject()) {
			//LOG << result.toJson().toStdString();
			QJsonObject resultObject = reply.object();
			if (!resultObject["data"].isNull() && !resultObject["data"].isUndefined()) {
				//webportalResults = resultObject["data"].toArray();
				if (resultObject["data"].toArray().at(0).isObject())
					webportalToolCount = resultObject["data"].toArray().at(0).toObject();
			}
		}
	}
	catch (std::exception &e)
	{
		networkManager.cleanManager();
		return 1;
	}

	qDebug() << webportalToolCount;
	LOG << "Local Emp Count:" << databaseTablesChecked[targetKey];
	LOG << "Remote Emp Count:" << webportalToolCount["total"].toInt();

	if (databaseTablesChecked[targetKey] == webportalToolCount["total"].toInt())
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% employees%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}

	if (databaseTablesChecked[targetKey] <= webportalToolCount["total"].toInt())
	{

	}
	
	std::string colQuery =
		"SHOW COLUMNS from employees";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	int hadEmpId = 0;

	//qDebug() << colQueryResults;

	std::string tableName = "employees";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "userId"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "empId")
			hadEmpId = 1;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	//std::vector<stringmap> result;

	sqliteManager.ExecQuery("CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")");

	columns.clear();

	if (!hadEmpId)
		(void)queryManager.ExecuteTargetSql("ALTER TABLE employees ADD empId VARCHAR(32) NOT NULL DEFAULT ''");

	try {

		select.erase(select.find("count"));

		select.insert("columns", QJsonArray({ "custId", "userId", "empId" }));

		query.insert("where", where);
		query.insert("select", select);

		qDebug() << query;

		if (!networkManager.makeGetRequest(apiUrl + "/employees", query, &reply)) {
			throw std::exception();
		}

		//webportalResults.empty();
		//QJsonObject webportalEmployeeIds;

		if (reply.isObject()) {
			//LOG << result.toJson().toStdString();
			QJsonObject resultObject = reply.object();
			if (!resultObject["data"].isNull() && !resultObject["data"].isUndefined()) {
				webportalResults = resultObject["data"].toArray();
				/*if (webportalResults.at(0).isObject()) {
					webportalEmployeeIds = webportalResults.at(0).toObject();
				}*/
			}
		}

	}
	catch (void*)
	{
		networkManager.cleanManager();
		return 1;
	}
	qDebug() << webportalResults;
	qDebug() << webportalResults.empty();
	//qDebug() << webportalEmployeeIds.empty;

	ServiceHelper().WriteToLog("Exporting Employees");

	QString employeeSelect;
	QJsonArray employeeIds;

	QVariantMap vMapConditionals = {};

	if (webportalResults.empty())
		employeeSelect = "SELECT * FROM employees WHERE userId <> '' AND userId IS NOT NULL ORDER BY id DESC LIMIT " + QString::number(queryLimit);
	else {
		for (const QJsonValue& employee : webportalResults)
		{
			if (!employee.isObject())
				continue;

			employeeIds.push_back(employee.toObject().value("empId"));
		}
		employeeSelect = "SELECT * FROM employees WHERE empId NOT IN (:empIds) ORDER BY id DESC LIMIT " + QString::number(queryLimit);
		vMapConditionals.insert("empIds", employeeIds);
	}

	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(employeeSelect, vMapConditionals);

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		//qDebug() << result;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		std::map<std::string, std::string> sqliteData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				sqliteData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

		data[QString::fromStdString(trakId)] = trakIdNum;

		if (!data.contains("userId"))
			continue;


		if (!data.contains("empId")) {			
			std::vector uuidVector = queryManager.ExecuteTargetSql("SELECT REGEXP_REPLACE(UUID(), '-', '') as uuid");
			
			QVariant empId(uuidVector.at(1)["uuid"]);
			
			qDebug() << uuidVector;

			if (!webportalResults.isEmpty()) {
				QJsonObject webportalEmployee;
				for (const QJsonValue& employee : webportalResults)
				{
					qDebug() << employee;
					if (!employee.isObject())
						continue;
					QJsonObject empObj = employee.toObject();
					qDebug() << "webportal userid:" << empObj.value("userId").toString() << "localdb userid: " << data.value("userId").toString();
					qDebug() << "webportal is same as localdb: " << (empObj.value("userId") == data.value("userId") ? "True" : "False");
					qDebug() << "webportal is same as localdb2: " << (empObj.value("userId").toString() == data.value("userId").toString() ? "True" : "False");
					qDebug() << "webportal is same as localdb3: " << (empObj.value("userId").toString().compare(data.value("userId").toString()) ? "True" : "False");
					if (empObj.value("userId").toString() != data.value("userId").toString()) continue;
					
					empId = empObj.value("empId").toVariant();
				}
			}
			

			(void)queryManager.ExecuteTargetSql("UPDATE employees SET empId = :empId WHERE userId = :userId", std::map<std::string, QVariant>({
				{"empId", empId},
				{"userId", data.value("userId").toVariant()},
				}));

			data["empId"] = empId.toString();
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			sqliteData
		);
#endif

		sqliteData.clear();

		QJsonObject body;
		body["data"] = data;

		//networkManager.makePostRequest(apiUrl + "/employees", result, body);

		QJsonDocument reply;

		qDebug() << data;

		if (networkManager.makePostRequest(apiUrl + "/employees", result, body, &reply)) {
			qDebug() << reply;
			
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				//databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				//databaseTablesChecked[targetKey]++;
				//LOG << result["status"].toDouble();
			}
		}
		else {
			LOG << "No rows were altered on db";
			//databaseTablesChecked[targetKey]++;
		}

		//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

		//if (makeNetworkRequest(apiUrl, res, &reply)) {
		//	if (!reply.isObject())
		//		continue;
		//	QJsonObject result = reply.object();
		//	LOG << result["result"].toString().toStdString();
		//	if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
		//		databaseTablesChecked[targetKey]++;
		//		continue;
		//	}
		//	LOG << "No rows were altered on db";
		//	databaseTablesChecked[targetKey]++;
		//	//databaseTablesChecked[targetKey] += queryLimit;
		//	//Sleep(100);
		//	continue;
		//	//break;
		//}

	}

	//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);
	
	/*if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% employees%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}*/

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/
	networkManager.cleanManager();
	return 1;
}
int DatabaseManager::addJobsIfNotExists()
{
	LOG << "Adding Jobs to Webportal";
	QString targetKey = "jobs";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM jobs");
	std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
	QString indexingCol = colsCheck[1].value("Column_name");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey] || colsCheck.size() <= 1 || !colsCheck[0].value("success").toInt())
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% jobs%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Jobs");
	std::string query =
		"SELECT * from jobs ORDER BY "+ indexingCol.toStdString() + " ASC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from jobs";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "jobs";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt"};
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "trailId", "description", "remark"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	//connect(&networkManager, &NetworkManager::requestFinished, this, [=](const QJsonDocument& result) {
	//	/*qCDebug(category) << result.toJson().toStdString().data();*/
	//	QString targetKey = "jobs";
	//	if (!result.isObject()) {
	//		LOG << "Reply was not an Object";
	//		databaseTablesChecked[targetKey]++;
	//		return;
	//	}
	//	LOG << result.toJson().toStdString();
	//	QJsonObject resultObject = result.object();
	//	if (resultObject["status"].toDouble() == 200 || resultObject["status"].toDouble() == 500) {
	//		LOG << resultObject["status"].toDouble();
	//		databaseTablesChecked[targetKey]++;
	//	}

	//	//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//	});

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result[indexingCol];

		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		//networkManager.makePostRequest(apiUrl + "/jobs", result, body);

		QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/jobs", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

		//QJsonDocument reply;
		//if (makeNetworkRequest(apiUrl, res, &reply)) {
		//	if (!reply.isObject())
		//		continue;
		//	QJsonObject result = reply.object();
		//	LOG << result["result"].toString().toStdString();
		//	if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
		//		databaseTablesChecked[targetKey]++;
		//		continue;
		//	}
		//	LOG << "No rows were altered on db";
		//	databaseTablesChecked[targetKey]++;
		//	//databaseTablesChecked[targetKey] += queryLimit;
		//	//Sleep(100);
		//	continue;
		//	//break;
		//}

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey] || colsCheck.size() <= 1 || !colsCheck[0].value("success").toInt())
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% jobs%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	networkManager.cleanManager();

	return 1;
}

// KabTRAK Syncs
int DatabaseManager::addKabsIfNotExists()
{
	LOG << "Adding Kabs to Webportal";
	QString targetKey = "kabs";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM itemkabs");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);

	std::string trakDir;
	std::string iniFile;
	QString trakModelNumber;
	if (rowCheck.size() > 1 && rowCheck[1][rowCheck[1].firstKey()].toInt() < 1) {

		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
		std::string kabId = ini.value("Customer/kabID", trakIdNum).toString().toStdString();
		std::string description = ini.value("Unit/Description", "KT-" + trakIdNum.last(3)).toString().toStdString();
		std::string serialNo = ini.value("Unit/SerialNo", "KT-" + trakIdNum.last(3)).toString().toStdString();
		std::string cols = "custId";
		std::string vals = "'" + std::to_string(custId) + "'";
		if (!kabId.empty()) {
			cols += ", kabId";
			vals += ", '" + kabId + "'";
		}
		if (!description.empty()) {
			cols += ", description";
			vals += ", '" + description + "'";
		}
		if (!serialNo.empty()) {
			cols += ", serialNumber";
			vals += ", '" + serialNo + "'";
		}
		if (!trakModelNumber.isEmpty()) {
			cols += ", modelNumber";
			vals += ", '" + trakModelNumber.toStdString() + "'";
		}
		queryManager.ExecuteTargetSql("INSERT INTO itemkabs (" + cols + ") VALUES (" + vals + ")");

		databaseTablesChecked[targetKey]--;
	}
	else {
		rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	}

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabs%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}

	if (databaseTablesChecked[targetKey] < 0)
		databaseTablesChecked[targetKey]++;

	ServiceHelper().WriteToLog("Exporting Kabs");
	std::string query =
		"SELECT * from itemkabs ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() > 0) {
		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		//size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
	}

	std::string colQuery =
		"SHOW COLUMNS from itemkabs";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "itemkabs";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "kabId" };

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	//connect(&networkManager, &NetworkManager::requestFinished, this, [=](const QJsonDocument& result) {
	//	/*qCDebug(category) << result.toJson().toStdString().data();*/
	//	QString targetKey = "kabs";
	//	if (!result.isObject()) {
	//		LOG << "Reply was not an Object";
	//		databaseTablesChecked[targetKey]++;
	//		return;
	//	}
	//	LOG << result.toJson().toStdString();
	//	QJsonObject resultObject = result.object();
	//	if (resultObject["status"].toDouble() == 200 || resultObject["status"].toDouble() == 500) {
	//		LOG << resultObject["status"].toDouble();
	//		databaseTablesChecked[targetKey]++;
	//	}

	//	//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//	});

	QStringList ensureValForTargetCols = { "description", "serialNumber", "modelNumber" };

	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		std::map<std::string, std::string> toolData;
		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (ensureValForTargetCols.contains(key) && (val.isEmpty() || val== "''")) {
				if (key == "description") {
					val = "KT-" + trakIdNum.last(3);
				}
				if (key == "serialNumber") {
					val = "KT-" + trakIdNum.last(3);
				}
				if (key == "modelNumber" && !trakModelNumber.isEmpty()) {
					val = trakModelNumber;
				}
			}
			data[key] = val;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		//networkManager.makePostRequest(apiUrl + "/kabtrak", result, body);

		QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/kabtrak", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabs%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addDrawersIfNotExists()
{
	LOG << "Adding Drawers to Webportal";
	QString targetKey = "kabDrawers";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM itemkabdrawers");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabdrawers%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kab Drawers");
	std::string query =
		"SELECT * from itemkabdrawers ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from itemkabdrawers";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "itemkabdrawers";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "kabId", "drawerCode"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	connect(&networkManager, &NetworkManager::requestFinished, this, [=](const QJsonDocument& result) {
		/*qCDebug(category) << result.toJson().toStdString().data();*/
		QString targetKey = "kabDrawers";
		if (!result.isObject()) {
			LOG << "Reply was not an Object";
			databaseTablesChecked[targetKey]++;
			return;
		}
		LOG << result.toJson().toStdString();
		QJsonObject resultObject = result.object();
		if (resultObject["status"].toDouble() == 200 || resultObject["status"].toDouble() == 500) {
			LOG << resultObject["status"].toDouble();
			databaseTablesChecked[targetKey]++;
		}

		//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
		});

	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		networkManager.makePostRequest(apiUrl + "/kabtrak/drawers", result, body);

		/*QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/kabtrak/drawers", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));*/

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numDrawersChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabdrawers%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addToolsInDrawersIfNotExists()
{
	LOG << "Adding Kab Tools to Webportal";
	QString targetKey = "kabDrawerBins";
	timeStamp = ServiceHelper().timestamp();

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	if (!databaseTablesChecked[targetKey]) {
		std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM itemkabdrawerbins");
		databaseTablesChecked[targetKey] = rowCheck[1][rowCheck[1].firstKey()].toInt();
		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	}

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();


	QJsonDocument reply;
	
	QJsonObject query;
	QJsonObject where;
	QJsonObject select;

	where.insert("custId", QString::number(custId));
	where.insert("kabId", trakIdNum);
	select.insert("count", "total");

	query.insert("where", where);
	query.insert("select", select);

	if(!networkManager.makeGetRequest(apiUrl + "/kabtrak/tools", query, &reply))
		return 1;
	
	QJsonArray webportalResults;
	QJsonObject webportalToolCount;

	if (reply.isObject()) {	
	//LOG << result.toJson().toStdString();
		QJsonObject resultObject = reply.object();
		if (!resultObject["data"].isNull() && !resultObject["data"].isUndefined()) {
			webportalResults = resultObject["data"].toArray();
			if (webportalResults.at(0).isObject())
				webportalToolCount = webportalResults.at(0).toObject();
		}
	}
	
	LOG << databaseTablesChecked[targetKey];
	LOG << webportalResults.size();
	LOG << webportalToolCount["total"].toInt();

	if (databaseTablesChecked[targetKey] <= webportalToolCount["total"].toInt())
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabdrawerbins%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}

	std::string colQuery =
		"SHOW COLUMNS from itemkabdrawerbins";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "itemkabdrawerbins";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "kabId", "toolId", "itemId", "drawerNum", "toolNumber"};

	int hadToolId = 0;
	
	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	ServiceHelper().WriteToLog("Exporting Kab tools in bins");

	std::vector drawerToolsCheck = queryManager.ExecuteTargetSql("SELECT drawerNum, COUNT(*) as total FROM itemkabdrawerbins GROUP BY drawerNum");

	//connect(&networkManager, &NetworkManager::requestFinished, this, [this](const QJsonDocument& result) {
	//	/*qCDebug(category) << result.toJson().toStdString().data();*/
	//	QString targetKey = "kabDrawerBins";
	//	if (!result.isObject()) {
	//		LOG << "Reply was not an Object";
	//		return;
	//	}
	//	LOG << result.toJson().toStdString();
	//	QJsonObject resultObject = result.object();
	//	if (resultObject["status"].toDouble() == 200 || resultObject["status"].toDouble() == 500) {
	//		LOG << resultObject["status"].toDouble();
	//	}

	//	//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//	//rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//});

	for (auto& result : drawerToolsCheck)
	{
		if (result.firstKey() == "success")
			continue;

		where.insert("drawerNum", result.value("drawerNum"));
		select.insert("count", "total");

		query["where"] = where;
		query["select"] = select;
		
		QJsonDocument reply;

		if (!networkManager.makeGetRequest(apiUrl + "/kabtrak/tools", query, &reply))
			continue;
		QJsonArray webportalResults;
		QJsonObject webportalToolCount;

		if (reply.isObject()) {
			//LOG << result.toJson().toStdString();
			QJsonObject resultObject = reply.object();
			if (!resultObject["data"].isNull() && !resultObject["data"].isUndefined()) {
				webportalResults = resultObject["data"].toArray();
				if(webportalResults.at(0).isObject())
					webportalToolCount = webportalResults.at(0).toObject();
			}
		}

		qDebug() << "Webportal total: " << webportalToolCount.value("total") << " loval total: " << result.value("total");

		if (QString::number(webportalToolCount.value("total").toInt()) == result.value("total"))
			continue;
		
		query.remove("select");

		networkManager.makeGetRequest(apiUrl + "/kabtrak/tools", query, &reply);

		if (reply.isObject()) {
			//LOG << result.toJson().toStdString();
			QJsonObject resultObject = reply.object();
			if (!resultObject["data"].isNull() && !resultObject["data"].isUndefined()) {
				webportalResults = resultObject["data"].toArray();
				if (webportalResults.at(0).isObject())
					webportalToolCount = webportalResults.at(0).toObject();
			}
		}

		QListIterator<QVariant> wit(webportalResults.toVariantList());

		std::map<std::string, QVariant> vMapConditionals;
		QList<QString> listConditionals;

		vMapConditionals.insert_or_assign("drawerNum", result.value("drawerNum"));
		
		if (webportalResults.size() > 0) {
			//listConditionals.append("t.id NOT IN (:toolId)");

			//listConditionals.append("i.itemId NOT IN (:itemId)");
			//listConditionals.append("i.drawerNum NOT IN (:drawerNum)");
			listConditionals.append("i.toolNumber NOT IN (:toolNumber)");
		}

		while (wit.hasNext()) {
			QJsonObject tool = wit.next().toJsonObject();
			
			if (QString::number(tool.value("drawerNum").toInt()) != result.value("drawerNum"))
				continue;
			
			//qDebug() << tool;

			//	if (vMapConditionals.contains("toolId")) {
			//		if (!vMapConditionals.at("toolId").toJsonArray().contains(tool["toolId"])) {
			//			QJsonArray newVal = vMapConditionals.at("toolId").toJsonArray();
			//			newVal.append(tool["toolId"]);
			//			//vMapConditionals.at("toolId") = newVal;
			//			vMapConditionals.insert_or_assign("toolId", newVal);
			//		}
			//	}
			//	else
			//		vMapConditionals.insert_or_assign("toolId", tool["toolId"].toVariant());

			/*if (vMapConditionals.contains("itemId")) {
				if (!vMapConditionals.at("itemId").toJsonArray().contains(tool["itemId"])) {
					QJsonArray newVal = vMapConditionals.at("itemId").toJsonArray();
					newVal.append(tool["itemId"]);
					vMapConditionals.at("itemId") = newVal;
				}
			}
			else
				vMapConditionals.insert_or_assign("itemId", tool["itemId"].toVariant());*/

			//	if (vMapConditionals.contains("drawerNum")) {
			//		if (!vMapConditionals.at("drawerNum").toJsonArray().contains(tool["drawerNum"])) {
			//			QJsonArray newVal = vMapConditionals.at("drawerNum").toJsonArray();
			//			newVal.append(tool["drawerNum"]);
			//			vMapConditionals.at("drawerNum") = newVal;
			//		}
			//	}
			//	else
			//		vMapConditionals.insert_or_assign("drawerNum", tool["drawerNum"].toVariant());

			if (vMapConditionals.contains("toolNumber")) {
				if (!vMapConditionals.at("toolNumber").toJsonArray().contains(tool["toolNumber"])) {
					QJsonArray newVal = vMapConditionals.at("toolNumber").toJsonArray();
					newVal.append(tool["toolNumber"]);
					vMapConditionals.at("toolNumber") = newVal;
				}
			}
			else
				vMapConditionals.insert_or_assign("toolNumber", tool["toolNumber"].toVariant());

			//	//qDebug() << vMapConditionals;
		}
		
		std::string query =
			"SELECT i.*, t.id as toolId from itemkabdrawerbins AS i LEFT JOIN tools AS t ON t.PartNo LIKE i.itemId OR t.stockcode LIKE i.itemId";
		query.append(" WHERE i.drawerNum = :drawerNum");
		if (listConditionals.size() > 0)
			query.append(" AND " + listConditionals.join(" AND ").toStdString());

		query.append(" ORDER BY i.id DESC LIMIT " + std::to_string(queryLimit));

		LOG << query;
		qDebug() << vMapConditionals;


		std::vector sqlQueryResults = vMapConditionals.size() > 0 ? queryManager.ExecuteTargetSql(query, vMapConditionals) : queryManager.ExecuteTargetSql(query);

		for (auto& result : sqlQueryResults) {
			if (result.firstKey() == "success")
				continue;
			QStringMap res;
			res["id"] = result["id"];

			QJsonObject data;
			std::map<std::string, std::string> toolData;

			for (auto it = result.cbegin(); it != result.cend(); ++it)
			{
				QString key = it.key();
				QString val = it.value().trimmed().simplified();
				if (val.isEmpty() || val == "''")
					continue;
				if (!skipTargetCols.contains(key))
					toolData[key.toStdString()] = val.toStdString();
				data[key] = val;
			}

#if true
			sqliteManager.AddEntry(
				tableName,
				toolData
			);
#endif

			toolData.clear();

			QJsonObject body;
			body["data"] = data;

			qDebug() << body;

			//QJsonDocument reply;
			qDebug() << "Tool: itemId: " << data.value("itemId") << " toolId:" << data.value("toolId") << " drawerNum: " << data.value("drawerNum") << " toolNumber: " << data.value("toolNumber");

			while (wit.hasNext()) {
				QJsonObject tool = wit.next().toJsonObject();
				qDebug() << tool;

				if (tool.value("toolId") != data.value("toolId"))
					continue;
				if (tool.value("itemId") != data.value("itemId"))
					continue;
				if (tool.value("drawerNum") != data.value("drawerNum"))
					continue;
				if (tool.value("toolNumber") != data.value("toolNumber"))
					continue;
				qDebug() << "webportal tool: itemId: " << tool.value("itemId") << " toolId:" << tool.value("toolId") << " drawerNum: " << tool.value("drawerNum") << " toolNumber: " << tool.value("toolNumber");;
			}
			if (data.value("drawerNum").toString() == "7" && data.value("toolNumber").toString() == "79") {
				qDebug() << "forced breakout";
			}
			
			QJsonDocument results;

			networkManager.makePostRequest(apiUrl + "/kabtrak/tools", result, body, &results);

			QString targetKey = "kabDrawerBins";
			if (!results.isObject()) {
				LOG << "Reply was not an Object";
				continue;
			}
			LOG << results.toJson().toStdString();
			QJsonObject resultObject = results.object();
			if (resultObject["status"].toDouble() == 200 || resultObject["status"].toDouble() == 500) {
				LOG << resultObject["status"].toDouble();
			}

		}
	}
	
	
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	/*if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey]) {
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkabdrawerbins%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}*/

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::createKabtrakTransactionsTable() {
	LOG << "Adding Kabtrak Transactions Table to Service SQLite Database";

	std::string tableName = "kabemployeeitemtransactions";
	std::string colQuery =
		"SHOW COLUMNS from " + tableName;
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::vector<stringmap> results;

	sqliteManager.ExecQuery(
		"PRAGMA table_info(" + tableName + ")",
		results
	);

	if (colQueryResults.size() <= 1 || !results.empty())
		return 0;

	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "kabId", "drawerNum", "toolNum", "userId", "trailId", "itemId", "transDate", "transTime", "transType", "toolId"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();
	return 1;
}

// CribTRAK Syncs
int DatabaseManager::addCribsIfNotExists()
{
	LOG << "Adding Cribs to Webportal";
	QString targetKey = "cribs";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM cribs");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);

	std::string trakDir;
	std::string iniFile;
	QString trakModelNumber;
	if (rowCheck.size() > 1 && rowCheck[1][rowCheck[1].firstKey()].toInt() < 1) {

		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
		std::string cribId = ini.value("Customer/cribID", trakIdNum).toString().toStdString();
		std::string description = ini.value("Unit/Description", "CT-" + trakIdNum.last(3)).toString().toStdString();
		std::string serialNo = ini.value("Customer/cribSerial", "CT-" + trakIdNum.last(3)).toString().toStdString();
		std::string cols = "custId";
		std::string vals = "'" + std::to_string(custId) + "'";
		if (!cribId.empty()) {
			cols += ", cribId";
			vals += ", '" + cribId + "'";
		}
		if (!description.empty()) {
			cols += ", description";
			vals += ", '" + description + "'";
		}
		if (!serialNo.empty()) {
			cols += ", serialNumber";
			vals += ", '" + serialNo + "'";
		}
		if (!trakModelNumber.isEmpty()) {
			cols += ", modelNumber";
			vals += ", '" + trakModelNumber.toStdString() + "'";
		}
		queryManager.ExecuteTargetSql("INSERT INTO cribs (" + cols + ") VALUES (" + vals + ")");

		databaseTablesChecked[targetKey]--;
	}
	else {
		rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	}
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% cribs%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}

	if (databaseTablesChecked[targetKey] < 0)
		databaseTablesChecked[targetKey]++;

	ServiceHelper().WriteToLog("Exporting CribTRAKS from crib");
	std::string query =
		"SELECT * from cribs ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() > 0) {
		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		//size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
	}

	std::string colQuery =
		"SHOW COLUMNS from cribs";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "cribs";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId"};

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	QStringList ensureValForTargetCols = { "description", "serialNumber", "modelNumber" };
	
	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		std::map<std::string, std::string> toolData;
		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (ensureValForTargetCols.contains(key) && (val.isEmpty() || val == "''")) {
				if (key == "description") {
					val = "CT-" + trakIdNum.last(3);
				}
				if (key == "serialNumber") {
					val = "CT-" + trakIdNum.last(3);
				}
				if (key == "modelNumber" && !trakModelNumber.isEmpty()) {
					val = trakModelNumber;
				}
			}
			data[key] = val;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/cribtrak", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% cribs%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addCribToolLocationIfNotExists()
{
	LOG << "Adding CribToolLocations to Webportal";
	QString targetKey = "toolLocation";
	timeStamp = ServiceHelper().timestamp();
	std::vector tableCheck = queryManager.ExecuteTargetSql("show tables like 'cribtoollocation'");
	//qDebug() << tableCheck;
	QString cribtoollocationTable;
	if (tableCheck.size() > 1) {
		cribtoollocationTable = "cribtoollocation";
	}
	else {
		cribtoollocationTable = "cribtoollocations";
	}
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM " + cribtoollocationTable.toStdString());
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% " + cribtoollocationTable.toStdString() + "%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tool Location from CribTRAK");
	std::string query =
		"SELECT * from "+ cribtoollocationTable.toStdString() +" ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from " + cribtoollocationTable.toStdString();
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = cribtoollocationTable.toStdString();
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "locationId" };
	
	int hadLocationId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "locationId") {
			hadLocationId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadLocationId)
		columns.push_back("locationId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["locationId"] = result["id"];

		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/cribtrak/tools/locations", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% " + cribtoollocationTable.toStdString() + "%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addCribToolsIfNotExists()
{
	LOG << "Adding CribTools to Webportal";
	QString targetKey = "cribtools";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM cribtools");
	std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM cribtools WHERE Key_name = 'PRIMARY'");
	QString indexingCol = colsCheck[1].value("Column_name");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% cribtools %' OR SQLString LIKE '% cribtools%') AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tools from crib");
	std::string query =
		"SELECT * from cribtools ORDER BY "+ indexingCol.toStdString() + " DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from cribtools";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "cribtools";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "toolId", "barcodeTAG", "itemId"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	for (auto & result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;

		std::vector<QStringMap> fetchTool;
		if (!result.contains("id")) {
			res["id"] = result["toolId"];
			result["id"] = res["id"];
			fetchTool = queryManager.ExecuteTargetSql(("SELECT id, PartNo FROM tools WHERE ((PartNo IS NOT NULL OR PartNo <> '') AND PartNo = '" + result["itemId"] + "') OR ((serialNo IS NOT NULL OR serialNo <> '') AND serialNo = '" + result["serialNo"] + "') GROUP BY description ORDER BY id DESC LIMIT 1;").toStdString());
			result["toolId"] = "";
		}
		else {
			res["id"] = result["id"];
			//result["toolId"] = result["id"];
			fetchTool = queryManager.ExecuteTargetSql(("SELECT id, PartNo FROM tools WHERE ((PartNo IS NOT NULL OR PartNo <> '') AND PartNo = '" + result["itemId"] + "') OR ((serialNo IS NOT NULL OR serialNo <> '') AND serialNo = '" + result["serialNo"] + "') OR (id = '" + result["toolId"] + "') GROUP BY description ORDER BY id DESC LIMIT 1;").toStdString());
		}

		qDebug() << fetchTool;

		if((fetchTool.size() > 1 && !fetchTool[1].value("id").isEmpty()) && (!result.contains("toolId") || result.value("toolId").isEmpty() || result.value("toolId") == "''" || result.value("toolId") == "0"))
			result["toolId"] = fetchTool[1].value("id");

		if ((fetchTool.size() > 1 && !fetchTool[1].value("PartNo").isEmpty()) && (result.contains("itemId") && result.value("itemId").isEmpty() || result.value("itemId") == "''")) {
			result["itemId"] = fetchTool[1].value("PartNo");
		}

		if (!result.contains("userId") || result.value("userId").isEmpty() || result.value("userId") == "''" || result.value("userId") == "0") {
			std::vector<QStringMap> fetchUser = queryManager.ExecuteTargetSql(("SELECT userId FROM cribemployeeitemtransactions WHERE barcode LIKE "+result["barcodeTAG"] + " ORDER BY id DESC LIMIT 1;").toStdString());
			result["userId"] = fetchUser[1].value("userId");
		}


		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		if (result.contains("nextcalibrationdate")) {
			data["currentcalibrationdate"] = result["nextcalibrationdate"];
			//result.remove("nextcalibrationdate");
		}

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		qDebug() << fetchTool;
		LOG << QJsonDocument(data).toJson();

		if (result.value("id") == "27987") {
			LOG << "breakpoint";
		}

		if (networkManager.makePostRequest(apiUrl + "/cribtrak/tools", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% cribtools %' OR SQLString LIKE '% cribtools%') AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addCribToolTransferIfNotExists()
{
	LOG << "Adding CribToolTransfer to Webportal";
	QString targetKey = "tooltransfer";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM tooltransfer");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% tooltransfer%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tool Transfers from crib");
	std::string query =
		"SELECT * from tooltransfer ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from tooltransfer";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "tooltransfer";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "transferId", "barcodeTAG", "userId", "transfer_userId", "tailId", "transfer_tailId"};

	int hadTransferId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "transferId") {
			hadTransferId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadTransferId)
		columns.push_back("transferId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];

		if(!result.contains("transferId"))
			result["transferId"] = result["id"];

		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		LOG << QJsonDocument(body).toJson();


		if (networkManager.makePostRequest(apiUrl + "/cribtrak/tools/transfer", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey]) {
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% tooltransfer%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addCribConsumablesIfNotExists()
{
	LOG << "Adding Cribtrak Consumables to Webportal";
	QString targetKey = "cribconsumables";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM cribconsumables");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% cribconsumables %' OR SQLString LIKE '% cribconsumables%') AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");

		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Consumables from crib");
	std::string query =
		"SELECT * from cribconsumables ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from cribconsumables";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "cribconsumables";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "toolId", "barcode", "userId", "tailId"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId")) {
			continue;
		}
		QStringMap res;
		/*if (!result.contains("id")) {
			res["id"] = result["toolId"];
		}
		else {*/
			res["id"] = result["id"];
			//result["toolId"] = result["id"];
		//}
		std::vector fetchTool = queryManager.ExecuteTargetSql(("SELECT t.id FROM cribtools AS ct LEFT JOIN tools AS t ON ((t.PartNo IS NOT NULL OR t.PartNo <> '') AND t.PartNo = ct.itemId) OR ((t.serialNo IS NOT NULL OR t.serialNo <> '') AND t.serialNo = ct.serialNo) OR (t.id = ct.toolId) WHERE ct.barcodeTAG LIKE '" + result["barcode"]+"'").toStdString());
		if (fetchTool.size() <= 1) {
			databaseTablesChecked[targetKey]++;
			continue;
		}
		if (!fetchTool[1]["id"].isEmpty())
			result["toolId"] = fetchTool[1]["id"];

		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		/*LOG << QJsonDocument(body).toJson();
		if (data.value("barcodeTAG").toString() == "29600526") {
			LOG << "";
		}*/

		if (networkManager.makePostRequest(apiUrl + "/cribtrak/consumables", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% cribconsumables %' OR SQLString LIKE '% cribconsumables%') AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addCribKitsIfNotExists()
{
	LOG << "Adding CribTools to Webportal";
	QString targetKey = "kittools";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM kittools");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% kittools %' OR SQLString LIKE '% kittools%') AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kits from crib");
	std::string query =
		"SELECT * from kittools ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from kittools";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "kittools";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "kitBarcode", "toolId", "itemId", "barcodeTAG"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];
		
		std::vector fetchTool = queryManager.ExecuteTargetSql(("SELECT t.PartNo as itemId, ct.serialNo, t.id AS toolId FROM cribtools AS ct LEFT JOIN tools AS t ON ((t.PartNo IS NOT NULL OR t.PartNo <> '') AND t.PartNo = ct.itemId) OR ((t.serialNo IS NOT NULL OR t.serialNo <> '') AND t.serialNo = ct.serialNo) OR (t.id = ct.toolId) WHERE ct.barcodeTAG LIKE '" + result["kitBarcode"] + "'").toStdString());
		
		if (fetchTool.size() > 1) {
			if(!fetchTool[1]["itemId"].isEmpty())
				result["itemId"] = fetchTool[1]["itemId"];
			if(!fetchTool[1]["serialNo"].isEmpty() && result["serialNo"].isEmpty())
				result["serialNo"] = fetchTool[1]["serialNo"];
			if (!fetchTool[1]["toolId"].isEmpty())
				result["toolId"] = fetchTool[1]["toolId"];
		}


		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		LOG << QJsonDocument(body).toJson();

		if (networkManager.makePostRequest(apiUrl + "/cribtrak/kits", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE (SQLString  LIKE '% kittools %' OR SQLString LIKE '% kittools%') AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::createCribtrakTransactionsTable() {
	LOG << "Adding Cribtrak Transactions Table to Service SQLite Database";

	std::string tableName = "cribemployeeitemtransactions";
	std::string colQuery =
		"SHOW COLUMNS from " + tableName;
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::vector<stringmap> results;

	sqliteManager.ExecQuery(
		"PRAGMA table_info(" + tableName + ")",
		results
	);

	if (colQueryResults.size() <= 1 || !results.empty())
		return 0;

	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "cribId", "toolId", "itemId", "barcode", "trailId", "issuedBy", "returnBy", "transDate", "transTime", "transType"};

	int hadToolId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "toolId") {
			hadToolId = 1;
			column["Type"] = "INT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadToolId)
		columns.push_back("toolId INT NOT NULL DEFAULT 0");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	return 1;
}

/* TODO
 - upload cribtoollockers
*/

// PortaTRAK Syncs
int DatabaseManager::addPortasIfNotExists()
{
	LOG << "Adding Portas to Webportal";
	QString targetKey = "scales";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM itemscale");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	std::string trakDir;
	std::string iniFile;
	QString trakModelNumber;
	if (rowCheck.size() > 1 && rowCheck[1][rowCheck[1].firstKey()].toInt() < 1) {
		
		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
		std::string scaleId = ini.value("Customer/scaleID", trakIdNum).toString().toStdString();
		std::string description = ini.value("Unit/Description", "PT-" + trakIdNum.last(3)).toString().toStdString();
		std::string serialNo = ini.value("Unit/SerialNo", "PT-" + trakIdNum.last(3)).toString().toStdString();
		std::string cols = "custId";
		std::string vals = "'"+std::to_string(custId)+"'";
		if (!scaleId.empty()) {
			cols += ", scaleId";
			vals += ", '" + scaleId+"'";
		}
		if (!description.empty()) {
			cols += ", description";
			vals += ", '" + description + "'";
		}
		if (!serialNo.empty()) {
			cols += ", serialNumber";
				vals += ", '" + serialNo + "'";
		}
		if (!trakModelNumber.isEmpty()) {
			cols += ", modelNumber";
				vals += ", '" + trakModelNumber.toStdString() + "'";
		}
		queryManager.ExecuteTargetSql("INSERT INTO itemscale (" + cols + ") VALUES (" + vals + ")");

		databaseTablesChecked[targetKey]--;
	}
	else {
		rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	}


	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemscale%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}

	if (databaseTablesChecked[targetKey] < 0)
		databaseTablesChecked[targetKey]++;
	
	ServiceHelper().WriteToLog("Exporting PortaTRAKS from itemscale");
	std::string query =
		"SELECT * from itemscale ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() > 0) {
		rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		trakDir = buffer;
		size = 1024;

		rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		iniFile = buffer;
		//size = 1024;

		QSettings ini((trakDir + "\\" + iniFile).data(), QSettings::IniFormat, this);
		ini.sync();
		trakModelNumber = ini.value("Customer/ModelNumber", "").toString();
	}

	std::string colQuery =
		"SHOW COLUMNS from itemscale";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "itemscale";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "scaleId" };

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	QStringList ensureValForTargetCols = { "description", "serialNumber", "modelNumber" };

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QJsonObject data;
		std::map<std::string, std::string> toolData;
		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (ensureValForTargetCols.contains(key) && (val.isEmpty() || val == "''")) {
				if (key == "description") {
					val = "PT-" + trakIdNum.last(3);
				}
				if (key == "serialNumber") {
					val = "PT-" + trakIdNum.last(3);
				}
				if (key == "modelNumber" && !trakModelNumber.isEmpty()) {
					val = trakModelNumber;
				}
			}
			if (val.isEmpty() || val == "''")
				continue;
			data[key] = val;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/portatrak", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemscale%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addItemKitsIfNotExists()
{
	LOG << "Adding Kits to Webportal";
	QString targetKey = "itemkits";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM itemkits");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkits%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Itemkits from PortaTRAK");
	std::string query =
		"SELECT * from itemkits ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from itemkits";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "itemkits";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year"};
	QStringList uniqueIndexCols = { "custId", "scaleId", "kitTAG", "kitId"};

	int hadKitId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "kitId") {
			hadKitId = 1;
			column["Type"] = "TEXT";
		}

		if (dates.contains(column.value("Type").toLower()) || column.value("Field") == "userId")
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadKitId)
		columns.push_back("kitId TEXT NOT NULL DEFAULT ''");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];

		if (!result.contains("kitId") || result.value("kitId").isEmpty()) {
			result["kitId"] = QString("000").slice(QString::number(custId).length()) + QString::number(custId) + QString("000").slice(result["id"].length()) + result["id"];
		}

		if (!result.contains("userId") || result.value("userId").isEmpty() || result.value("userId") == "0") {
			std::string userIdQuery = "SELECT userId FROM portaemployeeitemtransactions WHERE kitTAG = '" + result.value("kitTAG").toStdString() + "' AND transType = '3' ORDER BY id DESC LIMIT 1";
			std::vector queryRes = queryManager.ExecuteTargetSql(userIdQuery);
			if (queryRes.size() > 1 && queryRes[1].contains("userId") && (!queryRes[1].value("userId").isEmpty() || queryRes[1].value("userId") != "0")) {
				result["userId"] = queryRes[1].value("userId");
			}
		}

		qDebug() << result;

		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/portatrak/kit", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% itemkits%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addKitCategoryIfNotExists()
{
	LOG << "Adding Kit Categories to Webportal";
	QString targetKey = "kitCategory";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM kitcategory");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% kitcategory%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kit Categories from PortaTRAK");
	std::string query =
		"SELECT * from kitcategory ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from kitcategory";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "kitcategory";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "scaleId", "categoryId"};
	
	int hadCategoryId = 0;
	int hadScaleId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "categoryId") {
			hadCategoryId = 1;
			column["Type"] = "INT";
		}

		if (column.value("Field") == "scaleId") {
			hadScaleId = 1;
			column["Type"] = "TEXT";
		}		

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadCategoryId)
		columns.push_back("categoryId INT NOT NULL DEFAULT 0");

	if(!hadScaleId)
		columns.push_back("scaleId TEXT NOT NULL DEFAULT ''");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;

		QStringMap res;
		res["id"] = result["id"];

		result["categoryId"] = result["id"];

		if (!result.contains("scaleId") || result.value("scaleId").isEmpty())
			result["scaleId"] = trakIdNum;
		
		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/portatrak/kit/category", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% kitcategory%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::addKitLocationIfNotExists()
{
	LOG << "Adding Kit Locations to Webportal";
	QString targetKey = "kitLocation";
	timeStamp = ServiceHelper().timestamp();
	std::vector rowCheck = queryManager.ExecuteTargetSql("SELECT COUNT(*) FROM kitlocation");
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal((targetKey + "Checked").toUtf8(), REG_SZ, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% kitlocation%' AND DatePosted < '" + timeStamp[0] + "' AND (posted = 0 OR posted = 2)");
		}
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kit Location from PortaTRAK");
	std::string query =
		"SELECT * from kitlocation ORDER BY id DESC LIMIT " +
		std::to_string(databaseTablesChecked[targetKey]) + ", " + std::to_string(queryLimit);
	std::vector sqlQueryResults = queryManager.ExecuteTargetSql(query);

	std::string colQuery =
		"SHOW COLUMNS from kitlocation";
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::string tableName = "kitlocation";
	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "scaleId", "locationId"};

	int hadLocationId = 0;
	int hadScaleId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "locationId") {
			hadLocationId = 1;
			column["Type"] = "INT";
		}

		if (column.value("Field") == "scaleId") {
			hadScaleId = 1;
			column["Type"] = "TEXT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if (!hadLocationId)
		columns.push_back("locationId INT NOT NULL DEFAULT 0");

	if (!hadScaleId)
		columns.push_back("scaleId TEXT NOT NULL DEFAULT ''");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	/*if (restManager == nullptr)
		restManager = new QRestAccessManager(netManager, this);*/

	if (networkManager.isInternetConnected())
		networkManager.authenticateSession();

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["locationId"] = result["id"];

		if (!result.contains("scaleId") || result.value("scaleId").isEmpty())
			result["scaleId"] = trakIdNum;

		QJsonObject data;
		std::map<std::string, std::string> toolData;

		for (auto it = result.cbegin(); it != result.cend(); ++it)
		{
			QString key = it.key();
			QString val = it.value().trimmed().simplified();
			if (val.isEmpty() || val == "''")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
			data[key] = val;
		}

#if true
		sqliteManager.AddEntry(
			tableName,
			toolData
		);
#endif

		toolData.clear();

		QJsonObject body;
		body["data"] = data;

		QJsonDocument reply;

		if (networkManager.makePostRequest(apiUrl + "/portatrak/kit/location", result, body, &reply)) {
			if (!reply.isObject()) {
				LOG << "Reply was not an Object";
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << reply.toJson().toStdString();
			QJsonObject result = reply.object();
			if (result["status"].toDouble() == 200) {
				databaseTablesChecked[targetKey]++;
			}
		}
		else {
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
		}

		rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));

	}

	rtManager.SetVal((targetKey + "Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//netManager->finished(NULL);

	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		if (rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size)) {
			size = 1024;
			rtManager.SetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, timeStamp[0].data(), timeStamp[0].size());
			rtManager.GetVal((targetKey + "CheckedDate").toUtf8(), REG_SZ, buffer, size);
		}
		std::string timestamp(buffer);
		queryManager.ExecuteTargetSql("UPDATE cloudupdate SET posted = 4 WHERE SQLString LIKE '% kitlocation%' AND DatePosted < '" + timestamp + "' AND (posted = 0 OR posted = 2)");
	}

	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	return 1;
}
int DatabaseManager::createPortatrakTransactionsTable() {
	LOG << "Adding Portatrak Transactions Table to Service SQLite Database";

	std::string tableName = "portaemployeeitemtransactions";
	std::string colQuery =
		"SHOW COLUMNS from " + tableName;
	std::vector colQueryResults = queryManager.ExecuteTargetSql(colQuery);

	qDebug() << colQueryResults;

	std::vector<stringmap> results;

	sqliteManager.ExecQuery(
		"PRAGMA table_info(" + tableName + ")",
		results
	);

	if (colQueryResults.size() <= 1 || !results.empty())
		return 0;

	std::vector<std::string> columns;
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt" };
	QStringList dates = { "date", "datetime", "time", "timestamp", "year" };
	QStringList uniqueIndexCols = { "custId", "scaleId", "kitTAG", "kitId", "userId", "transType", "trailId", "issuedBy", "returnBy", "transDate", "transTime"};

	int hadKitId = 0;

	for (auto& column : colQueryResults) {
		if (column.firstKey() == "success" || skipTargetCols.contains(column.value("Field")))
			continue;

		if (column.value("Field") == "kitId") {
			hadKitId = 1;
			column["Type"] = "TEXT";
		}

		if (dates.contains(column.value("Type").toLower()))
			column["Type"] = "TEXT";

		columns.push_back((column.value("Field") + " " + column.value("Type").toUpper() + " " +
			(uniqueIndexCols.contains(column.value("Field"))
				? "NOT NULL DEFAULT " + QString(column.value("Type").toUpper() == "INT" ? "0" : "''") + ""
				: (column.value("Null") == "NO" ? "NOT NULL DEFAULT " + (column.value("Default") == "" ? "''" : column.value("Default")) : "NULL" + (column.value("Default") == "" ? "" : " DEFAULT " + column.value("Default"))))).toStdString());
	}

	if(!hadKitId)
		columns.push_back("kitId TEXT NOT NULL DEFAULT ''");

	sqliteManager.CreateTable(
		tableName,
		columns
	);

	std::vector<stringmap> result;

	sqliteManager.ExecQuery(
		"CREATE UNIQUE INDEX IF NOT EXISTS unique_" + uniqueIndexCols.join("_").toStdString() + " ON " + tableName + "(" + uniqueIndexCols.join(",").toStdString() + ")",
		result
	);

	columns.clear();

	return 1;
}

int DatabaseManager::connectToRemoteDB()
{
	ServiceHelper().WriteToLog(std::string("Attempting to connect to Remote Database"));
	timeStamp = ServiceHelper().timestamp();
	QString targetSchema;
	QSqlDatabase db;
	bool result = false;

	try {

		if (!networkManager.isInternetConnected())
			throw HenchmanServiceException("Could not connect to internet to sync unit data");

		if(!networkManager.authenticateSession())
			throw HenchmanServiceException("Could not authenticate with server");

		RegistryManager::CRegistryManager rtManagerAddDB(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\"+ targetApp + "\\Database").c_str());

		TCHAR buffer[1024] = "\0";
		DWORD size = sizeof(buffer);
		rtManagerAddDB.GetVal("Schema", REG_SZ, (TCHAR*)buffer, size);
		targetSchema = buffer;

		LOG << "Checking if database has been previously defined";

		if (!QSqlDatabase::contains(targetSchema))
			throw HenchmanServiceException("Provided schema not valid");
	
		if (apiUrl.trimmed().isEmpty())
			throw HenchmanServiceException("No target Database Url provided");
		
		ServiceHelper().WriteToLog("Creating session to db " + targetSchema.toStdString());
		
		db = QSqlDatabase::database(targetSchema);

		if (!db.open())
			throw HenchmanServiceException("Failed to open DB Connection");

		requestRunning = true;		

		std::vector<QStringMap> queries;
		QSqlQuery query(db);

		try {

			QString queryText = testingDBManager && doNotRunCloudUpdate ? "SHOW TABLES" : "SELECT * FROM cloudupdate WHERE posted = 0 OR posted = 2 ORDER BY DatePosted ASC, id ASC LIMIT " + QString::number(queryLimit);
			LOG << queryText;
			//query.prepare(queryText);
			if (!query.exec(queryText))
			{
				//query.finish();
				ServiceHelper().WriteToLog(std::string("Closing DB Session"));
				throw HenchmanServiceException("Failed to exec query: " + query.executedQuery().toStdString());
			}

			if (query.numRowsAffected() > 0)
			{
				ServiceHelper().WriteToCustomLog("Starting network requests to: " + apiUrl.toStdString(), "queries");

				/*if (restManager == nullptr)
					restManager = new QRestAccessManager(netManager, this->parent());*/
			}


			int count = 0;

			while (query.next())
			{
				if (!networkManager.isInternetConnected() || !networkManager.authenticateSession())
					throw HenchmanServiceException("Internet connection dropped, retrying when connection returns");

				count++;
				bool skipQuery = false;
				bool retryingQuery = false;
				QStringMap res;
				res["number"] = QString::number(count);

				QString queryType;
				QJsonObject data;

				if (!parseCloudUpdate)
				{
					res["id"] = "0";
					res["query"] = "SHOW TABLES";
					goto parsedQuery;
				}

				res["id"] = query.value(0).toString();
				res["query"] = query
					.value(2)
					.toString()
					.replace(
						QRegularExpression(
							"(NOW|CURDATE|CURTIME)+",
							QRegularExpression::ExtendedPatternSyntaxOption
						),
						"\'" + query
						.value(3)
						.toString()
						.replace("T", " ")
						//".000Z"
						.replace(QRegularExpression(
							"\....Z",
							QRegularExpression::ExtendedPatternSyntaxOption
						), "") + "\'"
					).replace("()", "").simplified();
				retryingQuery = query.value(4).toInt() == 2;
				ServiceHelper().WriteToCustomLog("Query fetched from database: " + res["query"].toStdString(), "queries");

#if false
				if (res["query"].contains(";") && res["query"].split(";").size() > 2 && !res["query"].split(";")[1].isEmpty() && !skipQuery) {
					LOG << res["query"].split(";").size();
					res["query"] = res["query"].split(";")[0];
					LOG << res["query"];
					/*skipQuery = true;
					goto parsedQuery;*/
				}
#endif				
				// Check if query contains customer id for verification. Containing custId that is not assigned to trak renders query unsafe.

				LOG << res["query"];
				//if (res["query"].contains("insert", Qt::CaseInsensitive))
				if (res["query"].startsWith("i", Qt::CaseInsensitive))
				{
					queryType = "insert";
				}
				//else if (res["query"].contains("update", Qt::CaseInsensitive))
				else if (res["query"].startsWith("u", Qt::CaseInsensitive))
				{
					queryType = "update";
				}
				//else if (res["query"].contains("delete", Qt::CaseInsensitive))
				else if (res["query"].startsWith("d", Qt::CaseInsensitive))
				{
					queryType = "delete";
				}
				else
				{
					queryType = "select";
				}

				// Handle queries that aren't skipped and are inserting into the Database
				if (queryType == "insert" && !skipQuery)
				{
					ServiceHelper().WriteToLog("Parsing insert to prevent duplication creation");
					processInsertStatement(res["query"], data, skipQuery);
					LOG << res["query"];

					QJsonDocument doc(data);
					ServiceHelper().WriteToLog("Insert parse results: " + doc.toJson().toStdString());

					if (skipQuery)
						goto parsedQuery;
				}

				// handles queries that aren't skipped and are attempting to update existing enteries in the database
				if (queryType == "update" && !skipQuery)
				{
					ServiceHelper().WriteToLog("Parsing update to prevent altering entries not for current device");
					LOG << res["query"];
					processUpdateStatement(res["query"], data, skipQuery);
					LOG << res["query"];

					QJsonDocument doc(data);
					ServiceHelper().WriteToLog("Update parse results: " + doc.toJson().toStdString());

					if (data.value("table").toString() == "cribtrak/transactions")
					{
						queryType = "insert";
						data.remove("update");
						data.remove("where");
					}

					if (skipQuery)
						goto parsedQuery;
				}

				// handles queries that aren't skipped and are attempting to delete existing enteries in the database
				if (queryType == "delete" && !skipQuery)
				{
					ServiceHelper().WriteToLog("Parsing delete to prevent removing entries not for current device");
					LOG << res["query"];
					processDeleteStatement(res["query"], data, skipQuery);
					LOG << res["query"];

					QJsonDocument doc(data);
					ServiceHelper().WriteToLog("Delete parse results: " + doc.toJson().toStdString());

					if (skipQuery)
						goto parsedQuery;
				}
				


			parsedQuery:
				LOG << res["query"];
				qDebug() << data;
				ServiceHelper().WriteToCustomLog("Query parsed. " + std::string(skipQuery ? "Query is getting skipped" : "Query is being run"), "queries");
				ServiceHelper().WriteToCustomLog("Query after being parsed: \n" + QJsonDocument(data).toJson().toStdString(), "queries");

				if (!pushCloudUpdate || skipQuery)
				{
					std::string sqlQuery = "UPDATE cloudupdate SET posted = " + std::string(skipQuery ? (retryingQuery ? "3" : "2") : "1") + " WHERE posted <> 1 AND id = " + res["id"].toStdString();

					ServiceHelper().WriteToCustomLog("Updating skipped query with id: " + res["id"].toStdString() + " with posted status " + std::string(skipQuery ? (retryingQuery ? "3" : "2") : "1"), "queries");
					if (skipQuery && !retryingQuery)
						ServiceHelper().WriteToCustomLog("Skipping query to try again later:\n\tid: " + res["id"].toStdString() + "\n\t query: " + res["query"].toStdString(), "queries-skipped");
					std::vector queryResult = queryManager.ExecuteTargetSql(sqlQuery);
					if (queryResult.size() > 0)
						for (auto result : queryResult)
							LOG << result["success"];

					continue;
				}

				QString targetTable = "";
				if (data.contains("table"))
				{
					targetTable = data.value("table").toString();
					data.remove("table");
				}

				QStringList tablesNotToDebug = { "kabtrak", "kabtrak/tools", "kabtrak/drawers", "kabtrak/transactions", "users", "employees", "cribtrak/transactions", "portatrak/transaction", "cribtrak/tools", "tools", "cribtrak/consumables", "jobs" };

				qDebug() << targetTable;
				qDebug() << queryType;
				LOG << QJsonDocument(data).toJson();

				if (!tablesNotToDebug.contains(targetTable))
					int tempVal = 0;

				QJsonObject body;
				body["data"] = data;


				QJsonDocument reply;

				switch (query_types[queryType]) {
				case INSERT: {
					if (!networkManager.makePostRequest(apiUrl + "/" + targetTable, res, body, &reply))
					{
						LOG << "reply: " << reply.isEmpty();
						if (reply.isEmpty() || !reply.isObject())
							throw HenchmanServiceException("Failed to make post request");
						LOG << "request failed";
						QJsonObject response = reply.object();
						QString sqlQuery;
						if (response.contains("message") && response.value("message").toString() == "Route has not been defined")
						{
							sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(2) + " WHERE posted <> 1 AND id = " + res["id"];
							ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status 2");
						}
						else
						{
							sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(retryingQuery ? 3 : 2) + " WHERE posted <> 1 AND id = " + res["id"];
							ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status " + std::string(retryingQuery ? "3" : "2"));
						}
						std::vector queryResult = queryManager.ExecuteTargetSql(sqlQuery);
						if (queryResult.size() > 0)
							for (auto result : queryResult)
								LOG << result["success"];

						continue;
					}
					break;
				}
				case UPDATE: {
					if (!networkManager.makePatchRequest(apiUrl + "/" + targetTable, res, body, &reply))
					{
						LOG << "reply: " << reply.isEmpty();
						if (reply.isEmpty() || !reply.isObject())
							throw HenchmanServiceException("Failed to make patch request");
						LOG << "request failed";
						QJsonObject response = reply.object();
						QString sqlQuery;
						if (response.contains("message") && response.value("message").toString() == "Route has not been defined")
						{
							sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(2) + " WHERE posted <> 1 AND id = " + res["id"];
							ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status 2");
						}
						else
						{
							sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(retryingQuery ? 3 : 2) + " WHERE posted <> 1 AND id = " + res["id"];
							ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status " + std::string(retryingQuery ? "3" : "2"));
						}

						std::vector queryResult = queryManager.ExecuteTargetSql(sqlQuery);
						if (queryResult.size() > 0)
							for (auto result : queryResult)
								LOG << result["success"];

						continue;
					}
					break;
				}
				case REMOVE: {
					if (!networkManager.makeDeleteRequest(apiUrl + "/" + targetTable, res, body, &reply))
					{
						LOG << "reply: " << reply.isEmpty();
						if (reply.isEmpty() || !reply.isObject())
							throw HenchmanServiceException("Failed to make patch request");
						LOG << "request failed";
						QJsonObject response = reply.object();
						QString sqlQuery;
						if (response.contains("message") && response.value("message").toString() == "Route has not been defined")
						{
							sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(2) + " WHERE posted <> 1 AND id = " + res["id"];
							ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status 2");
						}
						else
						{
							sqlQuery = "UPDATE cloudupdate SET posted = " + QString::number(retryingQuery ? 3 : 2) + " WHERE posted <> 1 AND id = " + res["id"];
							ServiceHelper().WriteToLog("Updating query with id: " + res["id"].toStdString() + " to posted status " + std::string(retryingQuery ? "3" : "2"));
						}
						std::vector queryResult = queryManager.ExecuteTargetSql(sqlQuery);
						if (queryResult.size() > 0)
							for (auto result : queryResult)
								LOG << result["success"];
						continue;
					}
					break;
				}
				default: {
					if (!networkManager.makeNetworkRequest(apiUrl, res, &reply))
					{
						LOG << "request failed";
						LOG << "reply: " << reply.isEmpty();

						QString sqlQuery = "UPDATE cloudupdate SET posted = 3 WHERE posted <> 1 AND id = " + res["id"];

						ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString() + " to posted status 3", timeStamp[0] + "-queries");
						std::vector queryResult = queryManager.ExecuteTargetSql(sqlQuery);
						if (queryResult.size() > 0)
						{
							for (auto result : queryResult)
								LOG << result["success"];
						}
						continue;

					}
					break;
				}
				}

				QString sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted <> 1 AND id = " + res["id"];

				ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString() + " to posted status 1", timeStamp[0] + "-queries");
				std::vector queryResult = queryManager.ExecuteTargetSql(sqlQuery);
				if (queryResult.size() > 0)
				{
					for (auto result : queryResult)
						LOG << result["success"];
				}
			}

			if (query.numRowsAffected() > 0)
				ServiceHelper().WriteToCustomLog("Finished network requests", timeStamp[0] + "-queries");
		}
		catch (std::exception& e) {		
			ServiceHelper().WriteToError(e.what());
		}

		query.clear();
		query.finish();


		if (db.isOpen())
			db.close();


		result = true;

	}
	catch (std::exception& e)
	{
		if(db.isOpen())
			db.close();
		ServiceHelper().WriteToError(e.what());
	}

	/*if (restManager)
	{
		restManager->deleteLater();
		restManager = nullptr;
	}*/
	
	return result;
}

int DatabaseManager::connectToLocalDB()
{
	timeStamp = ServiceHelper().timestamp();
	QSqlDatabase db;

	LOG << "Test Log";
	try {
		//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
		RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").c_str());
		TCHAR buffer[1024] = "\0";
		DWORD size = 1024;
		if (databaseDriver == "") {

			//QString dbtype = RegistryManager::GetStrVal(hKey, "Database", REG_SZ).data();
			rtManager.GetVal("Database", REG_SZ, (TCHAR *)buffer, size);
			databaseDriver = buffer;
		}

		LOG << databaseDriver << " | " << databaseDriver.size();
		if (!QSqlDatabase::isDriverAvailable(databaseDriver))
		{
			//ServiceHelper().WriteToError((string)("Provided Database Driver is not available"));
			//RegCloseKey(hKey);
			/*ServiceHelper().WriteToError((string)("The following Databases are supported"));
			ServiceHelper().WriteToError(checkValidDrivers());
			return 0;*/
			// HenchmanServiceException
			throw HenchmanServiceException("Provided database driver is not available");
		}

		//QString schema = RegistryManager::GetStrVal(hKey, "Schema", REG_SZ).data();
		size = 1024;
		rtManager.GetVal("Schema", REG_SZ, (TCHAR*)buffer, size);
		QString schema(buffer);
		LOG << schema;

		if (!QSqlDatabase::contains(schema))
		{

			//QString server = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Server", REG_SZ));
			size = 1024;
			rtManager.GetVal("Server", REG_SZ, (TCHAR *)buffer, size);
			QString server(buffer);

			//int port = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Port", REG_SZ)).toInt();
			size = 1024;
			rtManager.GetVal("Port", REG_SZ, (TCHAR *)buffer, size);
			int port = QString(buffer).toInt();
			//int installDir(buffer);

			//QString user = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Username", REG_SZ));
			size = 1024;
			rtManager.GetVal("Username", REG_SZ, (char*)buffer, size);
			QString user(buffer);

			//string pass = RegistryManager::GetStrVal(hKey, "Password", REG_SZ);
			size = 1024;
			rtManager.GetVal("Password", REG_SZ, (char*)buffer, size);
			QString pass(buffer);

			ServiceHelper().WriteToLog((std::string)"Creating session to db");
					
			db = QSqlDatabase::addDatabase(databaseDriver, schema);
			db.setHostName(server);
			db.setPort(port);
			db.setUserName(user);
			if (!pass.isEmpty())
				db.setPassword(pass);
			db.setConnectOptions("CLIENT_COMPRESS;");
		}
		else {
			db = QSqlDatabase::database(schema);
		}
		
		//RegCloseKey(hKey);

		if (!db.open())
			throw HenchmanServiceException("DB Connection failed to open");

		ServiceHelper().WriteToLog((std::string)"DB Connection successfully opened");
		
		if (!db.driver()->hasFeature(QSqlDriver::Transactions))
			throw HenchmanServiceException("Selected Driver does not support transactions");

		db.transaction();
		QSqlQuery query(db);


		query.exec("SHOW DATABASES;");
		bool dbFound = false;
		while (query.next())
		{
			QString res = query.value(0).toString();
			LOG << res;
			if (res == schema) {
				dbFound = true;
				break;
			}
		}
		query.clear();

		if (!dbFound) {
			ServiceHelper().WriteToLog((std::string)"Generating Database");
			QString targetQuery = "CREATE DATABASE " + schema + " CHARACTER SET utf8 COLLATE utf8_general_ci";
			LOG << targetQuery;
			if (!query.exec(targetQuery)) {
				ServiceHelper().WriteToError((std::string)"Failed to create database");
			}
			else {
				ServiceHelper().WriteToLog((std::string)"Successfully created Database");
			}
		}

		query.clear();
		query.finish();

		if (!db.commit())
			db.rollback();

		db.close();

		db.setDatabaseName(schema);

	}
	catch (std::exception& e)
	{

		if (db.isOpen()) {
			db.rollback();
			db.close();
		}

		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}


void DatabaseManager::parseData(QNetworkReply *netReply)
{
	QRestReply restReply(netReply);
	std::optional json = restReply.readJson();
	std::optional <QJsonObject> response = json->object();
	std::string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit).toStdString();
	std::stringstream errorRes;
	std::stringstream dataRes;

	if (restReply.error() != QNetworkReply::NoError) {
		qWarning() << "A Network error has occured: " << restReply.error() << restReply.errorString();
		ServiceHelper().WriteToError("A Network error has occured: " + restReply.errorString().toStdString());
		goto exit;
	}
	if (!restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		qWarning() << "A HTTP error has occured: " << status << reason;
		ServiceHelper().WriteToError("An HTTP error has occured: " + std::to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	if (restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		ServiceHelper().WriteToLog("Request was successful : " + std::to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	ServiceHelper().WriteToLog((std::string)"Parsing Response");

	queryManager.ExecuteTargetSql(sqlQuery);

	if (!json) {
		ServiceHelper().WriteToError((std::string)"Recieved empty data or failed to parse JSON.");
		goto exit;
	}

	if (response.value()["error"].toArray().count() > 0) {
		for (const auto& result : response.value()["error"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				errorRes << " - " << result.toArray()[i].toString().toStdString() << std::endl;
			}
		}

		ServiceHelper().WriteToError("Server responded with error: " + errorRes.str());
	}

	if (response.value()["data"].toArray().count() > 0) {
		for (const auto& result : response.value()["data"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				dataRes << " - " << result.toArray()[i].toString().toStdString() << std::endl;
			}

		}
		ServiceHelper().WriteToLog("Server responded with data: " + dataRes.str());
	}

exit:
	json.reset();
	//performCleanup();

	requestRunning = false;

	return;
}

void DatabaseManager::processKeysAndValues(const QStringMap &map, QString (&results)[])
{
	QString queryKeys = "";
	QString queryValues = "";
	//QString conditionals = "";
	int count = map.size();
	
	for (auto& key : map.keys()) {
		count--;
		QString value = map.value(key);
		if (key == "id" || value.isEmpty() || value == "0" || value == "'0'")
			continue;
		if (QRegularExpression("\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d:\\d\\d:\\d\\d.\\d\\d\\dZ").match(value).hasMatch())
			value =
			"'" + QRegularExpression("\\d\\d\\d\\d-\\d\\d-\\d\\d").match(value).captured(0) +
			" " + 
			QRegularExpression("\\d\\d:\\d\\d:\\d\\d").match(value).captured(0) + "'";
		else
			value = "'" + value + "'";
		queryKeys.append((queryKeys.size() > 0 ? ", " : "") + ("`" + key + "`"));
		queryValues.append((queryValues.size() > 0 ? ", " : "") + value);
	}
	
	results[0] = queryKeys;
	results[1] = queryValues;
}

static QString parseTimeValue(const QString& time) {
	QStringList splitTime;

	if (time.contains(" "))
		splitTime = time.split(" ").last().split(":");
	else
		splitTime = time.split(":");

	QStringList secondsSplit = splitTime.last().split(".");
	int hours = splitTime.first().toInt();
	int minutes = splitTime.at(1).toInt();
	int seconds = secondsSplit.first().toInt();
	if (secondsSplit.length() > 1 && QString::number(secondsSplit.last().toInt()).slice(0, 1).toInt() >= 5) {
		seconds++;
	}
	if (seconds >= 60) {
		seconds = 0;
		minutes++;
	}
	if (minutes >= 60) {
		minutes = 0;
		hours++;
	}
	if (hours >= 24) {
		hours = 0;
	}
	splitTime.last() = (seconds < 10 ? "0" : "") + QString::number(seconds);
	splitTime[1] = (minutes < 10 ? "0" : "") + QString::number(minutes);
	splitTime.first() = (hours < 10 ? "0" : "") + QString::number(hours);
	return splitTime.join(":");
}

void DatabaseManager::processInsertStatement(QString& query, QJsonObject& data,  bool& skipQuery)
{
	
	QString targetQuery = query;
	
	QStringList splitQuery;
	splitQuery.append(targetQuery.slice(0, query.indexOf("(")).trimmed());
	splitQuery.append(query.slice(query.indexOf("(")).trimmed().split("VALUES", Qt::SkipEmptyParts, Qt::CaseInsensitive));

	bool hasCustId = 0;
	bool hasTrakId = 0;
	bool hasId = 0;
	const char* idColumn = "id";
	bool switchToConditions = 0;
	int idFromQuery = 0;

	QJsonArray cols;
	QJsonArray vals;

	QStringList insertColumns = splitQuery.at(1).trimmed().split(",", Qt::SkipEmptyParts);
	for (auto it = insertColumns.cbegin(); it != insertColumns.cend(); ++it) {
		QString targetCol = it->trimmed();
		if (targetCol.startsWith("("))
			targetCol.slice(1);
		if (targetCol.endsWith(")"))
			targetCol.slice(0, targetCol.length() - 1);

		cols.append(targetCol);
	}

	QStringList insertValues = splitQuery.at(2).trimmed().split(",", Qt::SkipEmptyParts);
	QString tempVal;
	for (auto it = insertValues.cbegin(); it != insertValues.cend(); ++it) {
		QString targetVal = it->trimmed();
		if (targetVal.startsWith("("))
			targetVal.slice(1);
		if (targetVal.endsWith(")"))
			targetVal.slice(0, targetVal.length() - 1);

		if (!tempVal.isEmpty()) {
			tempVal.append(",");
			//tempVal.append(targetVal);
			//continue;
		}

		targetVal = targetVal.trimmed();
		
		if (targetVal.startsWith("'") && !targetVal.endsWith("'")) {
			tempVal.append(targetVal.slice(1));
			continue;
		}

		if (targetVal.endsWith("'") && !targetVal.startsWith("'")) {
			tempVal.append(targetVal.slice(0, targetVal.length() - 1));
			vals.append(tempVal);
			tempVal = "";
			continue;
		}

		if (targetVal.startsWith("'") && targetVal.endsWith("'")) {
			targetVal.slice(1, targetVal.length() - 2);
		}

		if (targetVal.contains("NULL"))
			continue;

		if (!tempVal.isEmpty())
			tempVal.append(targetVal);
		else
			vals.append(targetVal);

	}

	QJsonObject entry;

	int limit = cols.count();
	if (vals.count() > limit)
		limit = vals.count();

	for (int i = 0; i < limit; i++) {
		QString key = cols.at(i).toString().trimmed();
		QString val = vals.at(i).toString().trimmed();
		if (val.isEmpty())
			continue;
		entry[key] = val;
	}

	if (entry.contains("custid")) {
		entry["custId"] = entry.value("custid");
		entry.remove("custid");
	}

	if(shouldIgnoreDatabaseCustId)
		entry["custId"] = custId;
	if(shouldIgnoreDatabaseTrakId)
		entry[trakId.data()] = trakIdNum;

	hasCustId = entry.contains("custId");
	hasTrakId = entry.contains(trakId.data());

	if (entry.keys().contains("id", Qt::CaseInsensitive) || entry.keys().contains("toolId", Qt::CaseInsensitive)) {
		hasId = 1;
		idColumn = entry.keys().contains("id", Qt::CaseInsensitive) ? "id" : "toolId";
	}

	if (entry.contains("transTime") && !entry.value("transTime").toString().isEmpty()) {
		entry["transTime"] = parseTimeValue(entry.value("transTime").toString());
	}
	if (entry.contains("outTime") && !entry.value("outTime").toString().isEmpty()) {
		entry["outTime"] = parseTimeValue(entry.value("outTime").toString());
	}
	if (entry.contains("inTime") && !entry.value("inTime").toString().isEmpty()) {
		entry["inTime"] = parseTimeValue(entry.value("inTime").toString());
	}

	QJsonDocument doc(entry);
	ServiceHelper().WriteToLog("Insert parse results: " + doc.toJson().toStdString());

	QStringList skipTargetCols = { "id", "createdAt", "updatedAt", "table"};

	QMap<QString, QString> map;
	QString columns;
	QString values;
	QString newQuery;
	std::string targetTable = splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed().toStdString();
	QString table = splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed();
	switch (table_map[table])
	{
	case tools:
	{
		if (!hasCustId)
			entry["custId"] = custId;
		if(hasId && idColumn == "id" && (!entry.contains("toolId") || entry.value("toolId").toString().isEmpty()))
			entry["toolId"] = entry.value("id").toString();


		break;
	}
	case users:
	{
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case employees:
	{
		if (!hasCustId)
			entry["custId"] = custId;

		break;
	}
	case jobs:
	{
		if (!hasCustId)
			entry["custId"] = custId;
		
		if (!entry.contains("trailId")) {

			QString jobQuery = "SELECT * FROM jobs WHERE ";
			QVariantMap keyValue;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty())
					continue;
				/*if (value.startsWith("'"))
					jobQuery.append(it.key() + " = " + value);
				else
					jobQuery.append(it.key() + " = '" + value + "'");*/
				jobQuery.append(it.key() + " = :" + it.key());
				keyValue.insert(it.key(), value);
				if ((it + 1) != entry.constEnd())
					jobQuery.append(" AND ");
			}

			std::vector fetchedJob = queryManager.ExecuteTargetSql(jobQuery, keyValue);

			if (fetchedJob.size() <= 1) {
				skipQuery = true;
				break;
			}

			for (auto it = fetchedJob[1].cbegin(); it != fetchedJob[1].cend(); ++it) {
				if (entry.contains(it.key()) || it.value().isEmpty())
					continue;
				entry[it.key()] = it.value();
			}
		}
		

		break;
	}
	case kabs:
	{
		entry["table"] = "kabtrak";
		if(!hasCustId)
			entry["custId"] = custId;
		if(!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case drawers:
	{
		entry["table"] = "kabtrak/drawers";
		if(!hasCustId)
			entry["custId"] = custId;
		if(!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case toolbins:
	{
		entry["table"] = "kabtrak/tools";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;
		
		if (!entry.contains("toolId") || entry.value("toolId").toString().isEmpty()) {
			QString kabToolQuery = "SELECT t.id as toolId FROM itemkabdrawerbins AS kt INNER JOIN tools AS t ON t.custId = kt.custId AND (kt.itemId LIKE t.PartNo OR kt.itemId LIKE t.stockcode) WHERE ";
			QStringList targetKeys = { "custId", "kabId", "drawerNum", "toolNumber", "itemId" };
			QStringList queryConditions;
			QVariantMap keyValue;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				//if (value.startsWith("'"))
				//	value = value.slice(1);
				//	//queryConditions.append("kt." + key + " = " + value);
				//if (value.endsWith("'"))
				//	value.chop(1);

				//else
				queryConditions.append("kt." + key + " = :"+key);
				keyValue[key] = value;
			}
			kabToolQuery.append(queryConditions.join(" AND "));
			//qDebug() << kabToolQuery;
			std::vector fetchedKabTool = queryManager.ExecuteTargetSql(kabToolQuery, keyValue);

			if (fetchedKabTool.size() <= 1) {
				skipQuery = true;
				break;
			}

			for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
				if (entry.contains(it.key()) || it.value().isEmpty())
					continue;
				entry[it.key()] = it.value();
			}
		}

		break;
	}
	case cribs: {

		entry["table"] = "cribtrak";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case cribconsumables:
	{
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("toolId") || entry.value("toolId").toString().isEmpty()) {
			QString toolQuery = "SELECT t.id as toolId FROM cribtools AS ct INNER JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo) WHERE ";
			QStringList targetKeys = { "custId", "cribId", "barcode"};
			QStringList queryConditions;
			QVariantMap keyValueMap;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (key == "barcode")
					key = "barcodeTAG";
				/*if (value.startsWith("'"))
					queryConditions.append("ct." + key + " = " + value);
				else
					queryConditions.append("ct." + key + " = '" + value + "'");*/
				queryConditions.append("ct." + key + " = :" + key);
				keyValueMap[key] = value;
			}
			toolQuery.append(queryConditions.join(" AND "));
			toolQuery.append(" ORDER BY barcodeTAG DESC LIMIT 1");
			//qDebug() << toolQuery;
			std::vector fetchedTool = queryManager.ExecuteTargetSql(toolQuery, keyValueMap);

			if (fetchedTool.size() <= 1) {
				skipQuery = true;
				break;
			}

			for (auto it = fetchedTool[1].cbegin(); it != fetchedTool[1].cend(); ++it) {
				if (entry.contains(it.key()) || it.value().isEmpty())
					continue;
				entry[it.key()] = it.value();
			}
		}

		entry["table"] = "cribtrak/consumables";

		break;
	}
	case cribtoollocation:
	{
		entry["table"] = "cribtrak/tools/locations";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("locationId") && entry.contains("id"))
			entry["locationId"] = entry.value("id");
		else if (!entry.contains("locationId") && !entry.contains("id")) {
			QVariantMap keyValue;
			keyValue["description"] = entry.value("description").toString();
			std::vector locationId = queryManager.ExecuteTargetSql("SELECT id FROM " + table + " WHERE description = :description", keyValue);
			entry["locationId"] = locationId.at(1).value("id");
		}

		break;
	}
	//case cribtoollockers:
	case cribtools:
	{
		entry["table"] = "cribtrak/tools";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM cribtools WHERE Key_name = 'PRIMARY'");
		QString indexingCol = colsCheck[1].value("Column_name");

		QString cribToolQuery = "SELECT ct."+ indexingCol +" AS id, t.id as toolId FROM cribtools AS ct INNER JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo) WHERE ";
		QStringList targetKeys = { "custId", "cribId", "barcodeTAG" };
		QStringList queryConditions;
		QVariantMap keyValueMap;
		for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
			QString value = it.value().toString();
			if (value.isEmpty() || !targetKeys.contains(it.key()))
				continue;
			QString key = it.key();
			/*if (value.startsWith("'"))
				queryConditions.append("ct." + key + " = " + value);
			else
				queryConditions.append("ct." + key + " = '" + value + "'");*/
			queryConditions.append("ct." + key + " = :" + key);
			keyValueMap[key] = value;
		}
		cribToolQuery.append(queryConditions.join(" AND "));
		//qDebug() << cribToolQuery;
		std::vector fetchedKabTool = queryManager.ExecuteTargetSql(cribToolQuery, keyValueMap);

		if (fetchedKabTool.size() <= 1) {
			skipQuery = true;
			break;
		}

		for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
			if (!targetKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
				continue;
			entry[it.key()] = it.value();
		}

		break;
	}
	case kittools: {
		
		entry["table"] = "cribtrak/kits";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case tooltransfer:
	{
		entry["table"] = "cribtrak/tools/transfer";
		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if(!entry.contains("transferId") && entry.contains("id"))
			entry["transferId"] = entry.value("id");
		else if (!entry.contains("transferId") && !entry.contains("id")) {
			QVariantMap keyValue;
			keyValue["barcodeTAG"] = entry.value("barcodeTAG").toString();
			std::vector locationId = queryManager.ExecuteTargetSql("SELECT id FROM tooltransfer WHERE barcodeTAG = :barcodeTAG", keyValue);
			if (locationId.size() <= 1) {
				skipQuery = true;
				break;
			}
			entry["transferId"] = locationId.at(1).value("id");
		}
		
		break;
	}
	case itemkits: {
		entry["table"] = "portatrak/kit";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("kitId") || entry.value("kitId").toString().isEmpty()) {

			QString kitQuery = "SELECT id as kitId FROM itemkits WHERE ";
			QStringList targetKeys = { "custId", "scaleId", "kitTAG" };
			QStringList queryConditions;
			QVariantMap keyValue;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				QString key = it.key();
				if (value.isEmpty() || !targetKeys.contains(key))
					continue;
				/*if (value.startsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");*/

				queryConditions.append(key + " = :" + key);
				keyValue[key] = value;
			}
			kitQuery.append(queryConditions.join(" AND "));
			//qDebug() << kitQuery;
			std::vector fetchedRes = queryManager.ExecuteTargetSql(kitQuery, keyValue);

			if (fetchedRes.size() <= 1) {
				skipQuery = true;
				break;
			}
			QStringList targerResponseKeys = { "kitId" };
			for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
				if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
					continue;
				if (it.key() == "kitId") {
					entry[it.key()] = QString("000").slice(QString::number(custId).length()) + QString::number(custId) + QString("000").slice(it.value().length()) + it.value();
				}
				else {
					entry[it.key()] = it.value();
				}
			}
		}
		
		break;
	}
	case kitcategory:
	{
		entry["table"] = "portatrak/kit/category";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("categoryId") || entry.value("categoryId").toString().isEmpty()) {

			QString categoryQuery = "SELECT id as categoryId FROM kitcategory WHERE ";
			QStringList targetKeys = { "custId", "description" };
			QStringList queryConditions;
			QVariantMap keyValue;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString().simplified();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				/*if (value.startsWith("'") && value.endsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");*/

				queryConditions.append(key + " = :" + key);
				keyValue[key] = value;
			}
			categoryQuery.append(queryConditions.join(" AND "));
			//qDebug() << categoryQuery;
			std::vector fetchedRes = queryManager.ExecuteTargetSql(categoryQuery, keyValue);

			if (fetchedRes.size() <= 1) {
				skipQuery = true;
				break;
			}
			QStringList targerResponseKeys = { "categoryId" };
			for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
				if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
					continue;
				entry[it.key()] = it.value();
			}
		}

		break;
	}
	case kitlocation:
	{
		entry["table"] = "portatrak/kit/location";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (!entry.contains("locationId") || entry.value("locationId").toString().isEmpty()) {

			QString locationQuery = "SELECT id as locationId FROM kitlocation WHERE ";
			QStringList targetKeys = { "custId", "scaleId", "description" };
			QStringList queryConditions;
			QVariantMap keyValue;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString().simplified();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				/*if (value.startsWith("'") && value.endsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");*/

				queryConditions.append(key + " = :" + key);
				keyValue[key] = value;
			}
			locationQuery.append(queryConditions.join(" AND "));
			//qDebug() << locationQuery;
			std::vector fetchedRes = queryManager.ExecuteTargetSql(locationQuery, keyValue);

			if (fetchedRes.size() <= 1) {
				skipQuery = true;
				break;
			}
			QStringList targerResponseKeys = { "locationId" };
			for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
				if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
					continue;
				entry[it.key()] = it.value();
			}
		}

		break;
	}
	case kabemployeeitemtransactions: {
		entry["table"] = "kabtrak/transactions";

		if (!entry.contains("itemId") || entry.value("itemId").toString().isEmpty()) {

			QString kabToolQuery = "SELECT itemId FROM itemkabdrawerbins WHERE ";
			QStringList targetKeys = { "custId", "kabId", "drawerNum", "toolNum", "itemId" };
			QStringList queryConditions;
			QVariantMap keyValue;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (key == "toolNum")
					key = "toolNumber";
				//if (value.startsWith("'"))
				//	//queryConditions[key] = value;
				//	queryConditions.append(key + " = " + value);
				//else
				//	//queryConditions[key] = "'"+value+"'";
				//	queryConditions.append(key + " = '" + value + "'");
				queryConditions.append(key + " = :" + key);
				keyValue[key] = value;
			}
			kabToolQuery.append(queryConditions.join(" AND "));
			kabToolQuery.append(" LIMIT 1");
			//qDebug() << kabToolQuery;
			std::vector fetchedKabTool = queryManager.ExecuteTargetSql(kabToolQuery, keyValue);

			if (fetchedKabTool.size() > 1) {
				for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
					if (!targetKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
						continue;
					entry[it.key()] = it.value();
				}
			}

		}

		if (!entry.contains("toolId") || !entry.value("itemId").toString().isEmpty()) {

			QString kabToolQuery = "SELECT id AS toolId FROM tools WHERE PartNo LIKE :itemId OR stockcode LIKE :itemId";
			//qDebug() << kabToolQuery;
			QVariantMap keyValue;
			keyValue["itemId"] = entry.value("itemId").toString();
			std::vector fetchedKabTool = queryManager.ExecuteTargetSql(kabToolQuery, keyValue);

			if (fetchedKabTool.size() > 1) {
				for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
					if ((entry.contains(it.key()) || it.value().isEmpty()))
						continue;
					entry[it.key()] = it.value();
				}
			}

		}

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		break;
	}
	case cribemployeeitemtransactions: {
		entry["table"] = "cribtrak/transactions";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if ((!entry.contains("toolId") || entry.value("toolId").toString().isEmpty()) && ((entry.contains("barcode") && !entry.value("barcode").toString().isEmpty()) || (entry.contains("itemId") && !entry.value("itemId").toString().isEmpty()))) {
			std::vector<std::string> conditions;
			if (entry.contains("barcode") && !entry.value("barcode").toString().isEmpty())
				conditions.push_back("barcodeTAG = '" + entry.value("barcode").toString().toStdString() + "'");
			if (entry.contains("itemId") && !entry.value("itemId").toString().isEmpty())
				conditions.push_back("itemId = '" + entry.value("itemId").toString().toStdString() + "'");
			
			std::vector<stringmap> returnedData = sqliteManager.GetEntry(
				"cribtools",
				{ "itemId", "serialNo", "toolId" },
				conditions
			);

			//qDebug() << returnedData;

			if (!returnedData.empty()) {
				QStringList transactionQueryList = { };
				QVariantMap keyValue;

				if (returnedData[0].contains("itemId")) {
					transactionQueryList.append(QString::fromStdString("PartNo LIKE :itemId"));
					keyValue["itemId"] = QString::fromStdString(returnedData[0].at("itemId"));
				}
				if (returnedData[0].contains("serialNo")) {
					transactionQueryList.append(QString::fromStdString("serialNo LIKE :serialNo"));
					keyValue["serialNo"] = QString::fromStdString(returnedData[0].at("serialNo"));
				}
				if (returnedData[0].contains("toolId")) {
					transactionQueryList.append(QString::fromStdString("id = :toolId"));
					keyValue["toolId"] = QString::fromStdString(returnedData[0].at("toolId"));
				}

				/*QString transactionQuery = "SELECT t.id as toolId FROM cribtools AS ct LEFT JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo OR ct.toolId = t.id) WHERE ct.barcodeTAG = '" + conditionPairs.value("barcode").toString() + "' LIMIT 1";*/
				QString transactionQuery = "SELECT id as toolId FROM tools WHERE " + transactionQueryList.join(" OR ") + " LIMIT 1";

				//QString kabToolQuery = "SELECT t.id as toolId FROM cribtools as ct INNER JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo OR ct.toolId = t.id) WHERE ";
				QStringList targetKeys = { "custId", "cribId", "barcode" };
				/*QStringList queryConditions;
				for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
					QString value = it.value().toString();
					if (value.isEmpty() || !targetKeys.contains(it.key()))
						continue;
					QString key = it.key();
					if (key == "barcode")
						key = "barcodeTAG";
					if (value.startsWith("'"))
						queryConditions.append("ct." + key + " = " + value);
					else
						queryConditions.append("ct." + key + " = '" + value + "'");
				}
				kabToolQuery.append(queryConditions.join(" AND "));
				kabToolQuery.append(" LIMIT 1");*/
				//qDebug() << transactionQuery;
				std::vector fetchedKabTool = queryManager.ExecuteTargetSql(transactionQuery, keyValue);

				if (fetchedKabTool.size() > 1) {
					for (auto it = fetchedKabTool[1].cbegin(); it != fetchedKabTool[1].cend(); ++it) {
						if (!targetKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
							continue;
						entry[it.key()] = it.value();
					}
				}
			}
		}

		/*doc.setObject(entry);
		ServiceHelper().WriteToLog("Insert parse results after checking toolId: " + doc.toJson().toStdString());*/

		if (
			(!entry.contains("itemId") || entry.value("itemId").toString().isEmpty()) && 
			(entry.contains("toolId") && !entry.value("toolId").toString().isEmpty())) {
			QString transactionQuery = "SELECT PartNo as itemId FROM tools WHERE id = :toolId'";

			//LOG << transactionQuery;
			QVariantMap keyValue;
			keyValue["toolId"] = entry.value("toolId").toString();
			std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					entry[key] = it.value();
				}
			}
		}

		/*doc.setObject(entry);
		ServiceHelper().WriteToLog("Insert parse results after checking itemId: " + doc.toJson().toStdString());*/

		if ((!entry.contains("issuedBy") || entry.value("issuedBy").toString().isEmpty()) && entry.contains("userId")) {
			entry["issuedBy"] = entry.value("userId");
		}

		//doc.setObject(entry);
		////doc.fromJson(entry.toVariantHash().t);
		//ServiceHelper().WriteToLog("Insert parse results after checking issuedBy: " + doc.toJson().toStdString());

		if ((!entry.contains("tailId") || entry.value("tailId").toString().isEmpty()) && entry.contains("trailId")) {
			std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");

			QString transactionQuery = "SELECT description as tailId FROM jobs WHERE " + indexingCol + " = :trailId";

			//LOG << transactionQuery;
			QVariantMap keyValue;
			keyValue["trailId"] = entry.value("trailId").toString();
			std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					entry[key] = it.value();
				}
			}

		}

		/*doc.setObject(entry);
		ServiceHelper().WriteToLog("Insert parse results after checking tailId and trailId: " + doc.toJson().toStdString());*/

		if ((!entry.contains("trailId") || entry.value("trailId").toString().isEmpty() || entry.value("trailId").toString() == "0") && (entry.contains("tailId") && !entry.value("tailId").toString().isEmpty())) {
			std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");

			QString transactionQuery = "SELECT " + indexingCol + " FROM jobs WHERE description LIKE :tailId";

			//LOG << transactionQuery;
			QVariantMap keyValue;
			keyValue["tailId"] = entry.value("tailId").toString();
			std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					entry[key] = it.value();
				}
			}
		}

		/*doc.setObject(entry);
		ServiceHelper().WriteToLog("Insert parse results after checking tailId: " + doc.toJson().toStdString());*/

		break;
	}
	case portaemployeeitemtransactions: {
		entry["table"] = "portatrak/transaction";

		if (!hasCustId)
			entry["custId"] = custId;
		if (!hasTrakId)
			entry[trakId.data()] = trakIdNum;

		if (
			(entry.contains("kitTAG") && !entry.value("kitTAG").toString().isEmpty()) 
			&&
			(!entry.contains("kitId") || entry.value("kitId").toString().isEmpty())
			) {
			QString kitQuery = "SELECT id as kitId FROM itemkits WHERE ";
			QStringList targetKeys = { "custId", "scaleId", "kitTAG" };
			QStringList queryConditions;
			QVariantMap keyValue;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				/*if (value.startsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");*/
				queryConditions.append(key + " = :" + key);
				keyValue[key] = value;
			}
			kitQuery.append(queryConditions.join(" AND "));
			//qDebug() << kitQuery;
			std::vector fetchedRes = queryManager.ExecuteTargetSql(kitQuery, keyValue);
			//qDebug() << fetchedRes;
			if (fetchedRes.size() > 1) {	
				QStringList targerResponseKeys = { "kitId" };
				for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
					if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
						continue;
					if (it.key() == "kitId") {
						entry[it.key()] = QString("000").slice(QString::number(custId).length()) + QString::number(custId) + QString("000").slice(it.value().length()) + it.value();
					}
					else {
						entry[it.key()] = it.value();
					}
				}
			}
		}

		if (
			(entry.contains("tailId") && !entry.value("tailId").toString().isEmpty())
			&&
			(!entry.contains("trailId") || entry.value("trailId").toString().isEmpty())
			) {
			std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");
			QString jobQuery = "SELECT "+ indexingCol +" as trailId FROM jobs WHERE ";
			QStringList targetKeys = { "custId", "tailId" };
			QStringList queryConditions;
			QVariantMap keyValue;
			for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (key == "tailId")
					key = "description";
				/*if (value.startsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");*/
				queryConditions.append(key + " = :" + key);
				keyValue[key] = value;
			}
			jobQuery.append(queryConditions.join(" AND "));
			//qDebug() << jobQuery;
			std::vector fetchedRes = queryManager.ExecuteTargetSql(jobQuery, keyValue);
			//qDebug() << fetchedRes;
			if (fetchedRes.size() > 1) {
				QStringList targerResponseKeys = { "trailId" };
				for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
					if (!targerResponseKeys.contains(it.key()) && (entry.contains(it.key()) || it.value().isEmpty()))
						continue;
				
					entry[it.key()] = it.value();
				}
			}
		}

		break;
	}
	//case lokkaemployeeitemtransactions:
	default:
		skipQuery = true;
		break;
	}

	if (skipQuery)
		return;

	if(entry.value("table").toString().isEmpty())
		entry["table"] = splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed();

	std::map<std::string, std::string> toolData;

	for (auto it = entry.constBegin(); it != entry.constEnd(); ++it)
	{
		QString key = it.key();
		QString val = it.value().toString().trimmed().simplified();
		if (val.isEmpty() || val == "''" || key == "table")
			continue;
		if (!skipTargetCols.contains(key))
			toolData[key.toStdString()] = val.toStdString();
	}

	//QList<const char*> tablesToSkip = { "kabemployeeitemtransactions", "cribemployeeitemtransactions", "portaemployeeitemtransactions" };
	//if (!tablesToSkip.contains(targetTable)) {
	//qDebug() << toolData;
	sqliteManager.AddEntry(
		splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed().toStdString(),
		toolData
	);
	//}

	toolData.clear();

	data.swap(entry);

	//LOG << query;
	return;
}

void DatabaseManager::processUpdateStatement(QString& query, QJsonObject& data, bool& skipQuery)
{
	QStringList splitQueryForParsing = ServiceHelper::ExplodeString(query, " ");
	//qDebug() << splitQueryForParsing;
	QStringList querySections = query.split(" SET ", Qt::SkipEmptyParts, Qt::CaseInsensitive);
	//QStringList querySections = ServiceHelper::ExplodeString(query, splitBy.data());
	//qDebug() << querySections;
	if (querySections.size() > 2) {
		QStringList tempQuerySections = querySections;
		tempQuerySections.removeAt(0);
		querySections[1] = tempQuerySections.join(" SET ");
		querySections.remove(2, querySections.size() - 2);
	}
	//qDebug() << querySections;

	querySections.append(querySections[1].split(" WHERE ", Qt::SkipEmptyParts, Qt::CaseInsensitive));
	querySections.removeAt(1);
	//qDebug() << querySections;
	if (querySections.size() > 3) {
		QStringList tempQuerySections = querySections;
		tempQuerySections.removeAt(2);
		querySections[2] = tempQuerySections.join(" WHERE ");
		querySections.remove(3, querySections.size() - 3);
	}
	//qDebug() << querySections;
	QStringList returnVal;
	//QMap<QString, QString> setPairs;
	QJsonObject setPairs;
	//QMap<QString, QString> conditionPairs;
	QJsonObject conditionPairs;
	QStringList parsedSets;
	QStringList parsedConditionals;
	bool switchToConditions = 0;
	bool hadCustId = 0;
	bool hadTrakId = 0;
	bool hadId = 0;
	int idFromQuery = 0;

	QStringList querySetSection = querySections[1].split("=", Qt::SkipEmptyParts);
	QString priorVal;
	for (auto it = querySetSection.cbegin(); it != querySetSection.cend(); ++it)
	{	
		QString setTrimmed = it->trimmed();
		if (priorVal.isEmpty()) {
			//qDebug() << "priorVal.isEmpty() " << setTrimmed;
			priorVal = setTrimmed;
			continue;
		}
		
		auto nextIt = it + 1;
		//qDebug() << nextIt << " : " << querySetSection.cend() << (nextIt == querySetSection.cend());
		if (nextIt != querySetSection.cend()) {
			QString currVal = setTrimmed;
			currVal = currVal.slice(0, setTrimmed.lastIndexOf(",")).trimmed();
			if (currVal.startsWith("'") && currVal.endsWith("'")) {
				currVal.slice(1, currVal.length() - 2);
			}
			//qDebug() << "currVal "<<currVal;
			if (currVal == "NULL") {
				skipQuery = true;
				break;
			}
			setPairs[priorVal] = currVal;
			QString nextVal = setTrimmed;
			nextVal = nextVal.slice(setTrimmed.lastIndexOf(",")+1).trimmed();
			//qDebug() << "nextVal " <<nextVal;
			priorVal = nextVal;

		}
		else {
			if (setTrimmed.startsWith("'") && setTrimmed.endsWith("'")) {
				setTrimmed.slice(1, setTrimmed.length() - 2);
			}
			//qDebug() << "setTrimmed " << setTrimmed;
			setPairs[priorVal] = setTrimmed;
		}

		
	}

	//ServiceHelper().WriteToLog(QJsonDocument(setPairs).toJson().toStdString());

	if (skipQuery)
		return;

	//// Parse conditionals
	QStringList queryConditionalSections = querySections[2].split("AND", Qt::SkipEmptyParts, Qt::CaseInsensitive);
	priorVal = "";
	for (auto it = queryConditionalSections.cbegin(); it != queryConditionalSections.cend(); ++it)
	{
		QString conditionTrimmed = it->trimmed();
		if (conditionTrimmed.startsWith("("))
			conditionTrimmed = conditionTrimmed.slice(1);
		if (conditionTrimmed.endsWith(")"))
			conditionTrimmed = conditionTrimmed.slice(0, conditionTrimmed.length() - 1);
		QStringList colAndVal;
		QStringList listOfSplittableOperators = { "<>", "<=", ">=", "<", ">", "!=", "=", " "};
		QString splitOpUsed;
		for (const auto& splitOperator : listOfSplittableOperators) {

			if (!conditionTrimmed.contains(splitOperator))
				continue;

			colAndVal = conditionTrimmed.split(splitOperator);
			splitOpUsed = splitOperator;
			break;

		}


		QString key = colAndVal.at(0).trimmed();
		QString val;
		if (colAndVal.length() > 2) {
			colAndVal.pop_front();
			val = colAndVal.join(splitOpUsed);
		}
		else {
			val = colAndVal.at(1);
		}
		val = val.trimmed();
		if (val.startsWith("'") && val.endsWith("'")) {
			val.slice(1, val.length() - 2);
		}

		qDebug() << key << ": " << val;
		if (val.contains("NULL"))
			continue;
		
		//conditionPairs.insert(key, val);
		conditionPairs[key] = val;
	}
	//ServiceHelper().WriteToLog(QJsonDocument(conditionPairs).toJson().toStdString());

	if (skipQuery)
		return;

	if (conditionPairs.contains("custid")) {
		conditionPairs["custId"] = conditionPairs.value("custid");
		conditionPairs.remove("custid");
	}
	
	if (shouldIgnoreDatabaseCustId)
		conditionPairs["custId"] = custId;
	if (shouldIgnoreDatabaseTrakId)
		conditionPairs[trakId.data()] = trakIdNum;

	hadCustId = conditionPairs.contains("custId");
	hadTrakId = conditionPairs.contains(trakId.data());

	if (conditionPairs.keys().contains("id", Qt::CaseInsensitive))
		hadId = 1;

	if (setPairs.contains("transTime") && !setPairs.value("transTime").toString().isEmpty()) {
		setPairs["transTime"] = parseTimeValue(setPairs.value("transTime").toString());
	}
	if (setPairs.contains("outTime") && !setPairs.value("outTime").toString().isEmpty()) {
		setPairs["outTime"] = parseTimeValue(setPairs.value("outTime").toString());
	}
	if (setPairs.contains("inTime") && !setPairs.value("inTime").toString().isEmpty()) {
		setPairs["inTime"] = parseTimeValue(setPairs.value("inTime").toString());
	}

	if (conditionPairs.contains("transTime") && !conditionPairs.value("transTime").toString().isEmpty()) {
		conditionPairs["transTime"] = parseTimeValue(conditionPairs.value("transTime").toString());
	}
	if (conditionPairs.contains("outTime") && !conditionPairs.value("outTime").toString().isEmpty()) {
		conditionPairs["outTime"] = parseTimeValue(conditionPairs.value("outTime").toString());
	}
	if (conditionPairs.contains("inTime") && !conditionPairs.value("inTime").toString().isEmpty()) {
		conditionPairs["inTime"] = parseTimeValue(conditionPairs.value("inTime").toString());
	}

	/*qDebug() << setPairs;
	qDebug() << conditionPairs;
	qDebug() << data;
	qDebug() << querySections[0].split(" ").at(1) << " | " << table_map[querySections[0].split(" ").at(1)];*/

	QString targetTable = querySections[0].split(" ").at(1);
	QStringList skipTargetCols = { "id", "createdAt", "updatedAt", "table" };

	switch (table_map[targetTable])
	{
	case tools:
	{

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (hadId && (!conditionPairs.contains("toolId") || conditionPairs.value("toolId").toString().isEmpty()))
			conditionPairs["toolId"] = conditionPairs.value("id").toString();

		if (!hadId && !conditionPairs.contains("toolId") && conditionPairs.contains("stockcode") && !conditionPairs.value("stockcode").toString().isEmpty()) {
			QVariantMap keyValue;
			keyValue["stockcode"] = conditionPairs.value("stockcode").toString();
			std::vector response = queryManager.ExecuteTargetSql("SELECT id as toolId FROM tools WHERE stockcode = :stockcode", keyValue);
			if (response.size() > 1)
				conditionPairs["toolId"] = response[1].value("toolId");
		}

		break;
	}
	case users:
	{

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case employees: {
		if (!hadCustId)
			conditionPairs["custId"] = custId;

		break;
	}
	case jobs:
	{
		if (!hadCustId)
			conditionPairs["custId"] = custId;

		break;
	}
	case customer: 
	{

		if (!hadId)
			conditionPairs["id"] = custId;
		break;
	}
	case kabs: {
		data["table"] = "kabtrak";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if(!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;


		break;
	}
	case drawers: {
		data["table"] = "kabtrak/drawers";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case toolbins: 
	{

		data["table"] = "kabtrak/tools";

		if (setPairs.contains("toolNumber")) {
			skipQuery = true;
			break;
		}

		QString itemDrawerQuery = "SELECT i.*, t.id as toolId FROM itemkabdrawerbins as i LEFT JOIN tools as t ON i.itemId LIKE t.PartNo OR i.itemId LIKE t.stockcode WHERE ";
		QVariantMap keyValue;
		for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
			QString value = it.value().toString();
			if (value.isEmpty())
				continue;
			/*if (value.startsWith("'"))
				itemDrawerQuery.append("i." + it.key() + " = " + value);
			else
				itemDrawerQuery.append("i." + it.key() + " = '" + value + "'");*/
			itemDrawerQuery.append("i." + it.key() + " = :" + it.key());
			keyValue[it.key()] = value;
			if ((it + 1) != conditionPairs.constEnd())
				itemDrawerQuery.append(" AND ");
		}
		ServiceHelper().WriteToLog("itemkabdrawerbins search query: " + itemDrawerQuery.toStdString());

		std::vector itemDrawerRes = queryManager.ExecuteTargetSql(itemDrawerQuery, keyValue);
		//qDebug() << itemDrawerRes;
		if (itemDrawerRes.size() > 1) {	
			QStringList allowedKeyList = { "drawerNum", "toolNumber", "itemId", "toolId" };
			for (auto it = itemDrawerRes[1].cbegin(); it != itemDrawerRes[1].cend(); ++it) {
				if (!allowedKeyList.contains(it.key()) || conditionPairs.contains(it.key()) || it.value().isEmpty())
					continue;

				qDebug() << it.key() << ": " << setPairs.value(it.key()).toString() << " | " << it.value();
				if (setPairs.contains(it.key()) && setPairs.value(it.key()).toString() == it.value()) {
					skipQuery = true;
					break;
				}
				conditionPairs[it.key()] = it.value();
			
			}
		}

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case cribs: 
	{
		data["table"] = "cribtrak";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case cribconsumables: {
		data["table"] = "cribtrak/consumables";

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	//case cribtoollocation:
	//case cribtoollockers:
	case cribtools: 
	{
		data["table"] = "cribtrak/tools";

		std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM cribtools WHERE Key_name = 'PRIMARY'");
		QString indexingCol = colsCheck[1].value("Column_name");

		if (indexingCol == "toolId" && conditionPairs.contains("toolId")) {
			QString targetQuery = "SELECT ct.itemId, ct.barcodeTAG, t.id as toolId FROM cribtools AS ct LEFT JOIN tools AS t ON ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo WHERE ct.toolId = :toolId";
			QVariantMap keyValue;
			keyValue["toolId"] = conditionPairs.value("toolId").toString();
			std::vector response = queryManager.ExecuteTargetSql(targetQuery, keyValue);
			if (response.size() > 1) {
				if(!conditionPairs.contains("barcodeTAG"))
					conditionPairs["barcodeTAG"] = response[1].value("barcodeTAG");
				if (!conditionPairs.contains("itemId"))
					conditionPairs["itemId"] = response[1].value("itemId");
				//if (!conditionPairs.contains("barcodeTAG"))
				conditionPairs["toolId"] = response[1].value("toolId");
			}
		}

		if (!conditionPairs.contains("barcodeTAG") && setPairs.contains("serialNo") && !setPairs.value("serialNo").toString().isEmpty()) {
			/*if (conditionPairs.contains("toolId"))
				conditionPairs.remove("toolId");*/
			QString targetQuery = "SELECT ct.barcodeTAG FROM cribtools AS ct WHERE ";
			QStringList conditionals;
			QVariantMap keyValue;
			for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString();
				if (value.isEmpty() || key == "toolId" || key == "currentcalibrationdate")
					continue;

				/*if (value.contains("NULL"))
					conditionals.append("ct." + key + " " + value);
				else if (value.startsWith("'"))
					conditionals.append("ct." + key + " = " + value);
				else
					conditionals.append("ct." + key + " = '" + value + "'");*/
				conditionals.append("ct." + key + "= :" + key);
				keyValue[key] = value;
				/*if ((it + 1) != conditionPairs.constEnd())
					targetQuery.append(" AND ");*/
			}

			for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString();
				if (setPairs.contains(key) || value.isEmpty() || key == "toolId" || key == "currentcalibrationdate")
					continue;

				/*if (value.contains("NULL"))
					conditionals.append("ct." + key + " " + value);
				else if (value.startsWith("'"))
					conditionals.append("ct." + key + " = " + value);
				else
					conditionals.append("ct." + key + " = '" + value + "'");*/
				conditionals.append("ct." + key + "= :" + key);
				keyValue[key] = value;
				/*if ((it + 1) != conditionPairs.constEnd())
					targetQuery.append(" AND ");*/
			}
			targetQuery.append(conditionals.join(" AND "));
			targetQuery += " ORDER BY ct.createdDate DESC LIMIT 1";

			std::vector response = queryManager.ExecuteTargetSql(targetQuery, keyValue);

			qDebug() << response;

			if (response.size() > 1) {
				conditionPairs["barcodeTAG"] = response[1].value("barcodeTAG");
			}
		}

		if ((indexingCol == "toolId" || !conditionPairs.contains("toolId")) && (conditionPairs.contains("barcodeTAG") && !conditionPairs.value("barcodeTAG").toString().isEmpty())) {
			/*if (conditionPairs.contains("toolId"))
				conditionPairs.remove("toolId");*/
			QString targetQuery = "SELECT t.id as toolId FROM cribtools AS ct LEFT JOIN tools AS t ON (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo)";
			
			if (indexingCol != "toolId")
				targetQuery += " AND ct.toolId = t.id";
			
			QStringList conditionals;
			QVariantMap keyValue;

			for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString();
				if (value.isEmpty() || key == "toolId" || key == "currentcalibrationdate")
					continue;

				/*if (value.contains("NULL"))
					conditionals.append("ct." + key + " " + value);
				else if (value.startsWith("'"))
					conditionals.append("ct." + key + " = " + value);
				else
					conditionals.append("ct." + key + " = '" + value + "'");*/
				conditionals.append("ct." + key + "= :" + key);
				keyValue[key] = value;
				/*if ((it + 1) != conditionPairs.constEnd())
					targetQuery.append(" AND ");*/
			}

			for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString();
				if (setPairs.contains(key) || value.isEmpty() || key == "toolId" || key == "currentcalibrationdate")
					continue;

				/*if(value.contains("NULL"))
					conditionals.append("ct." + key + " " + value);
				else if (value.startsWith("'"))
					conditionals.append("ct." + key + " = " + value);
				else
					conditionals.append("ct." + key + " = '" + value + "'");*/
				conditionals.append("ct." + key + "= :" + key);
				keyValue[key] = value;
				/*if ((it + 1) != conditionPairs.constEnd())
					targetQuery.append(" AND ");*/
			}

			if(conditionals.length() > 0)
				targetQuery.append(" WHERE ");

			targetQuery.append(conditionals.join(" AND "));
			targetQuery += " ORDER BY ct.createdDate DESC LIMIT 1";

			std::vector response = queryManager.ExecuteTargetSql(targetQuery, keyValue);

			//qDebug() << response;

			if (response.size() > 1) {
				conditionPairs["toolId"] = response[1].value("toolId");
			}
		}

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case kittools: {

		data["table"] = "cribtrak/kits";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case tooltransfer:
	{
		data["table"] = "cribtrak/tools/transfer";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case itemscale: {
		data["table"] = "portatrak";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case itemkits: {
		data["table"] = "portatrak/kit";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		break;
	}
	case kitcategory: {
		data["table"] = "portatrak/kit/category";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		if (conditionPairs.contains("categoryId") && !conditionPairs.value("categoryId").toString().isEmpty())
			break;

		QString categoryQuery = "SELECT id as categoryId FROM kitcategory WHERE ";
		QStringList targetKeys = { "category" };
		QStringList queryConditions;
		QVariantMap keyValue;
		for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
			QString value = it.value().toString();
			if (value.isEmpty() || !targetKeys.contains(it.key()))
				continue;
			QString key = it.key();
			if (key == "category")
				key = "id";
			/*if (value.startsWith("'"))
				queryConditions.append(key + " = " + value);
			else
				queryConditions.append(key + " = '" + value + "'");*/
			queryConditions.append(key + "= :" + key);
			keyValue[key] = value;
		}
		categoryQuery.append(queryConditions.join(" AND "));
		//qDebug() << categoryQuery;
		std::vector fetchedRes = queryManager.ExecuteTargetSql(categoryQuery, keyValue);

		if (fetchedRes.size() <= 1) {
			skipQuery = true;
			break;
		}
		QStringList targerResponseKeys = { "categoryId" };
		for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
			if (!targerResponseKeys.contains(it.key()) && (conditionPairs.contains(it.key()) || it.value().isEmpty()))
				continue;

			conditionPairs[it.key()] = it.value();
		}

		break;
	}
	case kitlocation: {
		data["table"] = "portatrak/kit/location";
		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;

		if (!conditionPairs.contains("locationId") || conditionPairs.value("locationId").toString().isEmpty()) {

			QString categoryQuery = "SELECT id as locationId FROM kitlocation WHERE ";
			QStringList targetKeys = { "location" };
			QStringList queryConditions;
			QVariantMap keyValue;
			for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty() || !targetKeys.contains(it.key()))
					continue;
				QString key = it.key();
				if (key == "location")
					key = "id";
				/*if (value.startsWith("'"))
					queryConditions.append(key + " = " + value);
				else
					queryConditions.append(key + " = '" + value + "'");*/
				queryConditions.append(key + "=:" + key);
				keyValue[key] = value;
			}
			categoryQuery.append(queryConditions.join(" AND "));
			//qDebug() << categoryQuery;
			std::vector fetchedRes = queryManager.ExecuteTargetSql(categoryQuery, keyValue);

			if (fetchedRes.size() <= 1) {
				skipQuery = true;
				break;
			}
			QStringList targerResponseKeys = { "locationId" };
			for (auto it = fetchedRes[1].cbegin(); it != fetchedRes[1].cend(); ++it) {
				if (!targerResponseKeys.contains(it.key()) && (conditionPairs.contains(it.key()) || it.value().isEmpty()))
					continue;

				conditionPairs[it.key()] = it.value();
			}
		}

		break;
	}
	//case tblcounterid:
	//case kabemployeeitemtransactions:
	case cribemployeeitemtransactions:
	{

		if (!hadCustId)
			conditionPairs["custId"] = custId;
		if (!hadTrakId)
			conditionPairs[trakId.data()] = trakIdNum;
		
		QString transactionQuery = "SELECT * FROM cribemployeeitemtransactions WHERE ";
		QVariantMap keyValue;
		if (hadId) {
			transactionQuery += "id = :id";
			keyValue["id"] = conditionPairs.value("id").toString();
		}
		else {

			QStringList conditionsList;

			for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty())
					continue;
				/*if (value.startsWith("'"))
					conditionsList.append(it.key() + " = " + value);
				else
					conditionsList.append(it.key() + " = '" + value + "'");*/
				conditionsList.append(it.key() + "= :" + it.key());
				keyValue[it.key()] = value;
				/*if ((it + 1) != setPairs.constEnd())
					transactionQuery.append(" AND ");*/
			}

			for (auto it = conditionPairs.constBegin(); it != conditionPairs.constEnd(); ++it) {
				/*if (it == conditionPairs.constBegin())
					transactionQuery.append(" AND ");*/
				QString value = it.value().toString();
				if (value.isEmpty() || setPairs.contains(it.key()))
					continue;
				/*if (value.startsWith("'"))
					conditionsList.append(it.key() + " = " + value);
				else
					conditionsList.append(it.key() + " = '" + value + "'");*/
				conditionsList.append(it.key() + "= :" + it.key());
				keyValue[it.key()] = value;
				/*if ((it + 1) != conditionPairs.constEnd())
					transactionQuery.append(" AND ");*/
			}
			transactionQuery.append(conditionsList.join(" AND "));
			transactionQuery.append(" ORDER BY id ASC LIMIT 1");
		}

		//LOG << transactionQuery;
		std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
		//qDebug() << conditionPairs;
		//qDebug() << queryRes;
		if (queryRes.size() > 1) {
			for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
				QString key = it.key();
				if (it.value().isEmpty())
					continue;
				//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
				if (key == "inDate") {
					if (queryRes[1].contains("transDate"))
						queryRes[1]["transDate"] = "";
						//queryRes[1].remove("transDate");
					conditionPairs["transDate"] = it.value();
				}
				if (key == "inTime") {
					if (queryRes[1].contains("transTime"))
						queryRes[1]["transTime"] = "";
					conditionPairs["transTime"] = it.value();
				}
				conditionPairs[key] = it.value();
			}
		} else if (hadId || setPairs.contains("barcode")) {
			skipQuery = true;
			break;
		}
		else {
			QString transactionQuery = "SELECT * FROM cribemployeeitemtransactions WHERE ";

			QStringList conditionsList;

			for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it) {
				QString value = it.value().toString();
				if (value.isEmpty())
					continue;
				/*if (value.startsWith("'"))
					conditionsList.append(it.key() + " = " + value);
				else
					conditionsList.append(it.key() + " = '" + value + "'");*/
				conditionsList.append(it.key() + "= :" + it.key());
				keyValue[it.key()] = value;
				/*if ((it + 1) != setPairs.constEnd())
					transactionQuery.append(" AND ");*/
			}

			transactionQuery.append(conditionsList.join(" AND "));
			transactionQuery.append(" ORDER BY id ASC LIMIT 1");

			//LOG << transactionQuery;
			std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
			//qDebug() << conditionPairs;
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
					if (key == "inDate") {
						if (queryRes[1].contains("transDate"))
							queryRes[1]["transDate"] = "";
							//queryRes[1].remove("transDate");
						conditionPairs["transDate"] = it.value();
					}
					if (key == "inTime") {
						if (queryRes[1].contains("transTime"))
							queryRes[1]["transTime"] = "";
						conditionPairs["transTime"] = it.value();
					}
					conditionPairs[key] = it.value();
				}
				//qDebug() << conditionPairs;
				//LOG << "Breakpoint";
			}
		}

		/*QStringList allowedKeyList = { "custId", "cribId", "toolId", "itemId", "barcode", "trailId", "tailId", "issuedBy", "returnBy", "transType", "inDate", "inTime" };*/
		

		if (conditionPairs.contains("transType") && conditionPairs.value("transType").toString() == "1") {
			if((!conditionPairs.contains("returnBy") || conditionPairs.value("returnBy").toString().isEmpty()) && conditionPairs.contains("userId")) {
				conditionPairs["returnBy"] = conditionPairs.value("userId");
			}
		}
		else {
			if ((!conditionPairs.contains("issuedBy") || conditionPairs.value("issuedBy").toString().isEmpty()) && conditionPairs.contains("userId")) {
				conditionPairs["issuedBy"] = conditionPairs.value("userId");
			}
		}

		if ((!conditionPairs.contains("toolId") || conditionPairs.value("toolId").toString().isEmpty()) && ((conditionPairs.contains("barcode") && !conditionPairs.value("barcode").toString().isEmpty()) || (conditionPairs.contains("itemId") && !conditionPairs.value("itemId").toString().isEmpty()))) {

			std::vector<std::string> conditions;
			if (conditionPairs.contains("barcode") && !conditionPairs.value("barcode").toString().isEmpty())
				conditions.push_back("barcodeTAG = '" + conditionPairs.value("barcode").toString().toStdString() + "'");
			if (conditionPairs.contains("itemId") && !conditionPairs.value("itemId").toString().isEmpty())
				conditions.push_back("itemId = '" + conditionPairs.value("itemId").toString().toStdString() + "'");

			std::vector<stringmap> returnedData = sqliteManager.GetEntry(
				"cribtools",
				{ "itemId", "serialNo", "toolId" },
				conditions
			);

			//qDebug() << returnedData;

			if (!returnedData.empty()) {
				QStringList transactionQueryList = { };

				if (returnedData[0].contains("itemId")) {
					transactionQueryList.append(QString::fromStdString("PartNo LIKE :itemId"));
					keyValue["itemId"] = QString::fromStdString(returnedData[0].at("itemId"));
				}
				if (returnedData[0].contains("serialNo")) {
					transactionQueryList.append(QString::fromStdString("serialNo LIKE :serialNo"));
					keyValue["serialNo"] = QString::fromStdString(returnedData[0].at("serialNo"));
				}
				if (returnedData[0].contains("toolId")) {
					transactionQueryList.append(QString::fromStdString("id = :toolId"));
					keyValue["toolId"] = QString::fromStdString(returnedData[0].at("toolId"));
				}

				QString transactionQuery = "SELECT id as toolId FROM tools WHERE " + transactionQueryList.join(" OR ") + " LIMIT 1";

				//LOG << transactionQuery;
				std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
				//qDebug() << queryRes;
				if (queryRes.size() > 1) {
					for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
						QString key = it.key();
						if (it.value().isEmpty())
							continue;
						//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
						conditionPairs[key] = it.value();
					}
				}
			}
		}

		if ((!conditionPairs.contains("itemId") || conditionPairs.value("itemId").toString().isEmpty()) && (conditionPairs.contains("barcode") && !conditionPairs.value("barcode").toString().isEmpty())) {

			std::vector<std::string> conditions;
			if (conditionPairs.contains("barcode") && !conditionPairs.value("barcode").toString().isEmpty())
				conditions.push_back("barcodeTAG = '" + conditionPairs.value("barcode").toString().toStdString() + "'");
			/*if (entry.contains("itemId") && !entry.value("itemId").toString().isEmpty())
				conditions.push_back("itemId = '" + entry.value("itemId").toString().toStdString() + "'");*/

			std::vector<stringmap> returnedData = sqliteManager.GetEntry(
				"cribtools",
				{ "itemId", "serialNo", "toolId" },
				conditions
			);

			//qDebug() << returnedData;

			if (!returnedData.empty()) {
				QStringList transactionQueryList = { };

				if (returnedData[0].contains("itemId")) {
					transactionQueryList.append(QString::fromStdString("PartNo LIKE :itemId"));
					keyValue["itemId"] = QString::fromStdString(returnedData[0].at("itemId"));
				}
				if (returnedData[0].contains("serialNo")) {
					transactionQueryList.append(QString::fromStdString("serialNo LIKE :serialNo"));
					keyValue["serialNo"] = QString::fromStdString(returnedData[0].at("serialNo"));
				}
				if (returnedData[0].contains("toolId")) {
					transactionQueryList.append(QString::fromStdString("id = :toolId"));
					keyValue["toolId"] = QString::fromStdString(returnedData[0].at("toolId"));
				}

				QString transactionQuery = "SELECT PartNo as itemId FROM tools WHERE " + transactionQueryList.join(" OR ") + " LIMIT 1";

				//LOG << transactionQuery;
				std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
				//qDebug() << queryRes;
				if (queryRes.size() > 1) {
					for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
						QString key = it.key();
						if (it.value().isEmpty())
							continue;
						//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
						conditionPairs[key] = it.value();
					}
				}
			}
		}

		/*
		if ((!conditionPairs.contains("itemId") || conditionPairs.value("itemId").toString().isEmpty()) && conditionPairs.contains("barcode")) {
			std::vector<stringmap> returnedData = sqliteManager.GetEntry(
				"cribtools",
				{ "itemId", "serialNo", "toolId" },
				{ "barcodeTAG = '" + conditionPairs.value("barcode").toString().toStdString() + "'" }
			);

			qDebug() << returnedData;
			
			// QString transactionQuery = "SELECT t.PartNo as itemId FROM cribtools AS ct LEFT JOIN tools AS t ON t.custId = ct.custId AND (ct.itemId LIKE t.PartNo OR ct.serialNo LIKE t.serialNo OR ct.toolId = t.id) WHERE ct.barcodeTAG = '" + conditionPairs.value("barcode").toString() + "' LIMIT 1";
			QString transactionQuery = QString::fromStdString("SELECT PartNo as itemId FROM tools WHERE PartNo LIKE '" + returnedData[0].at("itemId") + "' OR serialNo LIKE '" + returnedData[0].at("serialNo") + "' OR id = '" + returnedData[0].at("toolId") + "' LIMIT 1");

			LOG << transactionQuery;
			vector queryRes = queryManager.ExecuteTargetSql(transactionQuery);
			qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
					conditionPairs[key] = it.value();
				}
			}
		}
		*/

		if ((!conditionPairs.contains("tailId") || conditionPairs.value("tailId").toString().isEmpty()) && conditionPairs.contains("trailId")) {
			std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");

			QString transactionQuery = "SELECT description as tailId FROM jobs WHERE " + indexingCol + " = :trailId";

			//LOG << transactionQuery;
			QVariantMap keyValue;
			keyValue["trailId"] = conditionPairs.value("trailId").toString();
			std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
					conditionPairs[key] = it.value();
				}
			}
		}

		if ((!conditionPairs.contains("trailId") || conditionPairs.value("trailId").toString().isEmpty() || conditionPairs.value("trailId").toString() == "0") && conditionPairs.contains("tailId")) {
			std::vector colsCheck = queryManager.ExecuteTargetSql("SHOW KEYS FROM jobs WHERE Key_name = 'PRIMARY'");
			QString indexingCol = colsCheck[1].value("Column_name");

			QString transactionQuery = "SELECT " + indexingCol + " FROM jobs WHERE description LIKE :tailId";

			//LOG << transactionQuery;
			QVariantMap keyValue;
			keyValue["tailId"] = conditionPairs.value("tailId").toString();
			std::vector queryRes = queryManager.ExecuteTargetSql(transactionQuery, keyValue);
			//qDebug() << queryRes;
			if (queryRes.size() > 1) {
				for (auto it = queryRes[1].cbegin(); it != queryRes[1].cend(); ++it) {
					QString key = it.key();
					if (it.value().isEmpty())
						continue;
					//qDebug() << key << ": " << setPairs.value(key).toString() << " | " << it.value();
					conditionPairs[key] = it.value();
				}
			}
		}

		qDebug() << conditionPairs;

		if (!conditionPairs.contains("transDate")) {
			LOG << "breakpoint";
		}

		data.swap(conditionPairs);
		data["table"] = "cribtrak/transactions";

		break;
	}
	//case portaemployeeitemtransactions:
	//case lokkaemployeeitemtransactions:
	default:
		skipQuery = true;
		break;
	}

	if (skipQuery)
		return;

	if (data.value("table").toString().isEmpty())
		data["table"] = querySections[0].split(" ").at(1);

	QList<const char*> tablesToSkip = { "kabemployeeitemtransactions", "cribemployeeitemtransactions", "portaemployeeitemtransactions" };
	if (tablesToSkip.contains(targetTable)) {
		std::map<std::string, std::string> toolData;

		for (auto it = data.constBegin(); it != data.constEnd(); ++it)
		{
			QString key = it.key();
			QString val = it.value().toString().trimmed().simplified();
			if (val.isEmpty() || val == "''" || key == "table")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();
		}

		//QList<const char*> tablesToSkip = { "kabemployeeitemtransactions", "cribemployeeitemtransactions", "portaemployeeitemtransactions" };
		//if (!tablesToSkip.contains(targetTable)) {
		//qDebug() << toolData;
		sqliteManager.AddEntry(
			targetTable.toStdString(),
			toolData
		);

		return;
	}
	else {
		//std::vector<std::string> setVector;
		stringmap setMap;
		for (auto it = setPairs.constBegin(); it != setPairs.constEnd(); ++it)
		{
			std::string key = it.key().toStdString();
			std::string val = it.value().toString().trimmed().simplified().toStdString();

			if (val.empty() || val == "''" || key == "table")
				continue;

			if (val.starts_with("'")) {
				setMap[key] = val;
			}
			else {
				setMap[key] = "'" + val + "'";
			}
			//setVector.push_back(set.toStdString());
			/*if (val.isEmpty() || val == "''" || key == "table")
				continue;
			if (!skipTargetCols.contains(key))
				toolData[key.toStdString()] = val.toStdString();*/
		}

		//stringmap conditionMap;
		std::vector<std::string> conditions;
		for (auto it = queryConditionalSections.cbegin(); it != queryConditionalSections.cend(); ++it)
		{
			/*QString key = it.key();
			QString val = it.value().toString().trimmed().simplified();*/

			/*if (val.isEmpty() || val == "''" || key == "table")
				continue;*/
			/*if (!skipTargetCols.contains(key))
				conditionMap[key.toStdString()] = val.toStdString();*/
			conditions.push_back(it->trimmed().toStdString());
		}


		sqliteManager.UpdateEntry(
			targetTable.toStdString(),
			conditions,
			setMap
		);
	}

	/*sqliteManager.AddEntry(
		splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed().toStdString(),
		toolData
	);*/
	
	data["update"] = setPairs;
	data["where"] = conditionPairs;

	//qDebug() << data;

	query = returnVal.join(" ");
}

void DatabaseManager::processDeleteStatement(QString& query, QJsonObject& data, bool& skipQuery)
{
	
	QStringList splitQuery = query.split(" WHERE ", Qt::SkipEmptyParts, Qt::CaseInsensitive);
	//QStringList querySections = ServiceHelper::ExplodeString(query, splitBy.data());
	//qDebug() << splitQuery;
	if (splitQuery.size() > 2) {
		QStringList tempQuerySections = splitQuery;
		tempQuerySections.removeAt(0);
		splitQuery[1] = tempQuerySections.join(" WHERE ");
		splitQuery.remove(2, splitQuery.size() - 2);
	}
	//qDebug() << splitQuery;

	bool hasCustId = 0;
	bool hasTrakId = 0;
	bool hasId = 0;
	const char* idColumn = "id";

	QStringList queryConditionalSections = splitQuery[1].split("AND", Qt::SkipEmptyParts, Qt::CaseInsensitive);

	for (auto it = queryConditionalSections.cbegin(); it != queryConditionalSections.cend(); ++it)
	{
		QString conditionTrimmed = it->trimmed();
		QStringList colAndVal = conditionTrimmed.split(" ");

		QString key = colAndVal.at(0);
		QString val = conditionTrimmed.slice(colAndVal.at(0).length() + colAndVal.at(1).length() + 2);
		val = val.trimmed();
		if(key.startsWith("("))
			key.slice(1);
		if (val.endsWith(")"))
			val.slice(0, val.length() - 1);
		val = val.trimmed();
		if (val.startsWith("'"))
			val.slice(1);
		if (val.endsWith("'"))
			val.slice(0, val.length() - 1);
		val = val.trimmed();
		//qDebug() << key << ": " << val;
		//conditionPairs.insert(key, val);
		data[key] = val;
	}

	if (data.contains("custid")) {
		data["custId"] = data.value("custid");
		data.remove("custid");
	}

	if (shouldIgnoreDatabaseCustId)
		data["custId"] = custId;
	if (shouldIgnoreDatabaseTrakId)
		data[trakId.data()] = trakIdNum;

	hasCustId = data.contains("custId");
	hasTrakId = data.contains(trakId.data());

	if (data.keys().contains("id", Qt::CaseInsensitive) || data.keys().contains("toolId", Qt::CaseInsensitive)) {
		hasId = 1;
		idColumn = data.keys().contains("id", Qt::CaseInsensitive) ? "id" : "toolId";
	}

	switch (table_map[splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed()])
	{
	case tools:
	{
		if (!hasCustId)
			data["custId"] = custId;
		if (hasId && idColumn == "id" && (!data.contains("toolId") || data.value("toolId").toString().isEmpty()))
			data["toolId"] = data.value("id").toString();

		break;
	}
	case users:
	{

		if (!data.contains("userId")) {
			QString targetQuery = "SELECT userId FROM users WHERE ";
			QStringList conditionals;
			QVariantMap keyValue;
			for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString().trimmed();
				if (value.isEmpty())
					continue;
				conditionals.append(key + " = :" + key);
				keyValue[key] = value;
			}
			targetQuery.append(conditionals.join(" AND "));

			std::vector results = queryManager.ExecuteTargetSql(targetQuery, keyValue);
			if (results.size() > 1) {
				for (auto it = results[1].cbegin(); it != results[1].cend(); ++it) {
					QString key = it.key();
					QString value = it.value();
					if (value.isEmpty())
						continue;

					data[key] = value;
				}
			}
		}

		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;

		break;
	}
	case employees:
	{
		if (!data.contains("userId")) {
			QString targetQuery = "SELECT userId FROM employees WHERE ";
			QStringList conditionals;
			QVariantMap keyValue;
			for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
				QString key = it.key();
				QString value = it.value().toString().trimmed();
				if (value.isEmpty())
					continue;
				//conditionals.append(key + " = " + value);
				conditionals.append(key + " = :" + key);
				keyValue[key] = value;
			}
			targetQuery.append(conditionals.join(" AND "));

			std::vector results = queryManager.ExecuteTargetSql(targetQuery, keyValue);
			if (results.size() > 1) {
				for (auto it = results[1].cbegin(); it != results[1].cend(); ++it) {
					QString key = it.key();
					QString value = it.value();
					if (value.isEmpty())
						continue;

					data[key] = value;
				}
			}
		}

		if (!hasCustId)
			data["custId"] = custId;

		break;
	}
	case jobs:
	{
		if (!hasCustId)
			data["custId"] = custId;

		if (data.contains("trailId"))
			break;

		QString jobQuery = "SELECT * FROM jobs WHERE ";
		QVariantMap keyValue;
		for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
			QString value = it.value().toString();
			if (value.isEmpty())
				continue;
			/*if (value.startsWith("'"))
				jobQuery.append(it.key() + " = " + value);
			else
				jobQuery.append(it.key() + " = '" + value + "'");*/
			jobQuery.append(it.key() + " = :" + it.key());
			keyValue[it.key()] = value;

			if ((it + 1) != data.constEnd())
				jobQuery.append(" AND ");
		}
		//qDebug() << jobQuery;
		std::vector fetchedJob = queryManager.ExecuteTargetSql(jobQuery, keyValue);

		if (fetchedJob.size() <= 1) {
			skipQuery = true;
			break;
		}

		for (auto it = fetchedJob[1].cbegin(); it != fetchedJob[1].cend(); ++it) {
			if (data.contains(it.key()) || it.value().isEmpty())
				continue;
			data[it.key()] = it.value();
		}

		break;
	}
	case kabs:
	{
		data["table"] = "kabtrak";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		
		break;
	}
	case drawers:
	{
		data["table"] = "kabtrak/drawers";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		
		break;
	}
	case toolbins:
	{

		data["table"] = "kabtrak/tools";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;

		break;
	}
	//case cribs:
	//case cribconsumables:
	//case cribtoollocation:
	//case cribtoollockers:
	//case cribtools:
	//case kittools:
	//case tooltransfer:
	case itemkits: {
		data["table"] = "portatrak/kit";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		break;
	}
	case kitcategory: {
		data["table"] = "portatrak/kit/category";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		break;
	}
	case kitlocation: {
		data["table"] = "portatrak/kit/location";
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;
		break;
	}
	//case kabemployeeitemtransactions:
	//case cribemployeeitemtransactions:
	//case portaemployeeitemtransactions:
	//case lokkaemployeeitemtransactions:
	default:
		if (!hasCustId)
			data["custId"] = custId;
		if (!hasTrakId)
			data[trakId.data()] = trakIdNum;

		break;
	}

	if (skipQuery)
		return;

	if (data.value("table").toString().isEmpty())
		data["table"] = splitQuery.at(0).split(" ", Qt::SkipEmptyParts).last().trimmed();

	//qDebug() << data;
	return;
}

void DatabaseManager::performCleanup()
{
	/*if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}
	if (netManager) {
		netManager->deleteLater();
		netManager = nullptr;
	}
	if (sock) {
		sock->close();
		sock->deleteLater();
		sock = nullptr;
	}*/
}
