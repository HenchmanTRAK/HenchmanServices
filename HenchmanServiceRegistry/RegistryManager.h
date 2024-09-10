#ifndef REGISTRY_MANAGER_H
#define REGISTRY_MANAGER_H

#pragma once

#include <iostream>
#include <Windows.h>

using namespace std;

HKEY OpenKey(HKEY hRootKey, string strKey);
void SetStrVal(HKEY hKey, LPCTSTR lpValue, string data, DWORD type);
void SetVal(HKEY hKey, LPCTSTR lpValue, DWORD data, DWORD type);
string GetStrVal(HKEY hKey, LPCTSTR lpValue, DWORD type);
DWORD GetVal(HKEY hKey, LPCTSTR lpValue, DWORD type);

#endif