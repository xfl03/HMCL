#include "stdafx.h"
#include "HMCL.h"

using namespace std;

const LPCWSTR JDK_OLD = L"SOFTWARE\\JavaSoft\\Java Development Kit";
const LPCWSTR JRE_OLD = L"SOFTWARE\\JavaSoft\\Java Runtime Environment";
const LPCWSTR JDK_NEW = L"SOFTWARE\\JavaSoft\\JDK";
const LPCWSTR JRE_NEW = L"SOFTWARE\\JavaSoft\\JRE";

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

LSTATUS MyRegQueryValue(HKEY hKey, LPCWSTR subKey, DWORD dwType, wstring &out)
{
	DWORD dwSize = 0;
	LSTATUS ret = RegQueryValueEx(hKey, subKey, 0, &dwType, NULL, &dwSize);
	if (ret != ERROR_SUCCESS) return ret;
	WCHAR *buffer = new WCHAR[dwSize];
	ret = RegQueryValueEx(hKey, subKey, 0, &dwType, (LPBYTE)buffer, &dwSize);
	if (ret != ERROR_SUCCESS) return ret;
	out = buffer;
	delete[] buffer;
	return ERROR_SUCCESS;
}

class Version
{
public:
	int ver[4];

	Version(const wstring &rawString)
	{
		int idx = 0;
		ver[0] = ver[1] = ver[2] = ver[3] = 0;
		for (auto &i : rawString)
		{
			if (idx >= 4) break;
			if (i == '.') ++idx;
			else if (i == '_') ++idx;
			else if (isdigit(i)) ver[idx] = ver[idx] * 10 + (i - L'0');
		}
	}

	bool operator<(const Version &other) const
	{
		for (int i = 0; i < 4; ++i)
			if (ver[i] != other.ver[i])
				return ver[i] < other.ver[i];
		return false;
	}
} JAVA_8(L"1.8"), JAVA_11(L"11");

bool oldJavaFound = false, newJavaFound = false;

bool FindJava(HKEY rootKey, LPCWSTR subKey, wstring &path)
{
	WCHAR javaVer[MAX_KEY_LENGTH];  // buffer for subkey name, special for JavaVersion
	DWORD cbName;                   // size of name string
	DWORD cSubKeys = 0;             // number of subkeys
	DWORD cbMaxSubKey;              // longest subkey size
	DWORD cValues;                  // number of values for key
	DWORD cchMaxValue;              // longest value name
	DWORD cbMaxValueData;           // longest value data
	LSTATUS result;

	HKEY hKey;
	if (ERROR_SUCCESS != (result = RegOpenKeyEx(rootKey, subKey, 0, KEY_WOW64_64KEY | KEY_READ, &hKey)))
		return false;

	RegQueryInfoKey(
		hKey,                    // key handle
		NULL,                    // buffer for class name
		NULL,                    // size of class string
		NULL,                    // reserved
		&cSubKeys,               // number of subkeys
		&cbMaxSubKey,            // longest subkey size
		NULL,                    // longest class string
		&cValues,                // number of values for this key
		&cchMaxValue,            // longest value name
		&cbMaxValueData,         // longest value data
		NULL,                    // security descriptor
		NULL);                   // last write time

	if (!cSubKeys)
		return false;

	bool flag = false;
	for (DWORD i = 0; i < cSubKeys; ++i)
	{
		cbName = MAX_KEY_LENGTH;
		if (ERROR_SUCCESS != (result = RegEnumKeyEx(hKey, i, javaVer, &cbName, NULL, NULL, NULL, NULL)))
			continue;

		HKEY javaKey;
		if (ERROR_SUCCESS != RegOpenKeyEx(hKey, javaVer, 0, KEY_READ, &javaKey))
			continue;

		if (ERROR_SUCCESS == MyRegQueryValue(javaKey, L"JavaHome", REG_SZ, path))
		{
			if (Version(javaVer) < JAVA_8)
				oldJavaFound = true;
			else if (!(Version(javaVer) < JAVA_11))
				newJavaFound = true;
			else
				flag = true;
		}

		if (flag)
			break;
	}

	RegCloseKey(hKey);

	return flag;
}

LSTATUS MyGetModuleFileName(HMODULE hModule, wstring &out)
{
	DWORD res, size = MAX_PATH;
	out = wstring();
	out.resize(size);
	while ((res = GetModuleFileName(hModule, &out[0], size)) == size)
	{
		out.resize(size += MAX_PATH);
	}
	if (res == 0)
		return GetLastError();
	else
	{
		out.resize(size - MAX_PATH + res);
		return ERROR_SUCCESS;
	}
}

LSTATUS MyGetEnvironmentVariable(LPCWSTR name, wstring &out)
{
	DWORD res, size = MAX_PATH;
	out = wstring();
	out.resize(size);
	while ((res = GetEnvironmentVariable(name, &out[0], size)) == size)
	{
		out.resize(size += MAX_PATH);
	}
	if (res == 0)
		return GetLastError();
	else
	{
		out.resize(size - MAX_PATH + res);
		return ERROR_SUCCESS;
	}
}

bool MyCreateProcess(wstring const& command)
{
	wstring writable_command = command;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	si.cb = sizeof(si);
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));

	return (CreateProcess(NULL, &writable_command[0], NULL, NULL, false, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi));
}

bool FindFirstFileExists(LPCWSTR lpPath, DWORD dwFilter)
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(lpPath, &fd);
	bool bFilter = (false == dwFilter) ? true : fd.dwFileAttributes & dwFilter;
	bool ret = ((hFind != INVALID_HANDLE_VALUE) && bFilter) ? true : false;
	FindClose(hFind);
	return ret;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	wstring path, exeName;

	if (ERROR_SUCCESS != MyGetModuleFileName(NULL, exeName))
		return 1;

	// Find JRE or JDK installation in registry.
	if (FindJava(HKEY_LOCAL_MACHINE, JDK_OLD, path) ||
		FindJava(HKEY_LOCAL_MACHINE, JRE_OLD, path) ||
		FindJava(HKEY_LOCAL_MACHINE, JDK_NEW, path) ||
		FindJava(HKEY_LOCAL_MACHINE, JRE_NEW, path) ||
		ERROR_SUCCESS == MyGetEnvironmentVariable(L"JAVA_HOME", path))
	{
		if (MyCreateProcess(L"\"" + path + L"\\bin\\javaw.exe\" -XX:MinHeapFreeRatio=5 -XX:MaxHeapFreeRatio=15 -jar \"" + exeName + L"\""))
			return 0;
	}
	//
	// Or we just call java in path.
	if (MyCreateProcess(L"javaw -XX:MinHeapFreeRatio=5 -XX:MaxHeapFreeRatio=15 -jar \"" + exeName + L"\""))
		return 0;

	// Or we try to search Java in C:\Program Files
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFile(L"C:\\Program Files\\Java\\*", &data);      // DIRECTORY

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			wstring javaw = wstring(L"C:\\Program Files\\Java\\") + data.cFileName + wstring(L"\\bin\\javaw.exe");
			if (FindFirstFileExists(javaw.c_str(), 0)) {
				if (MyCreateProcess(javaw + L" -XX:MinHeapFreeRatio=5 -XX:MaxHeapFreeRatio=15 -jar \"" + exeName + L"\""))
					return 0;
			}
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}

	MessageBox(NULL, L"Java installation cannot be found in this computer, please download it from https://java.com \n"
		L"未能在这台电脑上找到Java 8~Java 10，请从 https://java.com 下载安装Java", L"Error", MB_ICONERROR | MB_OK);
	ShellExecute(0, 0, L"https://java.com/", 0, 0, SW_SHOW);
	return 1;
}
