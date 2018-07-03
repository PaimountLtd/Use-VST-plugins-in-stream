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
#include <thread>
#include <mutex>
#include <condition_variable>

enum WM_USER_MSG {
	WM_USER_INITIALIZE = WM_USER + 0,
	WM_USER_FINALIZE,
	WM_USER_CONTENT,
	WM_USER_TITLE,
	WM_USER_TOGGLE,
	WM_USER_RESIZE
};

struct window_sync_data_t {
	// Synchronization
	std::mutex              mtx;
	std::condition_variable cv;
	bool                    ran = false;
};
struct window_content_data_t : window_sync_data_t {
	AEffect *effect;
	bool     clear = false;
};
struct window_title_data_t : window_sync_data_t {
	std::string title;
};

EditorWidget::~EditorWidget()
{
	if (windowHandlerThread.joinable()) {
		window_sync_data_t           wcd;
		std::unique_lock<std::mutex> ul(wcd.mtx);
		PostThreadMessage(GetThreadId(windowHandlerThread.native_handle()),
		                  WM_USER_FINALIZE,
		                  reinterpret_cast<WPARAM>(&wcd),
		                  0);
		wcd.cv.wait(ul, [&wcd]() { return wcd.ran; });

		windowHandlerThread.join();
	}
}

EditorWidget::EditorWidget()
{
	windowHandlerThread = std::thread(std::bind(&EditorWidget::handleWindowMessages, this));

	window_sync_data_t           wsd;
	std::unique_lock<std::mutex> ul(wsd.mtx);
	while (!PostThreadMessage(GetThreadId(windowHandlerThread.native_handle()),
	                          WM_USER_INITIALIZE,
	                          reinterpret_cast<WPARAM>(&wsd),
	                          0)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	wsd.cv.wait(ul, [&wsd]() { return wsd.ran; });
}

LRESULT WINAPI EditorWidget::EffectWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorWidget *ew = reinterpret_cast<EditorWidget *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (ew) {
		if (uMsg == WM_CLOSE) {
			ew->callWindowCloseCallback();
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void EditorWidget::setWindowContent(AEffect *effect)
{
	if (!m_hwnd)
		return;

	window_content_data_t*        wcd = new window_content_data_t();
	std::unique_lock<std::mutex> ul(wcd->mtx);
	wcd->effect = effect;
	PostThreadMessage(
	        GetThreadId(windowHandlerThread.native_handle()), WM_USER_CONTENT, reinterpret_cast<WPARAM>(wcd), 0);
	wcd->cv.wait(ul, [&wcd]() { return wcd->ran; });
}

void EditorWidget::clearWindowContent(AEffect *effect)
{
	if (!m_hwnd)
		return;

	window_content_data_t *      wcd = new window_content_data_t();
	std::unique_lock<std::mutex> ul(wcd->mtx);
	wcd->effect = effect;
	wcd->clear   = true;
	PostThreadMessage(
	        GetThreadId(windowHandlerThread.native_handle()), WM_USER_CONTENT, reinterpret_cast<WPARAM>(wcd), 0);
	// This is called by VSTPlugin->windowCloseCallback, so we can't also wait here.
	// wcd.cv.wait(ul, [&wcd]() { return wcd.ran; });
}

void EditorWidget::setWindowTitle(const char *title)
{
	if (m_hwnd == 0)
		return;

	window_title_data_t          wtd;
	std::unique_lock<std::mutex> ul(wtd.mtx);
	wtd.title = title;
	PostThreadMessage(
	        GetThreadId(windowHandlerThread.native_handle()), WM_USER_TITLE, reinterpret_cast<WPARAM>(&wtd), 0);
	wtd.cv.wait(ul, [&wtd]() { return wtd.ran; });
}

void EditorWidget::close()
{
	show(false);
}

void EditorWidget::show(bool show)
{
	if (m_hwnd == 0)
		return;

	window_sync_data_t*           wsd = new window_sync_data_t();
	std::unique_lock<std::mutex> ul(wsd->mtx);
	PostThreadMessage(GetThreadId(windowHandlerThread.native_handle()),
	                  WM_USER_TOGGLE,
	                  reinterpret_cast<WPARAM>(wsd),
	                  show ? 1 : 0);
	// Called by VSTPlugin->windowCloseCallback, we'll just trust it worked otherwise it gets complex.
	// wsd.cv.wait(ul, [&wsd]() { return wsd.ran; });
}

void EditorWidget::handleResizeRequest(int width, int height)
{
	if (m_hwnd == 0)
		return;

	union {
		LPARAM lParam;
		int    size[2];
	};
	size[0] = width;
	size[1] = height;

	window_sync_data_t           wcd;
	std::unique_lock<std::mutex> ul(wcd.mtx);
	PostThreadMessage(GetThreadId(windowHandlerThread.native_handle()),
	                  WM_USER_RESIZE,
	                  reinterpret_cast<WPARAM>(&wcd),
	                  lParam);
	wcd.cv.wait(ul, [&wcd]() { return wcd.ran; });
}

void EditorWidget::handleWindowMessages()
{
	MSG   msg;
	DWORD bRet;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	bool shutdown = false;
	while (!shutdown) {
		if ((bRet = GetMessage(&msg, NULL, 0, 0)) == 0) {
			continue;
		}
		if (bRet == -1) {
			break;
		}

		if (msg.hwnd == 0) {
			if (msg.message == WM_USER_INITIALIZE) {
				// Create a Window without showing it, containing no info.
				window_sync_data_t *         wsd = reinterpret_cast<window_sync_data_t *>(msg.wParam);
				std::unique_lock<std::mutex> ul(wsd->mtx);
				createWindowImpl();
				wsd->ran = true;
				wsd->cv.notify_all();
			} else if (msg.message == WM_USER_FINALIZE) {
				// Destroy the created window and any content.
				window_sync_data_t *         wsd = reinterpret_cast<window_sync_data_t *>(msg.wParam);
				std::unique_lock<std::mutex> ul(wsd->mtx);
				destroyWindowImpl();
				wsd->ran = true;
				wsd->cv.notify_all();
				break;
			} else if (msg.message == WM_USER_CONTENT) {
				// Update Window content
				window_content_data_t *wcd = reinterpret_cast<window_content_data_t *>(msg.wParam);
				std::unique_lock<std::mutex> ul(wcd->mtx);
				if (wcd->clear) {
					clearContentImpl(wcd->effect);
				} else {
					setContentImpl(wcd->effect);
				}
				wcd->ran = true;
				wcd->cv.notify_all();
				delete wcd;
			} else if (msg.message == WM_USER_TITLE) {
				window_title_data_t *        wtd = reinterpret_cast<window_title_data_t *>(msg.wParam);
				std::unique_lock<std::mutex> ul(wtd->mtx);
				setTitleImpl(wtd->title);
				wtd->ran = true;
				wtd->cv.notify_all();
			} else if (msg.message == WM_USER_TOGGLE) {
				window_sync_data_t *         wsd = reinterpret_cast<window_sync_data_t *>(msg.wParam);
				std::unique_lock<std::mutex> ul(wsd->mtx);
				showHideImpl(!!msg.lParam);
				wsd->ran = true;
				wsd->cv.notify_all();
				delete wsd;
			} else if (msg.message == WM_USER_RESIZE) {
				union {
					LPARAM lParam;
					int    size[2];
				};

				window_sync_data_t *         wsd = reinterpret_cast<window_sync_data_t *>(msg.wParam);
				std::unique_lock<std::mutex> ul(wsd->mtx);
				lParam = msg.lParam;
				resizeImpl(size[0], size[1]);
				wsd->ran = true;
				wsd->cv.notify_all();
			} else {
				DefWindowProc(m_hwnd, msg.message, msg.wParam, msg.lParam);
			}
		} else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void EditorWidget::createWindowImpl()
{
	if (m_hwnd)
		return;

	WNDCLASSEX wcex{sizeof(wcex)};
	wcex.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc   = EffectWindowProc;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hInstance     = 0;
	wcex.hIcon         = 0;
	wcex.hCursor       = 0;
	wcex.hbrBackground = 0;
	wcex.lpszMenuName  = 0;
	wcex.lpszClassName = TEXT("Minimal VST host - Guest VST Window Frame");
	wcex.hInstance     = GetModuleHandle(0);
	wcex.hIconSm       = 0;
	RegisterClassEx(&wcex);

	LONG style =
	        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DLGFRAME | WS_POPUP | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;
	LONG exStyle = WS_EX_DLGMODALFRAME;

	m_hwnd = CreateWindowEx(exStyle, wcex.lpszClassName, TEXT(""), style, 0, 0, 100, 100, NULL, NULL, NULL, this);
	SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&EffectWindowProc));
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

void EditorWidget::destroyWindowImpl()
{
	if (!m_hwnd)
		return;

	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;
}

void EditorWidget::setContentImpl(AEffect *effect)
{
	if (!m_hwnd) {
		return;
	}

	haveContent = true;

	VstRect *vstRect;
	effect->dispatcher(effect, effEditOpen, 0, 0, m_hwnd, 0);
	effect->dispatcher(effect, effEditGetRect, 0, 0, &vstRect, 0);

	if (vstRect) {
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
		SetWindowPos(m_hwnd, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
	}
}

void EditorWidget::clearContentImpl(AEffect *effect)
{
	haveContent = false;

	if (!m_hwnd) {
		return;
	}

	effect->dispatcher(effect, effEditClose, 0, 0, 0, 0);
}

void EditorWidget::setTitleImpl(std::string title)
{
	if (!m_hwnd) {
		return;
	}

	TCHAR wstrTitle[256];
	MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, &wstrTitle[0], 256);
	SetWindowText(m_hwnd, &wstrTitle[0]);
}

void EditorWidget::showHideImpl(bool show)
{
	if (!m_hwnd) {
		return;
	}

	this->is_visible = show;
	ShowWindow(m_hwnd, show ? SW_SHOWNORMAL : SW_HIDE);
}

void EditorWidget::resizeImpl(int width, int height)
{
	if (!m_hwnd) {
		return;
	}

	SetWindowPos(m_hwnd, 0, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOREPOSITION | SWP_NOZORDER);
}
