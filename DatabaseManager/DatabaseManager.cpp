
#include <iostream>

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
	CSimpleIniA ini;
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	string installDir = RegistryManager::GetStrVal(hKey, "INSTALLDIR", REG_SZ);
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
	}

	std::cout << "init db manager" << endl;
	
	targetApp = "";
	requestRunning = false;
	numToolsChecked = RegistryManager::GetVal(hKey, "numToolsChecked", REG_DWORD);
	numKabsChecked = RegistryManager::GetVal(hKey, "numKabsChecked", REG_DWORD);
	numDrawersChecked = RegistryManager::GetVal(hKey, "numDrawersChecked", REG_DWORD);
	numToolsInDrawersChecked = RegistryManager::GetVal(hKey, "numToolsInDrawersChecked", REG_DWORD);
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

int DatabaseManager::makeNetworkRequest(QString &url, QStringMap &query, QJsonDocument &results)
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

	ServiceHelper::WriteToCustomLog("Running query number: " + query["number"].toStdString() + " \n query: " + doc.toJson().toStdString(), timeStamp[0]+ "-queries");

	QNetworkReply* reply = restManager->post(request, doc, this, [this, &result, &results](QRestReply& reply) {
		std::cout << "networkrequested" << endl;
		if (reply.error() != QNetworkReply::NoError) {
			qWarning() << "A Network error has occured: " << reply.error() << reply.errorString();
			ServiceHelper::WriteToError("A Network error has occured: " + reply.errorString().toStdString());
			return;
		}

		int status = reply.httpStatus();
		QString reason = reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		
		if (!reply.isHttpStatusSuccess()) {
			qWarning() << "A HTTP error has occured: " << status << reason;
			ServiceHelper::WriteToLog("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");
		}

		if (reply.isHttpStatusSuccess()) {
			qDebug() << "Request was successful: " << status << reason;
			ServiceHelper::WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");
		}
		ServiceHelper::WriteToLog((string)"Parsing Response");
		
		QByteArray jsonRes = reply.readBody();

		int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
		int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
		std::cout << "starting index: " << startingIndex << " ending index: " << endingIndex << endl;
		/*if(startingIndex < endingIndex)
			jsonRes = jsonRes.sliced(startingIndex, endingIndex);*/

		//std::cout << jsonRes.toStdString() << endl;
		optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
		if (!json) {
			ServiceHelper::WriteToLog((string)"Recieved empty data or failed to parse JSON.");
			return;
		}
		results = *json;
		string parsedVal;
		if (json->isArray())
			parsedVal = parseData(json->array());
		else if (json->isObject())
			parsedVal = parseData(json->object());

		ServiceHelper::WriteToCustomLog("Webportal response: \n" + parsedVal, timeStamp[0] + "-queries");
		ServiceHelper::WriteToLog("Server responded with: \n" + parsedVal);

		result = reply.isSuccess();

		if (reply.isSuccess())
		{
			std::cout << "network request success" << endl;
		}
		else {
			std::cout << "network request failed" << endl;
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

int DatabaseManager::AddToolsIfNotExists()
{
	timeStamp = ServiceHelper::timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM tools");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= numToolsChecked)
	{
		return 0;
	}

	string query = 
		"SELECT * from tools ORDER BY id ASC LIMIT " + 
		to_string(numToolsChecked) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);
	
	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);
	
	for (const auto  result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];
		QString queryKeys;
		QString queryValues;
		vector<QString> keys;
		vector<QString> values;

		for (const auto & key : result.keys()) {
			if (key == "id" || result[key] == "" || result[key] == "0")
			{
				continue;
			}
			keys.push_back(key);
			values.push_back(result.value(key));
			queryKeys += "`"+key + "`, ";
			
			queryValues += "'" + result[key] + "', ";
		}
		if(queryKeys.size()	> 0 && queryKeys[queryKeys.size() - 2].toLatin1() == ',')
			queryKeys.chop(2);

		if (queryValues.size() > 0 && queryValues[queryValues.size() - 2].toLatin1() == ',')
			queryValues.chop(2);

		res["query"] = "INSERT INTO tools (" +
			queryKeys + 
			") SELECT " + 
			queryValues + 
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM tools WHERE custId = '" + 
			result.value("custId") + 
			"' AND PartNo = '" + 
			result.value("PartNo") + 
			"' AND description = '" + 
			result.value("description") + 
			"' AND stockcode = '" + 
			result.value("stockcode") + 
			"' ORDER BY id DESC LIMIT 1)";

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper::Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
				numToolsChecked++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			Sleep(100);
			numToolsChecked += queryLimit;
			break;
		}

	}
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numToolsChecked", numToolsChecked, REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//performCleanup();
	return 1;
}

int DatabaseManager::AddKabsIfNotExists()
{
	timeStamp = ServiceHelper::timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabs");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= numKabsChecked)
	{
		return 0;
	}

	string query =
		"SELECT * from itemkabs ORDER BY id ASC LIMIT " +
		to_string(numKabsChecked) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	for (const auto result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];
		QString queryKeys;
		QString queryValues;
		vector<QString> keys;
		vector<QString> values;

		for (const auto& key : result.keys()) {
			if (key == "id" || result[key] == "" || result[key] == "0")
			{
				continue;
			}
			keys.push_back(key);
			values.push_back(result.value(key));
			queryKeys += "`" + key + "`, ";

			queryValues += "'" + result[key] + "', ";
		}
		if (queryKeys.size() > 0 && queryKeys[queryKeys.size() - 2].toLatin1() == ',')
			queryKeys.chop(2);

		if (queryValues.size() > 0 && queryValues[queryValues.size() - 2].toLatin1() == ',')
			queryValues.chop(2);

		res["query"] = "INSERT INTO itemkabs (" +
			queryKeys +
			") SELECT " +
			queryValues +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabs WHERE custId = '" +
			result.value("custId") +
			"' AND kabId = '" +
			result.value("kabId") +
			"' ORDER BY id DESC LIMIT 1)";

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper::Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
				numKabsChecked++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			Sleep(100);
			numKabsChecked += queryLimit;
			break;
		}

	}
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numKabsChecked", numKabsChecked, REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	//performCleanup();
	return 1;
}

int DatabaseManager::AddDrawersIfNotExists()
{
	timeStamp = ServiceHelper::timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabdrawers");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= numDrawersChecked)
	{
		return 0;
	}

	string query =
		"SELECT * from itemkabdrawers ORDER BY id ASC LIMIT " +
		to_string(numDrawersChecked) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	for (const auto result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];
		QString queryKeys;
		QString queryValues;
		vector<QString> keys;
		vector<QString> values;

		for (const auto& key : result.keys()) {
			if (key == "id" || result[key] == "" || result[key] == "0")
			{
				continue;
			}
			keys.push_back(key);
			values.push_back(result.value(key));
			queryKeys += "`" + key + "`, ";

			queryValues += "'" + result[key] + "', ";
		}
		if (queryKeys.size() > 0 && queryKeys[queryKeys.size() - 2].toLatin1() == ',')
			queryKeys.chop(2);

		if (queryValues.size() > 0 && queryValues[queryValues.size() - 2].toLatin1() == ',')
			queryValues.chop(2);

		res["query"] = "INSERT INTO itemkabdrawers (" +
			queryKeys +
			") SELECT " +
			queryValues +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawers WHERE custId = '" +
			result.value("custId") +
			"' AND kabId = '" +
			result.value("kabId") +
			"' AND remarks = '" +
			result.value("remarks") +
			"' AND drawerCode = '" +
			result.value("drawerCode") +
			"' ORDER BY id DESC LIMIT 1)";

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper::Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
				numDrawersChecked++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			Sleep(100);
			numDrawersChecked += queryLimit;
			break;
		}

	}
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numDrawersChecked", numDrawersChecked, REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

int DatabaseManager::AddToolsInDrawersIfNotExists()
{
	timeStamp = ServiceHelper::timestamp();
	vector rowCheck = ExecuteTargetSql("SELECT COUNT(*) FROM itemkabdrawerbins");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= numToolsInDrawersChecked)
	{
		return 0;
	}

	string query =
		"SELECT * from itemkabdrawerbins ORDER BY id ASC LIMIT " +
		to_string(numToolsInDrawersChecked) + ", " + to_string(queryLimit);
	vector sqlQueryResults = ExecuteTargetSql(query);

	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	for (const auto result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QStringMap res;
		res["id"] = result["id"];
		QString queryKeys;
		QString queryValues;
		vector<QString> keys;
		vector<QString> values;

		for (const auto& key : result.keys()) {
			if (key == "id" || result[key] == "" || result[key] == "0")
			{
				continue;
			}
			keys.push_back(key);
			values.push_back(result.value(key));
			queryKeys += "`" + key + "`, ";

			queryValues += "'" + result[key] + "', ";
		}
		if (queryKeys.size() > 0 && queryKeys[queryKeys.size() - 2].toLatin1() == ',')
			queryKeys.chop(2);

		if (queryValues.size() > 0 && queryValues[queryValues.size() - 2].toLatin1() == ',')
			queryValues.chop(2);

		res["query"] = "INSERT INTO itemkabdrawerbins (" +
			queryKeys +
			") SELECT " +
			queryValues +
			" FROM DUAL WHERE NOT EXISTS (SELECT * FROM itemkabdrawerbins WHERE custId = '" +
			result.value("custId") +
			"' AND kabId = '" +
			result.value("kabId") +
			"' AND itemId = '" +
			result.value("itemId") +
			"' AND drawerNum = '" +
			result.value("drawerNum") +
			"' AND toolNumber = '" +
			result.value("toolNumber") +
			"' ORDER BY id DESC LIMIT 1)";

		QJsonDocument reply;
		if (makeNetworkRequest(apiUrl, res, reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper::Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
				numToolsInDrawersChecked++;
				continue;
			}
			std::cout << "No rows were altered on db" << endl;
			Sleep(100);
			numToolsInDrawersChecked += queryLimit;
			
			break;
		}

	}
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	RegistryManager::SetVal(hKey, "numToolsInDrawersChecked", numToolsInDrawersChecked, REG_DWORD);
	RegCloseKey(hKey);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

int DatabaseManager::connectToRemoteDB()
{
	ServiceHelper::WriteToLog(string("Attempting to connect to Remote Database"));
	timeStamp = ServiceHelper::timestamp();
	QString targetSchema;
	QSqlDatabase db;
	bool result = false;
	try {
		HKEY hKeyLocal = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
		targetSchema = QString::fromStdString(RegistryManager::GetStrVal(hKeyLocal, "Schema", REG_SZ));
		RegCloseKey(hKeyLocal);

		if (!checkValidConnections(targetSchema))
			throw HenchmanServiceException("Provided schema not valid");
	
		/*HKEY hKeyCloud = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Cloud"));

		RegCloseKey(hKeyCloud);*/

		if (apiUrl.trimmed().isEmpty())
			throw HenchmanServiceException("No target Database Url provided");
		
		ServiceHelper::WriteToLog("Creating session to db " + targetSchema.toStdString());
		
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
			ServiceHelper::WriteToLog(string("Closing DB Session"));
			throw HenchmanServiceException("Failed to exec query: " + query.executedQuery().toStdString());
		}

		ServiceHelper::WriteToLog("Updating backend Database with url: " + apiUrl.toStdString());
		ServiceHelper::WriteToCustomLog("Starting network requests to: " + apiUrl.toStdString(), timeStamp[0] + "-queries");
		
		bool continueLoop = query.next();
		int count = 0;

		while (continueLoop)
		{
			count++;
			
			QStringMap res;
			
			if (!testingDBManager) {
				res["id"] = query.value(0).toString();
				res["query"] = query
					.value(2)
					.toString()
					.replace(
						QRegularExpression(
							"(NOW|CURDATE|CURTIME)+", 
							QRegularExpression::MultilineOption | 
							QRegularExpression::DotMatchesEverythingOption | 
							QRegularExpression::UseUnicodePropertiesOption
						), 
						"\'" + query
						.value(3)
						.toString()
						.replace("T", " ") + "\'"
					)
					.replace("()", "");

			}
			else {
				res["id"] = "0";
				res["query"] = "SHOW TABLES";
			}
			res["number"] = QString::number(count);
			QJsonDocument reply;
			if (makeNetworkRequest(apiUrl, res, reply))
			{
				std::cout << "request successful" << endl;
				string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 AND id = " + res["id"].toStdString();

				if (!testingDBManager) {
					ServiceHelper::WriteToCustomLog("Updating query with id: " + res["id"].toStdString(), timeStamp[0] + "-queries");
					vector queryResult = ExecuteTargetSql(sqlQuery);
					if (queryResult.size() > 0) {
						for(const auto result: queryResult)
							std::cout << result["success"].toStdString() << endl;
					}
				}
			}
			else {
				std::cout << "request failed" << endl;
			}

			continueLoop = testingDBManager ? count < 5 : query.next();
		}
		ServiceHelper::WriteToCustomLog("Finished network requests", timeStamp[0] + "-queries");

		query.clear();
		query.finish();
		
		db.close();
		performCleanup();
		result = true;
		
	}
	catch (exception& e)
	{
		if(db.isOpen())
			db.close();
		ServiceHelper::WriteToError("DatabaseManager::connectToRemoteDB has thrown an exception: " + string(e.what()));
	}

	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);

	return result;
}

int DatabaseManager::connectToLocalDB()
{
	timeStamp = ServiceHelper::timestamp();
	try {
		HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));

		QString dbtype = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Database", REG_SZ));

		if (!QSqlDatabase::isDriverAvailable(dbtype))
		{
			ServiceHelper::WriteToError((string)("Provided Database Driver is not available"));
			RegCloseKey(hKey);
			ServiceHelper::WriteToError((string)("The following Databases are supported"));
			ServiceHelper::WriteToError(checkValidDrivers());
			return 0;
		}

		QString schema = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Schema", REG_SZ));

		QSqlDatabase db;
		if (!checkValidConnections(schema))
		{

			QString server = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Server", REG_SZ));
			int port = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Port", REG_SZ)).toInt();
			QString user = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Username", REG_SZ));
			string pass = RegistryManager::GetStrVal(hKey, "Password", REG_SZ);

			ServiceHelper::WriteToLog((string)"Creating session to db");
					
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
			ServiceHelper::WriteToError((string)"DB Connection failed to open");
			db.close();
			return 0;
		}
		ServiceHelper::WriteToLog((string)"DB Connection successfully opened");
		if (!db.driver()->hasFeature(QSqlDriver::Transactions))
		{
			ServiceHelper::WriteToError((string)"Selected Driver does not support transactions");
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
			ServiceHelper::WriteToLog((string)"Generating Database");
			QString targetQuery = "CREATE DATABASE " + schema + " CHARACTER SET utf8 COLLATE utf8_general_ci";
			query.prepare(targetQuery);
			if (!query.exec()) {
				ServiceHelper::WriteToError((string)"Failed to create database");
			}
			else {
				ServiceHelper::WriteToLog((string)"Successfully created Database");
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
		ServiceHelper::WriteToError("DatabaseManager::connectToLocalDB has thrown an exception: " + string(e.what()));
		return 0;
	}
	return 1;
}

int DatabaseManager::ExecuteTargetSqlScript(string& filepath)
{
	int successCount = 0;
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	QString schema = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Schema", REG_SZ));
	RegCloseKey(hKey);
	QSqlDatabase db = QSqlDatabase::database(schema);
	QFile file(filepath.data());
	try {

		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) 
			throw HenchmanServiceException("Failed to open target file");

		ServiceHelper::WriteToLog("Successfully opened target file: " + filepath);

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
		ServiceHelper::WriteToError("DatabaseManager::ExecuteTargetSqlScript has thrown an exception: " + string(e.what()));
	}
	return successCount;
}

vector<QStringMap> DatabaseManager::ExecuteTargetSql(string sqlQuery)
{
	int successCount = 0;
	vector<QStringMap> resultVector;
	QStringMap queryResult;
	queryResult["success"] = "0";
	resultVector.push_back( queryResult);
	HKEY hKey = RegistryManager::OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	QString schema = QString::fromStdString(RegistryManager::GetStrVal(hKey, "Schema", REG_SZ));
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
				resultVector[0] = queryResult;
				
				while (query.next())
				{
					queryResult.clear();

					for (int i = 0; i <= record.count() - 1; i++)
					{
						queryResult[record.fieldName(i)] = query.value(i).toString();
						std::cout << record.fieldName(i).toStdString() << ": " << query.value(i).toString().toStdString() << endl;
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
		ServiceHelper::WriteToError("DatabaseManager::ExecuteTargetSql has thrown an exception: " + string(e.what()));
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
		ServiceHelper::WriteToError("A Network error has occured: " + restReply.errorString().toStdString());
		goto exit;
	}
	if (!restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		qWarning() << "A HTTP error has occured: " << status << reason;
		ServiceHelper::WriteToError("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	if (restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		qDebug() << "Request was successful: " << status << reason;
		ServiceHelper::WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	ServiceHelper::WriteToLog((string)"Parsing Response");

	ExecuteTargetSql(sqlQuery);

	if (!json) {
		ServiceHelper::WriteToError((string)"Recieved empty data or failed to parse JSON.");
		goto exit;
	}

	if (response.value()["error"].toArray().count() > 0) {
		for (const auto& result : response.value()["error"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				errorRes << " - " << result.toArray()[i].toString().toStdString() << endl;
			}
		}

		ServiceHelper::WriteToError("Server responded with error: " + errorRes.str());
	}

	if (response.value()["data"].toArray().count() > 0) {
		for (const auto& result : response.value()["data"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				dataRes << " - " << result.toArray()[i].toString().toStdString() << endl;
			}

		}
		ServiceHelper::WriteToLog("Server responded with data: " + dataRes.str());
	}

exit:
	json.reset();
	performCleanup();

	requestRunning = false;

	return;
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
