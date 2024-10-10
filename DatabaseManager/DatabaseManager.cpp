
#include <iostream>

#include "DatabaseManager.h"

using namespace std;

int queryLimit = 100;
bool testingDBManager = false;

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
	SI_Error rc = ini.LoadFile(".\\service.ini");
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}
	else {
		testingDBManager = ini.GetBoolValue("DEVELOPMENT", "testingDBManager", 0);
	}

	networkManager = new QNetworkAccessManager(this);
	networkManager->setAutoDeleteReplies(true);
	connect(
		networkManager,
		&QNetworkAccessManager::finished,
		this,
		&QCoreApplication::quit,
		Qt::QueuedConnection
	);
	restManager = nullptr;
	netReply = nullptr;
	targetApp = "";
	requestRunning = false;
	form = nullptr;
}

DatabaseManager::~DatabaseManager() 
{
	cout << "Deleting DatabaseManager" << endl;
	if (netReply != nullptr) {
		cout << "Deleting netReply" << endl;
		//delete netReply;
		//netReply->deleteLater();
	}
	if (networkManager != nullptr) {
		cout << "Deleting networkManager" << endl;
		//delete networkManager;
		networkManager->deleteLater();
	}
	if (restManager != nullptr) {
		cout << "Deleting restManager" << endl;
		//delete restManager;
		//restManager->deleteLater();
	}
	if (form != nullptr) {
		cout << "Deleting form" << endl;
		//delete form;
		//form->deleteLater();
	}
}

bool DatabaseManager::isInternetConnected()
{
	QTcpSocket* sock = new QTcpSocket(this);
	sock->connectToHost("www.google.com", 80);
	bool connected = sock->waitForConnected(30000);//ms

	if (!connected)
	{
		sock->abort();
		return false;
	}
	sock->close();
	return true;
}

int DatabaseManager::connectToRemoteDB (string &target_app)
{
	
	WriteToLog(string("Attempting to connect to Remote Database"));
	targetApp = target_app;
	HKEY hKeyLocal = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
	QString schemaLocal = QString::fromStdString(GetStrVal(hKeyLocal, "Schema", REG_SZ));
	if (!checkValidConnections(schemaLocal))
	{
		RegCloseKey(hKeyLocal);
		return 0;
	}

	HKEY hKeyCloud = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Cloud"));
	QString schemaRemote = QString::fromStdString(GetStrVal(hKeyCloud, "Schema", REG_SZ));
	QString dbUrl = QString::fromStdString(GetStrVal(hKeyCloud, "url", REG_SZ));
	dbUrl = "https://webportal.henchmantrak.com/files/ntunnel_mysql.php";
	QSqlDatabase db;
	string pass;
	QString user;
	QString server;
	int port;

	if (QString::fromStdString(GetStrVal(hKeyCloud, "UseProxy", REG_SZ)).toInt())
	{
		server = QString::fromStdString(GetStrVal(hKeyCloud, "proxyhost", REG_SZ));
		port = QString::fromStdString(GetStrVal(hKeyCloud, "proxyport", REG_SZ)).toInt();
		user = QString::fromStdString(GetStrVal(hKeyCloud, "proxyusername", REG_SZ));
		pass = GetStrVal(hKeyCloud, "proxypassword", REG_SZ);
	}
	else
	{
		server = QString::fromStdString(GetStrVal(hKeyCloud, "Server", REG_SZ));
		port = QString::fromStdString(GetStrVal(hKeyCloud, "Port", REG_SZ)).toInt();
		user = QString::fromStdString(GetStrVal(hKeyCloud, "Username", REG_SZ));
		pass = GetStrVal(hKeyCloud, "Password", REG_SZ);
	}
		
	if (pass != "") pass =
		QByteArray::fromBase64Encoding(pass.data()).decoded;

	RegCloseKey(hKeyCloud);
	RegCloseKey(hKeyLocal);
	
	vector<QString> queries;

	try {
		WriteToLog("Creating session to db " +schemaLocal.toStdString());

		db = QSqlDatabase::database(schemaLocal);
		db.setDatabaseName(schemaLocal);

		//db.open();

		if (!db.open())
		{
			WriteToError(string("Failed to open DB Connection"));
			return 0;
		}

		db.transaction();

		QSqlQuery query(db);
		QString queryText = "SELECT * FROM cloudupdate WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit);
		//QString queryText = "SHOW TABLES";
		
		if (!query.exec(queryText))
		{
			WriteToError(string("Failed to exec query to cloudupdate table"));
			query.clear();
			query.finish();
			db.close();
			WriteToLog(string("Closing DB Session"));
			return 0;
		}

		bool continueLoop = query.next();
		while (continueLoop)
		{
			QString res = query.value(2).toString().replace(QRegularExpression("(NOW|CURDATE|CURTIME)+", QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption | QRegularExpression::UseUnicodePropertiesOption), "\'" + query.value(3).toString().replace("T", " ") + "\'").replace("()", "");
			if (!testingDBManager) {
				WriteToLog("Query Result: " + res.toStdString());
				queries.push_back(res);
			}
			continueLoop = query.next();
		}
		query.clear();
		query.finish();
		
		db.close();
		
	}
	catch (exception& e)
	{
		WriteToError("Throwing Exception: " + string(e.what()));
		throw e;
	}

	if (queries.size() <= 0)
	{
		queries.clear();
		WriteToLog(string("No entries in the cloudupdate table"));
		//netReply = networkManager->get(request);
		return 0;
	}
	
	if (dbUrl.trimmed().isEmpty()) {
		WriteToLog("No target Database Url provided");
		queries.clear();
		return 0;
	}

	form = new QHttpMultiPart(QHttpMultiPart::FormDataType);

	QHttpPart actnPart;
	actnPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"actn\""));
	actnPart.setBody("Q");
	form->append(actnPart);
	//parts.push_back(actnPart);
	QHttpPart hostPart;
	hostPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"host\""));
	hostPart.setBody(server.toUtf8());
	//parts.push_back(hostPart);
	form->append(hostPart);
	QHttpPart portPart;
	portPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"port\""));
	portPart.setBody(QString::number(port).toUtf8());
	//parts.push_back(portPart);
	form->append(portPart);
	QHttpPart loginPart;
	loginPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"login\""));
	loginPart.setBody(user.toUtf8());
	//parts.push_back(loginPart);
	form->append(loginPart);
	QHttpPart passwordPart;
	passwordPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"password\""));
	passwordPart.setBody(pass.data());
	//parts.push_back(passwordPart);
	form->append(passwordPart);
	QHttpPart schemaPart;
	schemaPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"db\""));
	schemaPart.setBody(schemaRemote.toUtf8());
	//parts.push_back(schemaPart);
	form->append(schemaPart);
	QHttpPart decodePart;
	decodePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"encodeBase64\""));
	decodePart.setBody("1");
	form->append(decodePart);

	if(!testingDBManager)
		for (const auto& query : queries)
		{
			QHttpPart queryPart;
			queryPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"q[]\""));
			WriteToLog("Running query: " + query.toStdString() + " on server.\n");
			QByteArray querySection = QByteArray(query.toUtf8()).toBase64();
			queryPart.setBody(querySection);
			form->append(queryPart);
		}
	else {
		QString testQuery = "SHOW TABLES;";
		QHttpPart queryPart;
		queryPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"q[]\""));
		WriteToLog("Running query: " + testQuery.toStdString() + " on server.\n");
		QByteArray querySection = QByteArray(testQuery.toUtf8()).toBase64();
		queryPart.setBody(querySection);
		form->append(queryPart);
	}
	
	if (netReply) {
		netReply->abort();
		netReply->deleteLater();
		netReply = nullptr;
	}

	try {
		QNetworkRequest request(dbUrl);
		WriteToLog("Making get request to " + dbUrl.toStdString());
		netReply = networkManager->post(request, form);
		netReply->setParent(networkManager);
		form->setParent(netReply);
		connect(
			netReply,
			&QNetworkReply::finished,
			this,
			&DatabaseManager::parseData,
			Qt::DirectConnection
		);
		
		requestRunning = true;
	}
	catch (exception& e)
	{
		WriteToError("Throwing Exception: " + string(e.what()));
		throw e;
	}
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

	}
	catch (exception& e)
	{
		WriteToError("Throwing Exception: " + string(e.what()));
		throw e;
	}
	return 1;
}

int DatabaseManager::ExecuteTargetSqlScript(string& targetApp, string& filename)
{

	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	QString schema = QString::fromStdString(GetStrVal(hKey, "Schema", REG_SZ));
	RegCloseKey(hKey);

	QFile file(filename.data());

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		cout << "Failed to open target file" << endl;
		return 0;
	}
	cout << "Successfully opened target file: " << filename.data() << endl;

	QTextStream in(&file);
	QString sql = in.readAll();

	QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

	int successCount = 0;

	QSqlDatabase db = QSqlDatabase::database(schema);

	if (!db.open())
	{
		cout << "Failed to open DB Connection" << endl;
		return 0;
	}
	db.transaction();
	QSqlQuery query(db);

	if (!query.exec("USE " + schema + ";")) {
		cout << "Failed to execute initial DB Query" << endl;
		return 0;
	}

	foreach(const QString & statement, sqlStatements)
	{
		if (statement.trimmed() == "")
			continue;
		if (query.exec(statement))
			successCount++;
		else
			qDebug() << "Failed: " << statement << "\nReason: " << query.lastError();
		query.clear();
	}
	query.finish();
	if (!db.commit())
		db.rollback();
	db.close();
	return successCount;
}

int DatabaseManager::ExecuteTargetSql(string& targetApp, string& sqlQuery)
{

	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database").data());
	QString schema = QString::fromStdString(GetStrVal(hKey, "Schema", REG_SZ));
	RegCloseKey(hKey);

	QSqlDatabase db = QSqlDatabase::database(schema);


	if (!db.open())
	{
		WriteToError((string)"Failed to open DB Connection" );
		return 0;
	}
	db.transaction();
	QSqlQuery query(db);

	QString sql = sqlQuery.data();

	QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

	int successCount = 0;

	if (!query.exec("USE " + schema + ";")) {
		WriteToError((string)"Failed to execute initial DB Query");
		return 0;
	}

	foreach(const QString & statement, sqlStatements)
	{
		if (statement.trimmed() == "")
			continue;
		if (query.exec(statement))
			successCount++;
		else {
			qDebug() << "Failed: " << statement << "\nReason: " << query.lastError();
			WriteToError("Failed: " + statement.toStdString() + "\nReason: " + query.lastError().text().toStdString());
		}
		query.clear();
	}
	query.finish();
	if (!db.commit())
		db.rollback();
	db.close();
	return successCount;
}

void DatabaseManager::checkRequest()
{
	
	
	
	if (!netReply) {
		cout << "NetReply is NULL" << endl;
		return;
	}
	
	if (netReply->isFinished()) {
		cout << "Request finished" << endl;
		return;
	}
	if (netReply->isRunning()) {
		cout << "Request running" << endl;
		return;
	}
	
}

void DatabaseManager::parseData()
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
	form->deleteLater();
	netReply->deleteLater();
	
	/*form = nullptr;
	netReply = nullptr;*/

	requestRunning = false;
	networkManager->clearAccessCache();
	return;
}

void DatabaseManager::performCleanup()
{
	networkManager->deleteLater();
	return;
}
