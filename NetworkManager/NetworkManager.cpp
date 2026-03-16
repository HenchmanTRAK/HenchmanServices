
#include "NetworkManager.h"


NetworkManager::NetworkManager(QObject* parent)
	:QObject(parent), sock(parent), cookieJar(parent), netManager(parent)
{
	loops = std::vector<QEventLoop*>();

	netManager.setCookieJar(&cookieJar);
	netManager.setStrictTransportSecurityEnabled(strict_transport);
	netManager.setAutoDeleteReplies(true);
	netManager.setTransferTimeout(30000);

	if (!restManager)
		restManager = new QRestAccessManager(&netManager, this);
}

NetworkManager::~NetworkManager()
{

	LOG << "Deconstructing NetworkManager";
	//if (cookieJar)
	//{
		cookieJar.deleteLater();
		//cookieJar = nullptr;
	//}

	cleanManager();

	if (restManager)
	{
		restManager->blockSignals(true);
		restManager->deleteLater();
		restManager = nullptr;
	}

	/*if (netManager)
	{
		netManager->blockSignals(true);
		netManager->deleteLater();
		netManager = nullptr;
	}*/

	netManager.blockSignals(true);
	netManager.deleteLater();

	if (sock.isOpen())
	{
		sock.close();
		sock.deleteLater();
		//sock = nullptr;
	}

	if (!loops.empty()) {
		loops.clear();
	}
}

void NetworkManager::createManager()
{


	/*if (!netManager) {
		netManager = new QNetworkAccessManager(this);
		netManager->setCookieJar(&cookieJar);
		netManager->setStrictTransportSecurityEnabled(strict_transport);
		netManager->setAutoDeleteReplies(true);
		netManager->setTransferTimeout(30000);
	}*/

	

#if false
		QJsonDocument results;

		makeOptionsRequest(api_url.sliced(0, api_url.lastIndexOf("/")) + "/auth/key", QJsonObject(), &results);

		qDebug() << results;
#endif
	//}

	return;
	//connect(this, &NetworkManager::requestFinished, netManager, &QNetworkAccessManager::finished);
}

void NetworkManager::cleanManager()
{
	/*if (restManager)
	{
		restManager->blockSignals(true);
		restManager->deleteLater();
		restManager = nullptr;
	}*/

	/*if (netManager)
	{
		netManager->blockSignals(true);
		netManager->deleteLater();
		netManager = nullptr;
	}*/
}


void NetworkManager::setApiKey(const QString& t_key)
{
	if(api_key != t_key)
		api_key = t_key;
}

void NetworkManager::setApiUrl(const QString& t_url)
{
	if (api_url != t_url)
		api_url = t_url;
}

void NetworkManager::toggleSecureTransport(const bool& secureTransport)
{
	if(secureTransport != NULL)
		strict_transport = secureTransport;
	else
		strict_transport = !strict_transport;

	//if(netManager)
	netManager.setStrictTransportSecurityEnabled(strict_transport);
}

bool NetworkManager::isInternetConnected()
{
	
	sock.connectToHost("www.google.com", 80);
	bool connected = sock.waitForConnected(30000);//ms
	sock.disconnectFromHost();
	if (connected)
	{
		createManager();
	}
	/*else {
		sock->close();
	}*/
	//sock->deleteLater();
	//sock = nullptr;
	return connected;
}

int NetworkManager::authenticateSession(const QString& url)
{

	/*if (request.header(QNetworkRequest::CookieHeader).toJsonObject().keys().contains("api-session"))
		return 1;*/
	QString placeholder;
	if (url.isEmpty())
	{
		placeholder = api_url;
		placeholder = placeholder.slice(0, api_url.lastIndexOf("/")) + "/auth/key";
	}
	else
		placeholder = url;


	for (const auto& cookie : cookieJar.cookiesForUrl(QUrl(placeholder)))
	{
		qDebug() << cookie.name().toStdString() << ":" << cookie.value().toStdString();
		if (cookie.name().toStdString() == "api-session")
		{
			return 1;
		}
	}

	QStringMap temp;
	QJsonObject temp2;
	QJsonDocument reply;

	makePostRequest(placeholder, temp, temp2, &reply);
	execRequests();
	if (reply.isEmpty() || !reply.isObject()) return 0;

	QJsonObject response = reply.object();
	qDebug() << response;
	if (response.keys().contains("error") || !response.keys().contains("data")) return 0;

	return 1;
}

int indentCount = 0;
std::string NetworkManager::parseData(QJsonArray array)
{
	std::stringstream dataRes;
	if (array.count() <= 0) {
		return "";
	}
	for (const auto& result : array) {
		QString res = "";
		if (result.isString())
		{
			res += result.toString();
		}
		if (result.isObject())
		{
			res += QString::fromStdString(parseData(result.toObject()));
		}
		if (result.isArray())
		{
			res += QString::fromStdString(parseData(result.toArray()));
		}
		dataRes << "[ " << res.toStdString() << "]\n";
		continue;
	}

	return dataRes.str();
}

std::string NetworkManager::parseData(QJsonObject object)
{

	std::stringstream dataRes;
	if (object.keys().count() <= 0) {
		return "";
	}
	for (auto& key : object.keys()) {
		QString res = "";
		QJsonValue value = object.value(key);
		/*if(indentCount)
			res.append("\n");*/
		for (int i = 0; i < indentCount; ++i) {
			res.append("\t");
			//res += "\t";
		}
		res.append(" - " + key + ": ");
		if (value.isString())
		{
			res += value.toString();
		}
		else if (value.isDouble()) {
			res += QString::number(value.toDouble());
		}
		else if (value.isObject())
		{
			//res = "\n";
			//res.push_front("\n");
			indentCount++;
			res += "{";
			res.append(parseData(value.toObject()));
			indentCount--;
			res += "}";
		}
		else if (value.isArray())
		{
			indentCount++;
			res += "\n\t";
			res.append(parseData(value.toArray()));
			indentCount--;
		}
		else {
			res += value.toString();
		}

		dataRes << res.toStdString() << "\n";

		continue;
	}
	return dataRes.str();
}

int retryCount = 0;

int NetworkManager::makeOptionsRequest(const QString& url, const QJsonObject& queryMap, QJsonDocument* results)
{
	int result = 0;
	QEventLoop loop(this);

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	QNetworkRequest request;
	QUrl requestUrl(url);

	if (!queryMap.isEmpty()) {
		QJsonDocument query(queryMap);
		QUrlQuery urlQuery;
		urlQuery.addQueryItem("query", query.toJson().toBase64());

		requestUrl.setQuery(urlQuery);
	}

	request.setUrl(requestUrl);

	request.setRawHeader("Authorization", "Bearer " + api_key.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");

	ServiceHelper().WriteToCustomLog("Making Options query to: " + request.url().toString().toStdString(), "queries");

	QByteArray body;

	QNetworkReply* reply = restManager->sendCustomRequest(request, "OPTIONS", body, &loop, [&result, &results, this](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.hasError()) {
				if (reply.error() == QNetworkReply::OperationCanceledError && retryCount < 3) {
					throw HenchmanServiceException(reply.errorString().toStdString());
				}
				if (reply.error() != QNetworkReply::NoError) {
					throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
				}
			}

			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((std::string)"Parsing Response");

			std::optional json = reply.readJson();

			/*int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;*/
			//optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);

			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			if (!json || !json.has_value()) {
				ServiceHelper().WriteToLog((std::string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json.value().toJson().toStdString(), "queries");
			if (results)
				json.value().swap(*results);
			else
				emit requestFinished(json.value());

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				ServiceHelper().WriteToCustomLog("network request success", "queries");
			}
			else {
				ServiceHelper().WriteToCustomLog("network request failed", "queries");
			}
		}
		catch (std::exception& e) {
			std::string error(e.what());
			ServiceHelper().WriteToError(error);
			//reply.networkReply()->abort();
			result = 0;

		}
		//reply.networkReply()->finished();

		});

	qDebug() << "Reply Get is Running? " << reply->isRunning();
	qDebug() << "Reply Get is finished? " << reply->isFinished();

	connect(restManager->networkAccessManager(), &QNetworkAccessManager::finished, reply, [=]() {
		qDebug() << "Network Access Manager has finished GET request";
		});

	//netManager->finished(reply);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	qDebug() << "Reply Get is finished? " << reply->isFinished();
	/*loop.deleteLater();*/
	reply->deleteLater();
	retryCount = 0;
	//QThread::sleep(1);
	return result;
}

int NetworkManager::makeGetRequest(const QString& url, const QStringMap& queryMap, QJsonDocument* results)
{
	
	QJsonObject query;


	if (queryMap.size() > 0) {


		QJsonObject where;

		QMapIterator<QString, QString> it(queryMap);
		while (it.hasNext()) {
			it.next();
			where.insert(it.key(), it.value());
		}
		query.insert("where", where);
	}
	
	return makeGetRequest(url, query, results);
}

int NetworkManager::makeGetRequest(const QString& url, const QJsonObject& queryMap, QJsonDocument* results)
{
	int result = 0;
	QEventLoop loop(this);

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	QNetworkRequest request;
	QUrl requestUrl(url);


	QJsonDocument query(queryMap);
	QUrlQuery urlQuery;
	urlQuery.addQueryItem("query", query.toJson().toBase64());

	requestUrl.setQuery(urlQuery);

	request.setUrl(requestUrl);

	request.setRawHeader("Authorization", "Bearer " + api_key.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");

	ServiceHelper().WriteToCustomLog("Making query to: " + request.url().toString().toStdString(), "queries");
	

	QNetworkReply* reply = restManager->get(request, &loop, [&result, &results, this](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.hasError()) {
				if (reply.error() == QNetworkReply::OperationCanceledError && retryCount < 3) {
					throw HenchmanServiceException(reply.errorString().toStdString());
				}
				if (reply.error() != QNetworkReply::NoError) {
					throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
				}
			}

			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((std::string)"Parsing Response");

			std::optional json = reply.readJson();

			/*int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;*/
			//optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);

			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			if (!json || !json.has_value()) {
				ServiceHelper().WriteToLog((std::string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json.value().toJson().toStdString(), "queries");
			if (results)
				results->swap(json.value());
			else
				emit requestFinished(json.value());

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				ServiceHelper().WriteToCustomLog("network request success", "queries");
			}
			else {
				ServiceHelper().WriteToCustomLog("network request failed", "queries");
				reply.networkReply()->abort();
			}
		}
		catch (std::exception& e) {
			std::string error(e.what());
			ServiceHelper().WriteToError(error);
			reply.networkReply()->abort();
			result = 0;
			return;
		}
		reply.networkReply()->finished();

		});

	qDebug() << "Reply Get is Running? " << reply->isRunning();
	qDebug() << "Reply Get is finished? " << reply->isFinished();

	connect(restManager->networkAccessManager(), &QNetworkAccessManager::finished, reply, [=]() {
		qDebug() << "Network Access Manager has finished GET request";
	});

	//netManager->finished(reply);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	qDebug() << "Reply Get is finished? " << reply->isFinished();
	/*loop.deleteLater();*/
	reply->deleteLater();
	//loop->deleteLater();
	retryCount = 0;
	//QThread::sleep(1);
	return result;
}

int NetworkManager::makePostRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results)
{
	QJsonObject query;

	qDebug() << queryMap;

	if (queryMap.size() > 0) {

		QJsonObject where;

		QMapIterator<QString, QString> it(queryMap);
		while (it.hasNext()) {
			it.next();
			where.insert(it.key(), it.value());
		}
		query.insert("where", where);
	}

	return makePostRequest(url, query, body, results);
}

int NetworkManager::makePostRequest(const QString& url, const QJsonObject& queryMap, const QJsonObject& body, QJsonDocument* results)
{	

	QJsonDocument response;
	int result = 0;
	QEventLoop loop(this);

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	QNetworkRequest request;
	request.setUrl(QUrl(url));
	request.setRawHeader("Authorization", "Bearer " + api_key.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");

	//data.fromVariantMap();

	/*QJsonObject body;
	body["data"] = body["data"];*/
	/*data["values"] = */
	//data["sql"] = query["query"];
	
	QJsonDocument doc(body);
	std::string log = "";
	log.append("Making Post request to: " + url.toStdString());
	if (!queryMap.isEmpty())
		log.append("\nRunning query number : " + (queryMap["number"].isUndefined() ? "undefined" : queryMap["number"].toString().toStdString()));
	if (!doc.isEmpty())
		log.append("\nquery : " + doc.toJson().toStdString());
	
	ServiceHelper().WriteToCustomLog(log, "queries");

	QNetworkReply* reply = restManager->post(request, doc, &loop, [&result, results, this, &response](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.error() == QNetworkReply::OperationCanceledError && retryCount < 3) {
				throw HenchmanServiceException(reply.errorString().toStdString());
			}
			if (reply.error() != QNetworkReply::NoError) {
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
			}
			
			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((std::string)"Parsing Response");

			std::optional json = reply.readJson();

			/*int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;*/
			//optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);
			
			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			if (!json || !json.has_value()) {
				ServiceHelper().WriteToLog((std::string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json.value().toJson().toStdString(), "queries");
			//finishRequest(json.value());
			//response = json.value();
			if (results) {
				json.value().swap(*results);
			}
			else {
				emit requestFinished(json.value());
			}

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				ServiceHelper().WriteToCustomLog("network request success", "queries");
				//reply.networkReply()->close();
			}
			else {
				ServiceHelper().WriteToCustomLog("network request failed", "queries");
				//reply.networkReply()->abort();
			}
		}
		catch (std::exception& e) {
			std::string error(e.what());
			/*if (error == "QNetworkReply::OperationCanceledError" && retryCount < 3) {
				result = makePostRequest(url, queryMap, body, results);
				if (result) {
					reply.networkReply()->close();
					return;
				}
			}*/
			ServiceHelper().WriteToError(error);
			//reply.networkReply()->abort();
			result = 0;

		}
		//reply.networkReply()->finished();
		//QTimer::singleShot(1, reply.networkReply(), &QNetworkReply::finished);
		});

	//netManager->finished(reply);
	/*connect(reply, &QNetworkReply::finished, [=]() {
		emit this->requestFinished();
	});*/
	connect(restManager->networkAccessManager(), &QNetworkAccessManager::finished, reply, [=]() {
		qDebug() << "Network Access Manager has finished POST request";
		});
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	//loop.deleteLater();
	reply->deleteLater();
	loop.deleteLater();
	/*connect(reply, &QNetworkReply::finished, this, [this, &response, reply]() {
		finishRequest(response);
		reply->deleteLater();
	});*/
	//reply->deleteLater();
	retryCount = 0;
	//QThread::sleep(1);
	return result;
}

int NetworkManager::makePatchRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results)
{
	QJsonObject query;

	qDebug() << queryMap;

	if (queryMap.size() > 0) {

		QJsonObject where;

		QMapIterator<QString, QString> it(queryMap);
		while (it.hasNext()) {
			it.next();
			where.insert(it.key(), it.value());
		}
		query.insert("where", where);
	}

	return makePatchRequest(url, query, body, results);
}

int NetworkManager::makePatchRequest(const QString& url, const QJsonObject& queryMap, const QJsonObject& body, QJsonDocument* results)
{
	int result = 0;
	QEventLoop loop(this);

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	QNetworkRequest request;
	request.setUrl(QUrl(url));
	request.setRawHeader("Authorization", "Bearer " + api_key.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");

	//data.fromVariantMap();

	/*QJsonObject body;
	body["data"] = body["data"];*/
	/*data["values"] = */
	//data["sql"] = query["query"];

	QJsonDocument doc(body);
	std::string log = "";
	log.append("Making Patch request to: " + url.toStdString());
	if (!queryMap.isEmpty())
		log.append("\nRunning query number : " + (queryMap["number"].isUndefined() ? "undefined" : queryMap["number"].toString().toStdString()));
	if (!doc.isEmpty())
		log.append("\nquery : " + doc.toJson().toStdString());

	ServiceHelper().WriteToCustomLog(log, "queries");

	QNetworkReply* reply = restManager->patch(request, doc, &loop, [&result, &results, this](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.error() == QNetworkReply::OperationCanceledError && retryCount < 3) {
				throw HenchmanServiceException(reply.errorString().toStdString());
			}
			if (reply.error() != QNetworkReply::NoError) {
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
			}

			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((std::string)"Parsing Response");

			std::optional json = reply.readJson();

			/*int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;*/
			//optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);

			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			if (!json || !json.has_value()) {
				ServiceHelper().WriteToLog((std::string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json.value().toJson().toStdString(), "queries");
			if (results) {
				json.value().swap(*results);
			}
			else {
				emit requestFinished(json.value());
			}

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				ServiceHelper().WriteToCustomLog("network request success", "queries");
			}
			else {
				ServiceHelper().WriteToCustomLog("network request failed", "queries");
			}
		}
		catch (std::exception& e) {
			std::string error(e.what());
			ServiceHelper().WriteToError(error);
			//reply.networkReply()->abort();
			result = 0;

		}
		//reply.networkReply()->finished();

		});

	qDebug() << "Reply patch is finished? " << reply->isFinished();

	//netManager->finished(reply);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	qDebug() << "Reply patch is finished? " << reply->isFinished();
	reply->deleteLater();
	loop.deleteLater();
	//QThread::sleep(1);
	retryCount = 0;

	return result;
}

int NetworkManager::makeDeleteRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results)
{
	int result = 0;
	QEventLoop loop(this);

	// Generate auth header for request.
	LOG << url;
	// Create network request object.
	QUrl targetUrl(url);
	QNetworkRequest request;
	request.setRawHeader("Authorization", "Bearer " + api_key.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");

	//data.fromVariantMap();

	/*QJsonObject body;
	body["data"] = body["data"];*/
	/*data["values"] = */
	//data["sql"] = query["query"];

	QJsonDocument doc(body);
	QUrlQuery query;
	query.addQueryItem("target", doc.toJson().toBase64());

	targetUrl.setQuery(query);

	request.setUrl(targetUrl);

	std::string log = "";
	log.append("Making DELETE request to: " + url.toStdString());
	if (!queryMap.isEmpty())
		log.append("\nRunning query number : " + queryMap["number"].toStdString());
	if (!doc.isEmpty())
		log.append("\nquery : " + doc.toJson().toStdString());

	ServiceHelper().WriteToCustomLog(log, "queries");

	QNetworkReply* reply = restManager->deleteResource(request, &loop, [&result, &results, this](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.error();
			if (reply.error() == QNetworkReply::OperationCanceledError && retryCount < 3) {
				throw HenchmanServiceException(reply.errorString().toStdString());
			}
			if (reply.error() != QNetworkReply::NoError) {
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
			}

			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");
			ServiceHelper().WriteToLog((std::string)"Parsing Response");

			std::optional json = reply.readJson();

			/*int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;*/
			//optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);

			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			if (!json || !json.has_value()) {
				ServiceHelper().WriteToLog((std::string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json.value().toJson().toStdString(), "queries");
			if (results) {
				json.value().swap(*results);
			}
			else {
				emit requestFinished(json.value());
			}

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				ServiceHelper().WriteToCustomLog("network request success", "queries");
			}
			else {
				ServiceHelper().WriteToCustomLog("network request failed", "queries");
			}
		}
		catch (std::exception& e) {
			std::string error(e.what());
			ServiceHelper().WriteToError(error);
			//reply.networkReply()->abort();
			result = 0;

		}
		//reply.networkReply()->finished();

		});

	qDebug() << "Reply patch is finished? " << reply->isFinished();

	//netManager->finished(reply);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	qDebug() << "Reply patch is finished? " << reply->isFinished();
	reply->deleteLater();
	loop.deleteLater();
	retryCount = 0;
	//QThread::sleep(1);
	return result;
}

int NetworkManager::makeNetworkRequest(const QString &url, const QStringMap &query, QJsonDocument *results)
{
	int result = 0;
	QEventLoop loop(this);

	// Generate auth header for request.
	//QString concatenated = apiUsername+":"+apiPassword;
	//QByteArray credentials = concatenated.toLocal8Bit().toBase64();
	//QString headerData = "Basic " + credentials;
	LOG << url;
	// Create network request object.
	QNetworkRequest request;
	request.setUrl(QUrl(url));
	request.setRawHeader("Authorization", "Bearer " + api_key.toLocal8Bit());
	request.setRawHeader("Content-Type", "application/json");
	
	//QJsonObject data;
	
	//data.fromVariantMap();

	QJsonObject body;
	body["data"] = query["data"];
	/*data["values"] = */
	//data["sql"] = query["query"];
	QJsonDocument doc(body);

	std::string log = "";
	log.append("Making Customer Network Post request to: " + url.toStdString());
	if (!query.isEmpty())
		log.append("\nRunning query number : " + query["number"].toStdString());
	if (!doc.isEmpty())
		log.append("\nquery : " + doc.toJson().toStdString());

	ServiceHelper().WriteToCustomLog(log, "queries");

	QNetworkReply* reply = restManager->post(request, doc, &loop, [&result, &results, this](QRestReply& reply) {
		LOG << "networkrequested";
		try {
			qDebug() << reply.networkReply()->request().headers().toMultiMap();
			qDebug() << reply.networkReply()->headers().toListOfPairs();
			if (reply.error() == QNetworkReply::OperationCanceledError && retryCount < 3) {
				throw HenchmanServiceException(reply.errorString().toStdString());
			}
			if (reply.error() != QNetworkReply::NoError) {
				throw HenchmanServiceException("A Network error has occured: " + reply.errorString().toStdString());
			}
			int status = reply.httpStatus();
			LOG << status;
			QString reason = reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

			ServiceHelper().WriteToLog((reply.isHttpStatusSuccess() ? "Request was successful: " : "An HTTP error has occured: ") + std::to_string(reply.httpStatus()) + " \"" + reply.networkReply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString() + "\"");

			ServiceHelper().WriteToLog((std::string)"Parsing Response");
			
			std::optional json = reply.readJson();

			/*int startingIndex = jsonRes.lastIndexOf('{') < 0 ? 0 : jsonRes.lastIndexOf('{');
			int endingIndex = jsonRes.lastIndexOf('}') < 0 ? 0 : jsonRes.lastIndexOf('}');
			LOG << "starting index: " << startingIndex << " ending index: " << endingIndex;*/
			//optional json = (optional<QJsonDocument>)QJsonDocument::fromJson(jsonRes);

			//string parsedVal;
			//if (json->isArray()) {
			//	parsedVal = parseData(json->array());
			//}
			//else if (json->isObject()) {
			//	QJsonObject retVal = json->object();
			//	/*if (retVal.contains("error") && retVal.find("error").value().isObject() && retVal.find("error").value().toObject().find("status").value().toString() == "23000")
			//		result = 1;*/
			//	parsedVal = parseData(json->object());
			//}
			if (!json || !json.has_value()) {
				ServiceHelper().WriteToLog((std::string)"Recieved empty data or failed to parse JSON.");
				return;
			}
			indentCount = 0;

			ServiceHelper().WriteToCustomLog("Webportal response: \n" + json.value().toJson().toStdString(), "queries");
			if (results) {
				json.value().swap(*results);
			}
			else {
				emit requestFinished(json.value());
			}

			if (!result)
				//result = reply.isSuccess();
				result = reply.isSuccess();

			if (reply.isSuccess())
			{
				ServiceHelper().WriteToCustomLog("network request success", "queries");
			}
			else {
				ServiceHelper().WriteToCustomLog("network request failed", "queries");
			}
		}
		catch (std::exception& e) {
			std::string error(e.what());
			ServiceHelper().WriteToError(error);
			//reply.networkReply()->abort();

			result = 0;

		}
		//reply.networkReply()->finished();
		
		});

	//connect(reply, &QNetworkReply::finished, restManager->networkAccessManager(), &QNetworkAccessManager::finished);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	//connect(&loop, &QEventLoop::quit, restManager->networkAccessManager(), &QNetworkAccessManager::finished);
	loop.exec();
	qDebug() << "Reply post is finished? " << reply->isFinished();
	reply->deleteLater();
	loop.deleteLater();
	retryCount = 0;
	//QThread::sleep(1);
	return result;
}

QRestAccessManager* NetworkManager::getRestManager()
{
	return restManager;
}

void NetworkManager::execRequests()
{
	for (auto& loop : loops) {
		if (loop == loops.at(-1)) {
			connect(loop, &QEventLoop::exit, this->parent(), &QCoreApplication::quit);
		}
		loop->exec();
		loop->deleteLater();
		loop = nullptr;
	}
	loops.clear();
}

void NetworkManager::finishRequest(const QJsonDocument& result)
{
	emit requestFinished(result);
}

void NetworkManager::replyFinished(QNetworkReply* rep)
{
	qDebug() << rep->readAll().toStdString();

	rep->deleteLater();
}

#include "moc_NetworkManager.cpp"