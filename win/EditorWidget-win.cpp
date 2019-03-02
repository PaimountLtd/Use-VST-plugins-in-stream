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
#include "../headers/EditorWidget.h"

LRESULT WINAPI EffectWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd)
		return NULL;

	VSTPlugin *plugin = (VSTPlugin *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg) {
	case WM_CLOSE:
		plugin->closeEditor();
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

EditorWidget::~EditorWidget()
{
}	

void EditorWidget::buildEffectContainer(AEffect *effect)
{
	m_effect     = effect;
	windowWorker = std::thread(std::bind(&EditorWidget::buildEffectContainer_worker, this));
}

void EditorWidget::buildEffectContainer_worker()
{
	WNDCLASSEXW wcex{sizeof(wcex)};

	wcex.lpfnWndProc   = EffectWindowProc;
	wcex.hInstance     = GetModuleHandle(0);
	wcex.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassExW(&wcex);

	LONG style =
	        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DLGFRAME | WS_POPUP | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;

	LONG exStyle = WS_EX_DLGMODALFRAME;

	VstRect *vstRect = nullptr;

	m_hwnd = CreateWindowEx(
	        exStyle, wcex.lpszClassName, TEXT(""), style, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);

	m_effect->dispatcher(m_effect, effEditOpen, 0, 0, m_hwnd, 0);
	m_effect->dispatcher(m_effect, effEditGetRect, 0, 0, &vstRect, 0);

	show();
	SetWindowPos(m_hwnd, 0, vstRect->left, vstRect->top, vstRect->right, vstRect->bottom, 0);

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

	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)plugin);

	MSG   msg;
	BOOL bRet;

	bool shutdown = false;
	while (!shutdown && (bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1) {
			break;
		} else {
			if (msg.message == WM_USER_SET_TITLE) {
				const char *title = reinterpret_cast<const char *>(msg.wParam);
				setWindowTitle(title);
			} else if (msg.message == WM_USER_SHOW) {
				show();
			} else if (msg.message == WM_USER_CLOSE_DISPATCHER) {
				dispatcherClose();
			} else if (msg.message == WM_USER_CLOSE) {
				close();
				shutdown = true;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

	}
	return;
}

void EditorWidget::setWindowTitle(const char *title)
{
	TCHAR wstrTitle[256];
	MultiByteToWideChar(CP_UTF8, 0, title, -1, &wstrTitle[0], 256);
	SetWindowText(m_hwnd, &wstrTitle[0]);
}

void EditorWidget::send_setWindowTitle(const char *title)
{
	PostThreadMessage(GetThreadId(windowWorker.native_handle()),
			  WM_USER_SET_TITLE,
	                  reinterpret_cast<WPARAM>(title),
	                  0);
}

void EditorWidget::close()
{
	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;
}

void EditorWidget::send_close()
{
	sync_data sd;
	PostThreadMessage(GetThreadId(windowWorker.native_handle()), WM_USER_CLOSE, 0, reinterpret_cast<WPARAM>(&sd));
}

void EditorWidget::show()
{
	ShowWindow(m_hwnd, SW_SHOW);
}

void EditorWidget::send_show()
{
	PostThreadMessage(GetThreadId(windowWorker.native_handle()),
		WM_USER_SHOW, 0, 0);
}

void EditorWidget::dispatcherClose()
{
	if (m_effect) {
		m_effect->dispatcher(m_effect, effEditClose, 0, 0, nullptr, 0);
	}
}

void EditorWidget::send_dispatcherClose()
{
	PostThreadMessage(GetThreadId(windowWorker.native_handle()), WM_USER_CLOSE_DISPATCHER, 0, 0);
}
