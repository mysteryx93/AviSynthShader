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
	static std::atomic<std::thread*> pThread;
	static std::atomic<HANDLE> WorkerWaiting;

	static void AddCommandToQueue(CommandStruct cmd, IScriptEnvironment* env) {
		std::lock_guard<std::mutex> lock(addLock);
		// Add command to queue.
		cmdBuffer.push(cmd);

		// If thread is idle or not running, make it run.
		//if (WorkerWaiting != NULL)
		if ((std::thread*)pThread != NULL)
			SetEvent(WorkerWaiting);
		else {
			pThread = new std::thread(StartWorkerThread, env);
		}
	}

	static void WorkerThread::StartWorkerThread(IScriptEnvironment* env) {
		if ((std::thread*)pThread != NULL)
			return;

		ProcessFrames Worker(env);

		// Start waiting event with a state meaning it's not waiting.
		WorkerWaiting = CreateEvent(NULL, FALSE, TRUE, NULL);

		// Process all commands in the queue.
		CommandStruct CurrentCmd, PreviousCmd;
		if (!cmdBuffer.try_pop(CurrentCmd))
			CurrentCmd.Path = NULL;
		while (CurrentCmd.Path != NULL) {
			// The result of the 1st execution will be returned on the 2nd call.
			if (FAILED(Worker.Execute(&CurrentCmd, NULL)))
				env->ThrowError("Shader: Failed to execute command");
			SetEvent(CurrentCmd.Event); // Notify that processing is completed.

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
			WaitForSingleObject(WorkerWaiting, 10000);

			while (CurrentCmd.Path == NULL) {
				Sleep(200);
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