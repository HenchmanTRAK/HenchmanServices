
#include "SQLiteManager.h"


SQLite_Manager::SQLite_Manager()
{
	try {
		SQLite::Database db(dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
		int attr = GetFileAttributesA(db.getFilename().c_str());
		if (!(attr & FILE_ATTRIBUTE_HIDDEN))
		{
			SetFileAttributes(db.getFilename().c_str(), attr | FILE_ATTRIBUTE_HIDDEN);
		}
	}
	catch (exception& e)
	{
		throw e;
	}
}

void SQLite_Manager::ToggleConsoleLogging() {
	logToConsole = !logToConsole;
}

int SQLite_Manager::InitDB()
{
	try 
	{
		SQLite::Database db(dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
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
}

int SQLite_Manager::CreateTable(string tableName, vector<string> &cols)
{
	try 
	{
		/*
		TODO:
		 - Loop through cols vector creating a string with each value
		 - bind new string to second slot
		 - create table
		*/ 
		SQLite::Database db(dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
		stringstream query;
		SQLite::Transaction transaction(db);
		query << "CREATE TABLE IF NOT EXISTS ? (id INTEGET PRIMARY KEY AUTOINCREMENT, ?)";
		SQLite::Statement statement{ db, query.str() };
		statement.bind(1, tableName);


	}
	catch (exception& e)
	{
		logToConsole&&
			cout << "exception: " << e.what() << endl;
		throw e;
	}
}