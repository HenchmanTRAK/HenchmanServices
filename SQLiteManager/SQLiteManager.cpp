


#include "SQLiteManager.h"

using namespace std;

//string SQLite_Manager::dbName;
//string SQLite_Manager::dbDir;
//bool SQLite_Manager::logToConsole;

SQLite_Manager::SQLite_Manager(string db_dir, string db_name)
{
	dbName = db_name;
	dbDir = db_dir;
	logToConsole = false;
	if (db_name == "" || filesystem::exists(db_dir + db_name))
	{
		WriteToError( "No db name passed or db file already exists");
		return;
	}
	try 
	{
		cout << "target: " << dbDir + dbName << endl;
		SQLite::Database db(dbDir+dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
		int attr = GetFileAttributesA(db.getFilename().c_str());
		if (!(attr & FILE_ATTRIBUTE_HIDDEN))
		{
			SetFileAttributesA(db.getFilename().c_str(), attr | FILE_ATTRIBUTE_HIDDEN);
		}
	}
	catch (exception& e)
	{
		throw e;
	}
}

SQLite_Manager::~SQLite_Manager()
{
	cout << "Deconstructing SQLite_Manager" << endl;
	dbName.clear();
	dbDir.clear();
	logToConsole = NULL;
}

void SQLite_Manager::ToggleConsoleLogging() {
	logToConsole = !logToConsole;
}

string SQLite_Manager::GetDBName()
{
	return dbName.c_str();
}

int SQLite_Manager::InitDB()
{
	try 
	{
		logToConsole&&
			cout << "target: " << dbDir + dbName << endl;
		SQLite::Database db(dbDir + dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
		stringstream query;
		query << "DROP TABLE IF EXISTS test";
		db.exec(query.str());
		logToConsole&&
			cout << "Excecuting query: \n'" << query.str() << "'" << endl;
		query.str(string());

		SQLite::Transaction transaction(db);
		query << "CREATE TABLE test (";
		query << "id INTEGET PRIMARY KEY,";
		query << "value TEXT)";
		db.exec(query.str());
		logToConsole&&
			cout << "Excecuting query: \n'" << query.str() << "'" << endl;
		query.str(string());

		transaction.commit();
	}
	catch (exception& e)
	{
		logToConsole && 
			cout << "exception: " << e.what() << endl;
		throw e;
	}
	return 0;
}

int SQLite_Manager::CreateTable(string &tableName, vector<string> &cols)
{
	/* 
	The string paramater is used as the tables name, the vector gets parsed into a comma seperated string.
	*/
	try 
	{
		logToConsole&&
			cout << "target: " << dbDir + dbName << endl;
		SQLite::Database db(dbDir + dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
		stringstream query;
		SQLite::Transaction transaction(db);
		query << "CREATE TABLE IF NOT EXISTS " << tableName << " (id INTEGER PRIMARY KEY AUTOINCREMENT, date_created TEXT NOT NULL DEFAULT (datetime('now','localtime')), date_updated TEXT NOT NULL DEFAULT (datetime('now','localtime')), ";
		for (unsigned i = 0; i < cols.size(); ++i)
		{
			query << cols[i];
			if (i < cols.size() - 1)
				query << ", ";
		}
		query << ")";
		logToConsole&&
			cout << query.str() << endl;
		db.exec(query.str());

		transaction.commit();
	}
	catch (exception& e)
	{
		logToConsole&&
			cout << "exception: " << e.what() << endl;
		throw e;
	}
	return 0;
}

int SQLite_Manager::AddRow(string& targetName, vector<string> & values)
{
	try
	{
		logToConsole&&
			cout << "target: " << dbDir + dbName << endl;
		SQLite::Database db(dbDir + dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
		stringstream query;
		SQLite::Transaction transaction(db);
		query << "INSERT INTO " << targetName << " (username, password) VALUES (";
		for (unsigned i = 0; i < values.size(); ++i)
		{
			query << "\"" << values[i] << "\"";
			if (i < values.size() - 1)
				query << ", ";
		}
		query << ")";
		logToConsole&&
			cout << query.str() << endl;
		db.exec(query.str());
		transaction.commit();
	}
	catch (exception& e)
	{
		logToConsole&&
			cout << "exception: " << e.what() << endl;
		throw e;
	}
	return 0;
}