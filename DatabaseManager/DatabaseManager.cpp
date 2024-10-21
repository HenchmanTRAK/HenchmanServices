
#include <iostream>

#include "DatabaseManager.h"

using namespace std;

static array<string, 2> timeStamp;

static string checkValidDrivers()
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

/*
Seperate out adding database with addDatabase from reconnecting to database to do work
*/

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent)
{
	CSimpleIniA ini;
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	string installDir = GetStrVal(hKey, "Install_DIR", REG_SZ);
	SI_Error rc = ini.LoadFile(installDir.append("\\service.ini").c_str());
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}
	else {
		testingDBManager = ini.GetBoolValue("DEVELOPMENT", "testingDBManager", 0);
		queryLimit = ini.GetLongValue("API", "numberOfQueries", 10);
		apiUsername = ini.GetValue("API", "Username", "");
		apiPassword = ini.GetValue("API", "Password", "");
		defaultProtocol = ini.GetValue("API", "defaultProt", "https");
		apiUrl = ini.GetValue("API", "url", "webportal.henchmantrak.com/webapi/public/api/portals/exec_query");
	}

	std::cout << "init db manager" << endl;
	
	targetApp = "";
	requestRunning = false;
	lastToolID = GetVal(hKey, "toolCheckLimit", REG_DWORD);
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

static string parseArray(QJsonArray array)
{
	stringstream dataRes;
	if (array.count() > 0) {
		for (const auto& result : array) {
			if (result.isString())
			{
				dataRes << " - " << result.toString().toStdString() << endl;
				continue;
			}
			if (result.isObject())
			{	
				string res = parseObject(result.toObject());
				dataRes << " - " << res << endl;
				continue;
			}
			if (result.isArray())
			{
				string res = parseArray(result.toArray());
				dataRes << " - " << res << endl;
				continue;
			}
		}

		//WriteToLog("Server responded with error: " + dataRes.str());
		return dataRes.str();
	}
	return "";
}

static string parseObject(QJsonObject object)
{
	stringstream dataRes;

	if (object.keys().count() > 0) {
		for (const auto& key : object.keys()) {
			if (object.value(key).isString())
			{
				dataRes << " - " << key.toStdString() << ": " << object.value(key).toString().toStdString() << endl;
				continue;
			}
			if (object.value(key).isObject())
			{
				string res = parseObject(object.value(key).toObject());
				dataRes << " - " << key.toStdString() << ": \n\t" << res << endl;
				continue;
			}
			if (object.value(key).isArray())
			{
				string res = parseArray(object.value(key).toArray());
				dataRes << " - " << key.toStdString() << ": \n\t" << res << endl;
				continue;
			}
		}

		return dataRes.str();
	}
	return "";
}

int DatabaseManager::makeNetworkRequest(QString &url, QMap<QString, QString> &query, QJsonDocument &results)
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
		//QString query = query;
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
		//WriteToCustomLog("Webportal response: " + jsonRes.toStdString(), "queries");
		//jsonRes = jsonRes.last((jsonRes.size() - query.size()));

		int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
		int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
		std::cout << "starting index: " << startingIndex << " ending index: " << endingIndex << endl;
		/*if(startingIndex < endingIndex)
			jsonRes = jsonRes.sliced(startingIndex, endingIndex);*/

		std::cout << jsonRes.toStdString() << endl;
		optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
		if (!json) {
			ServiceHelper::WriteToLog((string)"Recieved empty data or failed to parse JSON.");
			//goto exit;
			return;
		}
		results = *json;
		string parsedVal;
		if (json->isArray())
			parsedVal = parseArray(json->array());
		else if (json->isObject())
			parsedVal = parseObject(json->object());

		ServiceHelper::WriteToCustomLog("Webportal response: " + parsedVal, timeStamp[0] + "-queries");
		ServiceHelper::WriteToLog("Server responded with: \n" + parsedVal);

		result = reply.isSuccess();

		if (reply.isSuccess())
		{
			std::cout << "network request success" << endl;
			//string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 AND id = " + query["id"].toStdString();
			//	//+ " ORDER BY id LIMIT " + QString::number(queryLimit).toStdString();
			//if (!testingDBManager) {
			//	ServiceHelper::WriteToCustomLog("Updating query with id: " + query["id"].toStdString(), timeStamp[0] + "-queries");
			//	ExecuteTargetSql(targetApp, sqlQuery);
			//}
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

int DatabaseManager::AddToolsIfNotExists(string url, string query)
{
	vector rowCheck = ExecuteTargetSql(targetApp, "SELECT COUNT(*) FROM tools");
	if (rowCheck[1][rowCheck[1].firstKey()].toInt() <= lastToolID)
	{
		return 0;
	}

	query += " LIMIT " + to_string(lastToolID) + ", " + to_string(200);
	vector sqlQueryResults = ExecuteTargetSql(targetApp, query);
	
	performCleanup();

	netManager = new QNetworkAccessManager(this);
	if (!testingDBManager)
		netManager->setStrictTransportSecurityEnabled(true);
	netManager->setAutoDeleteReplies(true);
	netManager->setTransferTimeout(30000);
	connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

	restManager = new QRestAccessManager(netManager, this);

	QString targetUrl = defaultProtocol + "://" + apiUrl;
	
	for (const auto  result : sqlQueryResults) {
		if (result.firstKey() == "success")
			continue;
		QMap<QString, QString> res;
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
			"' LIMIT 1)";

		QJsonDocument reply;
		if (makeNetworkRequest(targetUrl, res, reply)) {
			if (!reply.isObject())
				continue;
			QJsonObject result = reply.object();
			if (ServiceHelper::Contain(result["result"].toString(), "1 rows were affected")) {
				std::cout << result["result"].toString().toStdString() << endl;
			}
			else
			{
				std::cout << "No rows were altered on db" << endl;
				Sleep(100);
			}
			lastToolID++;
			
		}

	}
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\HenchmanService"));
	SetVal(hKey, "toolCheckLimit", lastToolID, REG_DWORD);
	RegCloseKey(hKey);
	performCleanup();
	return 0;
}

int DatabaseManager::connectToRemoteDB (string &target_app)
{
	ServiceHelper::WriteToLog(string("Attempting to connect to Remote Database"));
	timeStamp = ServiceHelper::timestamp();
	targetApp = target_app;
	QString targetSchema;
	QSqlDatabase db;
	try {
		HKEY hKeyLocal = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
		targetSchema = QString::fromStdString(GetStrVal(hKeyLocal, "Schema", REG_SZ));
		RegCloseKey(hKeyLocal);
		if (!checkValidConnections(targetSchema))
		{
			throw HenchmanServiceException("Provided schema not valid");
		}
	
		HKEY hKeyCloud = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Cloud"));

		//QString dbUrl = QString::fromStdString(GetStrVal(hKeyCloud, "url", REG_SZ));
		QString dbUrl = defaultProtocol + "://" + apiUrl;

		RegCloseKey(hKeyCloud);

		if (dbUrl.trimmed().isEmpty()) {
			throw HenchmanServiceException("No target Database Url provided");
		}
		
		ServiceHelper::WriteToLog("Creating session to db " + targetSchema.toStdString());

		
		db = QSqlDatabase::database(targetSchema);
		//db.database(targetSchema);
		
		if (!db.open())
		{
			throw HenchmanServiceException("Failed to open DB Connection");
		}

		//db.transaction();


		requestRunning = true;

		performCleanup();

		netManager = new QNetworkAccessManager(this);
		if (!testingDBManager)
			netManager->setStrictTransportSecurityEnabled(true);
		netManager->setAutoDeleteReplies(true);
		netManager->setTransferTimeout(30000);
		connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);

		restManager = new QRestAccessManager(netManager, this);
		

		vector<QMap<QString, QString>> queries;
		QSqlQuery query(db);
		QString queryText = testingDBManager ? "SHOW TABLES" : "SELECT * FROM cloudupdate WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit);
		query.prepare(queryText);
		if (!query.exec())
		{
			query.finish();
			ServiceHelper::WriteToLog(string("Closing DB Session"));
			throw HenchmanServiceException("Failed to exec query: " + query.executedQuery().toStdString());
		}

		ServiceHelper::WriteToLog("Updating backend Database with url: " + dbUrl.toStdString());
		ServiceHelper::WriteToCustomLog("Starting network requests to: " + dbUrl.toStdString(), timeStamp[0] + "-queries");
		
		bool continueLoop = query.next();
		int count = 0;

		while (continueLoop)
		{
			count++;
			
			QMap<QString, QString> res;
			
			if (!testingDBManager) {
				res["id"] = query.value(0).toString();
				res["query"] = query
					.value(2)
					.toString()
					.replace(
						QRegularExpression("(NOW|CURDATE|CURTIME)+", 
							QRegularExpression::MultilineOption | 
							QRegularExpression::DotMatchesEverythingOption | 
							QRegularExpression::UseUnicodePropertiesOption
						), 
						"\'" + 
						query
						.value(3)
						.toString()
						.replace("T", " "
						) + "\'"
					)
					.replace(
						"()", 
						""
					);
				//string str = res["query"].toStdString();

				/*QString::iterator new_end =
					unique(res["query"].begin(), res["query"].end(),
						[=](QChar lhs, QChar rhs) { return (lhs == rhs) && (lhs == ' '); }
					);
				res["query"].erase(new_end, res["query"].end());
				cout << res["query"].toStdString() << endl;*/

				//res["query"] = str.c_str();

				/*string queryToSanitize = res["query"].toStdString();
				removeQuotes(queryToSanitize);
				res["query"] = queryToSanitize.data();*/
			}
			else {
				res["id"] = "0";
				res["query"] = "SHOW TABLES";
			}
			res["number"] = QString::number(count);
			QJsonDocument reply;
			if (makeNetworkRequest(dbUrl, res, reply))
			{
				cout << "request successful" << endl;
				string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 AND id = " + res["id"].toStdString();
				//+ " ORDER BY id LIMIT " + QString::number(queryLimit).toStdString();
				if (!testingDBManager) {
					ServiceHelper::WriteToCustomLog("Updating query with id: " + res["id"].toStdString(), timeStamp[0] + "-queries");
					vector queryResult = ExecuteTargetSql(targetApp, sqlQuery);
					if (queryResult.size() > 0) {
						for(const auto result: queryResult)
							cout << result["success"].toStdString() << endl;
					}
				}
			}
			else {
				cout << "request failed" << endl;
			}

			continueLoop = testingDBManager ? count < 5 : query.next();
		}
		ServiceHelper::WriteToCustomLog("Finished network requests", timeStamp[0] + "-queries");

		query.clear();
		query.finish();
		
		db.close();
		performCleanup();
		//connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);
		//connect(restManager->networkAccessManager(), &QNetworkAccessManager::finished, &requestLoop, &QEventLoop::quit);
		
	}
	catch (exception& e)
	{
		if(db.isOpen())
			db.close();
		ServiceHelper::WriteToError("DatabaseManager::connectToRemoteDB has thrown an exception: " + string(e.what()));
		//connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);
		QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
		return 0;
	}
	//connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);
	QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
	return 1;
}

int DatabaseManager::connectToLocalDB(string& target_app)
{
	try {
		targetApp = target_app;
		HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));

		QString dbtype = QString::fromStdString(GetStrVal(hKey, "Database", REG_SZ));

		if (!QSqlDatabase::isDriverAvailable(dbtype))
		{
			ServiceHelper::WriteToError((string)("Provided Database Driver is not available"));
			RegCloseKey(hKey);
			ServiceHelper::WriteToError((string)("The following Databases are supported"));
			ServiceHelper::WriteToError(checkValidDrivers());
			return 0;
		}

		QString schema = QString::fromStdString(GetStrVal(hKey, "Schema", REG_SZ));

		QSqlDatabase db;
		if (!checkValidConnections(schema))
		{

			QString server = QString::fromStdString(GetStrVal(hKey, "Server", REG_SZ));
			int port = QString::fromStdString(GetStrVal(hKey, "Port", REG_SZ)).toInt();
			QString user = QString::fromStdString(GetStrVal(hKey, "Username", REG_SZ));
			string pass = GetStrVal(hKey, "Password", REG_SZ);

			//cout << "Pulled the following values from registy: " << server.toUtf8().data() << " " << port << " " << schema.toUtf8().data() << " " << user.toUtf8().data() << " " << (pass != "" ? decodeBase64(pass) : pass.data()) << endl;
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

int DatabaseManager::ExecuteTargetSqlScript(string& targetApp, string& filename)
{
	int successCount = 0;
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	QString schema = QString::fromStdString(GetStrVal(hKey, "Schema", REG_SZ));
	RegCloseKey(hKey);
	QSqlDatabase db = QSqlDatabase::database(schema);
	QFile file(filename.data());
	try {

		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			throw HenchmanServiceException("Failed to open target file");
		}
		//std::cout << "Successfully opened target file: " << filename << endl;
		ServiceHelper::WriteToLog("Successfully opened target file: " + filename);

		QTextStream in(&file);
		QString sql = in.readAll();

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

		if (!db.open())
		{
			throw HenchmanServiceException("Failed to open DB Connection");
		}

		db.transaction();
		QSqlQuery query(db);

		if (!query.exec("USE " + schema + ";")) {
			/*cout << "Failed to execute initial DB Query" << endl;
			return 0;*/
			throw HenchmanServiceException("Failed to execute initial DB Query");
		}

		foreach(const QString & statement, sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			if (query.exec(statement))
				successCount++;
			else {
				//qDebug() << "Failed: " << statement << "\nReason: " << query.lastError();
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
		//return 0;
	}
	return successCount;
}

vector<QMap<QString, QString>> DatabaseManager::ExecuteTargetSql(string& targetApp, string sqlQuery)
{
	int successCount = 0;
	vector<QMap<QString, QString>> resultVector;
	QMap<QString, QString> queryResult;
	queryResult["success"] = "0";
	resultVector.push_back( queryResult);
	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	QString schema = QString::fromStdString(GetStrVal(hKey, "Schema", REG_SZ));
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

		if (!query.exec("USE " + schema + ";")) {
			/*WriteToError((string)"Failed to execute initial DB Query");
			return 0;*/
			throw HenchmanServiceException("Failed to execute initial DB Query");
		}

		foreach(const QString & statement, sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			cout << "Executing: " << statement.toStdString() << endl;
			if (query.exec(statement))
			{
				successCount++;
				QSqlRecord record(query.record());
				
				queryResult["success"] = QString::number(successCount);
				resultVector[0] = queryResult;
				
				while (query.next())
				{
					queryResult.clear();
					cout << query.size() << endl;
					cout << successCount << endl;
					cout << record.count() << endl;

					for (int i = 0; i <= record.count() - 1; i++)
					{
						queryResult[record.fieldName(i)] = query.value(i).toString();
						cout << record.fieldName(i).toStdString() << ": " << query.value(i).toString().toStdString() << endl;
					}

					resultVector.push_back(queryResult);
				}
			}
			else {
				/*qDebug() << "Failed: " << statement << "\nReason: " << query.lastError();
				WriteToError("Failed: " + statement.toStdString() + "\nReason: " + query.lastError().text().toStdString());*/
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
	//string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit).toStdString();
	ExecuteTargetSql(targetApp, sqlQuery);

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
	/*form->deleteLater();
	netReply->deleteLater();
	networkManager->deleteLater();*/
	
	/*form = nullptr;
	netReply = nullptr;*/

	requestRunning = false;
	/*networkManager->clearAccessCache();
	networkManager->clearConnectionCache();*/
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
