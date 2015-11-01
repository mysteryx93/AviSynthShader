#include "WorkerThread.h"

concurrency::concurrent_queue<CommandStruct> WorkerThread::cmdBuffer;
std::mutex WorkerThread::addLock;
std::atomic<std::thread*> WorkerThread::pThread;
std::atomic<HANDLE> WorkerThread::WorkerWaiting = NULL;