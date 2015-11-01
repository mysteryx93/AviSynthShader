#pragma once
#include "D3D9RenderImpl.h"
#include "ProcessFrames.h"
#include <list>
#include <thread>
#include <mutex>
#include <concurrent_queue.h>

class WorkerThread {
public:
	static concurrency::concurrent_queue<CommandStruct> cmdBuffer;
	static std::mutex initLock, startLock, waiterLock;
	static std::thread* pThread;
	static HANDLE WorkerWaiting;

	static void AddCommandToQueue(CommandStruct cmd, IScriptEnvironment* env) {
		// Add command to queue.
		cmdBuffer.push(cmd);

		// If thread is idle or not running, make it run.
		if (WorkerWaiting != NULL)
			SetEvent(WorkerWaiting);
		if (pThread == NULL) {
			startLock.lock();
			if (pThread == NULL)
				pThread = new std::thread(StartWorkerThread, env);
			startLock.unlock();
		}
	}

	static void WorkerThread::StartWorkerThread(IScriptEnvironment* env) {
		if (pThread != NULL)
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
				initLock.lock();
				SetEvent(CurrentCmd.Event); // Notify that processing is completed.
				initLock.unlock();

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
		pThread = NULL;
		startLock.unlock();
	}
};