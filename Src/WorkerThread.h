#pragma once
#include "D3D9RenderImpl.h"
#include "ProcessFrames.h"
#include <atomic>
#include <list>
#include <thread>
#include <mutex>
#include <concurrent_queue.h>

class WorkerThread {
public:
	static concurrency::concurrent_queue<CommandStruct> cmdBuffer;
	static std::mutex addLock;
	static std::thread* pThread;
	static HANDLE WorkerWaiting;

	static void AddCommandToQueue(CommandStruct cmd, IScriptEnvironment* env) {
		// Add command to queue.
		cmdBuffer.push(cmd);

		// If thread is idle or not running, make it run.
		std::lock_guard<std::mutex> lock(addLock);
		if (pThread)
			SetEvent(WorkerWaiting);
		else {
			pThread = new std::thread(StartWorkerThread, env);
		}
	}

	static void WorkerThread::StartWorkerThread(IScriptEnvironment* env) {
		ProcessFrames Worker(env);

		// Start waiting event with a state meaning it's not waiting.
		WorkerWaiting = CreateEvent(NULL, FALSE, TRUE, NULL);

		// Process all commands in the queue.
		CommandStruct CurrentCmd, PreviousCmd;
		PreviousCmd.Path = NULL;
		if (!cmdBuffer.try_pop(CurrentCmd))
			CurrentCmd.Path = NULL;
		while (CurrentCmd.Path != NULL) {
			// The result of the 1st execution will be returned on the 2nd call.
			if (FAILED(Worker.Execute(&CurrentCmd, PreviousCmd.Path ? &PreviousCmd : NULL)))
				env->ThrowError("Shader: Failed to execute command");
			if (PreviousCmd.Path)
				SetEvent(PreviousCmd.Event); // Notify that processing is completed.

			PreviousCmd = CurrentCmd;
			if (!cmdBuffer.try_pop(CurrentCmd))
				CurrentCmd.Path = NULL;

			while (CurrentCmd.Path != NULL) {
				if (FAILED(Worker.Execute(&CurrentCmd, &PreviousCmd)))
					env->ThrowError("Shader: Failed to execute command");
				SetEvent(PreviousCmd.Event); // Notify that processing is completed.

				PreviousCmd = CurrentCmd;
				if (!cmdBuffer.try_pop(CurrentCmd))
					CurrentCmd.Path = NULL;
			}

			// Flush the device to get last frame.
			Worker.Flush(&PreviousCmd);
			SetEvent(PreviousCmd.Event); // Notify that processing is completed.

			// When queue is empty, Wait for event to be set by AddCommandToQueue.
			// Note that the added item might have already be read by the previous loop, so keep requesting data until we get a clear timeout.
			while (CurrentCmd.Path == NULL && WaitForSingleObject(WorkerWaiting, 10000) != WAIT_TIMEOUT) {
				// If there are still no commands after timeout, stop thread.
				if (!cmdBuffer.try_pop(CurrentCmd))
					CurrentCmd.Path = NULL;
			}
		}

		// Release event and thread.
		std::lock_guard<std::mutex> lock(addLock);
		CloseHandle(WorkerWaiting);
		WorkerWaiting = NULL;
		pThread = NULL;
	}
};