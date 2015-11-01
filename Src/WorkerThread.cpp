#include "WorkerThread.h"

concurrency::concurrent_queue<CommandStruct> WorkerThread::cmdBuffer;
std::mutex WorkerThread::addLock;
std::thread* WorkerThread::pThread;
HANDLE WorkerThread::WorkerWaiting = NULL;