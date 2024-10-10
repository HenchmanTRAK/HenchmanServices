
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
	SI_Error rc = ini.LoadFile(".\\service.ini");
	if (rc < 0) {
		cerr << "Failed to Load INI File" << endl;
	}
	else {
		testingDBManager = ini.GetBoolValue("DEVELOPMENT", "testingDBManager", 0);
	}
	cout << "init db manager" << endl;
	targetApp = "";
	requestRunning = false;
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

int DatabaseManager::connectToRemoteDB (string &target_app)
{
	
	WriteToLog(string("Attempting to connect to Remote Database"));
	targetApp = target_app;
	QString targetSchema;
	try {
		HKEY hKeyLocal = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
		targetSchema = QString::fromStdString(GetStrVal(hKeyLocal, "Schema", REG_SZ));
		RegCloseKey(hKeyLocal);
		if (!checkValidConnections(targetSchema))
		{
			throw HenchmanServiceException("Provided schema not valid");
		}
	}
	catch (exception& e)
	{
		WriteToError("DatabaseManager::connectToRemoteDB has thrown an exception: " + string(e.what()));
		return 0;
	}

	HKEY hKeyCloud = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Cloud"));

	QString dbUrl = QString::fromStdString(GetStrVal(hKeyCloud, "url", REG_SZ));
	//dbUrl = "https://webportal.henchmantrak.com/files/ntunnel_mysql.php";
	dbUrl = "https://webportal.henchmantrak.com/webapi/public/api/employees/7"; ///api/portals
	RegCloseKey(hKeyCloud);

	QSqlDatabase db;

	
	vector<QString> queries;
	try {
		WriteToLog("Creating session to db " + targetSchema.toStdString());

		db = QSqlDatabase::database(targetSchema);
		db.setDatabaseName(targetSchema);

		//db.open();

		if (!db.open())
		{
			/*WriteToError(string("Failed to open DB Connection"));
			return 0;*/
			throw HenchmanServiceException("Failed to open DB Connection");
		}

		//db.transaction();

		QSqlQuery query(db);
		QString queryText = testingDBManager ? "SHOW TABLES" : "SELECT * FROM cloudupdate WHERE posted = 0 ORDER BY id LIMIT " + QString::number(queryLimit);

		
		if (!query.exec(queryText))
		{
			query.clear();
			query.finish();
			db.close();
			WriteToLog(string("Closing DB Session"));
			/*WriteToError(string("Failed to exec query to cloudupdate table"));
			return 0;*/
			throw HenchmanServiceException("Failed to exec query to cloudupdate table");
		}

		bool continueLoop = query.next();
		while (continueLoop)
		{
			QString res = query.value(2).toString().replace(QRegularExpression("(NOW|CURDATE|CURTIME)+", QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption | QRegularExpression::UseUnicodePropertiesOption), "\'" + query.value(3).toString().replace("T", " ") + "\'").replace("()", "");
			if (!testingDBManager) {
				WriteToCustomLog("Query Result: " + res.toStdString(), "queries");
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
		WriteToError("DatabaseManager::connectToRemoteDB has thrown an exception: " + string(e.what()));
		return 0;
	}
	cout << queries.size() << endl;
	if (queries.size() <= 0 && !testingDBManager)
	{
		queries.clear();
		WriteToLog(string("No entries in the cloudupdate table"));
		////netReply = networkManager->get(request);
		return 0;
		
	}
	
	if (dbUrl.trimmed().isEmpty()) {
		WriteToLog("No target Database Url provided");
		queries.clear();
		return 0;
	}

	try {
		QNetworkRequest request(dbUrl);
		QString concatenated = "root: ";
		QByteArray data = concatenated.toLocal8Bit().toBase64();
		QString headerData = "Basic " + data;
		request.setRawHeader("Authorization", headerData.toLocal8Bit());
		QNetworkAccessManager* netManager = new QNetworkAccessManager(this);
		//connect(netManager, &QNetworkAccessManager::finished, this, &QCoreApplication::quit);
		requestRunning = true;
		QRestAccessManager* restManager = new QRestAccessManager(netManager, this);
		restManager->get(request, this, [this](QRestReply& reply) {
			std::cout << "networkrequested" << endl;
			if (reply.isSuccess())
			{
				std::cout << "network request success" << endl;
			}
			else {
				std::cout << "network request failed" << endl;
			}
			QTimer::singleShot(1000, this->parent(), &QCoreApplication::quit);
			});
		//QTcpSocket* sock = new QTcpSocket(this);
		//sock->connectToHost(dbUrl, 80);
		//bool connected = sock->waitForConnected(30000);//ms
		//sock->
		//if (!connected)
		//{
		//	sock->abort();
		//	sock->deleteLater();
		//	//sock = nullptr;
		//	return false;
		//}
		//sock->close();
		//sock->deleteLater();

	}
	catch (exception& e)
	{
		WriteToError("DatabaseManager::connectToRemoteDB has thrown an exception: " + string(e.what()));
		return 0;
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
		WriteToError("DatabaseManager::connectToLocalDB has thrown an exception: " + string(e.what()));
		return 0;
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
	networkManager->deleteLater();
	return;
}
