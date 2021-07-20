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

#include "../headers/EditorWidget.h"

void EditorWidget::show()
{
	ShowWindow(windowHandle, SW_SHOW);
}

void EditorWidget::hide()
{
	shutdownWorker = true;
	if (windowWorker.joinable())
		windowWorker.join();

	ShowWindow(windowHandle, SW_HIDE);
}

void EditorWidget::close()
{
	DestroyWindow(windowHandle);
	windowHandle = nullptr;
}

void EditorWidget::setWindowTitle(const char *title)
{
	TCHAR wstrTitle[256];
	MultiByteToWideChar(CP_UTF8, 0, title, -1, &wstrTitle[0], 256);
	SetWindowText(windowHandle, &wstrTitle[0]);
}

LRESULT WINAPI EffectWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd)
		return NULL;

	// blog(LOG_WARNING, "EditorWidget: EffectWindowProc, uMsg: %u", uMsg);
	VSTPlugin *plugin = (VSTPlugin *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg) {
	case WM_CLOSE: {
		blog(LOG_WARNING, "VST Plug-in: EditorWidget: EffectWindowProc, received closeEditor msg");
		if (plugin) {
			plugin->closeEditor();
		}
	}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void EditorWidget::buildEffectContainer_worker()
{
	// set pointer to vst effect for window long
	LONG_PTR wndPtr = (LONG_PTR)effect;
	SetWindowLongPtr(windowHandle, -21 /*GWLP_USERDATA*/, wndPtr);

	// QWidget *widget = QWidget::createWindowContainer(QWindow::fromWinId((WId)windowHandle), this);
	// widget->move(0, 0);
	// widget->resize(300, 300);

	SetWindowPos(windowHandle, NULL, 0, 0, 300, 300, 0);

	MSG msg;
	PeekMessage(&msg, windowHandle, WM_USER, WM_USER, PM_NOREMOVE);

	BOOL bRet;

	while (!shutdownWorker && (bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
		if (bRet == -1) {
			blog(LOG_WARNING, "EditorWidget: Exit GetMessage loop");
			break;
		}
		else {
			if (msg.message == WM_USER_SET_TITLE) {
				const char *title = reinterpret_cast<const char *>(msg.wParam);
				SetWindowTextA(windowHandle, title);
			} else if (msg.message == WM_USER_SHOW) {
				//if (!m_effect) {
				//	blog(LOG_WARNING, "EditorWidget: m_effect is NULL");
				//	needs_to_show_window = true;
				//	continue;
				//}
				blog(LOG_WARNING, "EditorWidget Showing window");
				show();
				//this->createWindow();
			} else if (msg.message == WM_USER_HIDE) {
				//hiddenWindow = true;
				hide();
				//plugin->chunkDataBank      = plugin->getChunk(ChunkType::Bank, true);
				//plugin->chunkDataProgram   = plugin->getChunk(ChunkType::Program, true);
				//plugin->chunkDataParameter = plugin->getChunk(ChunkType::Parameter, true);
			} else if (msg.message == WM_USER_CLOSE) {
				if (shutdownWorker) {
					continue;
				}
				//plugin->chunkDataBank      = plugin->getChunk(ChunkType::Bank, true);
				//plugin->chunkDataProgram   = plugin->getChunk(ChunkType::Program, true);
				//plugin->chunkDataParameter = plugin->getChunk(ChunkType::Parameter, true);
				close();
				//dispatcherClose();
				shutdownWorker = true;
			} else if (msg.message == WM_USER_LOAD_DLL) {
				std::string* path_str(new std::string(*reinterpret_cast<std::string *>(msg.wParam)));
				//const char *       path = path_str->c_str();
				//m_path                  = path;
				plugin->loadEffectFromPath(path_str->c_str());
				effect = plugin->getEffect();
				effect->dispatcher(effect, effEditOpen, 0, 0, windowHandle, 0);

				VstRect *vstRect = nullptr;
				effect->dispatcher(effect, effEditGetRect, 0, 0, &vstRect, 0);
				if (vstRect) {
					// widget->resize(vstRect->right - vstRect->left, vstRect->bottom -
					// vstRect->top);
					SetWindowPos(windowHandle,
					             NULL,
					             0,
					             0,
					             vstRect->right - vstRect->left,
					             vstRect->bottom - vstRect->top,
					             0);
				}
				//if (!m_effect) {
				//	blog(LOG_WARNING, "EditorWidget: worker effect is NULL");
				//	this->send_close();
				//} else if (needs_to_show_window) {
				//	needs_to_show_window = false;
				//	this->createWindow();
				//}
			} else if (msg.message == WM_USER_SETCHUNK) {
				//plugin->setChunk(ChunkType::Parameter, plugin->chunkDataParameter);
				//plugin->setChunk(ChunkType::Program, plugin->chunkDataProgram);
				//plugin->setChunk(ChunkType::Bank, plugin->chunkDataBank);
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	//ResetEvent(m_threadStarted);
	return;
}

void EditorWidget::buildEffectContainer(AEffect *effect)
{
	WNDCLASSEXW wcex{sizeof(wcex)};

	wcex.lpfnWndProc   = EffectWindowProc;
	wcex.hInstance     = GetModuleHandleW(nullptr);
	wcex.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassExW(&wcex);

	this->effect = effect;

	const auto style = WS_CAPTION | WS_THICKFRAME | WS_OVERLAPPEDWINDOW;
	windowHandle =
	        CreateWindowW(wcex.lpszClassName, TEXT(""), style, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);

	show();

	shutdownWorker = false;
	windowWorker = std::thread(std::bind(&EditorWidget::buildEffectContainer_worker, this));
}

void EditorWidget::handleResizeRequest(int, int)
{
	// Some plugins can't resize automatically (like SPAN by Voxengo),
	// so we must resize window manually

	// get pointer to vst effect from window long
	LONG_PTR    wndPtr   = (LONG_PTR)GetWindowLongPtrW(windowHandle, -21 /*GWLP_USERDATA*/);
	AEffect *   effect   = (AEffect *)(wndPtr);
	VstRect *   rec      = nullptr;
	static RECT PluginRc = {0};
	RECT        winRect  = {0};

	GetWindowRect(windowHandle, &winRect);
	if (effect) {
		effect->dispatcher(effect, effEditGetRect, 1, 0, &rec, 0);
	}

	// compare window rect with VST Rect
	if (rec) {
		if (PluginRc.bottom != rec->bottom || PluginRc.left != rec->left || PluginRc.right != rec->right ||
		    PluginRc.top != rec->top) {
			PluginRc.bottom = rec->bottom;
			PluginRc.left   = rec->left;
			PluginRc.right  = rec->right;
			PluginRc.top    = rec->top;

			// set rect to our window
			AdjustWindowRectEx(&PluginRc, WS_CAPTION | WS_THICKFRAME | WS_OVERLAPPEDWINDOW, FALSE, 0);

			// move window to apply pos
			MoveWindow(windowHandle,
			           winRect.left,
			           winRect.top,
			           PluginRc.right - PluginRc.left,
			           PluginRc.bottom - PluginRc.top,
			           TRUE);
		}
	}
}

void EditorWidget::send_show()
{
	BOOL retMsg = PostThreadMessage(GetThreadId(windowWorker.native_handle()), WM_USER_SHOW, 0, 0);
	if (!retMsg) {
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget: send_show, getLastError: %lu", dw);
	}
}

void EditorWidget::send_close()
{
	BOOL retMsg = PostThreadMessage(
	        GetThreadId(windowWorker.native_handle()), WM_USER_CLOSE, 0, 0);
	if (!retMsg) {
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget: send_close, getLastError: %lu", dw);
	}
}

void EditorWidget::send_hide()
{
	BOOL retMsg = PostThreadMessage(GetThreadId(windowWorker.native_handle()), WM_USER_HIDE, 0, 0);
	if (!retMsg) {
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget::send_hide, getLastError: %lu", dw);
	}
}

void EditorWidget::send_setWindowTitle(const char *title)
{
	BOOL retMsg = PostThreadMessage(
	        GetThreadId(windowWorker.native_handle()), WM_USER_SET_TITLE, reinterpret_cast<WPARAM>(title), 0);
	if (!retMsg) {
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget: send_setWindowTitle, getLastError: %lu", dw);
	}
}

void EditorWidget::send_loadEffectFromPath(std::string path)
{
	PostThreadMessage(GetThreadId(windowWorker.native_handle()),
	                  WM_USER_LOAD_DLL,
	                  reinterpret_cast<WPARAM>(new std::string(path.c_str())),
	                  0);
}

void EditorWidget::send_setChunk()
{
	PostThreadMessage(GetThreadId(windowWorker.native_handle()), WM_USER_SETCHUNK, 0, 0);
}
