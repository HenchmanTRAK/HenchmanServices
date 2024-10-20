#ifndef REGISTRY_MANAGER_H
#define REGISTRY_MANAGER_H

#pragma once

#include <iostream>
//#include <tchar.h>
//#include <string>
#include <Windows.h>
HKEY OpenKey(HKEY hRootKey, std::string strKey);

int RemoveKey(HKEY hRootKey, std::string strKey);

void SetStrVal(HKEY &hKey, const char* lpValue, std::string data, DWORD type);

void SetVal(HKEY &hKey, const char* lpValue, DWORD data, DWORD type);

std::string GetStrVal(HKEY &hKey, const char* lpValue, DWORD type);

DWORD GetVal(HKEY &hKey, const char* lpValue, DWORD type);

#endif