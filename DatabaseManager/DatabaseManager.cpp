
#include "DatabaseManager.h"

using namespace std;

static array<string, 2> timeStamp;

static bool doNotRunCloudUpdate = 0;
static bool parseCloudUpdate = 1;
static bool pushCloudUpdate = 1;

string getValidDrivers()
{
	stringstream results;
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
: QObject(parent)
{
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal("INSTALL_DIR", REG_SZ, (char*)buffer, size);
	std::string installDir(buffer);

	//size = 1024;
	//rtManager.GetVal("APP_NAME", REG_SZ, (TCHAR*)buffer, size);
	//trakType = buffer;

	//RegistryManager::CRegistryManager rtManagerCustomer(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer").data());
	//size = 1024;
	//rtManagerCustomer.GetVal("trakID", REG_SZ, (TCHAR*)buffer, size);
	////string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
	//std::string trakID(buffer);
	//size = 1024;
	//rtManagerCustomer.GetVal("ID", REG_SZ, (TCHAR*)buffer, size);
	////string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
	//std::string strCustId(buffer);
	//custId = std::stoi(strCustId);

	//size = 1024;
	//rtManagerCustomer.GetVal(trakId.data(), REG_SZ, (TCHAR*)buffer, size);
	////string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);
	//std::string idNum(buffer);
	//trakIdNum = std::stoi(idNum);

	//trakId.resize(trakId.size() - 2);
	//trakId.append("Id");
	
	//string installDir = RegistryManager::GetStrVal(hKey, "INSTALL_DIR", REG_SZ);

	QSettings ini(installDir.append("\\service.ini").data(), QSettings::IniFormat, this);
	ini.sync();
	testingDBManager = ini.value("DEVELOPMENT/testingDBManager", 0).toBool();
	ini.beginGroup("API");
	queryLimit = ini.value("NumberOfQueries", 10).toInt();
	apiUsername = ini.value("Username", "").toString();
	apiPassword = ini.value("Password", "").toString();
	ini.endGroup();
	ini.beginGroup("SYSTEM");
	databaseDriver = ini.value("databaseDriver", "").toString();
	ini.endGroup();
	if(testingDBManager)
		apiUrl = ini.value("DEVELOPMENT/URL", "http://localhost/webapi/public/api/portals/exec_query").toString();
	else
		apiUrl = ini.value("API/URL", "http://localhost/webapi/public/api/portals/exec_query").toString();

	LOG << apiUrl;

	LOG << "init db manager";
	
	targetApp = "";
	requestRunning = false;
	for (auto i = databaseTablesChecked.begin(), end= databaseTablesChecked.end(); i!=end; ++i)
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

}

DatabaseManager::~DatabaseManager() 
{
	LOG << "Deleting DatabaseManager";

	performCleanup();
}

void DatabaseManager::loadTrakDetailsFromRegistry()
{
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");

	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	rtManager.GetVal("APP_NAME", REG_SZ, (TCHAR*)buffer, size);
	trakType = buffer;

	RegistryManager::CRegistryManager rtManagerCustomer(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer").data());

	size = 1024;
	rtManagerCustomer.GetVal("trakID", REG_SZ, (TCHAR*)buffer, size);
	//string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
	trakId = buffer;

	size = 1024;
	rtManagerCustomer.GetVal("ID", REG_SZ, (TCHAR*)buffer, size);
	//string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
	std::string strCustId(buffer);
	custId = std::stoi(strCustId);

	size = 1024;
	rtManagerCustomer.GetVal(trakId.data(), REG_SZ, (TCHAR*)buffer, size);
	//string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);
	std::string idNum(buffer);
	trakIdNum = idNum.data();

	trakId.resize(trakId.size() - 2);
	trakId.append("Id");
}

bool DatabaseManager::isInternetConnected()
{
	QTcpSocket* sock = new QTcpSocket(this);
	sock->connectToHost("www.google.com", 80);
	bool connected = sock->waitForConnected(30000);//ms

	if (!connected)
	{
		sock->abort();
	}
	else {
		sock->close();

	}
	sock->deleteLater();
	sock = nullptr;
	return connected;
}

string DatabaseManager::parseData(QJsonArray array)
{
	stringstream dataRes;
	if (array.count() <= 0) {
		return "";
	}
	for (const auto& result : array) {
		string res = "";
		if (result.isString())
		{
			res = result.toString().toStdString();
		}
		if (result.isObject())
		{	
			res = parseData(result.toObject());
		}
		if (result.isArray())
		{
			res = parseData(result.toArray());
		}
		dataRes << " - " << res << endl;
		continue;
	}

	return dataRes.str();
}

string DatabaseManager::parseData(QJsonObject object)
{
	stringstream dataRes;

	if (object.keys().count() <= 0) {
		return "";
	}
	for (auto& key : object.keys()) {
		string res = "";
		if (object.value(key).isString())
		{
			res = object.value(key).toString().toStdString();
		}
		if (object.value(key).isObject())
		{
			res = "\n\t" + parseData(object.value(key).toObject());
		}
		if (object.value(key).isArray())
		{
			res = "\n\t" + parseData(object.value(key).toArray());
		}
		dataRes << " - " << key.toStdString() << ": " << res << endl;
		continue;
	}

	return dataRes.str();
}

int DatabaseManager::makeNetworkRequest(QString &url, QStringMap &query, QJsonDocument *results)
{
	int result = 0;
	QEventLoop loop(this);

	// Generate auth header for request.
	QString concatenated = apiUsername+":"+apiPassword;
	QByteArray credentials = concatenated.toLocal8Bit().toBase64();
	QString headerData = "Basic " + credentials;
	
	// Create network request object.
	QNetworkRequest request;
	request.setUrl(QUrl(url));
	request.setRawHeader("Authorization", headerData.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");
	
	QJsonObject data;
	data["sql"] = query["query"];
	QJsonDocument doc(data);

	ServiceHelper().WriteToCustomLog(
		"Making query to: " +url.toStdString() + 
		"\nRunning query number : " + query["number"].toStdString() + 
		"\nquery : " + doc.toJson().toStdString(), 
		timeStamp[0]+ "-queries");

	QNetworkReply* reply = restManager->post(request, doc, this, [this, &result, &results](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			if (reply.error() != QNetworkReply::NoError) {
				// HenchmanServiceException
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
				//ServiceHelper().WriteToError("A Network error has occured: " + reply.errorString().toStdString());
				//return;
			}

			int status = reply.httpStatus();
			QString reason = reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

			if (!reply.isHttpStatusSuccess()) {
				ServiceHelper().WriteToLog("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");
			}

			if (reply.isHttpStatusSuccess()) {
				ServiceHelper().WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");
			}
			ServiceHelper().WriteToLog((string)"Parsing Response");

			QByteArray jsonRes = reply.readBody();

			int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;

			optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
			if (!json) {
				ServiceHelper().WriteToLog((string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			string parsedVal;
			if (json->isArray()) {
				parsedVal = parseData(json->array());
			}
			else if (json->isObject()) {
				QJsonObject retVal = json->object();
				if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
					result = 1;
				parsedVal = parseData(json->object());
			}

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + parsedVal, timeStamp[0] + "-queries");
			if (results)
				json.value().swap(*results);
			
			if(!result)
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				LOG << "network request success";
			}
			else {
				LOG << "network request failed";
			}
		}
		catch (exception& e) {
			ServiceHelper().WriteToError(e.what());
			reply.networkReply()->close();
		}
		});
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	return result;
}

//int queryRemoteDatabase(string url, string query)
//{
//
//}

// Misc Syncs
int DatabaseManager::addToolsIfNotExists()
{
	QString targetKey = "tools";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM tools");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tools");
	string query = 
		"SELECT * from tools ORDER BY id DESC LIMIT " + 
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);
	
	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);
	
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	//TCHAR buffer[1024] = "\0";
	//DWORD size = sizeof(buffer);

	//size = 1024;
	//rtManager.GetVal("APP_NAME", REG_SZ, (TCHAR*)buffer, size);
	//std::string trakType(buffer);

	//RegistryManager::CRegistryManager rtManagerSrv(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer").data());
	//size = 1024;
	//rtManagerSrv.GetVal("ID", REG_SZ, (TCHAR*)buffer, size);
	////string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
	//std::string custId(buffer);

	//size = 1024;
	//rtManagerSrv.GetVal("trakID", REG_SZ, (TCHAR*)buffer, size);
	////string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
	//std::string trakId(buffer);

	//size = 1024;
	//rtManagerSrv.GetVal(trakId.data(), REG_SZ, (TCHAR*)buffer, size);
	////string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);
	//std::string idNum(buffer);

	//trakId.resize(trakId.size() - 2);
	//trakId.append("Id");

	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["toolId"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		

		//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));

		//QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		//rtManager.GetVal("TRAK_DIR", REG_SZ, (char*)buffer, size);
		//QString trakDir(buffer);

		//QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		//rtManager.GetVal("INI_FILE", REG_SZ, (char*)buffer, size);
		//QString iniFile(buffer);
		//RegCloseKey(hKey);


		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		res["query"] = "INSERT INTO tools (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM tools" +
			" WHERE custId=" + result.value("custId") +
			/*(!result.value("PartNo").isEmpty() 
				? " AND PartNo=" + result.value("PartNo") : "") +*/
			(!result.value("stockcode").isEmpty() ?
				" AND stockcode=" + result.value("stockcode") : "") +
			(!result.value("serialNo").isEmpty() ?
				" AND serialNo=" + result.value("serialNo") : "") +
			(!result.value("description").isEmpty() ?
				" AND description=" + result.value("description") : "") +
			(!result.value("toolId").isEmpty() ?
				" AND toolId=" + result.value("toolId") : "") +
			" ORDER BY id DESC LIMIT 1)" +
			" ON DUPLICATE KEY UPDATE ";
		for (auto& key : result.keys())
		{
			QString value = result.value(key).simplified();
			if (key == "id" || value.isEmpty() || value == "0" || value == "''")
				continue;
			if (value.startsWith("'")) {
				value.remove(0, 1);
			}
			if(value.endsWith("'"))
				value.remove(value.length()-1, 1);
			value.replace("'", "\'");
			res["query"] += key + "='" + value + "', ";
		}
		res["query"] = res["query"].trimmed();
		res["query"].removeLast();
		LOG << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				LOG << result["result"].toString().toStdString();
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
			continue;
		}

	}
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	//RegistryManager::SetVal(hKey, "numToolsChecked", databaseTablesChecked[targetKey], REG_DWORD);
	//RegCloseKey(hKey);
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//performCleanup();
	return 1;
}
int DatabaseManager::addUsersIfNotExists()
{
	QString targetKey = "users";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM users");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Users");
	string query =
		"SELECT * from users ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	//TCHAR buffer[1024] = "\0";
	//DWORD size = sizeof(buffer);

	//size = 1024;
	//rtManager.GetVal("APP_NAME", REG_SZ, (TCHAR*)buffer, size);
	//std::string trakType(buffer);

	//RegistryManager::CRegistryManager rtManagerSrv(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer").data());
	//size = 1024;
	//rtManagerSrv.GetVal("trakID", REG_SZ, (TCHAR*)buffer, size);
	////string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
	//std::string trakId(buffer);
	//size = 1024;
	//rtManagerSrv.GetVal("ID", REG_SZ, (TCHAR*)buffer, size);
	////string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
	//std::string custId(buffer);

	//size = 1024;
	//rtManagerSrv.GetVal(trakId.data(), REG_SZ, (TCHAR*)buffer, size);
	////string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);
	//std::string idNum(buffer);

	//trakId.resize(trakId.size() - 2);
	//trakId.append("Id");

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;

		QStringMap res;
		res["id"] = result["id"];
		result.remove("id");

		//qDebug() << result;

		if (trakId != "kabId")
			result["kabId"] = "";
		if (trakId != "cribId")
			result["cribId"] = "";
		if(trakId != "scaleId")
			result["scaleId"] = "";
		
		if (result.value(trakId.data()).isEmpty())
			result[trakId.data()] = trakIdNum;

		//qDebug() << result;


		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = 
			"INSERT INTO users (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS"+
			" (SELECT * FROM users WHERE" +
			" userId=" + result.value("userId") +
			" AND custId=" + result.value("custId") +
			(result.value(trakId.data()).isEmpty()
				? (" AND " + trakId + " = ").data() + trakIdNum
				: (" AND " + trakId +" = ").data() + result.value(trakId.data())) +
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);*/

		//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));

		//QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();

		//QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		//RegCloseKey(hKey);
		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			continue;
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numUsersChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}
int DatabaseManager::addEmployeesIfNotExists()
{
	QString targetKey = "employees";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM employees");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Employees");
	string query =
		"SELECT * from employees ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		//qDebug() << result;
		QStringMap res;
		res["id"] = result["id"];
		result.remove("id");

		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = 
			"INSERT INTO employees ("+
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM employees WHERE"+
			" userId=" + result.value("userId") +
			" AND custId=" + result.value("custId") +
			" ORDER BY id DESC LIMIT 1)";

		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);*/

		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			LOG << "entry does not belong to customer";
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			continue;
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numEmployeesChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}
int DatabaseManager::addJobsIfNotExists()
{
	QString targetKey = "jobs";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM jobs");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Jobs");
	string query =
		"SELECT * from jobs ORDER BY trailId ASC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];
		result.remove("id");

		QString results[2];

		processKeysAndValues(result, results);

		res["query"] =
			"INSERT INTO jobs (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM jobs WHERE" +
			" trailId=" + result.value("trailId") +
			" AND description=" + result.value("description") +
			" AND custId=" + result.value("custId") +
			" ORDER BY id DESC LIMIT 1)";

		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);*/
		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			continue;
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numJobsChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

// KabTRAK Syncs
int DatabaseManager::addKabsIfNotExists()
{
	QString targetKey = "kabs";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabs");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kabs");
	string query =
		"SELECT * from itemkabs ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);
		
		res["query"] = "INSERT INTO itemkabs (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabs WHERE " +
			"custId="+result.value("custId")+
			" AND kabId="+result.value("kabId")+
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);*/

		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
			continue;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numKabsChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//performCleanup();
	return 1;
}
int DatabaseManager::addDrawersIfNotExists()
{
	QString targetKey = "kabDrawers";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabdrawers");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kab Drawers");
	string query =
		"SELECT * from itemkabdrawers ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = "INSERT INTO itemkabdrawers (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawers WHERE" +
			" custId="+result.value("custId")+
			" AND kabId="+result.value("kabId") +
			" AND drawerCode="+result.value("drawerCode")+
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);*/

		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
			continue;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numDrawersChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}
int DatabaseManager::addToolsInDrawersIfNotExists()
{
	QString targetKey = "kabDrawerBins";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabdrawerbins");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kab tools in bins");
	string query =
		"SELECT * from itemkabdrawerbins ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto & result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = "INSERT INTO itemkabdrawerbins (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawerbins WHERE " +
			"custId="+ result.value("custId") +
			"AND kabId="+ result.value("kabId") +
			"AND toolNumber=" + result.value("toolNumber")+
			"AND drawerNum=" + result.value("drawerNum") +
			"AND itemId=" + result.value("itemId") +
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);*/

		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			continue;
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numToolsInDrawersChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

// CribTRAK Syncs
int DatabaseManager::addCribsIfNotExists()
{
	QString targetKey = "cribs";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM cribs");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting CribTRAKS from crib");
	string query =
		"SELECT * from cribs ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();
		QString cribId = ini.value("Customer/cribID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'" || result.value("cribId") != "'" + QString(trakIdNum) + "'")
			continue;

		res["query"] = "INSERT INTO cribs (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM cribs " +
			"WHERE custId=" + result.value("custId") +
			"AND cribId=" + result.value("cribId") +
			"ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			continue;
		}

	}
	
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}
int DatabaseManager::addCribToolLocationIfNotExists()
{
	QString targetKey = "toolLocation";
	timeStamp = ServiceHelper().timestamp();
	vector tableCheck = ExecuteTargetSql("show tables like 'cribtoollocation'");
	//qDebug() << tableCheck;
	QString cribtoollocationTable;
	if (tableCheck.size() > 1) {
		cribtoollocationTable = "cribtoollocation";
	}
	else {
		cribtoollocationTable = "cribtoollocations";
	}
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM " + cribtoollocationTable.toStdString());
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tool Location from CribTRAK");
	string query =
		"SELECT * from "+ cribtoollocationTable.toStdString() +" ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["locationId"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);

		QSettings ini(trakDir.append("\\").append(iniFile), QSettings::IniFormat, this);*/
		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();
		QString cribId = ini.value("Customer/cribID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'" || result.value("cribId") != "'" + QString(trakIdNum) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		res["query"] = "INSERT INTO cribtoollocation (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (" +
			"SELECT * FROM cribtoollocation " +
			"WHERE custId=" + result.value("custId") +
			(!result.value("cribId").isEmpty()
				? "AND cribId=" + result.value("cribId")
				: "") +
			(!result.value("locationId").isEmpty()
				? "AND locationId=" + result.value("locationId")
				: "") +
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			else {
				LOG << "No rows were altered on db";
				databaseTablesChecked[targetKey]++;
			}
			
		}

	}
	
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}
int DatabaseManager::addCribToolsIfNotExists()
{
	QString targetKey = "cribtools";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM cribtools");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tools from crib");
	string query =
		"SELECT * from cribtools ORDER BY toolId DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto & result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		if (!result.contains("id")) {
			res["id"] = result["toolId"];
		}
		else {
			res["id"] = result["id"];
			//result["toolId"] = result["id"];
		}
		vector fetchTool = ExecuteTargetSql(("SELECT id FROM tools WHERE ((PartNo IS NOT NULL OR PartNo <> '') AND PartNo = '" + result["itemId"] + "') OR ((stockcode IS NOT NULL OR stockcode <> '') AND stockcode = '"+result["itemId"] + "') OR (id = '"+result["toolId"] + "') GROUP BY description;").toStdString());
		result["toolId"] = fetchTool[1]["id"];
		
		if (result.contains("nextcalibrationdate")) {
			result["currentcalibrationdate"] = result["nextcalibrationdate"];
			result.remove("nextcalibrationdate");
		}
		QString results[2];

		processKeysAndValues(result, results);		

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);*/

		//QSettings ini(trakDir.append("\\").append(iniFile), QSettings::IniFormat, this);

		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();
		QString cribId = ini.value("Customer/cribID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'" || result.value("cribId") != "'" + QString(trakIdNum) + "'") {
			continue;
		}

		res["query"] = "INSERT INTO cribtools (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM cribtools" +
			" WHERE custId=" + result.value("custId") +
			" AND cribId=" + result.value("cribId") +
			(!result.value("itemId").isEmpty()
				? " AND itemId=" + result.value("itemId")
				: "") +
			(!result.value("barcodeTAG").isEmpty()
				? " AND barcodeTAG=" + result.value("barcodeTAG")
				: "") +
			(!result.value("serialNo").isEmpty()
				? " AND serialNo=" + result.value("serialNo")
				: "") +
			(!result.value("toolId").isEmpty()
				? " AND toolId=" + result.value("toolId")
				: "") +
			" ORDER BY toolId DESC LIMIT 1)";
		LOG << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
			}
			else {
				LOG << "No rows were altered on db";
				databaseTablesChecked[targetKey]++;
			}

			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			/*continue;*/
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numCribTools", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}
int DatabaseManager::addCribToolTransferIfNotExists()
{
	QString targetKey = "tooltransfer";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM tooltransfer");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Tool Transfers from crib");
	string query =
		"SELECT * from tooltransfer ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	//rtManager.GetVal("APP_NAME", REG_SZ, (TCHAR*)buffer, size);
	//std::string trakType(buffer);

	//RegistryManager::CRegistryManager rtManagerCustomer(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer").data());

	//size = 1024;
	//rtManagerCustomer.GetVal("ID", REG_SZ, (TCHAR*)buffer, size);
	////string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
	//std::string custId(buffer);
	//
	//size = 1024;
	//rtManagerCustomer.GetVal("trakID", REG_SZ, (TCHAR*)buffer, size);
	////string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
	//std::string trakIdType(buffer);

	//size = 1024;
	//rtManagerCustomer.GetVal(trakIdType.data(), REG_SZ, (TCHAR*)buffer, size);
	////string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);
	//std::string trakId(buffer);

	/*std::array trakNini(GetTrakDirAndIni(rtManagerCustomer));

	QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
	QString custId = ini.value("Customer/ID", 0).toString();
	QString trakIdType = ini.value("Customer/trakID", 0).toString();
	QString trakId = ini.value("Customer/" + trakIdType, 0).toString();*/
	
	//trakIdType.resize(trakId.size() - 2);
	//trakIdType.append("Id");

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);*/

		//QSettings ini(trakDir.append("\\").append(iniFile), QSettings::IniFormat, this);

		if (result.value("custId") != "'" + QString::number(custId) + "'" || result.value(trakId.data()) != "'" + QString(trakIdNum) + "'") {
			continue;
		}

		res["query"] = "INSERT INTO tooltransfer (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM tooltransfer" +
			" WHERE custId=" + result.value("custId") +
			" AND cribId = " + result.value("cribId") +
			" AND barcodeTAG=" + result.value("barcodeTAG") +
			(!result.value("userId").isEmpty() ? " AND userId=" + result.value("userId") : "") +
			(!result.value("transfer_userId").isEmpty() ? " AND transfer_userId=" + result.value("transfer_userId") : "") +
			(!result.value("tailId").isEmpty() ? " AND tailId=" + result.value("tailId") : "") +
			(!result.value("transfer_tailId").isEmpty() ? " AND transfer_tailId=" + result.value("transfer_tailId") : "") +
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
			}
			else {
				LOG << "No rows were altered on db";
				databaseTablesChecked[targetKey]++;
			}

			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			/*continue;*/
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numCribTools", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

/* TODO
 - upload cribconsumables
 - upload cribtoollockers
 - upload kittools
*/

// PortaTRAK Syncs
int DatabaseManager::addItemKitsIfNotExists()
{
	QString targetKey = "itemkits";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkits");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Itemkits from PortaTRAK");
	string query =
		"SELECT * from itemkits ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success" || !result.contains("custId"))
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = "INSERT INTO itemkits (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS ("+
			"SELECT * FROM itemkits "+
			"WHERE scaleId=" + result.value("scaleId") +
			"AND custId=" + result.value("custId") +
			"AND kitTAG=" + result.value("kitTAG") +
			"AND serial=" + result.value("serial") +
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);

		QSettings ini(trakDir.append("\\").append(iniFile), QSettings::IniFormat, this);*/
		/*std::array trakNini(GetTrakDirAndIni(rtManager));

		QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numItemKits", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}
int DatabaseManager::addKitCategoryIfNotExists()
{
	QString targetKey = "kitCategory";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM kitcategory");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kit Categories from PortaTRAK");
	string query =
		"SELECT * from kitcategory ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;

		QStringMap res;
		res["id"] = result["id"];

		result["categoryId"] = result["id"];
		
		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = "INSERT INTO kitcategory (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (" +
			"SELECT * FROM kitcategory " +
			"WHERE custId=" + result.value("custId") +
			"AND categoryId=" + result.value("categoryId") +
			"AND description=" + result.value("description") +
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);

		QSettings ini(trakDir.append("\\").append(iniFile), QSettings::IniFormat, this);*/
		//std::array trakNini(GetTrakDirAndIni(rtManager));

		//QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		//QString custId = ini.value("Customer/ID", 0).toString();

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numKitCategory", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}
int DatabaseManager::addKitLocationIfNotExists()
{
	QString targetKey = "kitLocation";
	timeStamp = ServiceHelper().timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM kitlocation");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= databaseTablesChecked[targetKey])
	{
		return 0;
	}
	ServiceHelper().WriteToLog("Exporting Kit Location from PortaTRAK");
	string query =
		"SELECT * from kitlocation ORDER BY id DESC LIMIT " +
		to_string(databaseTablesChecked[targetKey]) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, "SOFTWARE\\HenchmanTRAK\\HenchmanService");
	/*TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);*/

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		result["locationId"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = "INSERT INTO kitlocation (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (" +
			"SELECT * FROM kitlocation " +
			"WHERE custId=" + result.value("custId") +
			(!result.value("scaleId").isEmpty() 
				? "AND scaleId=" + result.value("scaleId") 
				: "") +
			(!result.value("locationId").isEmpty()
				? "AND locationId=" + result.value("locationId")
				: "") +
			" ORDER BY id DESC LIMIT 1)";
		LOG << res["query"];

		/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
		QString trakDir = RegistryManager::GetStrVal(hKey, "TRAK_DIR", REG_SZ).data();
		QString iniFile = RegistryManager::GetStrVal(hKey, "INI_FILE", REG_SZ).data();
		RegCloseKey(hKey);

		QSettings ini(trakDir.append("\\").append(iniFile), QSettings::IniFormat, this);*/
		//std::array trakNini(GetTrakDirAndIni(rtManager));

		/*QSettings ini(trakNini[0].append("\\").append(trakNini[1]), QSettings::IniFormat, this);
		QString custId = ini.value("Customer/ID", 0).toString();*/

		if (result.value("custId") != "'" + QString::number(custId) + "'") {
			databaseTablesChecked[targetKey]++;
			continue;
		}

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			LOG << result["result"].toString().toStdString();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			LOG << "No rows were altered on db";
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
		}

	}
	/*HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numKitLocation", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);*/
	rtManager.SetVal(targetKey.append("Checked").toUtf8(), REG_DWORD, (DWORD*)&databaseTablesChecked[targetKey], sizeof(DWORD));
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}


int DatabaseManager::connectToRemoteDB()
{
	ServiceHelper().WriteToLog(string("Attempting to connect to Remote Database"));
	timeStamp = ServiceHelper().timestamp();
	QString targetSchema;
	QSqlDatabase db;
	bool result = false;
	try {
		//HKEY hKeyLocal = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
		RegistryManager::CRegistryManager rtManagerAddDB(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\"+ targetApp + "\\Database").c_str());

		/*targetSchema = QString::fromStdString(RegistryManager::GetStrVal(hKeyLocal, "Schema", REG_SZ));
		RegCloseKey(hKeyLocal);*/

		TCHAR buffer[1024] = "\0";
		DWORD size = sizeof(buffer);
		rtManagerAddDB.GetVal("Schema", REG_SZ, (char*)buffer, size);
		targetSchema = buffer;

		LOG << "Checking if database has been previously defined";
		// HenchmanServiceException
		if (!QSqlDatabase::contains(targetSchema))
			throw HenchmanServiceException("Provided schema not valid");
	
		if (apiUrl.trimmed().isEmpty())
			throw HenchmanServiceException("No target Database Url provided");
		
		ServiceHelper().WriteToLog("Creating session to db " + targetSchema.toStdString());
		
		db = QSqlDatabase::database(targetSchema);
		// HenchmanServiceException
		if (!db.open())
			throw HenchmanServiceException("Failed to open DB Connection");

		requestRunning = true;

		performCleanup();

		netManager = new QNetworkAccessManager(this);
		if (!testingDBManager)
			netManager->setStrictTransportSecurityEnabled(true);
		netManager->setAutoDeleteReplies(true);
		netManager->setTransferTimeout(30000);
		connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

		restManager = new QRestAccessManager(netManager, this);
		

		vector<QStringMap> queries;
		QSqlQuery query(db);
		QString queryText = testingDBManager && doNotRunCloudUpdate ? "SHOW TABLES" : "SELECT * FROM cloudupdate WHERE posted = 0 OR posted = 2 ORDER BY posted ASC, DatePosted ASC, id ASC LIMIT " + QString::number(queryLimit);
		LOG << queryText;
		query.prepare(queryText);
		if (!query.exec())
		{
			query.finish();
			ServiceHelper().WriteToLog(string("Closing DB Session"));
			throw HenchmanServiceException("Failed to exec query: " + query.executedQuery().toStdString());
		}
		
		if(query.numRowsAffected() > 0)
			ServiceHelper().WriteToCustomLog("Starting network requests to: " + apiUrl.toStdString(), timeStamp[0] + "-queries");

		int count = 0;

		//RegistryManager::CRegistryManager rtManagerMainSrv(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\HenchmanService").data());
		//size = 1024;
		//rtManagerMainSrv.GetVal("APP_NAME", REG_SZ, (TCHAR *)buffer, size);
		//std::string trakType(buffer);

		//RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer").data());
		//size = 1024;
		//rtManager.GetVal("trakID", REG_SZ, (TCHAR *)buffer, size);
		////string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
		//std::string trakId(buffer);
		//size = 1024;
		//rtManager.GetVal("ID", REG_SZ, (TCHAR *)buffer, size);
		////string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
		//std::string custId(buffer);

		//size = 1024;
		//rtManager.GetVal(trakId.data(), REG_SZ, (TCHAR *)buffer, size);
		////string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);
		//std::string idNum(buffer);


		//while (testingDBManager ? count < 5 : query.next())
		while (query.next())
		{
			count++;
			bool skipQuery = false;
			bool retryingQuery = false;
			QStringMap res;
			res["number"] = QString::number(count);
			
			if (parseCloudUpdate) {
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
						),"") + "\'"
					).replace("()", "").simplified();
				retryingQuery = query.value(4).toInt() == 2;
				ServiceHelper().WriteToCustomLog("Query fetched from database: " + res["query"].toStdString(), timeStamp[0] + "-queries");

				//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
				//string trakType = RegistryManager::GetStrVal(hKey, "APP_NAME", REG_SZ);
				//RegCloseKey(hKey);
				/*hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer"));
				string trakId = RegistryManager::GetStrVal(hKey, "trakID", REG_SZ);
				string custId = RegistryManager::GetStrVal(hKey, "ID", REG_SZ);
				string idNum = RegistryManager::GetStrVal(hKey, trakId.data(), REG_SZ);*/
				//RegCloseKey(hKey);
				if (res["query"].contains(";") && res["query"].split(";").size() > 2 && !res["query"].split(";")[1].isEmpty() && !skipQuery) {
					LOG << res["query"].split(";").size();
					res["query"] = res["query"].split(";")[0];
					LOG << res["query"];
					/*skipQuery = true;
					goto parsedQuery;*/
				}
				// Check if query contains customer id for verification. Containing custId that is not assigned to trak renders query unsafe.
				if (res["query"].contains("custId =", Qt::CaseInsensitive) && !skipQuery && false)
				{
					ServiceHelper().WriteToLog("Checking custId is same as device settings");
					int index = res["query"].indexOf("custId", Qt::CaseInsensitive);
					QString substr = res["query"].mid(index, res["query"].size() - index);
					int startpoint = substr.indexOf("=")+1;
					int endpoint = substr.indexOf("and", Qt::CaseInsensitive)-1;
					QString substr2 = ServiceHelper::ExplodeString(substr.mid(startpoint, endpoint-startpoint).trimmed(), " ")[0];
					LOG << substr2 << " | " << custId;
					if (substr2.toInt() != custId) {
						skipQuery = true;
						goto parsedQuery;
					}
				}
				
				// Check if query contains a owner TRAK id. Containing trak Id that is not assigned to trak renders query unsafe.
				if (res["query"].contains(QString(trakId.data()) + " =", Qt::CaseInsensitive) && !skipQuery && false)
				{
					ServiceHelper().WriteToLog("Checking trakId is same as device settings");
					int index = res["query"].indexOf(trakId.data(), Qt::CaseInsensitive);
					QString substr = res["query"].mid(index, res["query"].size() - index);
					int startpoint = substr.indexOf("=")+1;
					int endpoint = substr.indexOf("and", Qt::CaseInsensitive)-1;
					QString substr2 = ServiceHelper::ExplodeString(substr.mid(startpoint, endpoint - startpoint).trimmed(), " ")[0];
					LOG << substr2 << " | " << trakIdNum;
					if (!substr2.contains(trakIdNum)) {
						skipQuery = true;
						goto parsedQuery;
					}
					//res["query"].replace(substr2, QString(trakId.data()) + " = '" + QString(idNum.data()) + "'");
				}
				vector<std::string> splitQuery = ServiceHelper::ExplodeString(res["query"].toStdString(), " ");
				//LOG << splitQuery;
				
				// Handle queries that aren't skipped and are inserting into the Database
				if (QString::fromStdString(splitQuery[0]).contains("insert", Qt::CaseInsensitive) && !skipQuery) {
					ServiceHelper().WriteToLog("Parsing insert to prevent duplication creation");
					LOG << splitQuery;
					processInsertStatement(res["query"], skipQuery);
					LOG << res["query"];
					if(skipQuery)
						goto parsedQuery;
				}
				
				// handles queries that aren't skipped and are attempting to update existing enteries in the database
				if (QString::fromStdString(splitQuery[0]).contains("update", Qt::CaseInsensitive) && !skipQuery) {
					ServiceHelper().WriteToLog("Parsing update to prevent altering entries not for current device");
					LOG << splitQuery;
					processUpdateStatement(res["query"], skipQuery);
					LOG << res["query"];
					if (skipQuery)
						goto parsedQuery;
				}
			}
			else {
				res["id"] = "0";
				res["query"] = "SHOW TABLES";
			}


		parsedQuery:
			LOG << res["query"];
			ServiceHelper().WriteToCustomLog("Query parsed. " + string(skipQuery ? "Query is getting skipped" : "Query is being run"), timeStamp[0] + "-queries");
			ServiceHelper().WriteToCustomLog("Query after being parsed: \n" + res["query"].toStdString(), timeStamp[0] + "-queries");
			
			if (!pushCloudUpdate || skipQuery) {
				string sqlQuery = "UPDATE cloudupdate SET posted = " + string(skipQuery ? (retryingQuery ? "3" : "2") : "1") + " WHERE posted <> 1 AND id = " + res["id"].toStdString();

				ServiceHelper().WriteToCustomLog("Updating skipped query with id: " + res["id"].toStdString(), timeStamp[0] + "-queries");
				if (skipQuery && !retryingQuery)
					ServiceHelper().WriteToCustomLog("Skipping query to try again later:\n\tid: " + res["id"].toStdString() + "\n\t query: " + res["query"].toStdString(), timeStamp[0] + "-queries-skipped");
				vector queryResult = ExecuteTargetSql(sqlQuery);
				if (queryResult.size() > 0) {
					for (auto result : queryResult)
						LOG << result["success"];
				}
				continue;
			}

			//QStringMap queryMap;
			//queryMap.insert("start", "SET autocommit=0;START TRANSACTION;");
			//queryMap.insert("query", res["query"]);
			//queryMap.insert("commit", "COMMIT;SET autocommit = 1;");
			//queryMap.insert("rollback", "ROLLBACK;SET autocommit = 1;");
			//res["query"] = queryMap.value("start");
			////makeNetworkRequest(apiUrl, res);
			//res["query"] = queryMap.value("query");
			
			QJsonDocument reply;
			
			if (!makeNetworkRequest(apiUrl, res, &reply))
			{	
				LOG << "request failed";
				/*res["query"] = queryMap.value("rollback");
				makeNetworkRequest(apiUrl, res);*/
				QString sqlQuery = "UPDATE cloudupdate SET posted = 3 WHERE posted <> 1 AND id = " + res["id"];

				ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString(), timeStamp[0] + "-queries");
				vector queryResult = ExecuteTargetSql(sqlQuery);
				if (queryResult.size() > 0) {
					for (auto result : queryResult)
						LOG << result["success"];
				}
				continue;
				
			}

			if (!reply.isObject()) {
				LOG << "returned value not object";
				/*res["query"] = queryMap.value("rollback");
				makeNetworkRequest(apiUrl, res);*/
				QString sqlQuery = "UPDATE cloudupdate SET posted = 3 WHERE posted <> 1 AND id = " + res["id"];

				ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString(), timeStamp[0] + "-queries");
				vector queryResult = ExecuteTargetSql(sqlQuery);
				if (queryResult.size() > 0) {
					for (auto result : queryResult)
						LOG << result["success"];
				}
				continue;
			}

			QJsonObject result = reply.object();
			if (!result.contains("result")) {
				LOG << "returned value does not contain a result key";
				/*res["query"] = queryMap.value("rollback");
				makeNetworkRequest(apiUrl, res);*/
				QString sqlQuery = "UPDATE cloudupdate SET posted = 3 WHERE posted <> 1 AND id = " + res["id"];

				ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString(), timeStamp[0] + "-queries");
				vector queryResult = ExecuteTargetSql(sqlQuery);
				if (queryResult.size() > 0) {
					for (auto result : queryResult)
						LOG << result["success"];
				}
				continue;
			}
			QString resultVal;
			//qDebug() << result.value("result");
			if (result.value("result").isArray()) {
				resultVal = result.value("status").toObject().value("test").toString().trimmed();
				resultVal += resultVal == "SQL Executed Successfully" ? ".1" : ".2";
			}
			else {
				resultVal = result.value("result").toString();
			}
			LOG << resultVal;
			QString rowsAffected = resultVal.split(".")[1].trimmed().split(" ")[0];
			LOG << rowsAffected;
			if (rowsAffected.toInt() < 1) {
				//res["query"] = queryMap.value("commit");
				rowsAffected = "1";
			}
			if (rowsAffected.toInt() > 1) {
				rowsAffected = "3";
			}
			//else
				//res["query"] = queryMap.value("rollback");
			//makeNetworkRequest(apiUrl, res);

			QString sqlQuery = "UPDATE cloudupdate SET posted = " + rowsAffected + " WHERE posted <> 1 AND id = " + res["id"];

			ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString(), timeStamp[0] + "-queries");
			vector queryResult = ExecuteTargetSql(sqlQuery);
			if (queryResult.size() > 0) {
				for (auto result : queryResult)
					LOG << result["success"];
			}
		}
		
		if (query.numRowsAffected() > 0)
			ServiceHelper().WriteToCustomLog("Finished network requests", timeStamp[0] + "-queries");

		query.clear();
		query.finish();
		
		db.close();
		//performCleanup();
		result = true;
		
	}
	catch (exception& e)
	{
		if(db.isOpen())
			db.close();
		ServiceHelper().WriteToError(e.what());
	}

	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);

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
		TCHAR buffer[1024];
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

			ServiceHelper().WriteToLog((string)"Creating session to db");
					
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

		ServiceHelper().WriteToLog((string)"DB Connection successfully opened");
		
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
			ServiceHelper().WriteToLog((string)"Generating Database");
			QString targetQuery = "CREATE DATABASE " + schema + " CHARACTER SET utf8 COLLATE utf8_general_ci";
			LOG << targetQuery;
			if (!query.exec(targetQuery)) {
				ServiceHelper().WriteToError((string)"Failed to create database");
			}
			else {
				ServiceHelper().WriteToLog((string)"Successfully created Database");
			}
		}

		query.clear();
		query.finish();

		if (!db.commit())
			db.rollback();

		db.close();

		db.setDatabaseName(schema);

	}
	catch (exception& e)
	{

		if (db.isOpen())
			db.close();

		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}

int DatabaseManager::ExecuteTargetSqlScript(std::string& filepath)
{
	int successCount = 0;
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	//QString schema = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Schema", REG_SZ));
	rtManager.GetVal("Schema", REG_SZ, (char*)buffer, size);
	QString schema(buffer);
	//RegCloseKey(hKey);
	QSqlDatabase db = QSqlDatabase::database(schema);
	QFile file(filepath.data());
	try {
		// HenchmanServiceException
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) 
			throw HenchmanServiceException("Failed to open target file");

		ServiceHelper().WriteToLog("Successfully opened target file: " + filepath);

		QTextStream in(&file);
		QString sql = in.readAll();

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

		if (!db.open())
			throw HenchmanServiceException("Failed to open DB Connection");

		db.transaction();
		QSqlQuery query(db);

		if (!query.exec("USE " + schema + ";"))
			throw HenchmanServiceException("Failed to execute initial DB Query");

		for(QString &statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			if (query.exec(statement))
				successCount++;
			else {
				if(statement.length() <= 128)
					throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());
				else
					throw HenchmanServiceException("Failed executing statement. Reason provided: " + query.lastError().text().toStdString());

			}
			query.clear();
		}
		query.finish();
		if (!db.commit())
			db.rollback();
		db.close();
		file.close();
	}
	catch (exception& e)
	{
		if (file.isOpen())
			file.close();
		if (db.isOpen()) {
			if (!db.commit())
				db.rollback();
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
	}
	return successCount;
}

vector<QStringMap> DatabaseManager::ExecuteTargetSql(std::string sqlQuery)
{
	int successCount = 0;
	vector<QStringMap> resultVector;
	QStringMap queryResult;
	queryResult["success"] = "0";
	//HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	RegistryManager::CRegistryManager rtManager(HKEY_LOCAL_MACHINE, std::string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").c_str());
	TCHAR buffer[1024] = "\0";
	DWORD size = sizeof(buffer);
	//QString schema = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Schema", REG_SZ));
	rtManager.GetVal("Schema", REG_SZ, (char*)buffer, size);
	QString schema(buffer);
	//RegCloseKey(hKey);
	QSqlDatabase db = QSqlDatabase::database(schema);

	try {

		if (!db.open())
		{
			// HenchmanServiceException
			throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QSqlQuery query(db);

		QString sql = sqlQuery.data();

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

		if (!query.exec("USE " + schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + schema.toStdString() + ";");

		for(QString &statement:sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			LOG << "Executing: " << statement.toStdString();
			if (query.exec(statement))
			{
				successCount++;
				QSqlRecord record(query.record());
				
				queryResult["success"] = QString::number(successCount);
				resultVector.push_back(queryResult);
				
				while (query.next())
				{
					queryResult.clear();
					for (int i = 0; i <= record.count() - 1; i++)
					{
						queryResult[record.fieldName(i)] = query.value(i).toString();
					}

					resultVector.push_back(queryResult);
				}
			}
			else {
				throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());
			}
			query.clear();
		}
		query.finish();
		if (!db.commit())
			db.rollback();
		db.close();
	}
	catch (exception& e)
	{
		if (db.isOpen()) {
			if (!db.commit())
				db.rollback();
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
		resultVector[0] = queryResult;

	}

	return resultVector;
}

vector<QStringMap> DatabaseManager::ExecuteTargetSql(QString sqlQuery)
{
	return ExecuteTargetSql(sqlQuery.toStdString());
}

vector<QStringMap> DatabaseManager::ExecuteTargetSql(const TCHAR* sqlQuery)
{
	return ExecuteTargetSql((std::string)sqlQuery);
}

void DatabaseManager::parseData(QNetworkReply *netReply)
{
	QRestReply restReply(netReply);
	optional json = restReply.readJson();
	optional <QJsonObject> response = json->object();
	string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit).toStdString();
	stringstream errorRes;
	stringstream dataRes;

	if (restReply.error() != QNetworkReply::NoError) {
		qWarning() << "A Network error has occured: " << restReply.error() << restReply.errorString();
		ServiceHelper().WriteToError("A Network error has occured: " + restReply.errorString().toStdString());
		goto exit;
	}
	if (!restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		qWarning() << "A HTTP error has occured: " << status << reason;
		ServiceHelper().WriteToError("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	if (restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		ServiceHelper().WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	ServiceHelper().WriteToLog((string)"Parsing Response");

	ExecuteTargetSql(sqlQuery);

	if (!json) {
		ServiceHelper().WriteToError((string)"Recieved empty data or failed to parse JSON.");
		goto exit;
	}

	if (response.value()["error"].toArray().count() > 0) {
		for (const auto& result : response.value()["error"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				errorRes << " - " << result.toArray()[i].toString().toStdString() << endl;
			}
		}

		ServiceHelper().WriteToError("Server responded with error: " + errorRes.str());
	}

	if (response.value()["data"].toArray().count() > 0) {
		for (const auto& result : response.value()["data"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				dataRes << " - " << result.toArray()[i].toString().toStdString() << endl;
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

void DatabaseManager::processKeysAndValues(QStringMap &map, QString (&results)[])
{
	QString queryKeys = "";
	QString queryValues = "";
	//QString conditionals = "";
	int count = map.size();
	QStringList keys = map.keys();
	
	for (auto& key : map.keys()) {
		count--;
		if (key == "id" || map.value(key).isEmpty() || map.value(key) == "0" || map.value(key) == "'0'")
			continue;
		if (QRegularExpression("\\d\\d\\d\\d-\\d\\d-\\d\\dT\\d\\d:\\d\\d:\\d\\d.\\d\\d\\dZ").match(map.value(key)).hasMatch())
			map[key] = 
			"'" + QRegularExpression("\\d\\d\\d\\d-\\d\\d-\\d\\d").match(map.value(key)).captured(0) + 
			" " + 
			QRegularExpression("\\d\\d:\\d\\d:\\d\\d").match(map.value(key)).captured(0) + "'";
		else
			map[key] = "'" + map.value(key) + "'";
		queryKeys.append((queryKeys.size() > 0 ? ", " : "") + ("`" + key + "`"));
		queryValues.append((queryValues.size() > 0 ? ", " : "") + map.value(key));
	}
	
	results[0] = queryKeys;
	results[1] = queryValues;
}

void DatabaseManager::processInsertStatement(QString& query, bool& skipQuery)
{
	int startpoint = query.indexOf("(") + 1;
	int endpoint = query.indexOf(")", startpoint) - startpoint;
	LOG << startpoint << " | " << endpoint;
	QString queryStart = query.first(startpoint - 1);
	QStringList splitQueryStart = ServiceHelper::ExplodeString(queryStart, " ");

	QString columns = query.mid(startpoint, endpoint);
	QStringList splitColumns = ServiceHelper::ExplodeString(columns, ",");

	startpoint = query.indexOf("(", startpoint) + 1;
	endpoint = query.lastIndexOf(")") - startpoint;
	LOG << startpoint << " | " << endpoint;
	QString values = query.mid(startpoint, endpoint);
	QStringList splitValues = ServiceHelper::ExplodeString(values, ",");

	QStringMap map;
	columns.clear();
	values.clear();

	int hasCustId = 0;
	int hasTrakId = 0;

	for (int i = 0; i < splitColumns.size(); i++) {
		QString col = splitColumns.at(i).simplified();
		QString val = splitValues.at(i).simplified();

		if (val.isEmpty() || val == "''")
			continue;
		if (val[0] == "'" && !val.endsWith("'")) {
			val = val + "\, " + splitValues.at(i + 1).simplified();
			splitValues.removeAt(i + 1);
		}
		if (val[0] != '\'' && val != "NULL")
			val = "'" + val;
		if (val[val.size() - 1] != '\'' && val != "NULL")
			val.append("'");

		string valueCheck;
		if (col.contains(trakId.data(), Qt::CaseInsensitive)) {
			hasTrakId = 1;
			col = trakId.data();
			valueCheck = "'" + trakIdNum.toStdString() + "'";
		}
		if (col.contains("custId", Qt::CaseInsensitive)) {
			hasCustId = 1;
			col = "custId";
			valueCheck = "'" + std::to_string(custId) + "'";
		}

		if ((col == trakId.data() || col == "custId") && (valueCheck.data() != val))
		{
			ServiceHelper().WriteToLog(
				"Query is not for current device. Device value: " +
				valueCheck +
				". Query value: " +
				val.toStdString()
			);
			skipQuery = true;
			break;
		}
		map[col] = val;
		if (col != "id")
		{
			columns.append(col.toStdString() + (i < splitColumns.size() ? ", " : ""));
			values.append(val.toStdString() + (i < splitColumns.size() ? ", " : ""));
		}
	}

	if (skipQuery)
		return;

	if (columns.endsWith(", "))
		columns.resize(columns.size() - 2);
	if (values.endsWith(", "))
		values.resize(values.size() - 2);

	QString newQuery;

	switch (table_map[splitQueryStart.at(splitQueryStart.size() - 1)])
	{
	case tools:
	{
		QStringMap conditionals;
		if (!map.value("stockcode").isEmpty())
			conditionals.insert("stockcode", "stockcode=" + map.value("stockcode"));
		if (!map.value("PartNo").isEmpty())
			conditionals.insert("PartNo", "PartNo=" + map.value("PartNo"));
		if (!map.contains("toolId") && map.contains("description")) {
			//&& 
			QString query = "SELECT id FROM tools WHERE ";
			if (!map.value("PartNo").isEmpty())
				query.append("PartNo=" + map.value("PartNo") + " AND ");
			query.append("UPPER(description) LIKE UPPER(" + map.value("description") + ") GROUP BY description;");
			vector fetchTool = ExecuteTargetSql(query);
			if (fetchTool.size() <= 1) {
				skipQuery = true;
				break;
			}
			if (columns.contains("toolId", Qt::CaseInsensitive)) {
				QStringList updatedToolId = values.split(", ");
				updatedToolId[columns.split(", ").indexOf("toolId")] = "'" + fetchTool[1]["id"] + "'";
				values = updatedToolId.join(", ");
			}
			else {
				columns.append(", toolId");
				values.append(", '" + fetchTool[1]["id"] + "'");
			}
			conditionals.insert("toolId", "toolId =" + fetchTool[1]["id"]);
		}
			//conditionals.insert("toolId", "toolId=" + map.value("id"));
		newQuery =
			"INSERT INTO tools (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM tools WHERE" +
		(hasCustId 
			? " custId=" + map.value("custId")
			: " custId=" + custId)+
			" AND ( " +
			conditionals.values().join(" OR ") +
			")"
			" ORDER BY id DESC LIMIT 1)";
		LOG << newQuery;
		break;
	}
	case users:
	{
		newQuery =
			"INSERT INTO users (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM users WHERE" +
		(hasCustId 
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
			" AND userId=" + map.value("userId") +
		(map.value(trakId.data()).isEmpty()
			? (" AND " + trakId + " = ").data() + trakIdNum
			: (" AND " + trakId + " = ").data() + map.value(trakId.data())) +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	case employees:
	{
		newQuery =
			"INSERT INTO employees (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM employees WHERE" +
		(hasCustId ?
			" custId=" + map.value("custId")
			: " custId=" + custId) +
			" AND userId=" + map.value("userId") +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	case jobs:
	{
		newQuery =
			"INSERT INTO jobs (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM jobs WHERE" +
		(hasCustId ?
			" custId=" + map.value("custId")
			: " custId=" + custId) +
		(map.value("trailId").isEmpty()
			? ""
			: " AND trailId=" + map.value("trailId")) +
		(map.value("description").isEmpty()
			? ""
			: " AND description=" + map.value("description")) +
		(map.value("remark").isEmpty()
			? ""
			: " AND remark=" + map.value("remark")) +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	case kabs:
	{
		newQuery =
			"INSERT INTO itemkabs (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabs WHERE" +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	case drawers:
	{
		newQuery =
			"INSERT INTO itemkabdrawers (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawers WHERE" +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			" AND drawerCode=" + map.value("drawerCode") +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	case toolbins:
	{
		newQuery =
			"INSERT INTO itemkabdrawerbins (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawerbins WHERE" +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			" AND toolNumber=" + map.value("toolNumber") +
			" AND drawerNum=" + map.value("drawerNum") +
			" AND itemId=" + map.value("itemId") +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	case cribs:
		newQuery =
			"INSERT INTO cribs (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM cribs WHERE" +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			" ORDER BY id DESC LIMIT 1)";
		break;
	case cribconsumables:
		newQuery =
			"INSERT INTO cribconsumables (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM cribconsumables WHERE" +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			" AND userId=" + map.value("userId") +
			" AND tailId=" + map.value("tailId") +
			" AND barcode=" + map.value("barcode") +
			" ORDER BY id DESC LIMIT 1)";
		break;
	case cribtoollocation:
	{
		std::vector locationId = ExecuteTargetSql(QString("SELECT id FROM cribtoollocation WHERE description = ").append(map.value("description")).toStdString());
		map["locationId"] = locationId.at(1).value("id");

		newQuery =
			"INSERT INTO cribtoollocation (" +
			QString(columns.data()) + ", locationId" +
			") SELECT " +
			QString(values.data()) + ", '" + map.value("locationId") +
			"' FROM DUAL WHERE NOT EXISTS (" +
			"SELECT * FROM cribtoollocation WHERE" +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			(!map.value("locationId").isEmpty()
				? "AND locationId='" + map.value("locationId") + "'"
				: "") +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	//case cribtoollockers:
	case cribtools:
	{
		vector fetchTool = ExecuteTargetSql(("SELECT id FROM tools WHERE ((PartNo IS NOT NULL OR PartNo <> '') AND PartNo = '" + map.value("itemId") + "') OR ((stockcode IS NOT NULL OR stockcode <> '') AND stockcode = '" + map.value("itemId") + "') OR (id = '" + map.value("toolId") + "') GROUP BY PartNo, description;").toStdString());
		if (columns.contains("toolId", Qt::CaseInsensitive)) {
			QStringList updatedToolId = values.split(", ");
			updatedToolId[columns.split(", ").indexOf("toolId")] = "'" + fetchTool[1]["id"] + "'";
			values = updatedToolId.join(", ");
		}
		else {
			columns.append(", toolId");
			values.append(", '" + fetchTool[1]["id"] + "'");
		}
		if (!map.contains("toolId"))
			map.insert("toolId", "toolId =" + fetchTool[1]["id"]);
		else if (map.value("toolId").isEmpty())
			map.value("toolId") = "toolId =" + fetchTool[1]["id"];
		//conditionals.insert("toolId", "toolId =" + fetchTool[1]["id"]);

		newQuery =
			"INSERT INTO cribtools (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM cribtools WHERE" +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			" AND barcodeTAG=" + map.value("barcodeTAG") +
			" AND serialNo=" + map.value("serialNo") +
			" AND itemId=" + map.value("itemId") +
			" AND toolId=" + map.value("toolId") +
			" ORDER BY toolId DESC LIMIT 1)";
		LOG << newQuery;
		break;
	}
		//case kittools:
	case tooltransfer:
		newQuery =
			"INSERT INTO tooltransfer (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM tooltransfer WHERE" +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			" AND barcodeTAG=" + map.value("barcodeTAG") +
			(!map.value("userId").isEmpty() ? " AND userId=" + map.value("userId") : "") +
			(!map.value("transfer_userId").isEmpty() ? " AND transfer_userId=" + map.value("transfer_userId") : "") +
			(!map.value("tailId").isEmpty() ? " AND tailId=" + map.value("tailId") : "") +
			(!map.value("transfer_tailId").isEmpty() ? " AND transfer_tailId=" + map.value("transfer_tailId") : "") +
			(!map.value("transactionDate").isEmpty() ? " AND transactionDate=" + map.value("transactionDate") : "") +
			(!map.value("dateTransferred").isEmpty() ? " AND dateTransferred=" + map.value("dateTransferred") : "") +
			" ORDER BY id DESC LIMIT 1)";
		break;
		//case itemkits:
	case kitcategory:
	{
		std::vector categoryId = ExecuteTargetSql(QString("SELECT id FROM kitcategory WHERE description = ").append(map.value("description")).toStdString());
		map["categoryId"] = categoryId.at(1).value("id");

		newQuery =
			"INSERT INTO kitcategory (" +
			QString(columns.data()) + ", categoryId" +
			") SELECT " +
			QString(values.data()) + ", '" + map.value("categoryId") +
			"' FROM DUAL WHERE NOT EXISTS (" +
			"SELECT * FROM kitcategory WHERE " +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
			"AND categoryId='" + map.value("categoryId") + "'" +
			"AND description=" + map.value("description") +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	case kitlocation:
	{
		std::vector locationId = ExecuteTargetSql(QString("SELECT id FROM kitlocation WHERE description = ").append(map.value("description")).toStdString());
		map["locationId"] = locationId.at(1).value("id");

		newQuery =
			"INSERT INTO kitlocation (" +
			QString(columns.data()) + ", locationId" +
			") SELECT " +
			QString(values.data()) + ", '" + map.value("locationId") +
			"' FROM DUAL WHERE NOT EXISTS (" +
			"SELECT * FROM kitlocation WHERE " +
		(hasCustId
			? " custId=" + map.value("custId")
			: " custId=" + custId) +
		(hasTrakId
			? (" AND " + trakId + " = ").data() + map.value(trakId.data())
			: (" AND " + trakId + " = ").data() + trakIdNum) +
			(!map.value("locationId").isEmpty()
				? "AND locationId='" + map.value("locationId") + "'"
				: "") +
			" ORDER BY id DESC LIMIT 1)";
		break;
	}
	/*case kabemployeeitemtransactions:
	case cribemployeeitemtransactions:
	case portaemployeeitemtransactions:
	case lokkaemployeeitemtransactions:*/
	default:
		newQuery =
			queryStart + "(" +
			columns +
			") SELECT " +
			values +
			" FROM DUAL WHERE NOT EXISTS (" +
			"SELECT "+ columns +
			" FROM " + splitQueryStart.at(splitQueryStart.size() - 1) + " WHERE";

		QStringList conditionals;
		conditionals.append(
			(hasCustId
				? " custId=" + map.value("custId")
				: " custId=" + custId)
		);
		conditionals.append(
			(trakId + " = ").data() + (hasTrakId
				? map.value(trakId.data())
				: trakIdNum)
		);
		for (auto [key, value] : map.asKeyValueRange()) {
			if (key == "custId" || key == trakId.data())
				continue;
			conditionals.append(key + "=" + value);
		}
		newQuery += conditionals.join(" AND ") + " ORDER BY id DESC LIMIT 1)";

		break;
	}
	LOG << newQuery;
	query = newQuery;
	LOG << query;
	return;
}

void DatabaseManager::processUpdateStatement(QString& query, bool& skipQuery)
{
	QStringList splitQueryForParsing = ServiceHelper::ExplodeString(query, " ");
	string splitBy;
	if (query.contains("SET"))
		splitBy = " SET ";
	else
		splitBy = " set ";
	QStringList querySections = ServiceHelper::ExplodeString(query, splitBy.data());
	//qDebug() << splitQueryForParsing;
	//qDebug() << querySections;
	if (querySections.size() > 2) {
		QStringList tempQuerySections = querySections;
		tempQuerySections.removeAt(0);
		querySections[1] = tempQuerySections.join(" SET ");
		querySections.remove(2, querySections.size() - 2);
	}
	//qDebug() << querySections;
	if (query.contains("WHERE"))
		splitBy = " WHERE ";
	else
		splitBy = " where ";
	//QStringList querySections = ServiceHelper::ExplodeString(querySections[1], splitBy.data());
	querySections.append(ServiceHelper::ExplodeString(querySections[1], splitBy.data()));
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
	QMap<QString, QString> SetPairs;
	QMap<QString, QString> ConditionPairs;
	QStringList parsedSets;
	QStringList parsedConditionals;
	bool switchToConditions = 0;
	bool hadCustId = 0;
	bool hadTrakId = 0;
	bool hadId = 0;
	int idFromQuery = 0;

	QStringList querySetSection = ServiceHelper::ExplodeString(querySections[1], ",");
	for (const auto& set : querySetSection)
	{
		QString setTrimmed = set.trimmed();
		QStringList colAndVal;
		if (setTrimmed.contains("id"))
			continue;

		parsedSets.append(setTrimmed);
	}

	// Parse conditionals
	QStringList queryConditionalSections;
	if (query.contains("AND"))
		queryConditionalSections = ServiceHelper::ExplodeString(querySections[2], "AND");
	else
		queryConditionalSections = ServiceHelper::ExplodeString(querySections[2], "and");

	for (const auto& conditional : queryConditionalSections)
	{
		QString conditionalTrimmed = conditional.trimmed();
		QStringList colAndVal = ServiceHelper::ExplodeString(conditionalTrimmed, "=");
		if (colAndVal.size() < 2) {
			parsedConditionals.append(conditionalTrimmed);
			continue;
		}
		ConditionPairs.insert(colAndVal[0], colAndVal[1]);
		if (conditionalTrimmed.contains("custId", Qt::CaseInsensitive))
		{
			hadCustId = 1;
			if (colAndVal[1].trimmed() == QString::number(custId))
				parsedConditionals.append(colAndVal[0].trimmed() + " = '" + colAndVal[1].trimmed() + "'");
			else
				parsedConditionals.append(colAndVal[0].trimmed() + " = '" + QString::number(custId) + "'");
			continue;
		}
		if (conditionalTrimmed.contains(trakId.data(), Qt::CaseInsensitive))
		{
			hadTrakId = 1;
			if (colAndVal[1].trimmed() == trakIdNum)
				parsedConditionals.append(colAndVal[0].trimmed() + " = '" + colAndVal[1].trimmed() + "'");
			else
				parsedConditionals.append(colAndVal[0].trimmed() + " = '" + trakIdNum + "'");
			continue;
		}
		if (conditionalTrimmed.contains("toolId", Qt::CaseInsensitive))
		{
			std::vector cribToolItemId = ExecuteTargetSql(QString("SELECT itemId FROM " + splitQueryForParsing[1] + " WHERE  custId = '" + QString::number(custId) + "' AND cribId = '" + trakIdNum + "' AND toolId = '" + colAndVal[1].trimmed() + "'").toStdString());
			//qDebug() << cribToolItemId << " | " << cribToolItemId.size();
			if (cribToolItemId.size() <= 1 || !cribToolItemId[1].contains("itemId")) {
				skipQuery = true;
				continue;
			}
			parsedConditionals.append("itemId = '" + cribToolItemId[1]["itemId"] + "'");
			continue;
		}
		LOG << splitQueryForParsing[1];
		if (conditionalTrimmed.contains("id") && !(splitQueryForParsing[1] == "tools" || splitQueryForParsing[1] == "cribemployeeitemtransactions"))
		{
			hadId = 1;
			idFromQuery = colAndVal[1].trimmed().toInt();
			continue;
		}

		parsedConditionals.append(conditionalTrimmed);
	}
	if (skipQuery)
		return;
	//qDebug() << splitQueryForParsing;
	switch (table_map[splitQueryForParsing.at(1)])
	{
	case tools:
	{
		for (auto& value : parsedConditionals) {
			if (parsedConditionals.size() <= 1 && value.split("=")[1] == "''")
				skipQuery = true;
			if (value.split("=")[0].contains("id")) {
				std::vector toolStockcode = ExecuteTargetSql(("SELECT serialNo, stockcode, id FROM tools WHERE  custId = '" + QString::number(custId) + "' AND id = '" + value.split("=")[1].trimmed() + "'").toStdString());
				value = "(serialNo = '" + toolStockcode[1]["serialNo"] + "' OR stockcode = '" + toolStockcode[1]["stockcode"] + "' OR toolId='"+ toolStockcode[1]["id"] + "')";
			}
		}
		if (!hadCustId)
			parsedConditionals.append("custId = '" + QString::number(custId) + "'");
		
		if (!ConditionPairs.contains("id")) {
			std::vector toolId = ExecuteTargetSql(("SELECT id FROM tools WHERE " + parsedConditionals.join(" AND ")).toStdString());
			//qDebug() << toolId;
			if (toolId.size() > 1)
				parsedConditionals.append("toolId="+toolId[1]["id"]);
		}

		returnVal.append({ querySections[0], "SET", parsedSets.join(", "), "WHERE", parsedConditionals.join(" AND ") });
		LOG << returnVal.join(" ");
		break;
	}
	case users:
	{
		/*res["query"] =
			"INSERT INTO users (" +
			QString(columns.data()) +
			") SELECT " +
			QString(values.data()) +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM users WHERE" +
			" userId=" + map.value("userId") +
			" AND custId=" + map.value("custId") +
			(map.value(trakId.data()).isEmpty()
				? " AND " + trakId + " = " + idNum
				: " AND " + trakId + " = " + map.value(trakId.data()).toStdString()).data() +
			" ORDER BY id DESC LIMIT 1)";*/
		for (auto& value : parsedConditionals) {
			if (parsedConditionals.size() <= 1 && value.split("=")[1] == "''")
				skipQuery = true;
			if (value.split("=")[0].contains("id")) {
				std::vector userId = ExecuteTargetSql(("SELECT userId FROM users WHERE id = '" + value.split("=")[1].trimmed() + "'").toStdString());
				if(userId.size()<=1)
					skipQuery = true;
				else
					value = "userId='"+userId[1]["userId"]+"'";
			}
		}
		if (hadId && !query.contains("userId", Qt::CaseInsensitive)) {
			std::vector userId = ExecuteTargetSql("SELECT userId FROM users WHERE id = '" + QString::number(idFromQuery) + "'");
			if (userId.size() > 1)
				parsedConditionals.append("userId='" + userId[1]["userId"] + "'");
			else {
				skipQuery = true;
			}
		}
		if (!hadCustId)
			parsedConditionals.append("custId = '" + QString::number(custId) + "'");
		if (!hadTrakId)
			parsedConditionals.append(trakId.data() + (" = '" + trakIdNum + "'"));
		/*if (!query.contains("userId", Qt::CaseInsensitive))
			skipQuery = true;*/
		returnVal.append({ querySections[0], "SET", parsedSets.join(", "), "WHERE", parsedConditionals.join(" AND ") });
		LOG << returnVal.join(" ");
		break;
	}
	//case employees:
	case jobs:
	{
		for (auto& value : parsedConditionals) {
			if (parsedConditionals.size() <= 1 && value.split("=")[1] == "''")
				skipQuery = true;
			/*if (value.split("=")[0].contains("trailId")) {
				parsedConditionals.removeAt(parsedConditionals.indexOf(value));
			}*/
		}
		if (hadId) {
			skipQuery = true;
		}
		if (!hadCustId)
			parsedConditionals.append("custId = '" + QString::number(custId) + "'");
		returnVal.append({ querySections[0], "SET", parsedSets.join(", "), "WHERE", parsedConditionals.join(" AND ") });
		LOG << returnVal.join(" ");
		break;
	}
	case customer: 
	{
		if(!hadId)
			parsedConditionals.append("id=" + QString::number(custId));
		returnVal.append({ querySections[0], "SET", parsedSets.join(", "), "WHERE", parsedConditionals.join(" AND ") });
		LOG << returnVal.join(" ");
		break;
	}
	//case kabs:
	//case drawers:
	case toolbins: 
	{
		if (hadId && !query.contains("itemId", Qt::CaseInsensitive)) {
			std::vector itemId = ExecuteTargetSql("SELECT itemId FROM itemkabdrawerbins WHERE id = '" + QString::number(idFromQuery) + "'");
			if (itemId.size() > 1)
				parsedConditionals.append("itemId='" + itemId[1]["itemId"]+"'");
			else {
				skipQuery = true;
			}
		}

		if (!hadCustId)
			parsedConditionals.append("custId = '" + QString::number(custId) + "'");
		if (!hadTrakId)
			parsedConditionals.append(trakId.data() + (" = '" + trakIdNum + "'"));
		returnVal.append({ querySections[0], "SET", parsedSets.join(", "), "WHERE", parsedConditionals.join(" AND ") });
		LOG << returnVal.join(" ");
		break;
	}
	//case cribs:
	//case cribconsumables:
	//case cribtoollocation:
	//case cribtoollockers:
	//case cribtools:
	//case kittools:
	case tooltransfer:
	{

		if (!hadCustId)
			parsedConditionals.append("custId = '" + QString::number(custId) + "'");
		if (!hadTrakId)
			parsedConditionals.append(trakId.data() + (" = '" + trakIdNum + "'"));
		returnVal.append({ querySections[0], "SET", parsedSets.join(", "), "WHERE", parsedConditionals.join(" AND ") });
		LOG << returnVal.join(" ");
		break;
	}
	//case itemkits:
	//case kitcategory:
	//case kitlocation:
	case tblcounterid:
	{
		skipQuery = true;
		break;
	}
	case kabemployeeitemtransactions:
	{
		skipQuery = true;
		break;
	}
	case cribemployeeitemtransactions:
	{
		returnVal.append({ "INSERT INTO cribemployeeitemtransactions", "(" });
		QStringList cols;
		QStringList vals;
		for (int i = 0; i < parsedSets.size(); ++i)
		{
			QStringList parsedSetsSplit = parsedSets[i].split("=");
			if (parsedSetsSplit[0].trimmed() == "inDate")
				cols.append("transDate");
			else if (parsedSetsSplit[0].trimmed() == "inTime")
				cols.append("transTime");
			else
				cols.append(parsedSetsSplit[0].trimmed());
			vals.append(parsedSetsSplit[1].trimmed());
		}
		for (int i = 0; i < parsedConditionals.size(); ++i)
		{
			if (parsedConditionals.size() <= 1 && parsedConditionals[i].split("=")[1] == "''") {
				skipQuery = true;
				break;
			}
			QStringList parsedConditionalsSplit = parsedConditionals[i].split("=");
			if (cols.contains(parsedConditionalsSplit[0].trimmed()))
				continue;
			LOG << parsedConditionalsSplit[0].trimmed();
			if (parsedConditionalsSplit[0].trimmed() == "id") {
				parsedConditionalsSplit[1] = parsedConditionalsSplit[1].trimmed();
				if (parsedConditionalsSplit[1].startsWith("'"))
					parsedConditionalsSplit[1].removeFirst();
				if (parsedConditionalsSplit[1].endsWith("'"))
					parsedConditionalsSplit[1].removeLast();
				LOG << parsedConditionalsSplit[1];
				std::vector targetTransactionToBeAltered = ExecuteTargetSql(("SELECT * FROM cribemployeeitemtransactions WHERE  custId = '" + QString::number(custId) + "' AND cribId = '" + trakIdNum + "' AND id = '" + parsedConditionalsSplit[1] + "'").toStdString());
				//qDebug() << targetTransactionToBeAltered;
				if (targetTransactionToBeAltered.size() <= 1) {
					skipQuery = true;
					break;
				}
				for (const auto& key : targetTransactionToBeAltered[1].keys()) {
					const QString val = targetTransactionToBeAltered[1][key];
					const QString keyFromVal = targetTransactionToBeAltered[1].key(val);
					if (cols.contains(keyFromVal) || QStringList{ "id", "transDate", "transTime" }.contains(keyFromVal))
						continue;
					cols.append(keyFromVal);
					vals.append("'" + val + "'");
				}
				LOG << cols.join(", ");
				LOG << vals.join(", ");
				continue;
			}
			cols.append(parsedConditionalsSplit[0].trimmed());
			vals.append(parsedConditionalsSplit[1].trimmed());
		}
		if (skipQuery)
			break;
		//qDebug() << parsedSets << "\n" << parsedConditionals << "\n" << cols << "\n" << vals;
		if (((cols.contains("itemId") || cols.contains("toolId")) && !cols.contains("barcode", Qt::CaseInsensitive)) && !skipQuery) {
			cols.append("barcode");
			std::vector<QStringMap> barcode;
			if(cols.contains("itemId"))
				barcode = ExecuteTargetSql(("SELECT barcodeTAG FROM cribtools WHERE itemId='" + vals.at(cols.indexOf("itemId")) + "' GROUP BY itemId ORDER BY toolId DESC LIMIT 1").toStdString());
			else
				barcode = ExecuteTargetSql(("SELECT barcodeTAG FROM cribtools WHERE toolId='" + vals.at(cols.indexOf("toolId")) + "' GROUP BY toolId ORDER BY toolId DESC LIMIT 1").toStdString());
			vals.append("'" + barcode[1]["barcodeTAG"] + "'");
		}
		if (((cols.contains("barcode") || cols.contains("toolId")) && !cols.contains("itemId", Qt::CaseInsensitive)) && !skipQuery && false) {
			cols.append("itemId");
			QStringList queryString;
			queryString.resize(3);
			queryString[0] = "SELECT itemId, toolId FROM cribtools WHERE";
			queryString[2] = "ORDER BY toolId DESC LIMIT 1";

			std::vector<QStringMap> itemid;
			if (cols.contains("toolId")) {
				queryString[1] = "toolId = " + vals.at(cols.indexOf("toolId"));
				itemid = ExecuteTargetSql(queryString.join(" ").toStdString());
				//qDebug() << itemid;

			}

			if (cols.contains("barcode") && itemid.size() <= 1) {
				queryString[1] = "barcodeTAG = " + vals.at(cols.indexOf("barcode"));
				itemid.clear();
				itemid = ExecuteTargetSql(queryString.join(" ").toStdString());
				//qDebug() << itemid;
			}


			if (itemid.size() > 1 && !itemid[1]["itemId"].isEmpty())
				vals.append("'" + itemid[1]["itemId"] + "'");
			else if (itemid.size() > 1 && !itemid[1]["toolId"].isEmpty())
				vals.append("'" + itemid[1]["itemId"] + "'");
			else {
				skipQuery = true;
				break;
			}
			//qDebug() << itemid;
			//vals.append("(SELECT itemId FROM cribtools WHERE barcodeTAG = " + vals.at(cols.indexOf("barcode")) + " AND custId = '" + custId.data() + "' AND " + trakId.data() + " = '" + idNum.data() + "' ORDER BY id DESC LIMIT 1)");
		}
		if ((cols.contains("trailId") && !cols.contains("tailId", Qt::CaseInsensitive)) && !skipQuery) {
			cols.append("tailId");
			std::vector tailId = ExecuteTargetSql(("SELECT description FROM jobs WHERE trailId = '" + vals.at(cols.indexOf("trailId")) + "'").toStdString());
			vals.append("'" + tailId[1]["description"] + "'");
			/*vals.append("(SELECT description FROM jobs WHERE trailId = '" + vals.at(cols.indexOf("trailId")) + "' AND custId = '" + custId.data() + "' ORDER BY id DESC LIMIT 1)");*/
		}
		//qDebug() << cols;
		std::cout << (!cols.contains("transDate", Qt::CaseInsensitive)) << " && " << (!skipQuery) << "\n";
		if (!cols.contains("transDate", Qt::CaseInsensitive) && !skipQuery) {
			cols.append("transDate");
			vals.append("CURDATE()");
		}
		//qDebug() << cols;
		std::cout << (!cols.contains("transTime", Qt::CaseInsensitive)) << " && " << (!skipQuery) << "\n";
		if (!cols.contains("transTime", Qt::CaseInsensitive) && !skipQuery) {
			cols.append("transTime");
			vals.append("CURTIME()");
		}
		//qDebug() << cols;
		/*cols.append("remarks");
		vals.append(("(SELECT remarks FROM cribs WHERE custId = " + custId + " AND " + trakId + " = " + idNum + ")").data());*/
		//qDebug() << cols << " | " << vals;
		returnVal.append(cols.join(", "));
		returnVal.append(") VALUES (");
		returnVal.append(vals.join(", "));

		returnVal.append(")");
		LOG << returnVal.join(" ");
		//skipQuery = true;
		break;
	}
	case portaemployeeitemtransactions:
	{
		skipQuery = true;
		break;
	}
	case lokkaemployeeitemtransactions:
	{
		skipQuery = true;
		break;
	}
	default:
		if (hadId)
			skipQuery = true;
		if (!hadCustId)
			parsedConditionals.append("custId = '" + QString::number(custId) + "'");
		if (!hadTrakId)
			parsedConditionals.append(trakId.data() + (" = '" + trakIdNum + "'"));
		returnVal.append({ querySections[0], "SET", parsedSets.join(", "), "WHERE", parsedConditionals.join(" AND ") });
		LOG << returnVal.join(" ");
		break;
	}

	query = returnVal.join(" ");
}

void DatabaseManager::performCleanup()
{
	if (netManager) {
		netManager->deleteLater();
		netManager = nullptr;
	}
	if (restManager) {
		restManager->deleteLater();
		restManager = nullptr;
	}
}
