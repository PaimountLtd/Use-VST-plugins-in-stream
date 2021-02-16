#include <windows.h>
#include <tchar.h>
#include "aeffectx.h"
#include "../headers/VSTPlugin.h"
#include "../headers/EditorWidget.h"
#include "../headers/vst-plugin-callbacks.hpp"
#include <functional>
#include <string>
#include <wchar.h>

#include <windows.h>

HWND m_hwnd;
VSTPlugin *plugin;
AEffect *m_effect;

typedef AEffect *(*vstPluginMain)(audioMasterCallback audioMaster);
HINSTANCE dllHandle = nullptr;
BOOL      shutdownInProgress  = false;

static intptr_t hostCallback_static(AEffect *effect, int32_t opcode, int32_t index, intptr_t value, void *ptr, float opt)
{
	if (effect && effect->user) {
		auto *plugin = static_cast<VSTPlugin *>(effect->user);
		return plugin->hostCallback(effect, opcode, index, value, ptr, opt);
	}

	switch (opcode) {
	case audioMasterVersion:
		return (intptr_t)2400;

	default:
		return 0;
	}
}

LRESULT WINAPI EffectWindowProc2(HWND hWnd, UINT uMsg, WPARAM wParam,
				LPARAM lParam)
{
	if (!hWnd)
		return NULL;

	blog(LOG_WARNING, "EditorWidget: EffectWindowProc, uMsg: %u", uMsg);
	VSTPlugin *plugin = (VSTPlugin *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg) {
		case WM_CLOSE: {
			shutdownInProgress = true;
			blog(LOG_WARNING,
			     "EditorWidget: EffectWindowProc, received closeEditor msg");
			if (plugin) {
				plugin->closeEditor();
			}
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void show()
{
	blog(LOG_WARNING, "EditorWidget::show Window");
	ShowWindow(m_hwnd, SW_SHOW);
	ShowWindow(m_hwnd, SW_HIDE);
	ShowWindow(m_hwnd, SW_RESTORE);
}

void setWindowTitle(const char *title)
{
	TCHAR wstrTitle[256];
	MultiByteToWideChar(CP_UTF8, 0, title, -1, &wstrTitle[0], 256);
	SetWindowText(m_hwnd, &wstrTitle[0]);
}

void unloadLibrary()
{
	if (dllHandle) {
		FreeLibrary(dllHandle);
		dllHandle = nullptr;
	}
}


int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
		     _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	WNDCLASSEXW wcex{sizeof(wcex)};
	VstRect *vstRect = nullptr;
	plugin = new VSTPlugin(nullptr);
	plugin->loadEffectFromPath("C:/Program Files/VSTPlugins/Flux/BitterSweetV3.vst/Contents/x64/BitterSweetV3.dll");
	//plugin->loadEffectFromPath("C:/Program Files/Steinberg/VSTPlugins/TDR Nova.dll");
	plugin->openInterfaceWhenActive = false;
	        //	AEffect* m_effect = loadEffectFromPath("C:/Program Files/VSTPlugins/Flux/BitterSweetV3.vst/Contents/x64/BitterSweetV3.dll");
	m_effect = plugin->getEffect();

	wcex.lpfnWndProc = EffectWindowProc2;
	wcex.hInstance = GetModuleHandleW(nullptr);
	wcex.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassExW(&wcex);

	LONG style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DLGFRAME |
		     WS_POPUP | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;

	LONG exStyle = WS_EX_DLGMODALFRAME;

	m_hwnd = CreateWindowEx(exStyle, wcex.lpszClassName, TEXT(""), style, 0,
				0, 0, 0, nullptr, nullptr, nullptr, nullptr);

	//EnableMenuItem(GetSystemMenu(m_hwnd, FALSE), SC_CLOSE,	       MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

	m_effect->dispatcher(m_effect, effEditOpen, 0, 0, m_hwnd, 0);

	blog(LOG_WARNING, "EditorWidget after effEditOpen");

	m_effect->dispatcher(m_effect, effEditGetRect, 0, 0, &vstRect, 0);
	blog(LOG_WARNING, "EditorWidget after effEditGetRect");

	blog(LOG_WARNING,
	     "EditorWidget::buildEffectContainer_worker before show");
	show();
	if (vstRect) {
		SetWindowPos(m_hwnd, 0, vstRect->left, vstRect->top,
			     vstRect->right, vstRect->bottom, 0);
	}

	RECT rect = {0};
	RECT border = {0};

	if (vstRect) {
		RECT clientRect;

		GetWindowRect(m_hwnd, &rect);
		GetClientRect(m_hwnd, &clientRect);

		border.left = clientRect.left - rect.left;
		border.right = rect.right - clientRect.right;

		border.top = clientRect.top - rect.top;
		border.bottom = rect.bottom - clientRect.bottom;
	}

	/* Despite the use of AdjustWindowRectEx here, the docs lie to us
	   when they say it will give us the smallest size to accomodate the
	   client area wanted. Since Vista came out, we now have to take into
	   account hidden borders introduced by DWM. This can only be done
	   *after* we've created the window as well. */
	rect.left -= border.left;
	rect.top -= border.top;
	rect.right += border.left + border.right;
	rect.bottom += border.top + border.bottom;

	SetWindowPos(m_hwnd, 0, rect.left, rect.top, rect.right, rect.bottom,
		     0);
	SetWindowPos(m_hwnd, 0, 0, 0, 0, 0, SWP_NOSIZE);
	//show();
	setWindowTitle(plugin->effectName);
	MSG msg;
	// create thread message queue
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	BOOL bRet;
	while (!shutdownInProgress && (bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
		if (bRet == -1 || shutdownInProgress) {
			blog(LOG_WARNING, "Exit GetMessage loop");
			break;
		} else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	//Sleep(20000);
	
	return 0;
}

