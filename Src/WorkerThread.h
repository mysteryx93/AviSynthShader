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
	static std::list<std::thread*> pThreads;
	static std::atomic<int> MaxThreadCount, WorkerThreadCount;
	static std::atomic<HANDLE> WorkerWaiting;

	static void AddCommandToQueue(CommandStruct cmd, IScriptEnvironment* env) {
		// Add command to queue.
		cmdBuffer.push(cmd);

		if (WorkerWaiting)
			SetEvent(WorkerWaiting);
		else {
			std::lock_guard<std::mutex> lock(addLock);
			if (WorkerWaiting)
				SetEvent(WorkerWaiting);
			else {
				WorkerWaiting = CreateEvent(NULL, FALSE, TRUE, NULL);
			}
		}

		if (WorkerThreadCount < MaxThreadCount) {
			std::lock_guard<std::mutex> lock(addLock);
			pThreads.push_back(new std::thread(StartWorkerThread, WorkerThreadCount++, env));
		}
	}

	static void WorkerThread::StartWorkerThread(int threadId, IScriptEnvironment* env) {
		ProcessFrames Worker(env);

		// Process all commands in the queue.
		CommandStruct CurrentCmd, PreviousCmd;
		PreviousCmd.Path = NULL;
		if (!cmdBuffer.try_pop(CurrentCmd))
			CurrentCmd.Path = NULL;
		while (CurrentCmd.Path) {
			// The result of the 1st execution will be returned on the 2nd call.
			if (FAILED(Worker.Execute(&CurrentCmd, NULL))) //PreviousCmd.Path ? &PreviousCmd : 
				env->ThrowError("Shader: Failed to execute command");
			Worker.Flush(&CurrentCmd);
			SetEvent(CurrentCmd.Event);
			//if (PreviousCmd.Path)
			//	SetEvent(PreviousCmd.Event); // Notify that processing is completed.

			PreviousCmd = CurrentCmd;
			if (!cmdBuffer.try_pop(CurrentCmd))
				CurrentCmd.Path = NULL;

			while (CurrentCmd.Path) {
				if (FAILED(Worker.Execute(&CurrentCmd, NULL))) //&PreviousCmd
					env->ThrowError("Shader: Failed to execute command");
				Worker.Flush(&CurrentCmd);
				SetEvent(CurrentCmd.Event);
				//SetEvent(PreviousCmd.Event); // Notify that processing is completed.

				PreviousCmd = CurrentCmd;
				if (!cmdBuffer.try_pop(CurrentCmd))
					CurrentCmd.Path = NULL;
			}

			// Flush the device to get last frame.
			//Worker.Flush(&PreviousCmd);
			//SetEvent(PreviousCmd.Event); // Notify that processing is completed.

			// When queue is empty, Wait for event to be set by AddCommandToQueue.
			// Note that the added item might have already be read by the previous loop, so keep requesting data until we get a clear timeout.
			while (!CurrentCmd.Path && WaitForSingleObject(WorkerWaiting, 10000) != WAIT_TIMEOUT) {
				// If there are still no commands after timeout, stop thread.
				if (!cmdBuffer.try_pop(CurrentCmd))
					CurrentCmd.Path = NULL;
			}
		}

		// Release event and thread.
		std::lock_guard<std::mutex> lock(addLock);
		std::thread* DeleteThread = NULL;
		for (auto& it : pThreads) {
			if (it->get_id() == std::this_thread::get_id()) {
				DeleteThread = it;
				break;
			}
		}
		if (DeleteThread) {
			pThreads.remove(DeleteThread);
			WorkerThreadCount--;
		}
		if (WorkerThreadCount == 0) {
			CloseHandle(WorkerWaiting);
			WorkerWaiting = NULL;
		}
	}
};