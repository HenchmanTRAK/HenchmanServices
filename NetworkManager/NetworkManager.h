
#ifndef NETWORK_MANAGER_LIBRARY_H
#define NETWORK_MANAGER_LIBRARY_H

#pragma once

#ifdef NETWORK_MANAGER_EXPORTS
#define NETWORK_MANAGER_ __declspec(dllexport)
#else
#define NETWORK_MANAGER_ __declspec(dllimport)
#endif

#if defined(NETWORK_MANAGER)
#  define NETWORK_MANAGER_EXPORT Q_DECL_EXPORT
#else
#  define NETWORK_MANAGER_EXPORT Q_DECL_IMPORT
#endif

#include <optional>
#include <vector>

#include <QCoreApplication>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QNetworkAccessManager>
#include <QRestAccessManager>
#include <QNetworkReply>
#include <QRestReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>


#include "ServiceHelper.h";
#include "HenchmanServiceException.h";


class NETWORK_MANAGER_EXPORT NetworkManager : public QObject
{
	Q_OBJECT

private:
	/**
	 * @var netManager
	 *
	 * @brief The network manager for database connections.
	 *
	 * This object is used to make network requests to the database server.
	 */
	QNetworkAccessManager* netManager = nullptr;

	QTcpSocket* sock = nullptr;

	/**
	 * @var restManager
	 *
	 * @brief The REST manager for database operations.
	 *
	 * This object is used to make RESTful network requests to the database server.
	 */
	QRestAccessManager* restManager = nullptr;

	QNetworkCookieJar* cookieJar = nullptr;

	std::vector<QEventLoop*> loops;

	bool strict_transport = false;
	
	/**
	 * @brief The API Key.
	 *
	 * This is used to authenticate the service connection with the api server.
	 */
	QString api_key;

	QString api_url;

public:
	NetworkManager(QObject* parent);
	~NetworkManager();

	void setApiKey(const QString& apiKey);
	void setApiUrl(const QString& url);
	
	void toggleSecureTransport(bool &secureTransport);

	void createManager();

	/**
	 * @brief Checks if the internet connection is available by attempting to connect to www.google.com on port 80.
	 *
	 * This function creates a QTcpSocket object, connects to www.google.com on port 80, and waits for the connection to be established.
	 * If the connection is successful within the specified timeout, the function returns true. Otherwise, it returns false.
	 *
	 * @return Returns true if the internet connection is available, otherwise returns false.
	 *
	 * @throws None
	*/
	bool isInternetConnected();

	int authenticateSession(const QString& url = "");

	/**
	 * @brief Sends a network request to the specified URL with the provided query data.
	 *
	 * Ensures the request has appropriate headers and parses the data returned before logging it, then returns the pure json through the results pointer.
	 *
	 * @param url The URL to send the network request to.
	 * @param query The query data to be sent in the request.
	 * @param results The QJsonDocument to store the response in.
	 *
	 * @return Returns 1 if the network request was successful, otherwise returns 0.
	 *
	 * @throws Throws an exception if there is a network or HTTP error, or if there is an error executing the SQL query or parsing the JSON response.
	*/
	int makeNetworkRequest(const QString& url, const QStringMap& query, QJsonDocument* results= nullptr);

	int makeGetRequest(const QString& url, const QStringMap& queryMap, QJsonDocument* results = nullptr);

	int makePostRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results = nullptr);

	int makePatchRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results = nullptr);

	int makeDeleteRequest(const QString& url, const QStringMap& queryMap, const QJsonObject& body, QJsonDocument* results = nullptr);

	void execRequests();

private:
	/**
	 * @brief Parses a QJsonArray into a string representation.
	 *
	 * @param array The QJsonArray to be parsed.
	 *
	 * @return The string representation of the QJsonArray.
	 *
	 * @throws None.
	*/
	static std::string parseData(QJsonArray array);

	/**
	 * @brief Parses a QJsonObject and returns a formatted string.
	 *
	 * This function takes a QJsonObject and recursively parses its keys and values.
	 * If a key's value is a string, the key-value pair is added to the formatted string.
	 * If a key's value is an object, the function calls itself to parse the nested object.
	 * If a key's value is an array, the function calls another function to parse the array.
	 *
	 * @param object The QJsonObject to be parsed.
	 *
	 * @return The formatted string representing the parsed QJsonObject.
	 *
	 * @throws None.
	*/
	static std::string parseData(QJsonObject object);

public slots:
	void finishRequest(const QJsonDocument& result);

signals:
	void requestFinished(const QJsonDocument& requestResult);
};


#endif