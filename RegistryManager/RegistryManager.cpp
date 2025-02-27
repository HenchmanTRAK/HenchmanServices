// RegistryManager.cpp : Defines the exported functions for the DLL.
//

#include "RegistryManager.h"

using namespace RegistryManager;

#ifndef UNICODE
#define cout std::cout
#else
#define cout std::wcout
#endif

#ifndef UNICODE
#define tstring std::string
#else
#define tstring std::wstring
#endif


// This is an example of an exported variable
//REGISTRYMANAGER_API int nRegistryManager=0;

// This is an example of an exported function.
//REGISTRYMANAGER_API int fnRegistryManager(void)
//{
//    return 0;
//}

// This is the constructor of a class that has been exported.
CRegistryManager::CRegistryManager(HKEY hRootKey, const LPCTSTR& subKey)
	:hKey(hRootKey), lpSubKey(subKey)
{
	DWORD lpdwDisposition;
	/*LONG nError = RegCreateKeyEx(hKey, lpSubKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkRegistryKey, &lpdwDisposition);
	std::cout << lpdwDisposition << std::endl;*/

	//std::cout<< "Opening registry key: " << lpSubKey << "\n";
	//cout << "Attempting to open registry key: " << subKey << "\n";
	LONG nError = RegOpenKeyEx(hKey, lpSubKey, NULL, KEY_ALL_ACCESS, &hkRegistryKey);
	if (nError == ERROR_FILE_NOT_FOUND)
	{
		//std::cout << "Failed to open, attempting to create registry key instead\n";
		//cout << "Failed to open key, attempting to create instead\n";
		nError = RegCreateKeyEx(hKey, lpSubKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkRegistryKey, &lpdwDisposition);
		//LOG << lpdwDisposition << "\n";
	}

	if (nError)
	{
		tstring err = "Error: ";
		err.append(std::to_string(nError)).append(" Could not find or create: ").append(lpSubKey).append("\n");
		//throw std::runtime_error(err.c_str());
		cout << err.c_str();
		//cout << "Error: " << nError << " Could not get data from registry value " << lpValue << "\n";
	}

	if (nError)
		cout << "Error: " << nError << " Could not find or create " << lpSubKey << "\n";

	return;
}

CRegistryManager::~CRegistryManager()
{
	/*LONG nError = RegDeleteTree(hkRegistryKey, NULL);
	if (nError)
		std::cout << "Failed to delete regitry key error: " << nError << std::endl;*/
	cout << "closing registry key\n";
	if (hkRegistryKey)
		RegCloseKey(hkRegistryKey);
}

LONG CRegistryManager::SetVal(const TCHAR* lpValue, DWORD type, const PVOID& data, const DWORD& size)
{

	//std::cout << "supplied value: " << (char *)data << " with size of: " << size << "\n";

	LONG nError = RegSetKeyValue(hkRegistryKey, NULL, lpValue, type, data, size);
	if (nError)
	{
		tstring err = "Error: ";
		err.append(std::to_string(nError)).append(" Could not set registry value : ").append(lpValue).append("with data : ").append((TCHAR*)data).append("\n");
		//throw std::runtime_error(err.c_str());
		cout << err.c_str();
		//cout << "Error: " << nError << " Could not set registry value: " << lpValue << "with data: " << (TCHAR *)data << "\n";
	}

	return nError;
}

LONG CRegistryManager::GetVal(const TCHAR* lpValue, DWORD type, const PVOID& buffer, const DWORD& size)
{

	LONG nError = RegGetValue(hkRegistryKey, NULL, lpValue, RRF_RT_ANY | RRF_NOEXPAND | RRF_ZEROONFAILURE, &type, buffer, (DWORD*)&size);

	//if (nError == ERROR_FILE_NOT_FOUND)
	//	data = 0; // The value will be created and set to data next time SetVal() is called.
	//else
	if (nError)
	{
		tstring err = "Error: ";
		err.append(std::to_string(nError)).append(" Could not get data from registry value: ").append(lpValue).append("\n");
		//throw std::runtime_error(err.c_str());
		cout << err.c_str();
		//cout << "Error: " << nError << " Could not get data from registry value " << lpValue << "\n";
	}

	return nError;
}

int CRegistryManager::RemoveTargetKey(HKEY hRootKey, LPCTSTR strKey)
{
	//LONG nError = RegDeleteKey(hRootKey, strKey);
	LONG nError = RegDeleteTree(hRootKey, strKey);
	if (nError)
	{
		tstring err = "Error: ";
		err.append(std::to_string(nError)).append(" Failed to delete regitry key error: ").append(strKey).append("\n");
		//throw std::runtime_error(err.c_str());
		cout << err.c_str();
		//cout << "Error: " << nError << " Could not get data from registry value " << lpValue << "\n";
	}
	if (nError)
		cout << "Failed to delete regitry key error: " << nError << "\n";
	return nError;
}

int CRegistryManager::RemoveValue(LPCTSTR lpValue)
{
	LONG nError = RegDeleteValue(hkRegistryKey, lpValue);
	if (nError)
	{
		tstring err = "Error: ";
		err.append(std::to_string(nError)).append(" Failed to delete target value: ").append(lpValue).append("\n");
		//throw std::runtime_error(err.c_str());
		cout << err.c_str();
		//cout << "Error: " << nError << " Could not get data from registry value " << lpValue << "\n";
	}
	if (nError)
		cout << "Error: " << nError << " Failed to delete target value: " << lpValue << "\n";

	return nError;
}