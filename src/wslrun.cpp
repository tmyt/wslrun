#define WIN32_LEAN_AND_MEAN

#include <algorithm>
#include <Windows.h>
#include <tchar.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include "../inc/loader.h"

#pragma comment(lib, "shlwapi.lib")

// Avoid Windows 10(20175) bug.
// #define WORKAROUND_20175

#define MAX_CMDLINE (8192)
#define ERROR_EXIT(code, message, ...) { _tprintf(message _T("\n"), __VA_ARGS__); return code; }

struct RegHandle
{
	RegHandle() : _hKey(nullptr) {}
	RegHandle(HKEY hKey) : _hKey(hKey) {}
	~RegHandle() { CloseHandle(_hKey); }
	HKEY* operator& () { return &_hKey; }
	operator HKEY() const { return _hKey; }
private:
	HKEY _hKey;
};

struct RawString
{
	RawString() : _string(nullptr) {}
	RawString(int size) : _string(static_cast<TCHAR*>(malloc(sizeof(TCHAR)* size))) {}
	RawString(TCHAR* string) : _string(_tcsdup(string)) {}
	RawString(const RawString& source) noexcept : _string(_tcsdup(source._string)) {}
	RawString(RawString&& source) noexcept
	{
		_string = source._string;
		source._string = nullptr;
	}
	~RawString() { free(_string); }
	TCHAR& operator[](int i) const { return _string[i]; }
	RawString& operator = (const RawString& source)
	{
		if (_string) free(_string);
		_string = _tcsdup(source._string);
		return *this;
	}
	operator TCHAR* () const { return _string; }
private:
	TCHAR* _string;
};

int get_distribution_name(RawString& name);
int get_distribution_from_config(TCHAR* config_path, RawString& name);
int get_distribution_from_registry(RawString& name);
int create_hardlink(TCHAR* name);
void print_help();

int _tmain()
{
#ifdef WORKAROUND_20175
	SetEnvironmentVariable(L"PATH", L"");
#endif

	// Initialize WslApi
	WslApi wslapi;
	if (!wslapi.init()) ERROR_EXIT(-1, _T("Could not load wslapi.dll"));

	// Split args
	const auto raw_command = RawString(GetCommandLine());
	auto* const args_ptr = PathGetArgs(raw_command);
	const auto args = RawString(args_ptr);
	*args_ptr = 0;

	// Extract command name
	PathRemoveBlanks(raw_command);
	PathUnquoteSpaces(raw_command);
	auto* const command_name = PathFindFileName(raw_command);
	PathRemoveExtension(command_name);

	// Check default name
	if (_tcscmp(command_name, _T("wslrun")) == 0) {
		auto numArgs = 0;
		auto* argv = CommandLineToArgvW(GetCommandLineW(), &numArgs);
		if(numArgs == 3 && _tcscmp(argv[1], _T("--link")) == 0) return create_hardlink(argv[2]);
		return print_help(), -1;
	}

	// Build command
	const RawString command(MAX_CMDLINE);
	_stprintf_s(command, MAX_CMDLINE, _T("%s %s"), command_name, static_cast<TCHAR*>(args));

	// Get distribution name
	RawString distribution_name;
	if (get_distribution_name(distribution_name) < 0) return -1;

	// Launch process in Lxss
	DWORD exit_code = -1;
	const auto lxss_result = wslapi.WslLaunchInteractive(distribution_name, command, TRUE, &exit_code);
	if (FAILED(lxss_result)) ERROR_EXIT(-1, _T("Failed to launch wsl process. (HRESULT: %08x)"), lxss_result);
	return exit_code;
}

int get_distribution_name(RawString& name)
{
	TCHAR module_name[MAX_PATH];
	GetModuleFileName(nullptr, module_name, MAX_PATH);
	PathRemoveFileSpec(module_name);
	PathAppend(module_name, _T("wslrun.config"));
	if (PathFileExists(module_name)) return get_distribution_from_config(module_name, name);
	return get_distribution_from_registry(name);
}

int get_distribution_from_config(TCHAR* config_path, RawString& name)
{
	TCHAR distribution_name[128];
	const auto size = GetPrivateProfileString(_T("config"), _T("distribution"), nullptr, distribution_name, 128, config_path);
	if (size <= 0) ERROR_EXIT(-1, _T("Could not load config file."));
	name = RawString(distribution_name);
	return 0;
}

int get_distribution_from_registry(RawString& name)
{
	// HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Lxss
	RegHandle hKeyLxss, hKeyDistribution;
	auto result = RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Lxss"), 0, KEY_READ, &hKeyLxss);
	if (result != ERROR_SUCCESS) ERROR_EXIT(-1, _T("Failed to open Lxss registry."));

	// Query Default Distribution
	TCHAR default_distribution[128];
	DWORD size = 128, key_type;
	result = RegQueryValueEx(hKeyLxss, _T("DefaultDistribution"), nullptr, &key_type, reinterpret_cast<BYTE*>(default_distribution), &size);
	if (result != ERROR_SUCCESS) ERROR_EXIT(-1, _T("Failed to query DefaultDistribution value"));
	if (key_type != REG_SZ) ERROR_EXIT(-1, _T("Error DefaultDistribution is not REG_SZ"));

	// Open Distribution Info key
	result = RegOpenKeyEx(hKeyLxss, default_distribution, 0, KEY_READ, &hKeyDistribution);
	if (result != ERROR_SUCCESS) ERROR_EXIT(-1, _T("Failed to open %s key"), default_distribution);

	// Query Distribution Name
	TCHAR distribution_name[128];
	size = 128;
	result = RegQueryValueEx(hKeyDistribution, _T("DistributionName"), nullptr, &key_type, reinterpret_cast<BYTE*>(distribution_name), &size);
	if (result != ERROR_SUCCESS) ERROR_EXIT(-1, _T("Failed to query DistributionName value"));
	if (key_type != REG_SZ) ERROR_EXIT(-1, _T("Error DistributionName is not REG_SZ"));

	name = RawString(distribution_name);
	return 0;
}

void print_help()
{
	_tprintf(_T("wslrun is small WSL process launcher.\n"));
	_tprintf(_T("https://github.com/tmyt/wslrun\n\n"));
	_tprintf(_T("Options:\n"));
	_tprintf(_T("  --link [name]\n"));
	_tprintf(_T("    Create hardlink to [name]\n"));
}

int create_hardlink(TCHAR* arg)
{
	auto* name = PathFindFileName(arg);
	if(_tcscmp(name, _T(".")) == 0 || _tcscmp(name, _T("..")) == 0)
	{
		_tprintf(_T("error: could not create %s\n"), name);
		return -1;
	}
	TCHAR sourcePath[MAX_PATH] = { 0 };
	TCHAR targetPath[MAX_PATH] = { 0 };
	GetModuleFileName(nullptr, sourcePath, MAX_PATH);
	_tcscpy_s(targetPath, sourcePath);
	PathRemoveFileSpec(targetPath);
	PathCombine(targetPath, targetPath, name);
	auto* extension = PathFindExtension(targetPath);
	if (_tcsicmp(extension, _T(".exe")) != 0)
	{
		PathAddExtension(targetPath, _T(".exe"));
	}
	if(CreateHardLink(targetPath, sourcePath, nullptr))
	{
		_tprintf(_T("success: link %s created\n"), name);
	}else
	{
		_tprintf(_T("error: could not create %s. (%d)\n"), name, GetLastError());
	}
	return 0;
}