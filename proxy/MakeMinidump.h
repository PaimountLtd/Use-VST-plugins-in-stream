#pragma once

#include <Dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

namespace CrashHandler
{
	void makeMinidump(EXCEPTION_POINTERS* e)
	{
		// todo
		return;
	}

	LONG CALLBACK unhandledHandler(EXCEPTION_POINTERS* e)
	{
		makeMinidump(e);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	
}
