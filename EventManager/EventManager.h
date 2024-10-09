#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#pragma once


#include <event_log.h>

#include <iostream>
#include <strsafe.h>
#include <Windows.h>

class EventManager
{
public:
	EventManager(std::string source);
	~EventManager();
	void ReportCustomEvent(const char *function, std::string msg = "", int type = 3);

private:
	HANDLE hEventSource;
	std::string eventSource;
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;

	const char * EventMessage(const char *lpszFunction, std::string msg);
};

#endif
