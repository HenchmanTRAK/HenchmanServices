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
 * @brief The EventManager class represents a manager for events in the HenchmanService application.
 *
 * This class contains the necessary data members and member functions to manage events, including registering event sources, reporting custom events, and getting event messages.
 *
 * @author Willem Swanepoel
 * @version 1.0
 *
 * @details
 * The class has the following data members:
 * - `std::string eventSource`: The source of the event.
 * - `HANDLE hEventSource`: The handle to the event source.
 * - `LPVOID lpMsgBuf`: The message buffer for the event.
 * - `LPVOID lpDisplayBuf`: The display buffer for the event.
 *
 * The class has the following member functions:
 * - `EventManager(std::string source)`: The constructor for the EventManager class.
 * - `~EventManager()`: The destructor for the EventManager class.
 * - `const char* EventMessage(const char* lpszFunction, std::string msg)`: Gets the event message for a specific function and message.
 * - `void ReportCustomEvent(const char* function, std::string msg, int type)`: Reports a custom event.
 */
class EventManager
{
public:

	/**
	 * @brief Constructs an EventManager object.
	 *
	 * This constructor initializes the EventManager object with the specified event source.
	 *
	 * @param source The source of the event.
	 */
	EventManager(std::string source);

	/**
	 * @brief Destroys the EventManager object.
	 *
	 * This destructor performs cleanup operations for the EventManager object.
	 */
	~EventManager();

	/**
	 * @brief Reports a custom event.
	 *
	 * This member function reports a custom event with the specified function, message, and type.
	 *
	 * @param function The function that triggered the event.
	 * @param msg The message associated with the event.
	 * @param type The type of the event.
	 */
	void ReportCustomEvent(const char *function, std::string msg = "", int type = 3);

private:
	/**
	 * @var eventSource
	 *
	 * @brief The source of the event.
	 */
	std::string eventSource;

	/**
	 * @var eventSource
	 *
	 * @brief The source of the event.
	 */
	HANDLE hEventSource = NULL;

	/**
	 * @var lpMsgBuf
	 *
	 * @brief The message buffer for the event.
	 */
	LPVOID lpMsgBuf = nullptr;

	/**
	 * @var lpDisplayBuf
	 *
	 * @brief The display buffer for the event.
	 */
	LPVOID lpDisplayBuf = nullptr;

	/**
	 * @brief Gets the event message for a specific function and message.
	 *
	 * This member function gets the event message for a specific function and message.
	 *
	 * @param lpszFunction The function that triggered the event.
	 * @param msg The message associated with the event.
	 *
	 * @return The event message.
	 */
	const char * EventMessage(const char *lpszFunction, std::string msg);
};

#endif
