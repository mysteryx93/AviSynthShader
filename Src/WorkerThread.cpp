#include "WorkerThread.h"

concurrency::concurrent_queue<CommandStruct> WorkerThread::cmdBuffer;
std::mutex WorkerThread::startLock, WorkerThread::initLock, WorkerThread::waiterLock;
std::thread* WorkerThread::pThread = NULL;
HANDLE WorkerThread::WorkerWaiting = NULL;