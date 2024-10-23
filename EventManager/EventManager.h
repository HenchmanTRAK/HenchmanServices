#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#pragma once

#include <event_log.h>

#include <iostream>
#include <strsafe.h>
#include <Windows.h>

/**
 * @class EventManager
 *
 * @brief The EventManager class provides functions for managing events and logging messages.
 *
 * This class allows you to report custom events and log messages to the event log.
 * It provides methods for initializing the event manager, reporting custom events,
 * and logging messages.
 *
 * @author Willem Swanepoel
 * @version 1.0
 *
 * @details
 * - The EventManager class uses the Windows API for event logging.
 * - The class handles exceptions for common errors that may occur during event logging.
 * - The class provides a convenient way to manage events and log messages within your application.
 *
 * @see Windows Event Log
 * @see ReportEvent
 * @see RegisterEventSource
 */
class EventManager
{
public:

	/**
	 * @brief Initializes the event manager with the specified source.
	 *
	 * This function initializes the event manager with the specified source and registers
	 * the event source with the event log.
	 *
	 * @param source The source of the event.
	 */
	EventManager(std::string source);

	/**
	 * @brief Deregisters the event source and frees resources.
	 *
	 * This function deregisters the event source and frees resources when the event manager is destroyed.
	 */
	~EventManager();

	/**
	 * @brief Reports a custom event.
	 *
	 * This function reports a custom event with the specified function, message, and type.
	 *
	 * @param function The function that triggered the event.
	 * @param msg The message associated with the event.
	 * @param type The type of the event (1 = INFORMATION, 2 = WARNING, 3 = ERROR).
	 */
	void ReportCustomEvent(const char *function, std::string msg = "", int type = 3);

private:
	std::string eventSource;
	HANDLE hEventSource;
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;

	/**
	 * @brief Gets the event message for the specified function and message.
	 *
	 * This function gets the event message for the specified function and message.
	 *
	 * @param lpszFunction The function that triggered the event.
	 * @param msg The message associated with the event.
	 *
	 * @return The event message.
	 */
	const char * EventMessage(const char *lpszFunction, std::string msg);
};

#endif
