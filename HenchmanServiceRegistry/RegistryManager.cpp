
#include "RegistryManager.h"

HKEY OpenKey(HKEY hRootKey, string strKey) 
{
	HKEY hKey;
	//string strKey = "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" + SERVICE_NAME;
	//cout << strKey << endl;
	LONG nError = RegOpenKeyEx(hRootKey, strKey.data(), NULL, KEY_ALL_ACCESS, &hKey);
	if (nError == ERROR_FILE_NOT_FOUND)
	{
		cout << "Creating registry key: " << strKey << endl;
		nError = RegCreateKeyEx(hRootKey, strKey.data(), NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
	}

	if (nError)
		cout << "Error: " << nError << " Could not find or create " << strKey << endl;

	return hKey;
}

void SetStrVal(HKEY hKey, LPCTSTR lpValue, string data, DWORD type)
{
	LONG nError = RegSetValueEx(hKey, lpValue, NULL, type, (LPBYTE)data.c_str(), data.size() + 1);

	if (nError)
		cout << "Error: " << nError << " Could not set registry value: " << (char*)lpValue << endl;
}

void SetVal(HKEY hKey, LPCTSTR lpValue, DWORD data, DWORD type)
{
	LONG nError = RegSetValueEx(hKey, lpValue, NULL, type, (LPBYTE)&data, sizeof(data));

	if (nError)
		cout << "Error: " << nError << " Could not set registry value: " << (char*)lpValue << endl;
}

string GetStrVal(HKEY hKey, LPCTSTR lpValue, DWORD type)
{
	DWORD buffSize = 1024;
	char data[1024] = "\0";
	string reply;
	//cout << lpValue << endl;
	//LONG nError = RegQueryValueEx(hKey, lpValue, NULL, &type, (LPBYTE)data, &buffSize);
	LONG nError = RegGetValue(hKey, NULL, lpValue, RRF_RT_ANY, NULL, data, &buffSize);

	if (nError == ERROR_FILE_NOT_FOUND) {
		cout << "No File Found" << endl;
		reply = ""; // The value will be created and set to data next time SetVal() is called.
	}
	else if (nError)
		cout << "Error: " << nError << " Could not get registry value " << (char*)lpValue << endl;
	else {
		//cout << "data: " << data << bool(nError == ERROR_FILE_NOT_FOUND) << endl;
		reply = data;
		reply.resize(buffSize);
		//cout << "reply: " << reply << endl;

	}
	//getchar();
	return reply;
}

DWORD GetVal(HKEY hKey, LPCTSTR lpValue, DWORD type)
{
	DWORD size = sizeof(DWORD);
	DWORD data = 0;
	LONG nError = RegQueryValueEx(hKey, lpValue, NULL, &type, (LPBYTE)&data, &size);

	if (nError == ERROR_FILE_NOT_FOUND)
		data = 0; // The value will be created and set to data next time SetVal() is called.
	else if (nError)
		cout << "Error: " << nError << " Could not get registry value " << (char*)lpValue << endl;

	return data;
}