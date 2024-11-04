#ifndef REGISTRY_MANAGER_H
#define REGISTRY_MANAGER_H

#pragma once

#include <iostream>

//#include <tchar.h>
//#include <string>

#include <Windows.h>

/**
 * @class RegistryManager
 *
 * @brief The RegistryManager class provides functions for interacting with the Windows registry.
 *
 * This class allows you to read, write, and delete values from the Windows registry.
 * It provides methods for opening and closing the registry, reading and writing registry values,
 * and deleting registry keys and values.
 *
 * @author Willem Swanepoel
 * @version 1.0
 *
 * @details
 * - The RegistryManager class uses the Windows API for registry operations.
 * - The class handles exceptions for common errors that may occur during registry operations.
 * - The class provides a convenient way to manage the Windows registry within your application.
 */
class RegistryManager
{
public:

	/**
	 * @brief The function to open a registry key.
	 *
	 * This member function opens a registry key with the specified name and returns a handle to the key.
	 *
	 * @param hRootKey The root key of the registry.
	 * @param strKey The name of the registry key.
	 *
	 * @return A handle to the registry key.
	 */
	HKEY OpenKey(HKEY hRootKey, std::string strKey);

	/**
	 * @brief The function to remove a registry key.
	 *
	 * This member function removes a registry key with the specified name and returns the result of the operation.
	 *
	 * @param hRootKey The root key of the registry.
	 * @param strKey The name of the registry key.
	 *
	 * @return The result of the operation.
	 */
	int RemoveKey(HKEY hRootKey, std::string strKey);

	/**
	 * @brief The function to set a string value of a registry key.
	 *
	 * This member function sets a string value of a registry key with the specified name and returns the result of the operation.
	 *
	 * @param hKey The handle to the registry key.
	 * @param lpValue The name of the registry value.
	 * @param data The data of the registry value.
	 * @param type The type of the registry value.
	 *
	 * @return The result of the operation.
	 */
	int SetVal(HKEY &hKey, const char* lpValue, std::string data, DWORD type);

	/**
	 * @brief Sets a value of a registry key.
	 *
	 * This function sets a value of a registry key with the specified name and returns the result of the operation.
	 *
	 * @param hKey The handle to the registry key.
	 * @param lpValue The name of the registry value.
	 * @param data The data of the registry value.
	 * @param type The type of the registry value.
	 *
	 * @return The result of the operation.
	 */
	int SetVal(HKEY &hKey, const char* lpValue, DWORD data, DWORD type);

	/**
	 * @brief Gets a string value of a registry key.
	 *
	 * This function gets a string value of a registry key with the specified name and returns the data of the registry value.
	 *
	 * @param hKey The handle to the registry key.
	 * @param lpValue The name of the registry value.
	 * @param type The type of the registry value.
	 *
	 * @return The data of the registry value.
	 */
	std::string GetStrVal(HKEY &hKey, const char* lpValue, DWORD type);

	/**
	 * @brief Gets a value of a registry key.
	 *
	 * This function gets a value of a registry key with the specified name and returns the data of the registry value.
	 *
	 * @param hKey The handle to the registry key.
	 * @param lpValue The name of the registry value.
	 * @param type The type of the registry value.
	 *
	 * @return The data of the registry value.
	 */
	DWORD GetVal(HKEY &hKey, const char* lpValue, DWORD type);
};


#endif