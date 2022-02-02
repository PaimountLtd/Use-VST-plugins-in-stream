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
#include "../headers/VSTPlugin.h"
#include "../headers/vst-plugin-callbacks.hpp"
#include "..//StlBuffer.h"

#include <obs_vst_api.grpc.pb.h>

#include <util/platform.h>
#include <windows.h>
#include <string>
#include <grpcpp/grpcpp.h>
#include <filesystem>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

class grpc_vst_communicatorClient
{
public:
	grpc_vst_communicatorClient(std::shared_ptr<Channel> channel) :
		stub_(grpc_vst_communicator::NewStub(channel))
	{
		m_connected = channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(3));
	}

	intptr_t dispatcher(AEffect* a, int b, int c, intptr_t d, void* ptr, float f, size_t ptr_size)
	{
		grpc_dispatcher_Request request;
		request.set_param1(b);
		request.set_param2(c);
		request.set_param3(d);
		request.set_param4(f);
		request.set_ptr_value(int64_t(ptr));
		request.set_ptr_size(ptr_size);

		if (ptr_size > 0)
		{
			std::string str;
			str.resize(ptr_size);
			memcpy(str.data(), ptr, ptr_size);
			request.set_ptr_data(str);
		}

		grpc_dispatcher_Reply reply;
		ClientContext context;
		Status status = stub_->com_grpc_dispatcher(&context, request, &reply);

		if (!status.ok())
			m_connected = a->m_valid = false;
		
		if (ptr != nullptr)
		{
			void** realDeal = (void**)ptr;

			if (*realDeal == nullptr)
			{
				*realDeal = malloc(reply.ptr_data().size());
				memcpy(*realDeal, reply.ptr_data().data(), reply.ptr_data().size());
			}
			else if (ptr_size != 0)
			{
				if (ptr_size > reply.ptr_data().size())
					ptr_size = reply.ptr_data().size();

				memcpy(realDeal, reply.ptr_data().data(), ptr_size);
			}
			else
			{
				// Unsafe, we don't know how big ptr is
				m_connected = false;
				return 0;
			}
		}
		
		a->magic = reply.magic(); 
		a->numPrograms = reply.numprograms(); 
		a->numParams = reply.numparams(); 
		a->numInputs = reply.numinputs(); 
		a->numOutputs = reply.numoutputs(); 
		a->flags = reply.flags();
		a->initialDelay = reply.initialdelay();
		a->uniqueID = reply.uniqueid(); 
		a->version = reply.version();

		return reply.returnval();
	}

	void setParameter(AEffect* a, int b, float c) 
	{
		grpc_setParameter_Request request;
		request.set_param1(b);
		request.set_param2(c);

		grpc_setParameter_Reply reply;
		ClientContext context;
		Status status = stub_->com_grpc_setParameter(&context, request, &reply);

		if (!status.ok())
			m_connected = a->m_valid = false;
				
		a->magic = reply.magic(); 
		a->numPrograms = reply.numprograms(); 
		a->numParams = reply.numparams(); 
		a->numInputs = reply.numinputs(); 
		a->numOutputs = reply.numoutputs(); 
		a->flags = reply.flags();
		a->initialDelay = reply.initialdelay();
		a->uniqueID = reply.uniqueid(); 
		a->version = reply.version();
	}

	float getParameter(AEffect* a, int b)
	{
		grpc_getParameter_Request request;
		request.set_param1(b);

		grpc_getParameter_Reply reply;
		ClientContext context;
		Status status = stub_->com_grpc_getParameter(&context, request, &reply);

		if (!status.ok())
			m_connected = a->m_valid = false;
				
		a->magic = reply.magic(); 
		a->numPrograms = reply.numprograms(); 
		a->numParams = reply.numparams(); 
		a->numInputs = reply.numinputs(); 
		a->numOutputs = reply.numoutputs(); 
		a->flags = reply.flags();
		a->initialDelay = reply.initialdelay();
		a->uniqueID = reply.uniqueid(); 
		a->version = reply.version();
		return reply.returnval();
	}

	void processReplacing(AEffect* a, float** adata, float** bdata, int frames, int arraySize)
	{
		std::string adataBuffer;
		std::string bdataBuffer;

		for (int c = 0; c < arraySize; c++)
		{
			adataBuffer.append((char*)adata[c], frames * sizeof(float));
			bdataBuffer.append((char*)bdata[c], frames * sizeof(float));
		}

		grpc_processReplacing_Request request;
		request.set_arraysize(arraySize);
		request.set_frames(frames);
		request.set_adata(adataBuffer);
		request.set_bdata(bdataBuffer);

		grpc_processReplacing_Reply reply;
		ClientContext context;
		Status status = stub_->com_grpc_processReplacing(&context, request, &reply);

		if (!status.ok())
			m_connected = a->m_valid = false;
		
		size_t read_idx_a = 0;
		size_t read_idx_b = 0;

		for (int c = 0; c < arraySize; c++)
		{
			StlBuffer::pop_buffer(reply.adata(), read_idx_a, (char*)adata[c], frames * sizeof(float));
			StlBuffer::pop_buffer(reply.bdata(), read_idx_b, (char*)bdata[c], frames * sizeof(float));
		}

		a->magic = reply.magic(); 
		a->numPrograms = reply.numprograms(); 
		a->numParams = reply.numparams(); 
		a->numInputs = reply.numinputs(); 
		a->numOutputs = reply.numoutputs(); 
		a->flags = reply.flags();
		a->initialDelay = reply.initialdelay();
		a->uniqueID = reply.uniqueid(); 
		a->version = reply.version();
	}

	void sendHwndMsg(AEffect* a, int msgType)
	{
		grpc_sendHwndMsg_Request request;
		request.set_msgtype(msgType);

		grpc_sendHwndMsg_Reply reply;
		ClientContext context;
		Status status = stub_->com_grpc_sendHwndMsg(&context, request, &reply);

		if (!status.ok())
			m_connected = a->m_valid = false;
	}

	void updateAEffect(AEffect* a)
	{
		grpc_updateAEffect_Request request;
		request.set_nullreply(1);

		grpc_updateAEffect_Reply reply;
		ClientContext context;
		Status status = stub_->com_grpc_updateAEffect(&context, request, &reply);

		if (!status.ok())
			m_connected = a->m_valid = false;
				
		a->magic = reply.magic(); 
		a->numPrograms = reply.numprograms(); 
		a->numParams = reply.numparams(); 
		a->numInputs = reply.numinputs(); 
		a->numOutputs = reply.numoutputs(); 
		a->flags = reply.flags();
		a->initialDelay = reply.initialdelay();
		a->uniqueID = reply.uniqueid(); 
		a->version = reply.version();
	}

	void stopServer(AEffect* a)
	{
		grpc_stopServer_Request request;
		request.set_nullreply(0);

		grpc_stopServer_Reply reply;
		ClientContext context;
		Status status = stub_->com_grpc_stopServer(&context, request, &reply);

		if (!status.ok())
			m_connected = a->m_valid = false;
	}

	std::atomic<bool> m_connected{ false };

private:
	std::unique_ptr<grpc_vst_communicator::Stub> stub_;
};

class AEffect_win : public AEffect
{
public:
	PROCESS_INFORMATION m_winServer;
	std::unique_ptr<grpc_vst_communicatorClient> m_server;
};

AEffect* VSTPlugin::getEffect() {
	return effect.get();
}

AEffect* VSTPlugin::loadEffect()
{
	blog(LOG_WARNING, "VST Plug-in: path %s", pluginPath.c_str());

	unloadLibrary();

	wchar_t *wpath;
	os_utf8_to_wcs_ptr(pluginPath.c_str(), 0, &wpath);

	// Other filters trying to load at same time?
	static std::mutex mtx;
	static int32_t portCounter = 20000;

	{
		std::lock_guard<std::mutex> grd(mtx);

		// Find an open tcp port
		for (; portCounter < 25000 && !isPortAvailable(portCounter); ++portCounter)
			;

		if (++portCounter > 25000)
			portCounter = 20000;
	}

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	
	effect = std::make_unique<AEffect_win>();
	std::wstring startparams = L"streamlabs_vst.exe \"" + std::wstring(wpath) + L"\" " + std::to_wstring(portCounter) + L" " + std::to_wstring(GetCurrentProcessId());

	if (!CreateProcessW(L"win-streamlabs-vst.exe", (LPWSTR)startparams.c_str(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &static_cast<AEffect_win*>(effect.get())->m_winServer))
	{
		::MessageBoxA(NULL, (std::filesystem::path(pluginPath).filename().string() + " failed to launch.\n\nAfter closing this popup, audio will continue but the filter is not enabled. You may restart the application or recreate the filter to try again.").c_str(), "VST Filter Error",
			MB_ICONERROR | MB_TOPMOST);

		blog(LOG_ERROR, "VST Plug-in: can't start vst server, GetLastError = %d", GetLastError());
		effect = nullptr;
		return nullptr;
	}
	
	static_cast<AEffect_win*>(effect.get())->m_server = std::make_unique<grpc_vst_communicatorClient>(grpc::CreateChannel("localhost:" + std::to_string(portCounter), grpc::InsecureChannelCredentials()));

	afx_updateAEffect(effect.get());

	if (!verifyPluginIntegrity())
		return nullptr;

	return effect.get();
}

bool VSTPlugin::isPortAvailable(const int32_t portno)
{
#ifdef WIN32
	struct sockaddr_in serv_addr;
	struct hostent *server;

	auto sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
		return false;

	server = gethostbyname("localhost");

	if (server == NULL)
	{
	    closesocket(sockfd);
	    return false;
	}

	memset((void*)&serv_addr, NULL, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((void*)&serv_addr.sin_addr.s_addr, (void*)server->h_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	bool result = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0;
	closesocket(sockfd);
	return result;
#else
	return true;
#endif
}

void VSTPlugin::send_loadEffectFromPath(std::string path)
{
	loadEffectFromPath(path);
	editorWidget->send_loadEffectFromPath(path);
}

void VSTPlugin::send_setChunk()
{
	editorWidget->send_setChunk();
}

void VSTPlugin::send_unloadEffect()
{

}

void VSTPlugin::unloadLibrary()
{
	if (effect == nullptr)
		return;

	auto movedPtr = move(effect);
	auto winPtr = static_cast<AEffect_win*>(movedPtr.get());

	if (winPtr) {		
		winPtr->m_server->stopServer(winPtr);
		
		// Wait for graceful end in a thread, don't block here
		std::thread([](HANDLE hProcess, HANDLE hThread, INT nWaitTime) {
			
			// Might have to kill it, wait a moment but note that wait time is 0 if tcp connection already isn't valid
			if (WaitForSingleObject(hProcess, nWaitTime) == WAIT_TIMEOUT) {
				if (TerminateProcess(hProcess, 0) == FALSE) {
					blog(LOG_ERROR, "VST Plug-in: process is stuck somehow cannot terminate, GetLastError = %d", GetLastError());
				}
			}

			CloseHandle(hProcess);
			CloseHandle(hThread);

		}, winPtr->m_winServer.hProcess,
		   winPtr->m_winServer.hThread,
		   movedPtr->m_valid ? 3000 : 0).detach();
	}	
}

intptr_t afx_dispatcher(AEffect* a, int b, int c, intptr_t d, void* e, float f, const size_t ptr_size)
{
	if (!a->m_valid)
		return 0;
	
	return static_cast<AEffect_win*>(a)->m_server->dispatcher(a, b, c, d, e, f, ptr_size);
}

void afx_setParameter(AEffect* a, int b, float c)
{
	if (!a->m_valid)
		return;

	static_cast<AEffect_win*>(a)->m_server->setParameter(a, b, c);
}

float afx_getParameter(AEffect* a, int b)
{
	if (!a->m_valid)
		return 0.f;
	
	return static_cast<AEffect_win*>(a)->m_server->getParameter(a, b);
}

void afx_processReplacing(AEffect* a, float** b, float** c, int frames, int arraySize)
{
	if (!a->m_valid)
		return;
	
	static_cast<AEffect_win*>(a)->m_server->processReplacing(a, b, c, frames, arraySize);
}

void afx_sendHwndMsg(AEffect* a, int msgType)
{
	if (!a->m_valid)
		return;
	
	static_cast<AEffect_win*>(a)->m_server->sendHwndMsg(a, msgType);
}

void afx_updateAEffect(AEffect* a)
{
	if (!a->m_valid)
		return;
	
	static_cast<AEffect_win*>(a)->m_server->updateAEffect(a);
}
