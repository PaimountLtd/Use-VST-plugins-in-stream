/*****************************************************************************
Copyright (C) 2016-2017 by Colin Edwards.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include <Dwmapi.h>
#include <gl\gl.h>          // Header File For The OpenGL32 Library
#include <gl\glu.h>         // Header File For The GLu32 Library
//#include <gl\glaux.h>       // Header File For The Glaux Library
#include "../headers/EditorWidget.h"
#include <functional>
#include <string>

using namespace std;


LRESULT WINAPI EffectWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd)
		return NULL;

	//blog(LOG_WARNING, "EditorWidget: EffectWindowProc, uMsg: %u", uMsg);
	VSTPlugin *plugin = (VSTPlugin *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg) {
	case WM_CLOSE: {
		blog(LOG_WARNING, "EditorWidget: EffectWindowProc, received closeEditor msg");
		if (plugin) {
			plugin->closeEditor();
		}
	}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

EditorWidget::~EditorWidget()
{
}	

void EditorWidget::buildEffectContainer()
{
	//m_effect     = effect;
	blog(LOG_WARNING, "EditorWidget: buildEffectContainer, about to start thread");

	m_threadStarted  = CreateEvent(NULL,              // default security attributes
	              TRUE,              // manual-reset event
	              FALSE,             // initial state is nonsignaled
	              TEXT("ThreadStart") // object name
	); 

	m_hwnd       = 0;
	windowWorker = std::thread(std::bind(&EditorWidget::buildEffectContainer_worker, this));
}

void EditorWidget::createWindow() {
	if (m_hwnd) {
		show();
		return;
	}
	WNDCLASSEXW wcex{sizeof(wcex)};
	VstRect *   vstRect = nullptr;

	wcex.lpfnWndProc   = EffectWindowProc;
	wcex.hInstance     = GetModuleHandleW(nullptr);
	wcex.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassExW(&wcex);

	LONG style =
	        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DLGFRAME | WS_POPUP | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;

	LONG exStyle = WS_EX_DLGMODALFRAME;

	m_hwnd = CreateWindowEx(
	        exStyle, wcex.lpszClassName, TEXT(""), style, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);

	EnableMenuItem(GetSystemMenu(m_hwnd, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

	blog(LOG_WARNING, "EditorWidget::buildEffectContainer_worker CreateWindowEx, m_hwnd: %p ", m_hwnd);
	blog(LOG_WARNING, "EditorWidget::m_effect->dispatcher addr: %p", m_effect->dispatcher);

	m_effect->dispatcher(m_effect, effEditOpen, 0, 0, m_hwnd, 0);

	blog(LOG_WARNING, "EditorWidget after effEditOpen");

	m_effect->dispatcher(m_effect, effEditGetRect, 0, 0, &vstRect, 0);
	blog(LOG_WARNING, "EditorWidget after effEditGetRect");

	blog(LOG_WARNING, "EditorWidget::buildEffectContainer_worker before show");
	show();
	if (vstRect) {
		SetWindowPos(m_hwnd, 0, vstRect->left, vstRect->top, vstRect->right, vstRect->bottom, 0);
	}

	RECT rect   = {0};
	RECT border = {0};

	if (vstRect) {
		RECT clientRect;

		GetWindowRect(m_hwnd, &rect);
		GetClientRect(m_hwnd, &clientRect);

		border.left  = clientRect.left - rect.left;
		border.right = rect.right - clientRect.right;

		border.top    = clientRect.top - rect.top;
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

	SetWindowPos(m_hwnd, 0, rect.left, rect.top, rect.right, rect.bottom, 0);
	SetWindowPos(m_hwnd, 0, 0, 0, 0, 0, SWP_NOSIZE);
	//show();
	setWindowTitle(plugin->effectName);
}

void EditorWidget::buildEffectContainer_worker()
{
	blog(LOG_WARNING,
		"EditorWidget: buildEffectContainer_worker"
	);
	MSG msg;
	//create thread message queue
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	BOOL bRet;

	bool shutdown = false;
	if (!SetEvent(m_threadStarted)) {
		blog(LOG_WARNING, "Set Event Failed");
	}
	
	while (!shutdown && (bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1) {
			blog(LOG_WARNING, "Exit GetMessage loop");
			break;
		} else {
			if (msg.message == WM_USER_SET_TITLE) {
				const char *title = reinterpret_cast<const char *>(msg.wParam);
				blog(LOG_WARNING, "EditorWidget worker got title %s ", title);
				setWindowTitle(title);
			} else if (msg.message == WM_USER_SHOW) {
				blog(LOG_WARNING, "EditorWidget worker got show window plugin");
				if (!m_effect) {
					blog(LOG_WARNING, "EditorWidget m_effect is NULL");
					needs_to_show_window = true;
					continue;
				}
				this->createWindow();
			} else if (msg.message == WM_USER_CLOSE) {
				if (shutdown) {
					continue;
				}
				blog(LOG_WARNING, "EditorWidget worker got window close");
				close();
				dispatcherClose();
				shutdown = true;
			} else if (msg.message == WM_USER_LOAD_DLL) {
				shared_ptr<string> path_str (new string(*reinterpret_cast<string *>(msg.wParam)));
				const char *path = path_str->c_str();
				m_path = path;
				blog(LOG_WARNING, "EditorWidget worker got load DLL %s ", path);
				plugin->loadEffectFromPath(path);
				m_effect = plugin->getEffect();
				if (needs_to_show_window) {
					needs_to_show_window = false;
					this->createWindow();
				}
			} else if (msg.message == WM_USER_SETCHUNK) {
				shared_ptr<string> chunk_str = make_shared<string>(*reinterpret_cast<string *>(msg.wParam));
				const char *chunk = chunk_str->c_str();
				blog(LOG_WARNING, "EditorWidget worker got show chunk DLL %s ", chunk);
				plugin->setChunk(chunk);
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

	}
	return;
}

void EditorWidget::send_setChunk(string chunk) {
	blog(LOG_WARNING, "EditorWidget::send_setChunk , path: %s", chunk.c_str());

	DWORD res = WaitForSingleObject(m_threadStarted, // event handle
	                                INFINITE);       // indefinite wait
	if (res != WAIT_OBJECT_0) {
		blog(LOG_WARNING, "EditorWidget::send_setChunk WaitForSingeObject failed: %ul", res);
		return;
	}
	
	PostThreadMessage(GetThreadId(windowWorker.native_handle()),
	                  WM_USER_SETCHUNK,
	                  reinterpret_cast<WPARAM>(new string(chunk.c_str())),
	                  0);
			 
}

void EditorWidget::send_loadEffectFromPath(string path)
{
	blog(LOG_WARNING, "EditorWidget::send_loadEffectFromPath , path: %s", path.c_str());
	DWORD res = WaitForSingleObject(m_threadStarted, // event handle
	                                INFINITE);       // indefinite wait
	if (res != WAIT_OBJECT_0) {
		blog(LOG_WARNING, "EditorWidget::send_loadEffectFromPath WaitForSingeObject failed: %ul", res);
		return;
	}
	
	PostThreadMessage(GetThreadId(windowWorker.native_handle()),
	                  WM_USER_LOAD_DLL,
	                  reinterpret_cast<WPARAM>(new string(path.c_str())),
	                  0);
			  
}

void EditorWidget::setWindowTitle(const char *title)
{
	TCHAR wstrTitle[256];
	MultiByteToWideChar(CP_UTF8, 0, title, -1, &wstrTitle[0], 256);
	SetWindowText(m_hwnd, &wstrTitle[0]);
}

void EditorWidget::send_setWindowTitle(const char *title)
{
	blog(LOG_WARNING, "EditorWidget::send_setWindowTitle , title: %s", title);
	DWORD res = WaitForSingleObject(m_threadStarted, // event handle
	                                INFINITE);       // indefinite wait
	if (res != WAIT_OBJECT_0) {
		blog(LOG_WARNING, "EditorWidget::send_setWindowTitle WaitForSingeObject failed: %ul", res);
		return;
	}
	BOOL retMsg = PostThreadMessage(GetThreadId(windowWorker.native_handle()),
			  WM_USER_SET_TITLE,
	                  reinterpret_cast<WPARAM>(title),
	                  0);
	if (!retMsg) {
		blog(LOG_WARNING, "EditorWidget::send_setWindowTitle PostThreadMessage failed");
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget::send_setWindowTitle, getLastError: %lu", dw);
	}

}

void EditorWidget::close()
{
	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;
}

void EditorWidget::send_close()
{
	sync_data sd;
	blog(LOG_WARNING, "EditorWidget::send_close");

	DWORD res = WaitForSingleObject(m_threadStarted, // event handle
	                                   INFINITE);    // indefinite wait
	if (res != WAIT_OBJECT_0) {
		blog(LOG_WARNING, "EditorWidget::send_close WaitForSingelObject failed: %ul", res);
		return;
	}
	
	BOOL retMsg = PostThreadMessage(GetThreadId(windowWorker.native_handle()), WM_USER_CLOSE, 0, reinterpret_cast<WPARAM>(&sd));
	if (!retMsg) {
		blog(LOG_WARNING, "EditorWidget::send_close PostThreadMessage failed");
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget::send_close, getLastError: %lu", dw);
	}
}

void EditorWidget::show()
{
	blog(LOG_WARNING, "EditorWidget::show Window");
	ShowWindow(m_hwnd, SW_SHOW);
}

void EditorWidget::send_show()
{
	blog(LOG_WARNING, "EditorWidget::send_show");

	DWORD res = WaitForSingleObject(m_threadStarted, // event handle
	                                INFINITE);       // indefinite wait
	if (res != WAIT_OBJECT_0) {
		blog(LOG_WARNING, "EditorWidget::send_show WaitForSingeObject failed: %ul", res);
		return;
	}
	BOOL retMsg = PostThreadMessage(GetThreadId(windowWorker.native_handle()), WM_USER_SHOW, 0, 0);
	if (!retMsg) {
		blog(LOG_WARNING, "EditorWidget::send_show PostThreadMessage failed");
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget::send_show, getLastError: %lu", dw);
	}
}

void EditorWidget::dispatcherClose()
{
	if (m_effect) {
		m_effect->dispatcher(m_effect, effEditClose, 0, 0, nullptr, 0);
	}
}
