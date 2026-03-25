// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the REGISTRYMANAGER_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// REGISTRYMANAGER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#pragma once

#include <iostream>
#include <string>
#include <strsafe.h>
#include <vector>

#include <windows.h>
#include <stdio.h>


#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0600
#endif


// This class is exported from the dll
namespace RegistryManager {

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
	class CRegistryManager {

	private:
		HKEY hkRegistryKey;
		HKEY hKey;
		const TCHAR* lpSubKey;

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
		CRegistryManager(HKEY hRootKey, const TCHAR* subKey);

		/**
		 * @brief The function to remove a registry key.
		 *
		 * This member function removes a registry key with the specified name and returns the result of the operation.
		 *
		 * @return The result of the operation.
		 */
		~CRegistryManager();

		// TODO: add your methods here.

		DWORD GetValSize(const TCHAR* lpValue, DWORD type);
		//LONG GetValSize(const TCHAR* lpValue, DWORD type, LPDWORD size);
		LONG GetValSize(const TCHAR* lpValue, DWORD type, DWORD* size, std::vector<TCHAR>* buffer = nullptr);

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
		LONG SetVal(const TCHAR* lpValue, DWORD type, const PVOID& data, const DWORD& size);
		LONG SetVal(const TCHAR* lpValue, DWORD type, const PVOID& data, const LPDWORD& size);
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
		LONG GetVal(const TCHAR* lpValue, DWORD type, const PVOID& buffer, LPDWORD size);

		static int RemoveTargetKey(const HKEY& hRootKey, const LPCTSTR& strKey);

		int RemoveValue(const LPCTSTR& lpValue);

		HRESULT GetSystemError(const TCHAR* msg, const DWORD& errorCode, const LPCTSTR& buffer, const DWORD& bufferSize);

	};
}