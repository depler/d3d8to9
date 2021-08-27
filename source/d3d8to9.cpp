/**
 * Copyright (C) 2015 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/d3d8to9#license
 */

#include "d3dx9.hpp"
#include "d3d8to9.hpp"

PFN_D3DXAssembleShader D3DXAssembleShader = nullptr;
PFN_D3DXDisassembleShader D3DXDisassembleShader = nullptr;
PFN_D3DXLoadSurfaceFromSurface D3DXLoadSurfaceFromSurface = nullptr;

#ifndef D3D8TO9NOLOG
 // Very simple logging for the purpose of debugging only.
std::ofstream LOG;
#endif

extern "C" IDirect3D8 *WINAPI Direct3DCreate8(UINT SDKVersion)
{
#ifndef D3D8TO9NOLOG
	LOG << "Redirecting '" << "Direct3DCreate8" << "(" << SDKVersion << ")' ..." << std::endl;
	LOG << "> Passing on to 'Direct3DCreate9':" << std::endl;
#endif

	IDirect3D9 *const d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (d3d == nullptr)
	{
		return nullptr;
	}

	// Load D3DX
	if (!D3DXAssembleShader || !D3DXDisassembleShader || !D3DXLoadSurfaceFromSurface)
	{
		const HMODULE module = LoadLibrary(TEXT("d3dx9_43.dll"));

		if (module != nullptr)
		{
			D3DXAssembleShader = reinterpret_cast<PFN_D3DXAssembleShader>(GetProcAddress(module, "D3DXAssembleShader"));
			D3DXDisassembleShader = reinterpret_cast<PFN_D3DXDisassembleShader>(GetProcAddress(module, "D3DXDisassembleShader"));
			D3DXLoadSurfaceFromSurface = reinterpret_cast<PFN_D3DXLoadSurfaceFromSurface>(GetProcAddress(module, "D3DXLoadSurfaceFromSurface"));
		}
		else
		{
#ifndef D3D8TO9NOLOG
			LOG << "Failed to load d3dx9_43.dll! Some features will not work correctly." << std::endl;
#endif
			if (MessageBox(nullptr, TEXT(
				"Failed to load d3dx9_43.dll! Some features will not work correctly.\n\n"
				"It's required to install the \"Microsoft DirectX End-User Runtime\" in order to use d3d8to9.\n\n"
				"Please click \"OK\" to open the official download page or \"CANCEL\" to continue anyway."), nullptr, MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND | MB_OKCANCEL | MB_DEFBUTTON1) == IDOK)
			{
				ShellExecute(nullptr, TEXT("open"), TEXT("https://www.microsoft.com/download/details.aspx?id=35"), nullptr, nullptr, SW_SHOW);

				return nullptr;
			}
		}
	}

	return new Direct3D8(d3d);
}

UINT fpsLimit = 0;
bool windowedMode = false;
bool fixTextures = false;

bool WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) 
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
#ifndef D3D8TO9NOLOG
		static bool LogMessageFlag = true;

		if (!LOG.is_open())
			LOG.open("d3d8.log", std::ios::trunc);

		if (!LOG.is_open() && LogMessageFlag)
		{
			LogMessageFlag = false;
			MessageBox(nullptr, TEXT("Failed to open debug log file \"d3d8.log\"!"), nullptr, MB_ICONWARNING);
		}
#endif

		wchar_t path[MAX_PATH];
		GetModuleFileName((HINSTANCE)&__ImageBase, path, MAX_PATH);

		*wcsrchr(path, L'\\') = L'\0';
		wcscat_s(path, L"\\d3d8.ini");
		
		fpsLimit = GetPrivateProfileInt(L"main", L"fps_limit", 0, path);
		windowedMode = GetPrivateProfileInt(L"main", L"windowed_mode", 0, path) == 1;
		fixTextures = GetPrivateProfileInt(L"main", L"fix_textures", 0, path) == 1;

#ifndef D3D8TO9NOLOG
		LOG << "External settings loaded:" << std::endl;
		LOG << "fps_limit = " << fpsLimit << std::endl;
		LOG << "windowed_mode = " << windowedMode << std::endl;
		LOG << "fix_textures = " << fixTextures << std::endl;
#endif
		break;
	}
	case DLL_PROCESS_DETACH:
		FreeLibrary(hModule);
		break;
	}
	return true;
}

void ForceWindowedMode(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HMONITOR monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	int DesktopResX = info.rcMonitor.right - info.rcMonitor.left;
	int DesktopResY = info.rcMonitor.bottom - info.rcMonitor.top;

	int left = (int)(((float)DesktopResX / 2.0f) - ((float)pPresentationParameters->BackBufferWidth / 2.0f));
	int top = (int)(((float)DesktopResY / 2.0f) - ((float)pPresentationParameters->BackBufferHeight / 2.0f));

	pPresentationParameters->Windowed = true;

	SetWindowPos(pPresentationParameters->hDeviceWindow, HWND_NOTOPMOST, left, top, pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, SWP_SHOWWINDOW);
}

void ForceFpsLimit()
{
	static LARGE_INTEGER PerformanceCount1;
	static LARGE_INTEGER PerformanceCount2;
	static bool bOnce = false;
	static double targetFrameTime = 1000.0 / fpsLimit;
	static double t = 0.0;
	static DWORD i = 0;

	if (!bOnce)
	{
		bOnce = true;
		QueryPerformanceCounter(&PerformanceCount1);
		PerformanceCount1.QuadPart = PerformanceCount1.QuadPart >> i;
	}

	while (true)
	{
		QueryPerformanceCounter(&PerformanceCount2);

		if (t == 0.0)
		{
			LARGE_INTEGER PerformanceCount3;
			static bool bOnce2 = false;

			if (!bOnce2)
			{
				bOnce2 = true;
				QueryPerformanceFrequency(&PerformanceCount3);
				i = 0;
				t = 1000.0 / (double)PerformanceCount3.QuadPart;
				auto v = t * 2147483648.0;
				if (60000.0 > v)
				{
					while (true)
					{
						++i;
						v *= 2.0;
						t *= 2.0;

						if (60000.0 <= v)
							break;
					}
				}
			}

			SleepEx(0, 1);
			break;
		}

		if (((double)((PerformanceCount2.QuadPart >> i) - PerformanceCount1.QuadPart) * t) >= targetFrameTime)
			break;

		SleepEx(0, 1);
	}

	QueryPerformanceCounter(&PerformanceCount2);
	PerformanceCount1.QuadPart = PerformanceCount2.QuadPart >> i;
}