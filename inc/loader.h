#pragma once

#include <tchar.h>
#include <windows.h>
#include <wslapi.h>

#define DECL(name) decltype(name)* name
#define LOAD(handle, name) { (name) = (decltype(name))GetProcAddress(handle, #name); if(!(name)) return false; }

struct WslApi
{
	WslApi() : WslLaunchInteractive(nullptr), hDll(nullptr) {}
	~WslApi() { FreeLibrary(hDll); }

	bool init()
	{
		hDll = LoadLibrary(_T("wslapi.dll"));
		if (!hDll) return false;
		// get proc
		LOAD(hDll, WslLaunchInteractive);
		return true;
	}

	DECL(WslLaunchInteractive);

private:
	HMODULE hDll;
};

#undef DECL
#undef LOAD
