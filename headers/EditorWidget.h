/*****************************************************************************
Copyright (C) 2016-2017 by Colin Edwards.
Additional Code Copyright (C) 2016-2017 by c3r1c3 <c3r1c3@nevermindonline.com>

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

#ifndef OBS_STUDIO_EDITORDIALOG_H
#define OBS_STUDIO_EDITORDIALOG_H

#if WIN32
#include <Windows.h>
#elif __linux__
#include <xcb/xcb.h>
#endif

#include "aeffectx.h"
#include <thread>

class VSTPlugin;

class VstRect {

public:
	short top;
	short left;
	short bottom;
	short right;
};

typedef void (*editorwidget_close_cb_t)(void*);

class EditorWidget {
	std::thread windowHandlerThread;
	bool        haveContent = false;
	bool        is_visible  = false;

#ifdef __APPLE__
#elif WIN32
	HWND m_hwnd = 0;

	static LRESULT WINAPI EffectWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#elif __linux__
	xcb_window_t m_wid;
#endif

	editorwidget_close_cb_t cb = nullptr;
	void *                  cb_data;

public:
	EditorWidget();
	virtual ~EditorWidget();
	void setWindowContent(AEffect *effect);
	void clearWindowContent(AEffect *effect);
	void setWindowTitle(const char *title);

	bool hasContent();
	bool isVisible();

	void show(bool show = true);
	void close();

	void setWindowCloseCallback(editorwidget_close_cb_t cb, void *data);
	void callWindowCloseCallback();

	void handleResizeRequest(int width, int height);

private:
	void handleWindowMessages();
	void createWindowImpl();
	void destroyWindowImpl();

	void setContentImpl(AEffect *effect);
	void clearContentImpl(AEffect *effect);
	void setTitleImpl(std::string title);
	void showHideImpl(bool show);
	void resizeImpl(int width, int height);
};

#endif // OBS_STUDIO_EDITORDIALOG_H
