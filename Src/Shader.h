#pragma once
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <cstdio>		//needed by OutputDebugString()
#include "avisynth.h"
#include "D3D9RenderImpl.h"
#include "ProcessFrames.h"
#include <list>
#include <vector>
#include <thread>
#include <mutex>
#include <concurrent_queue.h>
#include <future>
using namespace concurrency;

class Shader : public GenericVideoFilter {
public:
	Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel, int precision,
		const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8, const char* _param9, 
		PClip _clip2, PClip _clip3, PClip _clip4, PClip _clip5, PClip _clip6, PClip _clip7, PClip _clip8, PClip _clip9, int _width, int _height, IScriptEnvironment* env);
	~Shader();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	const int m_precision;
	PClip m_clip[9];
	CommandStruct m_cmd;
	
	static int threadCount;
	static concurrent_queue<CommandStruct> cmdBuffer;
	static std::mutex initLock, startLock, waiterLock;
	static std::thread* WorkerThread;
	static HANDLE WorkerWaiting;
	static void AddCommandToQueue(CommandStruct* cmd, IScriptEnvironment* env);
	static void StartWorkerThread(IScriptEnvironment* env);
};