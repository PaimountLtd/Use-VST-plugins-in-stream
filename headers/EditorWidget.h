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
#include <gl\gl.h>          // Header File For The OpenGL32 Library
#include <gl\glu.h>   
#elif __linux__
#include <xcb/xcb.h>
#elif __APPLE__
typedef HWND unsigned long;
typedef void *HANDLE;
#endif

#include "aeffectx.h"
#include "VSTPlugin.h"
#include <thread>
#include <mutex>
#include <condition_variable>



struct sync_data {
	std::mutex              mtx;
	std::condition_variable cv;
	bool                    ran = false;
};

enum WM_USER_MSG {
	// Start at index user + 5 because some plugins were causing issues when sending invalid
	// messages to the main window
	WM_USER_SET_TITLE = WM_USER + 5,
	WM_USER_SHOW,
	WM_USER_CLOSE,
	WM_USER_LOAD_DLL,
	WM_USER_SETCHUNK
};

class VSTPlugin;

class VstRect {

public:
	short top;
	short left;
	short bottom;
	short right;
};

class EditorWidget {
	friend class VSTPlugin;
	VSTPlugin *plugin;
	AEffect *   m_effect;
	std::string m_title;
	std::string m_path;
	bool        needs_to_show_window;
	HWND        m_hwnd;
	HANDLE      m_threadStarted;
	void        createWindow();
#ifdef __linux__
	xcb_window_t m_wid;
#endif

public:
	std::thread windowWorker;

	EditorWidget(VSTPlugin *plugin);
	virtual ~EditorWidget();
	void setWindowTitle(const char *title);
	void show();
	void dispatcherClose();
	void close();
	void buildEffectContainer();

	void buildEffectContainer_worker();
	void send_setChunk(std::string chunk);
	void send_loadEffectFromPath(std::string path);
	void send_setWindowTitle(const char *title);
	void send_show();
	void send_close();
};

#endif // OBS_STUDIO_EDITORDIALOG_H
