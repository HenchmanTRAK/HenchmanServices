
#include <iostream>

#include "DatabaseManager.h"



using namespace std;

static void checkValidDrivers()
{
	for (const auto& str : QSqlDatabase::drivers())
	{
		cout << " - " << str.toUtf8().data() << endl;
	}
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

int connectToRemoteDB(string &targetApp)
{
	/*sql::Driver* driver;
	sql::Connection* conn;*/

	try {
		HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Cloud"));
	
		QString schema = GetStrVal(hKey, "Schema", REG_SZ).c_str();

		QString dbUrl = GetStrVal(hKey, "url", REG_SZ).c_str();
		QSqlDatabase db;
		if (!checkValidConnections(schema))
		{

			string pass;
			QString user;
			QString server;
			int port;

			if (QString(GetStrVal(hKey, "UseProxy", REG_SZ).c_str()).toInt())
			{
				server = GetStrVal(hKey, "proxyhost", REG_SZ).c_str();
				port = QString(GetStrVal(hKey, "proxyport", REG_SZ).c_str()).toInt();
				user = GetStrVal(hKey, "proxyusername", REG_SZ).c_str();
				pass = GetStrVal(hKey, "proxypassword", REG_SZ);
			}
			else
			{
				server = GetStrVal(hKey, "Server", REG_SZ).c_str();
				port = QString(GetStrVal(hKey, "Port", REG_SZ).c_str()).toInt();
				user = GetStrVal(hKey, "Username", REG_SZ).c_str();
				pass = GetStrVal(hKey, "Password", REG_SZ);
			}

			cout << "Pulled the following values from registy: " << server.toUtf8().data() << " " << port << " " << schema.toUtf8().data() << " " << user.toUtf8().data() << " " << (pass != "" ? decodeBase64(pass) : pass.c_str()) << endl;
			cout << "Creating session to db" << endl;

		}

		QNetworkAccessManager manager;

		
		QUrl url(dbUrl);


			
		RegCloseKey(hKey);

	}
	catch (exception& e)
	{
		cout << "Throwing Exception: " << e.what() << endl;
		throw e;
	}
	return 1;
}

int connectToLocalDB(string& targetApp)
{
	try {
		HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));

		QString dbtype = GetStrVal(hKey, "Database", REG_SZ).c_str();

		if (!QSqlDatabase::isDriverAvailable(dbtype))
		{
			cout << "Provided Database Driver is not available" << endl;
			RegCloseKey(hKey);
			cout << "The following Databases are supported" << endl;
			checkValidDrivers();
			return 0;
		}

		QString schema = GetStrVal(hKey, "Schema", REG_SZ).c_str();

		QSqlDatabase db;
		if (!checkValidConnections(schema))
		{

			string pass;
			QString user;
			QString server;
			int port;

			if (QString(GetStrVal(hKey, "UseProxy", REG_SZ).c_str()).toInt())
			{
				server = GetStrVal(hKey, "proxyhost", REG_SZ).c_str();
				port = QString(GetStrVal(hKey, "proxyport", REG_SZ).c_str()).toInt();
				user = GetStrVal(hKey, "proxyusername", REG_SZ).c_str();
				pass = GetStrVal(hKey, "proxypassword", REG_SZ);
			}
			else
			{
				server = GetStrVal(hKey, "Server", REG_SZ).c_str();
				port = QString(GetStrVal(hKey, "Port", REG_SZ).c_str()).toInt();
				user = GetStrVal(hKey, "Username", REG_SZ).c_str();
				pass = GetStrVal(hKey, "Password", REG_SZ);
			}

			cout << "Pulled the following values from registy: " << server.toUtf8().data() << " " << port << " " << schema.toUtf8().data() << " " << user.toUtf8().data() << " " << (pass != "" ? decodeBase64(pass) : pass.c_str()) << endl;
			cout << "Creating session to db" << endl;

		
			db = QSqlDatabase::addDatabase(dbtype, schema);
			db.setHostName(server);
			db.setPort(port);
			//db.setDatabaseName(schema);
			db.setUserName(user);
			if (pass != "")
				db.setPassword(decodeBase64(pass));
			db.setConnectOptions("CLIENT_COMPRESS;");
		}
		else {
			db = QSqlDatabase::database(schema);

		}
		RegCloseKey(hKey);
		//checkValidConnections();


		bool dbOpen = db.open();
		//cout << "DB Connection is OK? " << (dbOpen ? "Yes" : "No") << endl;
		if (!dbOpen)
		{
			cout << "DB Connection failed to open" << endl;
			db.close();
			return 0;
		}
		cout << "DB Connection successfully opened" << endl;
		if (!db.driver()->hasFeature(QSqlDriver::Transactions))
		{
			cout << "Selected Driver does not support transactions";
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
		/*if (!dbFound) {
			cout << "Generating Database" << endl;
			QString targetQuery = "CREATE DATABASE " + schema + " CHARACTER SET utf8 COLLATE utf8_general_ci";
			query.prepare(targetQuery);
			if (!query.exec()) {
				cout << "Failed to create database" << endl;
			}
			else {
				cout << "Successfully created Database" << endl;
			}

			while (query.next())
			{
				QString res = query.value(0).toString();
				cout << res.toUtf8().data() << endl;
			}
		}*/

		query.clear();
		query.finish();

		if (!db.commit())
			db.rollback();

		db.close();

	}
	catch (exception& e)
	{
		cout << "Throwing Exception: " << e.what() << endl;
		throw e;
	}
	return 1;
}

int ExecuteTargetSqlScript(string& targetApp, string& filename)
{

	HKEY hKey = OpenKey(HKEY_LOCAL_MACHINE, string("SOFTWARE\\HenchmanTRAK\\" + targetApp + "\\Database"));
	QString schema = GetStrVal(hKey, "Schema", REG_SZ).c_str();
	RegCloseKey(hKey);

	QFile file(filename.c_str());

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		cout << "Failed to open target file" << endl;
		return 0;
	}

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