
#include "HenchmanServiceMailer.h"


//void HenchmanService::ConnectWithSMTP() {
//	WSADATA wsaData;
//	int iResult;
//
//	struct sockaddr_in clientService;
//	ZeroMemory(&clientService, sizeof(clientService));
//	vector<string> files;
//
//	ctx = InitCTX();
//
//	char buff[1024];
//	int buffLen = sizeof(buff);
//
//	struct addrinfo hints;
//	string reply;
//	int iProtocolPort;
//	try {
//		logx << "Socket setup started" << endl;
//		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
//		if (iResult != NO_ERROR) {
//			//printf("WSAStartup failed: %d\n", iResult);
//			//WriteToError("Failed To Setup Socket");
//			//return;
//			throw HenchmanServiceException("Failed to Setup Socket");
//		}
//
//		ZeroMemory(&hints, sizeof(hints));
//		hints.ai_family = AF_UNSPEC;
//		hints.ai_socktype = SOCK_STREAM;
//		hints.ai_protocol = IPPROTO_TCP;
//
//		logx << "Getting Mail Address Info" << endl;
//		iResult = getaddrinfo("mail.henchmantrak.com", "smtp", &hints, &mailAddrInfo);
//		if (iResult != NO_ERROR) {
//			//printf("getaddrinfo failed with error: %d\n", iResult);
//			freeaddrinfo(mailAddrInfo);
//			WSACleanup();
//			//WriteToError("getaddrinfo failed with error: " + iResult);
//			//return;
//			throw HenchmanServiceException("getaddrinfo failed with error: " + iResult);
//		}
//		logx << "Setting up SMTP Mail Socket" << endl;
//		mailSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
//		if (mailSocket == INVALID_SOCKET) {
//			//printf("Failed to connect to Socket: %ld\n", WSAGetLastError());
//			freeaddrinfo(mailAddrInfo);
//			WSACleanup();
//			//WriteToError("Failed to connect to Socket: " + WSAGetLastError());
//			//return;
//			throw HenchmanServiceException("Failed to connect to Socket: " + WSAGetLastError());
//		}
//		logx << "Getting Mail Service Port" << endl;
//		LPSERVENT lpServEntry = getservbyname("mail", 0);
//		if (!lpServEntry) {
//			logx << "Using IPPORT_SMTP" << endl;
//			iProtocolPort = htons(IPPORT_SMTP);
//		}
//		else {
//			logx << "Using port provided from lpServEntry" << endl;
//			iProtocolPort = lpServEntry->s_port;
//		}
//		std::cout << "Connecting on port: " << iProtocolPort << endl;
//
//		clientService.sin_family = AF_INET;
//		clientService.sin_port = iProtocolPort;
//		inet_pton(AF_INET, inet_ntoa(((struct sockaddr_in*)mailAddrInfo->ai_addr)->sin_addr), (SOCKADDR*)&clientService.sin_addr.s_addr);
//		logx << "Connecting to Mail Socket" << endl;
//		iResult = connect(mailSocket, (SOCKADDR*)&clientService, sizeof(clientService));
//		if (iResult == SOCKET_ERROR) {
//			//printf("Unable to connect to server: %ld\n", WSAGetLastError());
//			WSACleanup();
//			freeaddrinfo(mailAddrInfo);
//			//WriteToError("Unable to connect to server: " + WSAGetLastError());
//			//return;
//			throw HenchmanServiceException("Unable to connect to server: " + WSAGetLastError());
//		}
//		else {
//			logx << "Connected to: " << inet_ntoa(clientService.sin_addr) << " on port: " << iProtocolPort << endl;
//		}
//
//		logx << "Initiating communication through SMTP" << endl;
//		char szMsgLine[255] = "";
//		sprintf(szMsgLine, "HELO %s%s", "mail.henchmantrak.com", CRLF);
//		string E1 = "ehlo ";
//		E1.append("mail.henchmantrak.com");
//		E1.append(CRLF);
//		char* hello = E1.data();
//		char* hellotls = (char*)"STARTTLS\r\n";
//
//		// initiate connection
//		iResult = recv(mailSocket, buff, sizeof(buff), 0);
//		reply = buff;
//		reply.resize(iResult);
//		logx << "[Server] [" << iResult << "] " << buff << endl;
//		//logx.adjustfield;
//
//		//memset(buff, 0, sizeof buff);
//		buff[0] = '\0';
//
//		// send ehlo command
//		send(mailSocket, hello, strlen(hello), 0);
//		logx << "[HELO] " << hello << " " << iResult << endl;
//
//		iResult = recv(mailSocket, buff, sizeof(buff), 0);
//		reply = buff;
//		reply.resize(iResult);
//		logx << "[Server] [" << iResult << "] " << reply << endl;
//
//		while (!Contain(buff, "250 ")) {
//			iResult = recv(mailSocket, buff, sizeof(buff), 0);
//			if (Contain(buff, "501 ") || Contain(buff, "503 ")) {
//				freeaddrinfo(mailAddrInfo);
//				closesocket(mailSocket);
//				WSACleanup();
//				//WriteToError(logx.str());
//				//logx.str(string());
//				//return;
//				throw HenchmanServiceException("Target server responded with: " + string(buff) + "\n which contains 501 or 503");
//			}
//		}
//
//		if (!Contain(buff, "STARTTLS")) {
//			logx << "[EXTERNAL_SERVER_NO_TLS] " << "mail.henchmantrak.com" << " " << buff << "[CLOSING_CONNECTION]" << endl;
//			closesocket(mailSocket);
//			freeaddrinfo(mailAddrInfo);
//			WSACleanup();
//			//WriteToError(logx.str());
//			//logx.str(string());
//			//return;
//			throw HenchmanServiceException("Target SMTP server does not support TLS");
//		}
//
//		memset(buff, 0, sizeof buff);
//		buff[0] = '\0';
//		char buff1[1024];
//
//		// starttls connecion
//		send(mailSocket, hellotls, strlen(hellotls), 0);
//		logx << "[STARTTLS] " << hellotls << endl;
//
//		iResult = recv(mailSocket, buff1, sizeof(buff1), 0);
//		reply = buff1;
//		reply.resize(iResult);
//		logx << "[Server] " << iResult << " " << buff << endl;
//
//		ctx = InitCTX();
//		ssl = SSL_new(ctx);
//		SSL_set_fd(ssl, mailSocket);
//
//		logx << "Connection to smtp via tls" << endl;
//
//		if (SSL_connect(ssl) == -1) {
//			ERR_print_errors_fp(stderr);            
//			logx << "[TLS_SMTP_ERROR]" << endl;
//			freeaddrinfo(mailAddrInfo);
//			closesocket(mailSocket);
//			WSACleanup();
//			//WriteToError(logx.str());
//			//logx.str(string());
//			//return;
//			throw HenchmanServiceException("Failed to establish TLS ssl connection to SMTP sever");
//		}
//		else {
//			logx << "Connected with " << SSL_get_cipher(ssl) << " encryption" << endl;
//			string cert = ShowCerts(ssl);
//
//			vector<string> attachments;
//			for (const auto& entry : filesystem::directory_iterator(GetExportsPath())) {
//				std::cout << entry.path().string() << endl;
//				attachments.push_back(entry.path().string());
//			}
//			logx << "---" << "Generating and sending Email" << "---\r\n" << endl;
//			WriteToLog(logx.str());
//			//logx.str(string());
//			logx.clear();
//			SendEmail(ssl, attachments);
//		}
//		sslError(ssl, 1, logx);
//
//		closesocket(mailSocket);
//		SSL_CTX_free(ctx);
//		freeaddrinfo(mailAddrInfo);
//		WSACleanup();
//	}
//	catch (exception& e) {
//		WriteToError("HenchmanService::ConnectWithSMTP threw exception: " + (string)e.what());
//		WriteToError(logx.str());
//	}
//	//logx.str(string());
//	logx.clear();
//}

//void HenchmanService::SendEmail(SSL*& ssl, vector<string> attachments) {
//
//	char buff[1024] = "\0";
//	int buffLen = sizeof(buff);
//	try {
//		int counter = 1;
//		if (SSL_connect(ssl) == -1) {
//			ERR_print_errors_fp(stderr);            
//			logx << "[TLS_SMTP_ERROR]" << endl;
//			//return;
//			throw HenchmanServiceException("SSL certificate verification failed");
//		}
//		else {
//			WriteToLog("Greeting email server");
//			buff[0] = '\0';
//			ostringstream f0;
//			f0 << "EHLO " << "mail.henchmantrak.com" << "\r\n";
//			string f00 = f0.str();
//			char* helo = f00.data();
//			logx << "SEND TO SERVER " << helo << endl;
//			SSL_write(ssl, helo, strlen(helo));
//			int bytes = SSL_read(ssl, buff, sizeof(buff));
//			buff[bytes] = 0;
//			logx << counter << " [RECEIVED_TLS] " << buff << endl;
//			WriteToLog(logx.str());
//			//logx.str(string());
//			logx.clear();
//			if (!Contain(buff, "250")) throw HenchmanServiceException("Did not recieve status code 250");
//			counter++;
//
//			WriteToLog("Generating authentication string");
//			buff[0] = '\0';
//			ostringstream f1;
//			f1 << "AUTH PLAIN ";
//			string f2;
//			using namespace string_literals;
//			f2 = mail_username + "\0"s + mail_username + "\0"s + QByteArray::fromBase64Encoding(mail_password.data()).decoded.toStdString();
//			f1 << QByteArray(QByteArray::fromRawData(f2.c_str(), f2.size())).toBase64().toStdString();
//			f1 << " \r\n";
//			string f11 = f1.str();
//			char* auth = f11.data();
//			logx << "SEND TO SERVER " << auth << endl;
//			SSL_write(ssl, auth, strlen(auth));
//			bytes = SSL_read(ssl, buff, sizeof(buff));
//			buff[bytes] = 0;
//			logx << counter << " [RECEIVED_TLS] " << buff << endl;
//			WriteToLog(logx.str());
//			//logx.str(string());
//			logx.clear();
//			if (!Contain(buff, "235")) throw HenchmanServiceException("Did not recieve status code 235");
//			counter++;
//
//			buff[0] = '\0';
//			ostringstream f4;
//			f4 << "mail from: <" << mail_username << ">\r\n";
//			string f44 = f4.str();
//			char* fromemail = f44.data();
//			logx << "SEND TO SERVER " << fromemail << endl;
//			SSL_write(ssl, fromemail, strlen(fromemail));
//			bytes = SSL_read(ssl, buff, sizeof(buff));
//			buff[bytes] = 0;
//			logx << counter << " [RECEIVED_TLS] " << buff << endl;
//			WriteToLog(logx.str());
//			//logx.str(string());
//			logx.clear();
//			if (!Contain(buff, "250")) throw HenchmanServiceException("Did not recieve status code 250");
//			counter++;
//
//			buff[0] = '\0';
//			string rcpt = "rcpt to: <";
//			rcpt.append("wjaco.swanepoel@gmail.com").append(">\r\n");
//			char* rcpt1 = rcpt.data();
//			logx << "SEND TO SERVER " << rcpt1 << endl;
//			SSL_write(ssl, rcpt1, strlen(rcpt1));
//			bytes = SSL_read(ssl, buff, sizeof(buff));
//			buff[bytes] = 0;
//			logx << counter << " [RECEIVED_TLS] " << buff << endl;
//			WriteToLog(logx.str());
//			//logx.str(string());
//			logx.clear();
//			if (!Contain(buff, "250")) throw HenchmanServiceException("Did not recieve status code 250");
//			counter++;
//
//			buff[0] = '\0';
//			char* data = (char*)"DATA\r\n";
//			logx << "SEND TO SERVER " << data << endl;
//			SSL_write(ssl, data, strlen(data));
//			bytes = SSL_read(ssl, buff, sizeof(buff));
//			buff[bytes] = 0;
//			logx << counter << " [RECEIVED_TLS] " << buff << endl;
//			WriteToLog(logx.str());
//			//logx.str(string());
//			logx.clear();
//			if (!Contain(buff, "354")) throw HenchmanServiceException("Did not recieve status code 354");
//			counter++;
//
//			string Encoding = "iso-8859-2"; // charset: utf-8, utf-16, iso-8859-2, iso-8859-1
//			Encoding = "utf-8";
//
//			string subject = "Testin Mailer";
//			string msg = "This is a test Message";
//
//			// add html page layout
//			stringstream msghtml;
//
//			msghtml << "<!DOCTYPE html>\n";
//			msghtml << "<HTML lang = 'en'>\n";
//			msghtml << "<body>\n";
//			msghtml << "	<p>Please find attched report(/ s).</p>\n";
//
//			// 
//			// add atachments
//			//
//			stringstream attachmentSection;
//			vector<string>files = attachments;
//			if (files.size() > 0) {
//				for (unsigned int i = 0;i < files.size();i++) {
//					string path = files.at(i);
//					string filename = fileBasename(path.data());
//					string fc = QByteArray(get_file_contents(path.c_str())).toBase64().toStdString();
//					string extension = GetFileExtension(filename.data());
//					const char* mimetype = GetMimeTypeFromFileName(extension.data());
//					if (!(extension == "csv"))
//						continue;
//					attachmentSection << "--ToJestSeparator0000\r\n";
//					attachmentSection << "Content-Type: " << mimetype << "; name=\"" << filename << "\"\r\n";
//					attachmentSection << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n";
//					attachmentSection << "Content-Transfer-Encoding: base64" << "\r\n";
//					attachmentSection << "Content-ID: <" << QByteArray(get_file_contents(filename.c_str())).toBase64().toStdString() << ">\r\n";
//					attachmentSection << "X-Attachment-Id: " << QByteArray(get_file_contents(filename.c_str())).toBase64().toStdString() << "\r\n";
//					attachmentSection << "\r\n";
//					attachmentSection << fc << "\r\n";
//					attachmentSection << "\r\n";
//				}
//			}
//
//			msghtml << "</body>\n";
//
//			stringstream m;
//			m << "X-Priority: " << "1" << "\r\n";
//			m << "From: " << mail_username << "\r\n";
//			m << "To: " << "wjaco.swanepoel@gmail.com" << "\r\n";
//			m << "Subject: =?" << Encoding << "?Q?" << subject << "?=\r\n";
//			m << "Reply-To: " << mail_username << "\r\n";
//			m << "Return-Path: " << "willem.swanepoel@henchmantools.com" << "\r\n";
//			m << "MIME-Version: 1.0\r\n";
//			m << "Content-Type: multipart/mixed; boundary=\"ToJestSeparator0000\"\r\n\r\n";
//			m << "--ToJestSeparator0000\r\n";
//			m << "Content-Type: multipart/alternative; boundary=\"ToJestSeparatorZagniezdzony1111\"\r\n\r\n";
//
//			m << "--ToJestSeparatorZagniezdzony1111\r\n";
//			m << "Content-Type: text/plain; charset=\"" << Encoding << "\"\r\n";
//			m << "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
//			m << msg << "\r\n\r\n";
//			m << "--ToJestSeparatorZagniezdzony1111\r\n";
//			m << "Content-Type: text/html; charset=\"" << Encoding << "\"\r\n";
//			m << "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
//			m << msghtml.str() << "\r\n\r\n";
//			m << "--ToJestSeparatorZagniezdzony1111--\r\n\r\n";
//			m << attachmentSection.str();
//			m << "--ToJestSeparator0000--\r\n\r\n";
//			m << "\r\n.\r\n";
//
//			// create mime message string
//			string mimemsg = m.str();
//			logx << "Email body being sent: " << mimemsg.data() << endl;
//			char* mdata = mimemsg.data();
//			SSL_write(ssl, mdata, strlen(mdata));
//			bytes = SSL_read(ssl, buff, sizeof(buff));
//			buff[bytes] = 0;
//			logx << counter << " [RECEIVED_TLS] " << buff << endl;
//			counter++;
//
//			// send log
//			WriteToLog(logx.str());
//			if (!Contain(buff, "250")) throw HenchmanServiceException("Did not receive status code 250 from server");
//			//logx.str(string());
//			logx.clear();
//			char* qdata = (char*)"quit\r\n";
//			SSL_write(ssl, qdata, strlen(qdata));
//			bytes = SSL_read(ssl, buff, sizeof(buff));
//			buff[bytes] = 0;
//			logx << counter << " [RECEIVED_TLS] " << buff << endl;
//			if (!Contain(buff, "221")) throw HenchmanServiceException("Did not receive status code 221 from server");
//
//			SSL_free(ssl);
//		}
//
//	}
//	catch (exception& e) {
//		WriteToError("HenchmanService::SendEmail threw exception: " + (string)e.what());
//		WriteToError(logx.str());
//	}
//	//logx.str(string());
//	logx.clear();
//}