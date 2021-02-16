#ifndef SLOBS_VST_LAUNCHER
#define SLOBS_VST_LAUNCHER
#ifdef WIN32

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <string>

class SlobsVSTProcessLauncher {
	public:
		PROCESS_INFORMATION SlobsVSTProcessLauncher::launchProcess(std::string app,
						std::string arg);
		bool checkIfProcessIsActive(PROCESS_INFORMATION pi);
		bool stopProcess(PROCESS_INFORMATION &pi);	
};


#endif //win32
#endif
