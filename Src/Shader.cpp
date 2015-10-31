#include "Shader.h"
// http://gamedev.stackexchange.com/questions/13435/loading-and-using-an-hlsl-shader

Shader::Shader(PClip _child, const char* _path, const char* _entryPoint, const char* _shaderModel, int _precision,
	const char* _param1, const char* _param2, const char* _param3, const char* _param4, const char* _param5, const char* _param6, const char* _param7, const char* _param8, const char* _param9,
	PClip _clip2, PClip _clip3, PClip _clip4, PClip _clip5, PClip _clip6, PClip _clip7, PClip _clip8, PClip _clip9, int _height, int _width, IScriptEnvironment* env) :
	GenericVideoFilter(_child), m_precision(_precision) {

	m_clip[0] = _child;
	m_clip[1] = _clip2;
	m_clip[2] = _clip3;
	m_clip[3] = _clip4;
	m_clip[4] = _clip5;
	m_clip[5] = _clip6;
	m_clip[6] = _clip7;
	m_clip[7] = _clip8;
	m_clip[8] = _clip9;

	ZeroMemory(&m_cmd, sizeof(CommandStruct));
	m_cmd.Path = _path;
	m_cmd.EntryPoint = _entryPoint;
	m_cmd.ShaderModel = _shaderModel;
	m_cmd.Param[0] = _param1;
	m_cmd.Param[1] = _param2;
	m_cmd.Param[2] = _param3;
	m_cmd.Param[3] = _param4;
	m_cmd.Param[4] = _param5;
	m_cmd.Param[5] = _param6;
	m_cmd.Param[6] = _param7;
	m_cmd.Param[7] = _param8;
	m_cmd.Param[8] = _param9;

	// Validate parameters
	if (_path == NULL || _path[0] == '\0')
		env->ThrowError("Shader: path to a compiled shader must be specified");

	// If Width or Height are not specified, they remain the same as input clip.
	if (_width > 0)
		vi.width = _width * m_precision;
	if (_height > 0)
		vi.height = _height;

	threadCount++;
	m_cmd.Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	//Worker = new ProcessFrames(env);
}

Shader::~Shader() {
	//delete Worker;
	CloseHandle(m_cmd.Event);
}

PVideoFrame __stdcall Shader::GetFrame(int n, IScriptEnvironment* env) {
	// Create a separate copy of the command for each frame.
	CommandStruct cmd = m_cmd;

	// Set input/output clips.
	PVideoFrame Src;
	for (int i = 0; i < 9; i++) {
		if (m_clip[i] != NULL) {
			Src = m_clip[i]->GetFrame(n, env);
			cmd.Input[i] = FrameRef(Src, (byte*)Src->GetReadPtr(), m_precision);
		}
		else
			cmd.Input[i] = FrameRef();
	}
	PVideoFrame dst = env->NewVideoFrame(vi);
	cmd.Output = FrameRef(dst, dst->GetWritePtr(), m_precision);
/*
	Worker->Execute(&cmd, NULL);
	return dst;*/
	
	// Add command to the worker thread queue and wait for result.
	AddCommandToQueue(&cmd, env);
	WaitForSingleObject(cmd.Event, 10000);
	initLock.lock();
	ResetEvent(cmd.Event);
	initLock.unlock();

	return dst;
}




/// Static Worker Thread. Couldn't get it to work when defined in a separate file.

int Shader::threadCount = 0;
concurrent_queue<CommandStruct> Shader::cmdBuffer;
std::mutex Shader::startLock, Shader::initLock, Shader::waiterLock;
std::once_flag OnceFlag;
std::thread* Shader::WorkerThread = NULL;
HANDLE Shader::WorkerWaiting = NULL;

	void Shader::AddCommandToQueue(CommandStruct* cmd, IScriptEnvironment* env) {
		// Add command to queue.
		cmdBuffer.push(*cmd);

		// If thread is idle or not running, make it run.
		if (WorkerWaiting != NULL)
			SetEvent(WorkerWaiting);
		if (WorkerThread == NULL) {
			startLock.lock();
			if (WorkerThread == NULL)
				WorkerThread = new std::thread(StartWorkerThread, env);
			startLock.unlock();
		}
	}

void Shader::StartWorkerThread(IScriptEnvironment* env) {
	if (WorkerThread != NULL)
		return;

	ProcessFrames Worker(env);

	// Start waiting event with a state meaning it's not waiting.
	WorkerWaiting = CreateEvent(NULL, TRUE, TRUE, NULL);

	// Process all commands in the queue.
	CommandStruct CurrentCmd, PreviousCmd;
	if (!cmdBuffer.try_pop(CurrentCmd))
		CurrentCmd.Path = NULL;
	while (CurrentCmd.Path != NULL) {
		// The result of the 1st execution will be returned on the 2nd call.
		if (FAILED(Worker.Execute(&CurrentCmd, NULL)))
			env->ThrowError("Shader: Failed to execute command");
		initLock.lock();
		SetEvent(CurrentCmd.Event); // Notify that processing is completed.
		initLock.unlock();

		PreviousCmd = CurrentCmd;
		if (!cmdBuffer.try_pop(CurrentCmd))
			CurrentCmd.Path = NULL;

		while (CurrentCmd.Path != NULL) {
			if (FAILED(Worker.Execute(&CurrentCmd, NULL)))
				env->ThrowError("Shader: Failed to execute command");
			SetEvent(CurrentCmd.Event); // Notify that processing is completed.

			PreviousCmd = CurrentCmd;
			if (!cmdBuffer.try_pop(CurrentCmd))
				CurrentCmd.Path = NULL;
		}

		// Flush the device to get last frame.
		//Worker.Flush(&PreviousCmd);
		//SetEvent(PreviousCmd.WorkerEvent); // Notify that processing is completed.

		// When queue is empty, Wait for event to be set by AddCommandToQueue.
		waiterLock.lock();
		WaitForSingleObject(WorkerWaiting, 10000);
		ResetEvent(WorkerWaiting);

		// If there are still no commands after timeout, stop thread.
		if (!cmdBuffer.try_pop(CurrentCmd))
			CurrentCmd.Path = NULL;
		waiterLock.unlock();
	}

	// Release event and thread.
	startLock.lock();
	CloseHandle(WorkerWaiting);
	WorkerWaiting = NULL;
	WorkerThread = NULL;
	startLock.unlock();
}