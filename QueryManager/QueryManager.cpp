#include "QueryManager.h";

QueryManager::QueryManager(QObject* parent, const s_DATABASE_INFO& database_info)
	:QObject(parent), db_info(database_info)
{

}

QueryManager::~QueryManager()
{
	LOG << "Deconstructing QueryManager";

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

	//QSqlDatabase::removeDatabase(db_info.schema);
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

s_TZ_INFO QueryManager::GetTimezone()
{

	if (!tz_info.time_zone.isEmpty() && tz_info.time_zone_offset) {
		return tz_info;
	}

	qDebug() << "Schema: " << db_info.schema;

	QSqlDatabase db(QSqlDatabase::database(db_info.schema));

	if (db.isOpen())
		qDebug() << "Database Open";
	else if(!db.open())
		qDebug() <<	"Failed to open Database";

	{
		QSqlQueryModel time_zone_modal(this);
		time_zone_modal.setQuery("SELECT LPAD(TIME_FORMAT(TIMEDIFF(NOW(), UTC_TIMESTAMP),'%H:%i'),6,'+') as tz, TIMESTAMPDIFF(SECOND, UTC_TIMESTAMP, NOW()) as tz_offset", db);
		QString tz = time_zone_modal.record(0).value("tz").toString();
		int tz_offset = time_zone_modal.record(0).value("tz_offset").toInt();
		time_zone_modal.clear();
		tz_info.time_zone = tz;
		tz_info.time_zone_offset = tz_offset;

		time_zone_modal.deleteLater();
	}

	if (db.isOpen())
		db.close();

	if (!db.isOpen())
		qDebug() << "Database Closed";

	return tz_info;
}


QList<QVariantMap> QueryManager::execute(const TCHAR* sql, const QVariantMap& placeholders)
{
	QList<QVariantMap> results;

	QSqlDatabase db = QSqlDatabase::database(db_info.schema, false);

	if(!db.open())
		throw HenchmanServiceException("Failed to open DB Connection");

	try {

		db.transaction();

		QString statement(sql);

		if (statement.trimmed() == "")
			throw HenchmanServiceException("No Query was provided.");

		QSqlQuery query(db);

		LOG << "Executing: " << statement.toStdString();

		QVariantMap boundValues;

		if (statement.contains(":tz") && !placeholders.contains("tz")) {
			GetTimezone();
			boundValues.insert(":tz", tz_info.time_zone);
		}

		if (!placeholders.isEmpty()) {
			QMapIterator it(placeholders);

			do {
				it.next();

				QString key = it.key();
				QVariant value = it.value();
				key = ":" + key;

				if (value.typeId() == QVariant::DateTime) {
					boundValues.insert(key, value.toDateTime().toString(Qt::ISODateWithMs));
					continue;
				}

				if (value.canConvert<QString>()) {
					boundValues.insert(key, value.toString());
					continue;
				}

				if (value.canConvert<QJsonArray>()) {
					QStringList values;
					foreach(QJsonValue val, value.toJsonArray()) {
						values << "'" + (val.isDouble() ? QString::number(val.toInt()) : val.toString()) + "'";
					}
					QString val = values.join(",");

					QString firstSection = statement.sliced(0, statement.indexOf(key));
					QString secondSection = statement.sliced(statement.indexOf(key) + key.size());
					statement = firstSection + val + secondSection;
					continue;
				}

				boundValues.insert(key, value);

			} while (it.hasNext());
		}

		query.prepare(statement);

		qDebug() << statement;
		qDebug() << boundValues;

		QMapIterator it(boundValues);

		while (it.hasNext()) {
			it.next();
			query.bindValue(it.key(), it.value());
		}

		if (!query.exec())
			throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());

		while (query.next())
		{
			QSqlRecord record = query.record();

			QVariantMap queryResult;

			for (int i = 0; i <= record.count() - 1; i++)
			{
				QString column = record.fieldName(i);
				QVariant value = query.value(i);

				qDebug() << column << ": " << value;
				switch (value.typeId()) {
				case QVariant::String: {
					queryResult.insert(column, value.toString());
					break;
				}
				case QVariant::Int: {
					queryResult.insert(column, value.toInt());
					break;
				}
				case QVariant::Date: {
					if (!query.value(i).toDate().isValid())
					{
						queryResult.insert(column, "");
						break;
					}

					queryResult.insert(column, value.toDate().toString(Qt::ISODateWithMs));
					break;
				}
				case QVariant::Time: {
					if (!query.value(i).toTime().isValid())
					{
						queryResult.insert(column, "");
						break;
					}

					queryResult.insert(column, value.toTime().toString(Qt::ISODateWithMs));
					break;
				}
				case QVariant::DateTime: {
					if (!query.value(i).toDateTime().isValid())
					{
						queryResult.insert(column, "");
						break;
					}

					QDateTime dt = value.toDateTime();

					dt.setOffsetFromUtc(tz_info.time_zone_offset);

					queryResult.insert(column, dt.toUTC().toString(Qt::ISODateWithMs));
					break;
				}
				default: {
					queryResult.insert(column, value);
					break;
				}
				}
			}

			results.append(queryResult);
		}

		query.finish();

		db.commit();

	}
	catch (const std::exception& e) {
		db.rollback();

		ServiceHelper().WriteToError(e.what());
	}

	if (db.isOpen())
		db.close();

	return results;
}

QJsonArray QueryManager::ExecuteTargetSql(const TCHAR* sqlQuery, const QJsonObject& params)
{
	return ExecuteTargetSql(std::string(sqlQuery), params);
}

QJsonArray QueryManager::ExecuteTargetSql(const QString& sqlQuery, const QJsonObject& params)
{
	QVariantMap localMap = params.toVariantMap();

	return ExecuteTargetSql_Array(sqlQuery, localMap);
}

QMap<int, QList<QVariantMap>> QueryManager::ExecuteTargetSql_Map(const QString& sqlQuery, const QVariantMap& params)
{
	QMutexLocker locker(&p_thread_controller);

	int successCount = 0;

	QMap<int, QList<QVariantMap>> results;
	//QJsonArray results;

	QSqlDatabase db(QSqlDatabase::database(db_info.schema));

	try {
		if (!db.open())
			throw HenchmanServiceException("Failed to open DB Connection");
		db.transaction();

		QStringList sqlStatements = sqlQuery.split(';', Qt::SkipEmptyParts);

		qDebug() << "SQL Statements to Execute:" << sqlStatements;

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif
		QSqlQuery query(db);

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;

			LOG << "Executing: " << statement.toStdString();

			QVariantMap boundValues;

			if (statement.contains(":tz") && !params.contains("tz")) {
				GetTimezone();
				boundValues.insert(":tz", tz_info.time_zone);
			}

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

			while (query.next())
			{
				QSqlRecord record(query.record());

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
				//qDebug() << queryResult;
			}
			/*if (sqlStatements.size() > 1)
				*/
			results.insert(results.size(), multi_query_results);


			query.clear();
		}
		query.finish();

		if (!db.commit())
			db.rollback();
	}
	catch (const std::exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
		}
		ServiceHelper().WriteToError(e.what());
		/*if (!queryResult.empty())
			resultVector[0] = queryResult;*/

	}

	if (db.isOpen())
		db.close();

	return results;
}

void QueryManager::ExecuteTargetSql(const QString& sqlQuery, const QVariantMap& params, QList<QList<QVariantMap>>* results)
{
	QMutexLocker locker(&p_thread_controller);

	int successCount = 0;

	QList<QList<QVariantMap>> localResults;

	QSqlDatabase db(QSqlDatabase::database(db_info.schema));

	try {
		if (!db.isOpen()) {
			if (!db.open())
				throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QStringList sqlStatements = sqlQuery.split(';', Qt::SkipEmptyParts);

		qDebug() << "SQL Statements to Execute:" << sqlStatements;

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif

		QSqlQuery query(db);
		
		for (int i = 0; i < sqlStatements.size(); ++i)
		{
			QString statement = sqlStatements[i];

			if (statement.trimmed() == "")
				continue;


			LOG << "Executing: " << statement.toStdString();

			QVariantMap boundValues;

			if (statement.contains(":tz") && !params.contains("tz")) {
				GetTimezone();
				boundValues.insert(":tz", tz_info.time_zone);
			}

			if (!params.isEmpty()) {
			
				for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
					QString key = it.key();
					QVariant value = it.value();
					//qDebug() << key << ": " << value;
					key = ":" + key;
					//qDebug() << value.typeName();

					if (value.typeId() == QVariant::DateTime) {
						qDebug() << value;
						//qDebug() << "utcOffset" << value.toDateTime().offsetFromUtc();

						boundValues.insert(key, value.toDateTime());
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
							QString parsedVal = "'" + (val.isDouble() ? QString::number(val.toInt()) : val.toString()) + "'";
							if(!values.contains(parsedVal))
								values << parsedVal;
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

			QList<QVariantMap> multi_query_results;
			//QJsonArray multi_query_results;

			if (!query.exec())
				throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());

			qDebug() << query.executedQuery();
			qDebug() << query.boundValues();
			successCount++;

			while (query.next())
			{
				QSqlRecord record(query.record());
				QVariantMap queryResult;

				for (int i = 0; i < record.count(); ++i)
				{

					qDebug() << record.fieldName(i) << ": " << query.value(i);
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
				}
				multi_query_results.append(queryResult);
			}
			if (multi_query_results.size() > 0)
				localResults.append(multi_query_results);

			query.clear();
		}

		query.finish();

		if (!db.commit())
			db.rollback();

	}
	catch (const std::exception& e) {
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
		}
		ServiceHelper().WriteToError(e.what());
		/*if (!queryResult.empty())
			resultVector[0] = queryResult;*/

	}

	if (db.isOpen()) {
		db.close();
	}

	if (results)
		localResults.swap(*results);

}


void QueryManager::ExecuteTargetSql(const QString& sqlQuery, const QVariantMap& params, QJsonArray* results)
{
	QMutexLocker locker(&p_thread_controller);

	int successCount = 0;

	QJsonArray localResults;

	QSqlDatabase db(QSqlDatabase::database(db_info.schema));

	try {
		if (!db.isOpen()) {
			if (!db.open())
				throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QStringList sqlStatements = sqlQuery.split(';', Qt::SkipEmptyParts);

		qDebug() << "SQL Statements to Execute:" << sqlStatements;

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif

		QSqlQuery query(db);
		
		for (QString& statement : sqlStatements)
		{

			if (statement.trimmed() == "")
				continue;

			LOG << "Executing: " << statement.toStdString();

			QVariantMap boundValues;

			if (statement.contains(":tz") && !params.contains("tz")) {
				GetTimezone();
				boundValues.insert(":tz", tz_info.time_zone);
			}

			if (!params.isEmpty()) {
				
				for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
					QString key = it.key();
					QVariant value = it.value();
					//qDebug() << key << ": " << value;
					key = ":" + key;
					//qDebug() << value.typeName();

					if (value.typeId() == QVariant::DateTime) {
						qDebug() << value;
						//qDebug() << "utcOffset" << value.toDateTime().offsetFromUtc();

						boundValues.insert(key, value.toDateTime());
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

			while (query.next())
			{
				QSqlRecord record(query.record());
				//QMap<QString, QVariant> queryResult;
				QJsonObject queryResult;

				for (int i = 0; i < record.count(); ++i)
				{

					qDebug() << record.fieldName(i) << ": " << query.value(i);
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
					multi_query_results.append(queryResult);
				else
					localResults.append(queryResult);
			}
			if (multi_query_results.size() > 0)
				localResults.append(multi_query_results);
			query.clear();
		}
		query.finish();

		if (!db.commit())
			db.rollback();
	}
	catch (const std::exception& e) {
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
		}
		ServiceHelper().WriteToError(e.what());
		/*if (!queryResult.empty())
			resultVector[0] = queryResult;*/

	}

	if (db.isOpen()) {
		db.close();
	}


	if(results)
		localResults.swap(*results);

}

QJsonArray QueryManager::ExecuteTargetSql_Array(const QString& sqlQuery, const QVariantMap& params)
{
	QMutexLocker locker(&p_thread_controller);

	int successCount = 0;

	//QList<QVariant> results;
	QJsonArray results;
	
	QSqlDatabase db(QSqlDatabase::database(db_info.schema));

	try {
		if (!db.isOpen()) {
			if (!db.open())
				throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();
			
		QStringList sqlStatements = sqlQuery.split(';', Qt::SkipEmptyParts);

		qDebug() << "SQL Statements to Execute:" << sqlStatements;

		QSqlQuery query(db);

#if true
		if (!query.exec("USE " + db_info.schema + ";"))
			throw HenchmanServiceException("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
			//ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif
		
		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;
			LOG << "Executing: " << statement.toStdString();

			QVariantMap boundValues;

			if (statement.contains(":tz") && !params.contains("tz")) {
				GetTimezone();
				boundValues.insert(":tz", tz_info.time_zone);
			}

			if (!params.isEmpty()) {

				for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
					QString key = it.key();
					QVariant value = it.value();
					key = ":" + key;

					if (value.typeId() == QVariant::DateTime) {
						boundValues.insert(key, value.toDateTime());
						continue;
					}

					if (value.canConvert<QString>()) {
						boundValues.insert(key, value.toString());
						continue;
					}

					if (value.canConvert<QJsonArray>()) {
						QStringList values;
						foreach(QJsonValue val, value.toJsonArray()) {
							values << "'" + (val.isDouble() ? QString::number(val.toInt()) : val.toString()) + "'";
						}
						
						QString val = values.join(",");

						QString firstSection = statement.sliced(0, statement.indexOf(key));
						QString secondSection = statement.sliced(statement.indexOf(key) + key.size());
						statement = firstSection + val + secondSection;
						continue;
					}

					boundValues.insert(key, value.toString());
				}
			}

			query.prepare(statement);

			QMapIterator it(boundValues);

			while (it.hasNext()) {
				it.next();
				query.bindValue(it.key(), it.value());
			}

			if (!query.exec())
				throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());
			
			qDebug() << query.executedQuery();
			qDebug() << query.boundValues();
			qDebug() << boundValues;

			successCount++;
			
			QJsonArray multi_query_results;
			
			while (query.next())
			{
				
				QSqlRecord record = query.record();

				QJsonObject queryResult;

				for (int i = 0; i < record.count(); ++i)
				{

					qDebug() << record.fieldName(i) << ": " << query.value(i);
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

						if (!query.value(i).toDateTime().isValid())
						{
							queryResult.insert(record.fieldName(i), "");
							break;
						}

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
				}

				qDebug() << queryResult;

				if (sqlStatements.size() > 1)
					multi_query_results.append(queryResult);
				else
					results.append(queryResult);

			}
			if (sqlStatements.size() > 1)
				results.append(multi_query_results);
		
			query.clear();
		}
		query.finish();

		if (!db.commit())
			db.rollback();

	} catch (const std::exception& e) {
		if (db.isOpen()) {
			db.rollback();
		}
		ServiceHelper().WriteToError(e.what());
		/*if (!queryResult.empty())
			resultVector[0] = queryResult;*/

	}

	if (db.isOpen()) {
		db.close();
	}

	qDebug() << "Return Arr" << results;

	return results;
}

QJsonArray QueryManager::ExecuteTargetSql(const std::string& sqlQuery, const QJsonObject& params)
{

	int successCount = 0;

	QJsonArray results;

	QMutexLocker locker(&p_thread_controller);
	
	QSqlDatabase db = QSqlDatabase::database(db_info.schema);


	try {
		if (db.isOpen()) {
			qDebug() << "Database Open";
		}

		if (!db.open())
			throw HenchmanServiceException("Failed to open DB Connection");
		db.transaction();

		QStringList sqlStatements = QString::fromStdString(sqlQuery).split(';', Qt::SkipEmptyParts);

		qDebug() << "SQL Statements to Execute:" << sqlStatements;

		QSqlQuery query(db);

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif
		

		qDebug() << "Query Valid" << query.isValid();
		qDebug() << "Query Active" << query.isActive();
		qDebug() << "Query Active" << db.isOpen();
		qDebug() << "Query Active" << db.isOpenError();

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;

			LOG << "Executing: " << statement.toStdString();

			QMap<QString, QVariant> boundValues;

			if (statement.contains(":tz") && !params.contains("tz")) {
				GetTimezone();
				qDebug() << tz_info.time_zone;
				qDebug() << tz_info.time_zone_offset;
				boundValues.insert(":tz", tz_info.time_zone);
			}

			if (!params.isEmpty()) {

				/*query.prepare(statement);*/
				for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
					QString key = it.key();
					QVariant value = it.value().toVariant();
					//qDebug() << key << ": " << value;
					key = ":" + key;
					//qDebug() << value.typeName();
					
					if (value.typeId() == QVariant::DateTime) {
						qDebug() << value;

						boundValues.insert(key, value.toDateTime());
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

			qDebug() << "query: " << statement;
			qDebug() << "boundVals: " << boundValues;

			query.prepare(statement);

			QMapIterator it(boundValues);
			
			while (it.hasNext()) {
				it.next();
				qDebug() << "binding " << it.value() << " to " << it.key();
				query.bindValue(it.key(), it.value());
			}

			QJsonArray multi_query_results;
			//query.setForwardOnly(true);

			qDebug() << "Query Valid" << query.isValid();
			qDebug() << "Query Active" << query.isActive();
			qDebug() << "DB Is Open" << db.isOpen();

			if (!query.exec())
				throw HenchmanServiceException("Failed executing: " + statement.toStdString() + "\nReason provided: " + query.lastError().text().toStdString());

			qDebug() << "Query Valid" << query.isValid();
			qDebug() << "Query Active" << query.isActive();
			qDebug() << "DB Is Open" << db.isOpen();
			
			qDebug() << query.executedQuery();
			qDebug() << query.boundValues();

			successCount++;

			while (query.next())
			{
				QSqlRecord record = query.record();
					
				QJsonObject queryResult;

				qDebug() << "Query Valid" << query.isValid();
				qDebug() << "Query Active" << query.isActive();
				qDebug() << "DB Is Open" << db.isOpen();
					
				for (int i = 0; i < record.count(); ++i)
				{
					qDebug() << record.fieldName(i) << ": " << query.value(i);
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
						if (!query.value(i).toDateTime().isValid())
						{
							qDebug() << "Entry value not valid";
							queryResult.insert(record.fieldName(i), "");
							break;
						}

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
			if (multi_query_results.size() > 1)
				results.push_back(multi_query_results);
			
			query.clear();
		}

		query.finish();

		qDebug() << "Query Valid" << query.isValid();
		qDebug() << "Query Active" << query.isActive();
		qDebug() << "DB is Open" << db.isOpen();

		if (!db.commit())
			db.rollback();
	}
	catch (const std::exception& e)
	{
		db.rollback();
		
		ServiceHelper().WriteToError(e.what());
		/*if (!queryResult.empty())
			resultVector[0] = queryResult;*/

	}

	if (db.isOpen()) {
		//if (!db.commit())
		db.close();
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

	QSqlDatabase db(QSqlDatabase::database(db_info.schema));

	try {
		if (!db.isOpen()) {
			qDebug() << "Database is open";
			if (!db.open())
				throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QString sql = QString::fromStdString(sqlQuery);

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);
		qDebug() << sqlStatements;
#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif
		QSqlQuery query(db);

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;


			LOG << "Executing: " << statement.toStdString();
			QMap<QString, QVariant> boundValues;

			if (statement.contains(":tz") && !params.contains("tz")) {
				GetTimezone();
				boundValues.insert(":tz", tz_info.time_zone);
			}

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
				

				queryResult["success"] = QString::number(successCount);
				resultVector.push_back(queryResult);

				while (query.next())
				{

					QSqlRecord record(query.record());

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
	}
	catch (const std::exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
		}
		ServiceHelper().WriteToError(e.what());
		if (!queryResult.empty())
			resultVector[0] = queryResult;

	}

	if (db.isOpen())
		db.close();

	return resultVector;
}

std::vector<QStringMap> QueryManager::ExecuteTargetSql(const std::wstring& sqlQuery)
{
	int successCount = 0;
	std::vector<QStringMap> resultVector;
	QStringMap queryResult;
	queryResult["success"] = TEXT("0");
	
	QSqlDatabase db(QSqlDatabase::database(db_info.schema));

	try {

		if (!db.open())
		{
			// HenchmanServiceException
			throw HenchmanServiceException("Failed to open DB Connection");
		}
		db.transaction();

		QString sql = QString::fromStdWString(sqlQuery);
		//sqlQuery.data();

		QStringList sqlStatements = sql.split(';', Qt::SkipEmptyParts);

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			//throw HenchmanServiceException("Failed to execute DB Query: USE " + schema.toStdString() + ";");
			ServiceHelper().WriteToError("Failed to execute DB Query: USE " + db_info.schema.toStdString() + ";");
#endif
		QSqlQuery query(db);

		for (QString& statement : sqlStatements)
		{
			if (statement.trimmed() == "")
				continue;

			LOG << "Executing: " << statement.toStdString();
			if (query.exec(statement))
			{
				successCount++;

				queryResult["success"] = QString::number(successCount);
				resultVector.push_back(queryResult);

				while (query.next())
				{
					QSqlRecord record(query.record());
					
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
	}
	catch (const std::exception& e)
	{
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
		}
		ServiceHelper().WriteToError(e.what());
		resultVector[0] = queryResult;

	}

	if (db.isOpen())
		db.close();

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

	QSqlDatabase db(QSqlDatabase::database(db_info.schema));

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

#if false
		if (!query.exec("USE " + db_info.schema + ";"))
			throw HenchmanServiceException("Failed to execute initial DB Query");
#endif

		QSqlQuery query(db);
		
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

		if (!db.commit())
			db.rollback();
	}
	catch (std::exception& e)
	{
		
		if (db.isOpen()) {
			db.rollback();
			//if (!db.commit())
		}
		ServiceHelper().WriteToError(e.what());
	}

	if (db.isOpen())
		db.close();
	if (file.isOpen())
		file.close();

	return successCount;
}
