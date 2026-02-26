#include "QueryManager.h";

QueryManager::QueryManager(QObject* parent, const s_DATABASE_INFO& database_info)
	:QObject(parent), db_info(database_info)
{
	/*p_thread_controller = new QMutex();*/
}

QueryManager::~QueryManager()
{
	/*if (p_thread_controller)
	{
		p_thread_controller->unlock();
		p_thread_controller = nullptr;
	}*/

	if (db_info.schema.isEmpty())
	{
		qDebug() << "Schema for database has not been set";
		return;
	}


	if (!QSqlDatabase::contains(db_info.schema))
	{
		qDebug() << "connection to " << db_info.schema << " doesnt exist";
		return;
	}

	try {
		QSqlDatabase db = QSqlDatabase::database(db_info.schema);
		if (db.isOpen()) {
			db.close();
		}
	}catch(void*){}

	QSqlDatabase::removeDatabase(db_info.schema);
}

void QueryManager::setSchema(const QString& new_schema)
{
	/*if (schema != new_schema)
		schema = new_schema;*/
	db_info.schema = new_schema;
}

void QueryManager::set_database_details(const s_DATABASE_INFO& database_info)
{
	/*if (schema != new_schema)
		schema = new_schema;*/
	db_info = database_info;
}

QString QueryManager::getSchema()
{
	return db_info.schema;
}

QSqlDatabase QueryManager::GetDatabaseConnection()
{

	if (db_info.driver.isEmpty())
		throw HenchmanServiceException("Must provide a Database Driver for QueryManager to get connection");
			
	if (!QSqlDatabase::isDriverAvailable(db_info.driver))
	{
		throw HenchmanServiceException("Provided database driver is not available");
	}
	
	if(db_info.schema.isEmpty())
		throw HenchmanServiceException("Must provide a Database Schema for QueryManager to connect to");
	
	if (QSqlDatabase::contains(db_info.schema))
		return QSqlDatabase::database(db_info.schema);

	QSqlDatabase db;

	ServiceHelper().WriteToLog((std::string)"Creating session to db");

	db = QSqlDatabase::addDatabase(db_info.driver, db_info.schema);
	db.setHostName(db_info.server);
	db.setPort(db_info.port);
	db.setUserName(db_info.username);
	if (!db_info.password.isEmpty())
		db.setPassword(db_info.password);
	db.setConnectOptions(db_info.conn_options.join(""));
	db.setDatabaseName(db_info.schema);	

	bool ok = db.open();

	if (!ok) 
		return db;

	/*if (ok)
		db.close();*/

	return db;
}


s_TZ_INFO QueryManager::GetTimezone()
{

	if (!tz_info.time_zone.isEmpty() && tz_info.time_zone_offset) {
		return tz_info;
	}

	QJsonObject db_timezone = ExecuteTargetSql("SELECT LPAD(TIME_FORMAT(TIMEDIFF(NOW(), UTC_TIMESTAMP),'%H:%i'),6,'+') as tz, TIMESTAMPDIFF(SECOND, UTC_TIMESTAMP, NOW()) as tz_offset", QJsonObject()).at(0).toObject();

	qDebug() << "db_timezone" << db_timezone;

	tz_info.time_zone = db_timezone.value("tz").toString();
	tz_info.time_zone_offset = db_timezone.value("tz_offset").toInt();

	return tz_info;
}

QJsonArray QueryManager::ExecuteTargetSql(const TCHAR* sqlQuery, const QJsonObject& params)
{
	return ExecuteTargetSql(std::string(sqlQuery), params);
}

QJsonArray QueryManager::ExecuteTargetSql(const QString& sqlQuery, const QJsonObject& params)
{
	return ExecuteTargetSql_Array(sqlQuery, params.toVariantMap());
}

QMap<int, QList<QVariantMap>> QueryManager::ExecuteTargetSql_Map(const QString& sqlQuery, const QVariantMap& params)
{
	QMutexLocker locker(&p_thread_controller);

	int successCount = 0;

	QMap<int, QList<QVariantMap>> results;
	//QJsonArray results;

	if(!QSqlDatabase::contains(db_info.schema))
		GetDatabaseConnection();

	QSqlDatabase db = QSqlDatabase::database(db_info.schema);

	try {
		if (db.isOpen()) {
			db.close();
		}
		if (!db.open())
			throw HenchmanServiceException("Failed to open DB Connection");
		db.transaction();

		QSqlQuery query(db);

		QStringList sqlStatements = sqlQuery.split(';', Qt::SkipEmptyParts);

		qDebug() << "SQL Statements to Execute:" << sqlStatements;

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			LOG << "Executing: " << statement.toStdString();

			QVariantMap boundValues;
			if (!params.isEmpty()) {
				/*query.prepare(statement);*/
				for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
					QString key = it.key();
					QVariant value = it.value();
					//qDebug() << key << ": " << value;
					key = ":" + key;
					
					if (value.typeId() == QVariant::DateTime) {
						qDebug() << value;
						boundValues.insert(key, value.toDateTime().toString(Qt::ISODateWithMs));
						continue;
					}

					/*if (value.typeId() == QVariant::Int) {
						boundValues.insert(key, value.toInt());
						continue;
					}*/

					if (value.canConvert<QString>()) {
						boundValues.insert(key, value.toString());
						continue;
					}

					if (value.canConvert<QJsonArray>()) {
						QStringList values;
						foreach(QJsonValue val, value.toJsonArray()) {
							//qDebug() << val;
							values << "'" + (val.isDouble() ? QString::number(val.toInt()) : val.toString()) + "'";
						}
						//qDebug() << values;
						//qDebug() << "Binding " << QString::fromStdString(":" + key) << " with " << values.join(",");
						QString val = values.join(",");

						QString firstSection = statement.sliced(0, statement.indexOf(key));
						QString secondSection = statement.sliced(statement.indexOf(key) + key.size());
						statement = firstSection + val + secondSection;
						//boundValues.insert(":" + key, val);
						continue;
					}

					boundValues.insert(key, value);
				}
			}

			if (query.exec("SELECT * FROM employees LIMIT 1"))
				qDebug() << "Successfully fetched";
			else
				qDebug() << "Failed to Successfully fetched";


			query.prepare(statement);
			
			qDebug() << statement;
			qDebug() << boundValues;
			
			QMapIterator it(boundValues);

			while (it.hasNext()) {
				it.next();
				query.bindValue(it.key(), it.value());
			}

			QList<QVariantMap> multi_query_results;
			//QJsonArray multi_query_results;

			qDebug() << query.executedQuery();
			qDebug() << query.boundValues();

			if (!query.exec())
				throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());
			
			qDebug() << query.executedQuery();
			qDebug() << query.boundValues();
			successCount++;
			QSqlRecord record(query.record());

			while (query.next())
			{
				QVariantMap queryResult;
				//QJsonObject queryResult;
				for (int i = 0; i <= record.count() - 1; i++)
				{
					qDebug() << query.value(i).typeName();
					switch (query.value(i).typeId()) {
					case QVariant::String: {
						queryResult.insert(record.fieldName(i), query.value(i).toString());
						break;
					}
					case QVariant::Int: {
						queryResult.insert(record.fieldName(i), query.value(i).toInt());
						break;
					}
					case QVariant::Date: {
						queryResult.insert(record.fieldName(i), query.value(i).toDate().toString(Qt::ISODateWithMs));
						break;
					}
					case QVariant::Time: {
						queryResult.insert(record.fieldName(i), query.value(i).toTime().toString(Qt::ISODateWithMs));
						break;
					}
					case QVariant::DateTime: {
						s_TZ_INFO tzMap = GetTimezone();

						QDateTime dt = query.value(i).toDateTime();

						dt.setOffsetFromUtc(tzMap.time_zone_offset);

						queryResult.insert(record.fieldName(i), dt.toUTC().toString(Qt::ISODateWithMs));
						break;
					}
					default: {
						queryResult.insert(record.fieldName(i), query.value(i).toJsonValue());
						break;
					}
					}
					//queryResult.insert(record.fieldName(i), query.value(i));
					//queryResult[record.fieldName(i)] = query.value(i).toString();
				}
				multi_query_results.push_back(queryResult);
				/*if (sqlStatements.size() > 1)
				else
					results.push_back(queryResult);*/
				//results.insert(results.size(), queryResult);
				qDebug() << queryResult;
			}
			/*if (sqlStatements.size() > 1)
				*/
			results.insert(results.size(), multi_query_results);
		}

		query.finish();

		if (!db.commit())
			db.rollback();
		db.close();
	}
	catch (std::exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
		/*if (!queryResult.empty())
			resultVector[0] = queryResult;*/

	}

	return results;
}

QJsonArray QueryManager::ExecuteTargetSql_Array(const QString& sqlQuery, const QVariantMap& params)
{
	QMutexLocker locker(&p_thread_controller);

	int successCount = 0;

	//QList<QVariant> results;
	QJsonArray results;

	if (!QSqlDatabase::contains(db_info.schema))
		GetDatabaseConnection();

	QSqlDatabase db = QSqlDatabase::database(db_info.schema);


	try {
		if (!db.isOpen()) {
			if (!db.open())
				throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QSqlQuery query(db);

		QStringList sqlStatements = sqlQuery.split(';', Qt::SkipEmptyParts);

		qDebug() << "SQL Statements to Execute:" << sqlStatements;

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			LOG << "Executing: " << statement.toStdString();

			QVariantMap boundValues;

			if (!params.isEmpty()) {
				/*query.prepare(statement);*/
				for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
					QString key = it.key();
					QVariant value = it.value();
					//qDebug() << key << ": " << value;
					key = ":" + key;
					//qDebug() << value.typeName();

					if (value.typeId() == QVariant::DateTime) {
						qDebug() << value;
						//qDebug() << "utcOffset" << value.toDateTime().offsetFromUtc();

						boundValues.insert(key, value);
						continue;
					}

					if (value.canConvert<QString>()) {
						boundValues.insert(key, value.toString());
						continue;
					}

					if (value.canConvert<QJsonArray>()) {
						QStringList values;
						foreach(QJsonValue val, value.toJsonArray()) {
							//qDebug() << val;
							values << "'" + (val.isDouble() ? QString::number(val.toInt()) : val.toString()) + "'";
						}
						//qDebug() << values;
						//qDebug() << "Binding " << QString::fromStdString(":" + key) << " with " << values.join(",");
						QString val = values.join(",");

						QString firstSection = statement.sliced(0, statement.indexOf(key));
						QString secondSection = statement.sliced(statement.indexOf(key) + key.size());
						statement = firstSection + val + secondSection;
						//boundValues.insert(":" + key, val);
						continue;
					}

					boundValues.insert(key, value);
				}
			}

			query.prepare(statement);

			QMapIterator it(boundValues);

			while (it.hasNext()) {
				it.next();
				query.bindValue(it.key(), it.value());
			}

			//QList<QVariant> multi_query_results;
			QJsonArray multi_query_results;

			if (!query.exec())
				throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());

			qDebug() << query.executedQuery();
			qDebug() << query.boundValues();
			successCount++;
			QSqlRecord record(query.record());

			while (query.next())
			{
				//QMap<QString, QVariant> queryResult;
				QJsonObject queryResult;
				for (int i = 0; i <= record.count() - 1; i++)
				{
					qDebug() << query.value(i).typeName();
					switch (query.value(i).typeId()) {
					case QVariant::String: {
						queryResult.insert(record.fieldName(i), query.value(i).toString());
						break;
					}
					case QVariant::Int: {
						queryResult.insert(record.fieldName(i), query.value(i).toInt());
						break;
					}
					case QVariant::Date: {
						queryResult.insert(record.fieldName(i), query.value(i).toDate().toString(Qt::ISODateWithMs));
						break;
					}
					case QVariant::Time: {
						queryResult.insert(record.fieldName(i), query.value(i).toTime().toString(Qt::ISODateWithMs));
						break;
					}
					case QVariant::DateTime: {
						s_TZ_INFO tzMap = GetTimezone();

						QDateTime dt = query.value(i).toDateTime();
							
						dt.setOffsetFromUtc(tzMap.time_zone_offset);
							
						queryResult.insert(record.fieldName(i), dt.toUTC().toString(Qt::ISODateWithMs));
						break;
					}
					default: {
						queryResult.insert(record.fieldName(i), query.value(i).toJsonValue());
						break;
					}
					}
					//queryResult[record.fieldName(i)] = query.value(i).toString();
				}
				if (sqlStatements.size() > 1)
					multi_query_results.push_back(queryResult);
				else
					results.push_back(queryResult);
				qDebug() << queryResult;
			}
			if (sqlStatements.size() > 1)
				results.push_back(multi_query_results);
		}

		query.finish();

		if (!db.commit())
			db.rollback();
		db.close();
	}
	catch (std::exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
		/*if (!queryResult.empty())
			resultVector[0] = queryResult;*/

	}

	if (db.isOpen()) {
		db.close();
	}

	return results;
}

QJsonArray QueryManager::ExecuteTargetSql(const std::string& sqlQuery, const QJsonObject& params)
{
	QMutexLocker locker(&p_thread_controller);

	int successCount = 0;

	QJsonArray results;

	if (!QSqlDatabase::contains(db_info.schema))
		GetDatabaseConnection();

	QSqlDatabase db = QSqlDatabase::database(db_info.schema);


	try {
		if (!db.isOpen()) {
			if (!db.open())
				throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QSqlQuery query(db);

		QString sql = QString::fromStdString(sqlQuery);

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

		qDebug() << "SQL Statements to Execute:" << sqlStatements;

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			LOG << "Executing: " << statement.toStdString();

			QMap<QString, QVariant> boundValues;
			if (!params.isEmpty()) {
				/*query.prepare(statement);*/
				for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
					QString key = it.key();
					QVariant value = it.value().toVariant();
					//qDebug() << key << ": " << value;
					key = ":" + key;
					//qDebug() << value.typeName();
					if (key.contains("updatedAt")) {
						qDebug() << "key";
						qDebug() << value;
						qDebug() << "statement";
					}

					if (value.canConvert<QString>()) {
						boundValues.insert(key, value.toString());
						continue;
					}

					if (value.canConvert<QJsonArray>()) {						
						QStringList values;
						foreach(QJsonValue val, value.toJsonArray()) {
							//qDebug() << val;
							values << "'" + (val.isDouble() ? QString::number(val.toInt()) : val.toString()) + "'";
						}
						//qDebug() << values;
						//qDebug() << "Binding " << QString::fromStdString(":" + key) << " with " << values.join(",");
						QString val = values.join(",");

						QString firstSection = statement.sliced(0, statement.indexOf(key));
						QString secondSection = statement.sliced(statement.indexOf(key) + key.size());
						statement = firstSection + val + secondSection;
						//boundValues.insert(":" + key, val);
						continue;
					}

					boundValues.insert(key, value);
				}
			}

			query.prepare(statement);

			QMapIterator it(boundValues);
			
			while (it.hasNext()) {
				it.next();
				query.bindValue(it.key(), it.value());
			}

			QJsonArray multi_query_results;

			if (query.exec())
			{
				//qDebug() << query.executedQuery();
				successCount++;
				QSqlRecord record(query.record());

				while (query.next())
				{
					QJsonObject queryResult;
					for (int i = 0; i <= record.count() - 1; i++)
					{
						qDebug() << query.value(i).typeName();
						switch (query.value(i).typeId()) {
						case QVariant::String: {
							queryResult.insert(record.fieldName(i), query.value(i).toString());
							break;
						}
						case QVariant::Int: {
							queryResult.insert(record.fieldName(i), query.value(i).toInt());
							break;
						}
						case QVariant::Date: {
							queryResult.insert(record.fieldName(i), query.value(i).toDate().toString(Qt::ISODateWithMs));
							break;
						}
						case QVariant::Time: {
							queryResult.insert(record.fieldName(i), query.value(i).toTime().toString(Qt::ISODateWithMs));
							break;
						}
						case QVariant::DateTime: {
							qDebug() << "Returned Val" << query.value(i).toDateTime();
							qDebug() << "UTC Val" << query.value(i).toDateTime().toUTC();
							queryResult.insert(record.fieldName(i), query.value(i).toDateTime().toUTC().toString(Qt::ISODateWithMs));
							break;
						}
						default: {
							queryResult.insert(record.fieldName(i), query.value(i).toJsonValue());
							break;
						}
						}
						//queryResult[record.fieldName(i)] = query.value(i).toString();
					}
					if (sqlStatements.size() > 1)
						multi_query_results.push_back(queryResult);
					else
						results.push_back(queryResult);
					qDebug() << queryResult;
				}
				if (sqlStatements.size() > 1)
					results.push_back(multi_query_results);
			}
			else {
				throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());
			}
		}

		query.finish();

		if (!db.commit())
			db.rollback();
		db.close();
	}
	catch (std::exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
		/*if (!queryResult.empty())
			resultVector[0] = queryResult;*/

	}

	return results;
}

std::vector<QStringMap> QueryManager::ExecuteTargetSql(const std::string& sqlQuery, const std::map<std::string, QVariant>& params)
{
	int successCount = 0;
	std::vector<QStringMap> resultVector;
	QStringMap queryResult;
	queryResult["success"] = "0";
	
	QMutexLocker locker(&p_thread_controller);

	if (!QSqlDatabase::contains(db_info.schema))
		GetDatabaseConnection();

	QSqlDatabase db = QSqlDatabase::database(db_info.schema);


	try {
		if (db.isOpen()) {
			qDebug() << "Database is open";
		}
		if (!db.open())
		{
			// HenchmanServiceException
			throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QSqlQuery query(db);

		QString sql = QString::fromStdString(sqlQuery);

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);
		qDebug() << sqlStatements;
#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			LOG << "Executing: " << statement.toStdString();
			QMap<QString, QVariant> boundValues;
			if (params.size() > 0) {
				/*query.prepare(statement);*/
				for (auto it = params.cbegin(); it != params.cend(); ++it) {
					std::string key = it->first;
					QVariant value = it->second;
					//qDebug() << key << ": " << value;

					qDebug() << value.typeName();

					if (value.canConvert<QString>()) {
						boundValues.insert(QString::fromStdString(":" + key), "'" + value.toString() + "'");
						continue;
					}

					if (value.canConvert<QJsonArray>()) {
						QStringList values;
						foreach(QJsonValue val, value.toJsonArray()) {
							//qDebug() << val;
							values << "'" + (val.isDouble() ? QString::number(val.toInt()) : val.toString()) + "'";
						}
						//qDebug() << values;
						//qDebug() << "Binding " << QString::fromStdString(":" + key) << " with " << values.join(",");
						QString val = values.join(",");
						boundValues.insert(QString::fromStdString(":" + key), val);
						continue;
					}

					boundValues.insert(QString::fromStdString(":" + key), value);
				}
			}
			qDebug() << boundValues;

			QMapIterator it(boundValues);

			while (it.hasNext()) {
				it.next();
				statement.replace(it.key(), it.value().toString());
			}
			//qDebug() << statement;
			//if (params.size() <= 0 ? query.exec(statement) : query.exec())
			if (query.exec(statement))
			{
				qDebug() << query.executedQuery();
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
			//query.clear();
		}
		query.finish();
		if (!db.commit())
			db.rollback();
		db.close();
	}
	catch (std::exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
		if (!queryResult.empty())
			resultVector[0] = queryResult;

	}

	return resultVector;
}

std::vector<QStringMap> QueryManager::ExecuteTargetSql(const std::wstring& sqlQuery)
{
	int successCount = 0;
	std::vector<QStringMap> resultVector;
	QStringMap queryResult;
	queryResult["success"] = "0";
	

	if (!QSqlDatabase::contains(db_info.schema))
		GetDatabaseConnection();

	QSqlDatabase db = QSqlDatabase::database(db_info.schema);

	try {

		if (!db.open())
		{
			// HenchmanServiceException
			throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QSqlQuery query(db);

		QString sql = QString::fromStdWString(sqlQuery);
		//sqlQuery.data();

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif

		for (QString& statement : sqlStatements)
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
	catch (std::exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
		resultVector[0] = queryResult;

	}

	return resultVector;
}

//std::vector<QStringMap> DatabaseManager::ExecuteTargetSql(const QString& sqlQuery, const QStringMap& params)
//{	
//	std::map<std::string, QVariant> paramsMap;
//	if(params.size() > 0)
//		for (auto it = params.cbegin(); it != params.cend(); ++it) {
//			paramsMap[it.key().toStdString()] = it.value();
//		}
//	
//	return ExecuteTargetSql(sqlQuery.toStdString(), paramsMap);
//}

std::vector<QStringMap> QueryManager::ExecuteTargetSql(const QString& sqlQuery, const QVariantMap& params)
{
	std::map<std::string, QVariant> paramsMap;
	if (params.size() > 0)
		for (auto it = params.cbegin(); it != params.cend(); ++it) {
			paramsMap[it.key().toStdString()] = it.value();
		}

	return ExecuteTargetSql(sqlQuery.toStdString(), paramsMap);
}

std::vector<QStringMap> QueryManager::ExecuteTargetSql(const TCHAR* sqlQuery, const std::map<const TCHAR*, const TCHAR*>& params)
{
	std::map<std::string, QVariant> paramsMap;

	if (params.size() > 0)
		for (auto it = params.cbegin(); it != params.cend(); ++it) {
			paramsMap[std::string(it->first)] = it->second;
		}

	return ExecuteTargetSql((std::string)sqlQuery, paramsMap);
}

int QueryManager::ExecuteTargetSqlScript(const std::string& filepath)
{
	int successCount = 0;

	if (!QSqlDatabase::contains(db_info.schema))
		GetDatabaseConnection();

	QSqlDatabase db = QSqlDatabase::database(db_info.schema);
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

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			throw HenchmanServiceException("Failed to execute initial DB Query");
#endif

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			if (query.exec(statement))
				successCount++;
			else {
				if (statement.length() <= 128)
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
	catch (std::exception& e)
	{
		if (file.isOpen())
			file.close();
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
			db.close();
		}
		ServiceHelper().WriteToError(e.what());
	}
	return successCount;
}
