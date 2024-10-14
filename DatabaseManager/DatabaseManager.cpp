
#include <iostream>

#include "DatabaseManager.h"

using namespace std;

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
		cout << " - " << str.toUtf8().data() << endl;
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
	cout << "init db manager" << endl;
	targetApp = "";
	requestRunning = false;
	RegCloseKey(hKey);
}

DatabaseManager::~DatabaseManager() 
{
	cout << "Deleting DatabaseManager" << endl;
}

bool DatabaseManager::isInternetConnected()
{
	QTcpSocket* sock = new QTcpSocket(this);
	sock->connectToHost("www.google.com", 80);
	bool connected = sock->waitForConnected(30000);//ms

	if (!connected)
	{
		sock->abort();
		sock->deleteLater();
		//sock = nullptr;
		return false;
	}
	sock->close();
	sock->deleteLater();
	//sock = nullptr;
	return true;
}

void  DatabaseManager::makeNetworkRequest(QUrl &url, QMap<QString, QString> &query)
{
	QEventLoop loop(this);
	/*QUrlQuery urlQuery;
	if(!query["query"].endsWith(';'))
		query["query"].push_back(';');
	cout << query["query"].toStdString() << endl;
	urlQuery.addQueryItem("sql", query["query"]);
	url.setQuery(urlQuery);*/

	QNetworkRequest request(url);
	QString concatenated = apiUsername+":"+apiPassword;
	QByteArray data = concatenated.toLocal8Bit().toBase64();
	QString headerData = "Basic " + data;
	request.setRawHeader("Authorization", headerData.toLocal8Bit());
	//netManager->setStrictTransportSecurityEnabled(true);
	//restManager->time
	//netManager->deleteLater();
	//connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);
	//restManager->deleteLater();
	QJsonDocument doc = QJsonDocument::fromJson("{\"sql\": \"" + query["query"].toUtf8() + "\"}");
	//doc.object()["sql"] = query["query"];
	QNetworkReply* reply = restManager->post(request, doc, this, [this, query](QRestReply& reply) {
		std::cout << "networkrequested" << endl;
		//QString query = query;
		if (reply.error() != QNetworkReply::NoError) {
			qWarning() << "A Network error has occured: " << reply.error() << reply.errorString();
			WriteToError("A Network error has occured: " + reply.errorString().toStdString());
			return;
		}

		WriteToLog((string)"Parsing Response");
		QByteArray jsonRes = reply.readBody();
		WriteToCustomLog("Full response: " + jsonRes.toStdString(), "networkLog");
		//jsonRes = jsonRes.last((jsonRes.size() - query.size()));
		jsonRes = jsonRes.sliced(
			jsonRes.lastIndexOf('>') < 0 ? 0 : jsonRes.lastIndexOf('>'), 
			jsonRes.lastIndexOf(']')<0 ? jsonRes.size() : jsonRes.lastIndexOf(']')
		);
		cout << jsonRes.toStdString() << endl;
		optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
		optional <QJsonArray> response = json->array();
		cout << response.value().count() << endl;
		stringstream errorRes;
		stringstream dataRes;
		if (!json) {
			WriteToError((string)"Recieved empty data or failed to parse JSON.");
			//goto exit;
			return;
		}
		if (!reply.isHttpStatusSuccess()) {
			int status = reply.httpStatus();
			QString reason = reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
			qWarning() << "A HTTP error has occured: " << status << reason;
			WriteToError("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");

			if (response.value().count() > 0) {
				for (const auto& result : response.value()) {
					if (result.isArray())
						for (auto i = 0; i < result.toArray().size(); i++) {
							dataRes << " - " << result.toArray().at(i).toString().toStdString() << endl;
						}
					if (result.isObject())
						for (const auto& key : result.toObject().keys()) {
							dataRes << " - " << result.toObject().value(key).toString().toStdString() << endl;
						}
				}

				WriteToError("Server responded with error: " + errorRes.str());
			}
		}
		if (reply.isHttpStatusSuccess()) {
			int status = reply.httpStatus();
			QString reason = reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
			qDebug() << "Request was successful: " << status << reason;
			WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");

			if (response.value().count() > 0) {
				for (const auto& result : response.value()) {
					if (result.isArray())
						for (auto i = 0; i < result.toArray().size(); i++) {
							dataRes << " - " << result.toArray().at(i).toString().toStdString() << endl;
						}
					if (result.isObject())
						for (const auto& key : result.toObject().keys()) {
							dataRes << " - " << key.toStdString() << ": " << result.toObject().value(key).toString().toStdString() << endl;
						}
				}
				WriteToLog("Server responded with data: " + dataRes.str());
			}

		}

		errorRes.clear();
		dataRes.clear();

		if (reply.isSuccess())
		{
			std::cout << "network request success" << endl;
			string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 AND id = " + query["id"].toStdString();
				//+ " ORDER BY id LIMIT " + QString::number(queryLimit).toStdString();
			if (!testingDBManager) {
				WriteToCustomLog("Updating entry with: " + sqlQuery, "queries");
				ExecuteTargetSql(targetApp, sqlQuery);
			}
			//reply.readJson();
		}
		else {
			std::cout << "network request failed" << endl;
		}
		//exit:
			//reply.networkReply()->deleteLater();
			//QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
		});
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
}

int DatabaseManager::connectToRemoteDB (string &target_app)
{
	
	WriteToLog(string("Attempting to connect to Remote Database"));
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
		//dbUrl = "https://webportal.henchmantrak.com/files/ntunnel_mysql.php";
		//dbUrl = "http://webportal.henchmantrak.com/webapi/public/api/employees/7";
		//dbUrl = defaultProtocol + "://webportal.henchmantrak.com/webapi/public/api/portals/exec_query";
		//dbUrl = "https://localhost/ntunnel_mysql.php";
		//dbUrl = defaultProtocol + "://localhost/webapi/public/api/portals/exec_query";

		RegCloseKey(hKeyCloud);
		QUrl url(dbUrl);
		if (dbUrl.trimmed().isEmpty()) {
			/*WriteToLog("No target Database Url provided");
			return 0;*/
			throw HenchmanServiceException("No target Database Url provided");
		}
		
		WriteToLog("Creating session to db " + targetSchema.toStdString());

		
		db = QSqlDatabase::database(targetSchema);
		//db.database(targetSchema);
		
		if (!db.open())
		{
			/*WriteToError(string("Failed to open DB Connection"));
			return 0;*/
			throw HenchmanServiceException("Failed to open DB Connection");
		}

		//db.transaction();


		requestRunning = true;

		if (netManager)
		{
			netManager->deleteLater();
			netManager = nullptr;
		}
		if (restManager)
		{
			restManager->deleteLater();
			restManager = nullptr;
		}
		netManager = new QNetworkAccessManager(this);
		if(!testingDBManager)
			netManager->setStrictTransportSecurityEnabled(true);
		netManager->setAutoDeleteReplies(true);
		netManager->setTransferTimeout(30000);
		restManager = new QRestAccessManager(netManager, this);

		vector<QMap<QString, QString>> queries;
		//vector<QString> queries;
		QSqlQuery query(db);
		QString queryText = testingDBManager ? "SHOW TABLES" : "SELECT * FROM cloudupdate WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit);
		query.prepare(queryText);
		if (!query.exec())
		{
			//query.clear();
			query.finish();
			WriteToLog(string("Closing DB Session"));
			/*WriteToError(string("Failed to exec query to cloudupdate table"));
			return 0;*/
			throw HenchmanServiceException("Failed to exec query: " + query.executedQuery().toStdString());
		}

		bool continueLoop = query.next();
		int count = 0;
		//QEventLoop requestLoop(this);
		cout << "Updating backend Database with url: " << dbUrl.toStdString() << endl;
		while (continueLoop)
		{
			count++;
			/*QString id;
			QString res;*/
			QMap<QString, QString> res;
			//QString res;
			if (!testingDBManager) {
				res["id"] = query.value(0).toString();
				res["query"] = query.value(2).toString().replace(QRegularExpression("(NOW|CURDATE|CURTIME)+", QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption | QRegularExpression::UseUnicodePropertiesOption), "\'" + query.value(3).toString().replace("T", " ") + "\'").replace("()", "");
			}
			else {
				res["id"] = "0";
				res["query"] = "SHOW TABLES";
			}
			WriteToCustomLog("Query Result: " + res["query"].toStdString(), "queries");
			makeNetworkRequest(url, res);
			//queries.push_back(res);
			continueLoop = testingDBManager ? count < 5 : query.next();
			
		}
		query.clear();
		query.finish();
		
		db.close();
		connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);
		//connect(restManager->networkAccessManager(), &QNetworkAccessManager::finished, &requestLoop, &QEventLoop::quit);
		
	}
	catch (exception& e)
	{
		if(db.isOpen())
			db.close();
		WriteToError("DatabaseManager::connectToRemoteDB has thrown an exception: " + string(e.what()));
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
			WriteToError((string)("Provided Database Driver is not available"));
			RegCloseKey(hKey);
			WriteToError((string)("The following Databases are supported"));
			WriteToError(checkValidDrivers());
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
			WriteToLog((string)"Creating session to db");

		
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
			WriteToError((string)"DB Connection failed to open");
			db.close();
			return 0;
		}
		WriteToLog((string)"DB Connection successfully opened");
		if (!db.driver()->hasFeature(QSqlDriver::Transactions))
		{
			WriteToError((string)"Selected Driver does not support transactions");
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
			cout << res.toUtf8().data() << endl;
			if (res.toUtf8().data() == schema) {
				dbFound = true;
				continueLoop = false;
			}
			else {
				continueLoop = query.next();
			}

		}
		query.clear();

		cout << "Was Target DB Found? " << (dbFound ? "Yes" : "No") << endl;
		if (!dbFound) {
			WriteToLog((string)"Generating Database");
			QString targetQuery = "CREATE DATABASE " + schema + " CHARACTER SET utf8 COLLATE utf8_general_ci";
			query.prepare(targetQuery);
			if (!query.exec()) {
				WriteToError((string)"Failed to create database");
			}
			else {
				WriteToLog((string)"Successfully created Database");
			}

			while (query.next())
			{
				QString res = query.value(0).toString();
				cout << res.toUtf8().data() << endl;
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
		WriteToError("DatabaseManager::connectToLocalDB has thrown an exception: " + string(e.what()));
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
		cout << "Successfully opened target file: " << filename << endl;
		WriteToLog("Successfully opened target file: " + filename);

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
		WriteToError("DatabaseManager::ExecuteTargetSqlScript has thrown an exception: " + string(e.what()));
		//return 0;
	}
	return successCount;
}

int DatabaseManager::ExecuteTargetSql(string& targetApp, string& sqlQuery)
{
	int successCount = 0;
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
			if (query.exec(statement))
				successCount++;
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
		WriteToError("DatabaseManager::ExecuteTargetSql has thrown an exception: " + string(e.what()));
	}
	return successCount;
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
		WriteToError("A Network error has occured: " + restReply.errorString().toStdString());
		goto exit;
	}
	if (!restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		qWarning() << "A HTTP error has occured: " << status << reason;
		WriteToError("An HTTP error has occured: " + to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	if (restReply.isHttpStatusSuccess()) {
		int status = restReply.httpStatus();
		QString reason = restReply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		qDebug() << "Request was successful: " << status << reason;
		WriteToLog("Request was successful : " + to_string(status) + " \"" + reason.toStdString() + "\"");
	}
	WriteToLog((string)"Parsing Response");
	//string sqlQuery = "UPDATE cloudupdate SET posted = 1 WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit).toStdString();
	ExecuteTargetSql(targetApp, sqlQuery);

	if (!json) {
		WriteToError((string)"Recieved empty data or failed to parse JSON.");
		goto exit;
	}

	if (response.value()["error"].toArray().count() > 0) {
		for (const auto& result : response.value()["error"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				errorRes << " - " << result.toArray()[i].toString().toStdString() << endl;
			}
		}

		WriteToError("Server responded with error: " + errorRes.str());
	}

	if (response.value()["data"].toArray().count() > 0) {
		for (const auto& result : response.value()["data"].toArray()) {
			for (auto i = 0; i < result.toArray().size(); i++) {
				dataRes << " - " << result.toArray()[i].toString().toStdString() << endl;
			}

		}
		WriteToLog("Server responded with data: " + dataRes.str());
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
