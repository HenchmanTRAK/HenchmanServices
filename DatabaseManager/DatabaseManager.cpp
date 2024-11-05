
#include "DatabaseManager.h"

using namespace std;

static array<string, 2> timeStamp;

string checkValidDrivers()
{
	stringstream results;
	for (const auto& str : QSqlDatabase::drivers())
	{
		results << " - " << str.toUtf8().data() << endl;
	}
	return results.str();
}

static int checkValidConnections(QString &targetConnection)
{
	for (const auto& str : QSqlDatabase::connectionNames())
	{
		std::cout << " - " << str.toUtf8().data() << endl;
		if (str == targetConnection)
			return TRUE;
	}

	return FALSE;
}

DatabaseManager::DatabaseManager(QObject* parent) 
: QObject(parent)
{
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	string installDir = RegistryManager().GetStrVal(hKey, "INSTALL_DIR", REG_SZ);

	/*CSimpleIniA ini;
	SI_Error rc = ini.LoadFile(installDir.append("\\service.ini").c_str());
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}
	else {
		testingDBManager = ini.GetBoolValue("DEVELOPMENT", "testingDBManager", 0);
		queryLimit = ini.GetLongValue("API", "numberOfQueries", 10);
		apiUsername = ini.GetValue("API", "Username", "");
		apiPassword = ini.GetValue("API", "Password", "");
		apiUrl.append(ini.GetValue(
				"API", 
				"defaultProt", 
				"https"))
			.append("://")
			.append(ini.GetValue(
					"API", 
					"url", 
					"webportal.henchmantrak.com/webapi/public/api/portals/exec_query"));
	}*/

	QSettings ini(installDir.append("\\service.ini").data(), QSettings::IniFormat, this);
	ini.sync();
	testingDBManager = ini.value("DEVELOPMENT/testingDBManager", 0).toBool();
	ini.beginGroup("API");
	queryLimit = ini.value("NumberOfQueries", 10).toInt();
	apiUsername = ini.value("Username", "").toString();
	apiPassword = ini.value("Password", "").toString();
	apiUrl.append(ini.value("Url", "https://webportal.henchmantrak.com/webapi/public/api/portals/exec_query").toString());
	ini.endGroup();

	std::cout << "init db manager" << endl;
	
	targetApp = "";
	requestRunning = false;
	databaseTablesChecked["tools"] = RegistryManager().GetVal(hKey, "numToolsChecked", REG_DWORD);
	databaseTablesChecked["kabs"] = RegistryManager().GetVal(hKey, "numKabsChecked", REG_DWORD);
	databaseTablesChecked["kabDrawers"] = RegistryManager().GetVal(hKey, "numDrawersChecked", REG_DWORD);
	databaseTablesChecked["kabDrawerBins"] = RegistryManager().GetVal(hKey, "numToolsInDrawersChecked", REG_DWORD);
	databaseTablesChecked["users"] = RegistryManager().GetVal(hKey, "numUsersChecked", REG_DWORD);
	databaseTablesChecked["employees"] = RegistryManager().GetVal(hKey, "numEmployeesChecked", REG_DWORD);
	databaseTablesChecked["jobs"] = RegistryManager().GetVal(hKey, "numJobsChecked", REG_DWORD);
	RegCloseKey(hKey);

}

DatabaseManager::~DatabaseManager() 
{
	std::cout << "Deleting DatabaseManager" << endl;

	performCleanup();
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
	for (const auto& key : object.keys()) {
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
		std::cout << "networkrequested" << endl;
		try {
			if (reply.error() != QNetworkReply::NoError) {
				qWarning() << "A Network error has occured: " << reply.error() << reply.errorString();
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
				//ServiceHelper().WriteToError("A Network error has occured: " + reply.errorString().toStdString());
				//return;
			}

			int status = reply.httpStatus();
			QString reason = reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

			if (!reply.isHttpStatusSuccess()) {
				qWarning() << "A HTTP error has occured: " << status << reason;
				ServiceHelper().WriteToLog("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");
			}

			if (reply.isHttpStatusSuccess()) {
				qDebug() << "Request was successful: " << status << reason;
				ServiceHelper().WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");
			}
			ServiceHelper().WriteToLog((string)"Parsing Response");

			QByteArray jsonRes = reply.readBody();

			int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			std::cout << "starting index: " << startingIndex << " ending index: " << endingIndex << endl;
			/*if(startingIndex < endingIndex)
				jsonRes = jsonRes.sliced(startingIndex, endingIndex);*/

				//std::cout << jsonRes.toStdString() << endl;
			optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
			if (!json) {
				ServiceHelper().WriteToLog((string)"Recieved empty data or failed to parse JSON.");
				return;
			}

			string parsedVal;
			if (json->isArray())
				parsedVal = parseData(json->array());
			else if (json->isObject())
				parsedVal = parseData(json->object());

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + parsedVal, timeStamp[0] + "-queries");

			if (results)
				json.value().swap(*results);
			
			result = reply.isSuccess();

			if (reply.isSuccess())
			{
				std::cout << "network request success" << endl;
			}
			else {
				std::cout << "network request failed" << endl;
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
int DatabaseManager::AddToolsIfNotExists()
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
	
	for (auto &result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];

		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = "INSERT INTO tools (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM tools WHERE" +
			" custId="+result.value("custId")+
			" AND PartNo="+result.value("PartNo")+
			" AND stockcode="+result.value("stockcode")+
			" ORDER BY id DESC LIMIT 1)";

		qDebug() << res["query"];
 		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
				databaseTablesChecked[targetKey]++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
			continue;
		}

	}
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager().SetVal(hKey, "numToolsChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//performCleanup();
	return 1;
}

int DatabaseManager::AddUsersIfNotExists()
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

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];
		result.remove("id");

		QString results[2];

		processKeysAndValues(result, results);

		res["query"] = "INSERT INTO users (" +
			results[0] +
			") SELECT " +
			results[1] +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM users WHERE " +
			"userId=" + result.value("userId") +
			" AND custId=" + result.value("custId") +
			(result.value("scaleId").isEmpty()
				? ""
				: " AND scaleId=" + result.value("scaleId")) +
			(result.value("kabId").isEmpty()
				? ""
				: " AND kabId=" + result.value("kabId")) +
			(result.value("cribId").isEmpty()
				? ""
				: " AND cribId=" + result.value("cribId")) +
			" ORDER BY id DESC LIMIT 1)";
		qDebug() << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			std::cout << result["result"].toString().toStdString() << endl;
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			continue;
			//break;
		}

	}
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager().SetVal(hKey, "numUsersChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

int DatabaseManager::AddEmployeesIfNotExists()
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

	for (auto& result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
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

		qDebug() << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			std::cout << result["result"].toString().toStdString() << endl;
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			continue;
			//break;
		}

	}
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager().SetVal(hKey, "numEmployeesChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

int DatabaseManager::AddJobsIfNotExists()
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

		qDebug() << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			std::cout << result["result"].toString().toStdString() << endl;
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				databaseTablesChecked[targetKey]++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			continue;
			//break;
		}

	}
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager().SetVal(hKey, "numJobsChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

// KabTRAK Syncs
int DatabaseManager::AddKabsIfNotExists()
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
		qDebug() << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
				databaseTablesChecked[targetKey]++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
			continue;
		}

	}
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager().SetVal(hKey, "numKabsChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//performCleanup();
	return 1;
}

int DatabaseManager::AddDrawersIfNotExists()
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
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawers WHERE " +
			"custId="+result.value("custId")+
			" AND kabId="+result.value("kabId") +
			" AND drawerCode="+result.value("drawerCode")+
			" ORDER BY id DESC LIMIT 1)";
		qDebug() << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
				databaseTablesChecked[targetKey]++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			//break;
			continue;
		}

	}
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager().SetVal(hKey, "numDrawersChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

int DatabaseManager::AddToolsInDrawersIfNotExists()
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
		qDebug() << res["query"];

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, &reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper().Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
				databaseTablesChecked[targetKey]++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			databaseTablesChecked[targetKey]++;
			//databaseTablesChecked[targetKey] += queryLimit;
			//Sleep(100);
			continue;
			//break;
		}

	}
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager().SetVal(hKey, "numToolsInDrawersChecked", databaseTablesChecked[targetKey], REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

// CribTRAK Syncs

/* TODO
 - upload cribconsumables
 - upload cribs
 - upload cribtoollocation
 - upload cribtoollockers
 - upload cribtools
 - upload kittools
 - upload tooltransfer
*/

// PortaTRAK Syncs

/* TODO
 - upload itemkits
 - upload kitcategory
 - upload kitlocation
*/


int DatabaseManager::connectToRemoteDB()
{
	ServiceHelper().WriteToLog(string("Attempting to connect to Remote Database"));
	timeStamp = ServiceHelper().timestamp();
	QString targetSchema;
	QSqlDatabase db;
	bool result = false;
	try {
		HKEY hKeyLocal = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
		targetSchema = QString::fromStdString(RegistryManager().GetStrVal(hKeyLocal, "Schema", REG_SZ));
		RegCloseKey(hKeyLocal);

		if (!checkValidConnections(targetSchema))
			throw HenchmanServiceException("Provided schema not valid");
	
		/*HKEY hKeyCloud = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Cloud"));

		RegCloseKey(hKeyCloud);*/

		if (apiUrl.trimmed().isEmpty())
			throw HenchmanServiceException("No target Database Url provided");
		
		ServiceHelper().WriteToLog("Creating session to db " + targetSchema.toStdString());
		
		db = QSqlDatabase::database(targetSchema);
		
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
		QString queryText = testingDBManager ? "SHOW TABLES" : "SELECT * FROM cloudupdate WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit);
		query.prepare(queryText);
		if (!query.exec())
		{
			query.finish();
			ServiceHelper().WriteToLog(string("Closing DB Session"));
			throw HenchmanServiceException("Failed to exec query: " + query.executedQuery().toStdString());
		}

		ServiceHelper().WriteToCustomLog("Starting network requests to: " + apiUrl.toStdString(), timeStamp[0] + "-queries");

		int count = 0;

		while (testingDBManager ? count < 5 : query.next())
		{
			count++;
			bool skipQuery = false;
			
			QStringMap res;
			res["number"] = QString::number(count);
			
			if (!testingDBManager) {
				res["id"] = query.value(0).toString();
				res["query"] = query
					.value(2)
					.toString()
					.replace(
						QRegularExpression(
							"(NOW|CURDATE|CURTIME)+",
							QRegularExpression::MultilineOption
						),
						"\'" + query
						.value(3)
						.toString()
						.replace("T", " ") + "\'"
					).replace("()", "").simplified();
				//qDebug() << res;

				ServiceHelper().WriteToCustomLog("Query fetched from database: " + res["query"].toStdString(), timeStamp[0] + "-queries");

				HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
				string trakType = RegistryManager().GetStrVal(hKey, "APP_NAME", REG_SZ);
				RegCloseKey(hKey);
				hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + trakType + "\\Customer"));
				string trakId = RegistryManager().GetStrVal(hKey, "trakID", REG_SZ);
				string custId = RegistryManager().GetStrVal(hKey, "ID", REG_SZ);
				string idNum = RegistryManager().GetStrVal(hKey, trakId.data(), REG_SZ);
				RegCloseKey(hKey);

				if (res["query"].contains("custId =", Qt::CaseInsensitive) && !skipQuery)
				{
					ServiceHelper().WriteToLog("Checking custId is same as device settings");
					int index = res["query"].indexOf("custId", Qt::CaseInsensitive);
					QString substr = res["query"].mid(index, res["query"].size() - index);
					int startpoint = substr.indexOf("=")+1;
					int endpoint = substr.indexOf("and", Qt::CaseInsensitive)-1;
					QString substr2 = substr.mid(startpoint, endpoint-startpoint).trimmed();
					if (substr2.toStdString() != custId) {
						qDebug() << substr2 << " | " << custId;
						//qDebug() << "skipping query";
						skipQuery = true;
						goto parsedQuery;
					}
				}
				
				trakId.resize(trakId.size()-2);
				trakId.append("Id");

				if (res["query"].contains(QString(trakId.data()) + " =", Qt::CaseInsensitive) && !skipQuery)
				{
					ServiceHelper().WriteToLog("Checking trakId is same as device settings");
					int index = res["query"].indexOf(trakId.data(), Qt::CaseInsensitive);
					QString substr = res["query"].mid(index, res["query"].size() - index);
					int startpoint = substr.indexOf("=")+1;
					int endpoint = substr.indexOf("and", Qt::CaseInsensitive)-1;
					QString substr2 = substr.mid(startpoint, endpoint - startpoint).trimmed();
					if (substr2.toStdString() != "'" + idNum + "'") {
						qDebug() << substr2 << " | " << "'" + idNum + "'";
						//qDebug() << "skipping query";
						skipQuery = true;
						goto parsedQuery;
					}
					//res["query"].replace(substr2, QString(trakId.data()) + " = '" + QString(idNum.data()) + "'");
				}
				vector<std::string> splitQuery = ServiceHelper::ExplodeString(res["query"].toStdString(), " ");
				qDebug() << splitQuery;
				
				if (QString::fromStdString(splitQuery[0]).contains("insert", Qt::CaseInsensitive) && !skipQuery) {
					ServiceHelper().WriteToLog("Parsing insert to prevent duplication creation");
					int startpoint = res["query"].indexOf("(") + 1;
					int endpoint = res["query"].indexOf(")", startpoint) - startpoint;
					QString queryStart = res["query"].first(startpoint - 1);
					QStringList splitQueryStart = ServiceHelper::ExplodeString(queryStart, " ");

					QString columns = res["query"].mid(startpoint, endpoint);
					QStringList splitColumns = ServiceHelper::ExplodeString(columns, ",");
					//qDebug() << columns << "|" << splitColumns;

					startpoint = res["query"].indexOf("(", startpoint) + 1;
					endpoint = res["query"].indexOf(")", startpoint) - startpoint;
					QString values = res["query"].mid(startpoint, endpoint);
					QStringList splitValues = ServiceHelper::ExplodeString(values, ",");
					//qDebug() << values << "|" << splitValues;

					QStringMap map;
					columns.clear();
					values.clear();
					//qDebug() << splitColumns << "\n | \n" << splitValues;
					for (int i = 0; i < splitColumns.size(); i++) {
						QString col = splitColumns.at(i).simplified();
						QString val = splitValues.at(i).simplified();

						if (val.isEmpty() || val == "''" || col == "id")
							continue;
						if (val[0] != '\'')
							val = "'" + val;
						if (val[val.size()-1] != '\'')
							val.append("'");

						string valueCheck;
						if (col.contains(trakId.data(), Qt::CaseInsensitive)) {
							col = trakId.data();
							valueCheck = "'" + idNum + "'";
						}
						if (col.contains("custId", Qt::CaseInsensitive)) {
							col = "custId";
							valueCheck = "'" + custId + "'";
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
						columns.append(col.toStdString() + (i < splitColumns.size() ? ", " : ""));
						values.append(val.toStdString() + (i < splitColumns.size() ? ", " : ""));
					}

					if (skipQuery)
						goto parsedQuery;

					if (columns.endsWith(", "))
						columns.resize(columns.size() - 2);
					if (values.endsWith(", "))
						values.resize(values.size() - 2);

					switch (table_map[splitQueryStart.at(splitQueryStart.size() - 1)])
					{
					case tools: 
						res["query"] = 
							"INSERT INTO tools ("+
							QString(columns.data())+
							") SELECT "+
							QString(values.data())+
							" FROM DUAL WHERE NOT EXISTS (SELECT * FROM tools WHERE"+
							" custId=" + map.value("custId")+
							" AND PartNo=" + map.value("PartNo")+
							" AND stockcode=" + map.value("stockcode")+
							" ORDER BY id DESC LIMIT 1)";
						break;

					case users: 
						res["query"] = 
							"INSERT INTO users ("+
							QString(columns.data())+ 
							") SELECT "+ 
							QString(values.data())+
							" FROM DUAL WHERE NOT EXISTS (SELECT * FROM users WHERE"+
							" userId=" + map.value("userId") +
							" AND custId=" + map.value("custId") +
						(map.value("scaleId").isEmpty()
							? ""
							: " AND scaleId=" + map.value("scaleId")) +
						(map.value("kabId").isEmpty()
							? ""
							: " AND kabId=" + map.value("kabId")) +
						(map.value("cribId").isEmpty()
							? ""
							: " AND cribId=" + map.value("cribId")) +
							" ORDER BY id DESC LIMIT 1)";
						break;
					case employees:
						res["query"] = 
							"INSERT INTO employees (" +
							QString(columns.data()) +
							") SELECT " +
							QString(values.data())+
							" FROM DUAL WHERE NOT EXISTS (SELECT * FROM employees WHERE"+
							" userId=" + map.value("userId") +
							" AND custId=" + map.value("custId") +
							" ORDER BY id DESC LIMIT 1)";
						break;
					case jobs:
						res["query"] = 
							"INSERT INTO jobs ("+
							QString(columns.data())+
							") SELECT "+ 
							QString(values.data())+
							" FROM DUAL WHERE NOT EXISTS (SELECT * FROM jobs WHERE"+
							" custId=" + map.value("custId") +
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
					case kabs: 
						res["query"] = 
							"INSERT INTO itemkabs ("+
							QString(columns.data())+
							") SELECT "+
							QString(values.data())+ 
							" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabs WHERE"+
							" custId=" + map.value("custId") +
							" AND kabId=" + map.value("kabId") +
							" ORDER BY id DESC LIMIT 1)";
						break;
					case drawers: 
						res["query"] = 
							"INSERT INTO itemkabdrawers ("+
							QString(columns.data())+
							") SELECT "+
							QString(values.data())+
							" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawers WHERE"+
							" custId=" + map.value("custId") +
							" AND kabId=" + map.value("kabId") +
							" AND drawerCode=" + map.value("drawerCode") +
							" ORDER BY id DESC LIMIT 1)";
						break;
					case toolbins: 
						res["query"] =
							"INSERT INTO itemkabdrawerbins ("+
							QString(columns.data())+
							") SELECT "+
							QString(values.data())+
							" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawerbins WHERE"+
							" custId=" + map.value("custId") +
							" AND kabId=" + map.value("kabId") +
							" AND toolNumber=" + map.value("toolNumber") +
							" AND drawerNum=" + map.value("drawerNum") +
							" AND itemId=" + map.value("itemId") +
							" ORDER BY id DESC LIMIT 1)";
						break;
					case cribs:
					case cribconsumables:
					case cribtoollocation:
					case cribtoollockers:
					case cribtools:
					case kittools:
					case tooltransfer:
					case itemkits:
					case kitcategory:
					case kitlocation:
					case kabemployeeitemtransactions:
					case cribemployeeitemtransactions:
					case portaemployeeitemtransactions:
					case lokkaemployeeitemtransactions:
					default: 
						res["query"] =
							queryStart + "(" +
							columns +
							") VALUES (" +
							values +
							")";
						break;
					}
				}

				if (QString::fromStdString(splitQuery[0]).contains("update", Qt::CaseInsensitive) && !skipQuery) {
					ServiceHelper().WriteToLog("Parsing update to prevent altering entries not for current device");
					QStringList splitQueryForParsing = ServiceHelper::ExplodeString(res["query"], " ");
					QStringList returnVal;

					for (int i = 0; i < splitQueryForParsing.size(); i++) {
						returnVal.push_back(splitQueryForParsing[i]);

						if (splitQueryForParsing[i] != "=")
							continue;

						QString targetCol = splitQueryForParsing[i - 1];
						QString valueToUse = splitQueryForParsing[i + 1];

						if (
							(targetCol == "id") 
							|| (targetCol.contains("custId", Qt::CaseInsensitive) && !valueToUse.contains(custId.data()))
							|| (targetCol.contains(trakId.data(), Qt::CaseInsensitive) && !valueToUse.contains(idNum.data()))
							) {
							qDebug() << targetCol << ": " << valueToUse << " <> " << custId << " | " << idNum;
							skipQuery = true;
							goto parsedQuery;
						}
						
					}
					res["query"] = returnVal.join(" ");
				}
			}
			else {
				res["id"] = "0";
				res["query"] = "SHOW TABLES";
			}

		parsedQuery:
			ServiceHelper().WriteToLog("Query parsed. " + string(skipQuery ? "Query is getting skipped" : "Query is being run"));
			ServiceHelper().WriteToCustomLog("Query after being parsed: \n" + res["query"].toStdString(), timeStamp[0] + "-queries");
			if (skipQuery ? true : makeNetworkRequest(apiUrl, res))
			{
				string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 AND id = " + res["id"].toStdString();

				if (!testingDBManager) {
					ServiceHelper().WriteToCustomLog("Updating query with id: " + res["id"].toStdString(), timeStamp[0] + "-queries");
					vector queryResult = ExecuteTargetSql(sqlQuery);
					if (queryResult.size() > 0) {
						for(const auto result: queryResult)
							std::cout << result["success"].toStdString() << endl;
					}
				}
			}
			else {
				std::cout << "request failed" << endl;
				break;
			}
		}
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
	try {
		HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));

		QString dbtype = QString::fromStdString(RegistryManager().GetStrVal(hKey, "Database", REG_SZ));

		if (!QSqlDatabase::isDriverAvailable(dbtype))
		{
			ServiceHelper().WriteToError((string)("Provided Database Driver is not available"));
			RegCloseKey(hKey);
			ServiceHelper().WriteToError((string)("The following Databases are supported"));
			ServiceHelper().WriteToError(checkValidDrivers());
			return 0;
		}

		QString schema = QString::fromStdString(RegistryManager().GetStrVal(hKey, "Schema", REG_SZ));

		QSqlDatabase db;
		if (!checkValidConnections(schema))
		{

			QString server = QString::fromStdString(RegistryManager().GetStrVal(hKey, "Server", REG_SZ));
			int port = QString::fromStdString(RegistryManager().GetStrVal(hKey, "Port", REG_SZ)).toInt();
			QString user = QString::fromStdString(RegistryManager().GetStrVal(hKey, "Username", REG_SZ));
			string pass = RegistryManager().GetStrVal(hKey, "Password", REG_SZ);

			ServiceHelper().WriteToLog((string)"Creating session to db");
					
			db = QSqlDatabase::addDatabase(dbtype, schema);
			db.setHostName(server);
			db.setPort(port);
			db.setUserName(user);
			if (pass != "")
				db.setPassword(pass.data());
			db.setConnectOptions("CLIENT_COMPRESS;");
		}
		else {
			db = QSqlDatabase::database(schema);

		}
		RegCloseKey(hKey);

		if (!db.open())
		{
			ServiceHelper().WriteToError((string)"DB Connection failed to open");
			db.close();
			return 0;
		}
		ServiceHelper().WriteToLog((string)"DB Connection successfully opened");
		if (!db.driver()->hasFeature(QSqlDriver::Transactions))
		{
			ServiceHelper().WriteToError((string)"Selected Driver does not support transactions");
			db.close();
			return 0;
		}

		db.transaction();
		QSqlQuery query(db);


		query.exec("SHOW DATABASES;");
		bool dbFound = false;
		bool continueLoop = query.next();
		while (continueLoop)
		{
			QString res = query.value(0).toString();
			std::cout << res.toUtf8().data() << endl;
			if (res.toUtf8().data() == schema) {
				dbFound = true;
				continueLoop = false;
			}
			else {
				continueLoop = query.next();
			}

		}
		query.clear();

		std::cout << "Was Target DB Found? " << (dbFound ? "Yes" : "No") << endl;
		if (!dbFound) {
			ServiceHelper().WriteToLog((string)"Generating Database");
			QString targetQuery = "CREATE DATABASE " + schema + " CHARACTER SET utf8 COLLATE utf8_general_ci";
			query.prepare(targetQuery);
			if (!query.exec()) {
				ServiceHelper().WriteToError((string)"Failed to create database");
			}
			else {
				ServiceHelper().WriteToLog((string)"Successfully created Database");
			}

			while (query.next())
			{
				QString res = query.value(0).toString();
				std::cout << res.toUtf8().data() << endl;
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
		ServiceHelper().WriteToError(e.what());
		return 0;
	}
	return 1;
}

int DatabaseManager::ExecuteTargetSqlScript(string& filepath)
{
	int successCount = 0;
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	QString schema = QString::fromStdString(RegistryManager().GetStrVal(hKey, "Schema", REG_SZ));
	RegCloseKey(hKey);
	QSqlDatabase db = QSqlDatabase::database(schema);
	QFile file(filepath.data());
	try {

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

		foreach(const QString & statement, sqlStatements)
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

vector<QStringMap> DatabaseManager::ExecuteTargetSql(string sqlQuery)
{
	int successCount = 0;
	vector<QStringMap> resultVector;
	QStringMap queryResult;
	queryResult["success"] = "0";
	HKEY hKey = RegistryManager().OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	QString schema = QString::fromStdString(RegistryManager().GetStrVal(hKey, "Schema", REG_SZ));
	RegCloseKey(hKey);
	QSqlDatabase db = QSqlDatabase::database(schema);

	try {

		if (!db.open())
		{
			throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QSqlQuery query(db);

		QString sql = sqlQuery.data();

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

		if (!query.exec("USE " + schema + ";"))
			throw HenchmanServiceException("Failed to execute initial DB Query");

		foreach(const QString & statement, sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			std::cout << "Executing: " << statement.toStdString() << endl;
			if (query.exec(statement))
			{
				successCount++;
				QSqlRecord record(query.record());
				
				queryResult["success"] = QString::number(successCount);
				resultVector.push_back(queryResult);
				
				while (query.next())
				{
					queryResult.clear();
					//std::cout << "{" << std::endl;
					for (int i = 0; i <= record.count() - 1; i++)
					{
						queryResult[record.fieldName(i)] = query.value(i).toString();
						//std::cout << record.fieldName(i).toStdString() << ": " << query.value(i).toString().toStdString() << endl;
					}

					resultVector.push_back(queryResult);
					//std::cout << "}" << std::endl;
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
		qDebug() << "Request was successful: " << status << reason;
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
	
	for (const auto& key : map.keys()) {
		count--;
		if (!(key == "id" || map.value(key).isEmpty() || map.value(key) == "0"))
		{
			//std::cout << "Process Loop: " << key.toStdString() << ": " << map.value(key).toStdString() << std::endl;
			map[key] = "'" + map.value(key) + "'";
			queryKeys.append((queryKeys.size() > 0 ? ", " : "") + ("`" + key + "`"));
			queryValues.append((queryValues.size() > 0 ? ", " : "") + map.value(key));
			//conditionals.append((conditionals.size() > 0 ? " AND " : "") +key + "=" + map.value(key));
		}
		continue;
	}
	
	results[0] = queryKeys;
	results[1] = queryValues;
	//results[2] = conditionals;
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
