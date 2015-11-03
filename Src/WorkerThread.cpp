#include "WorkerThread.h"

concurrency::concurrent_queue<CommandStruct> WorkerThread::cmdBuffer;
std::mutex WorkerThread::addLock;
std::list<std::thread*> WorkerThread::pThreads;
std::atomic<HANDLE> WorkerThread::WorkerWaiting = NULL;
std::atomic<int> WorkerThread::MaxThreadCount = 4, WorkerThread::WorkerThreadCount = 0;
