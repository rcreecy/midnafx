// Exercise the actual upstream JKRDecomp translation unit with a controlled OS seam.
// Queue operations preserve the GameCube API's blocking and nonblocking behavior.
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#include "JSystem/JKernel/JKRDecomp.h"
#include "JSystem/JKernel/JKRAramPiece.h"

namespace {
using namespace std::chrono_literals;
std::mutex queueMutex;
std::condition_variable changed;
std::thread worker;
OSThread threadRecord{};
JKRThread* threadObject = nullptr;
bool ready = false;
bool drainAllowed = false;
bool workerExited = false;
bool emptyAfterDrain = false;
bool recoveryUsed = false;
bool sentinelAccepted = false;
int sentinelFlags = -1;
int processed = 0;

[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    std::fflush(stderr);
    std::_Exit(3);
}

void enqueue(OSMessageQueue* queue, OSMessage message) {
    queue->msgArray[(queue->firstIndex + queue->usedCount) % queue->msgCount] = message;
    ++queue->usedCount;
}
} // namespace

// The unrelated heap and thread infrastructure is intentionally excluded.
JKRHeap* JKRHeap::sSystemHeap = nullptr;
void* operator new(size_t size, JKRHeapToken, JKRHeap*, int) noexcept { return std::malloc(size); }
void operator delete(void* pointer, JKRHeapToken) noexcept { std::free(pointer); }
JKRDisposer::JKRDisposer() : mHeap(nullptr), mLink(this) {}
JKRDisposer::~JKRDisposer() = default;
JSUPtrLink::JSUPtrLink(void* object)
    : mObject(object), mList(nullptr), mPrev(nullptr), mNext(nullptr) {}
JSUPtrLink::~JSUPtrLink() = default;
JKRThread::JKRThread(u32, int, int) : mThreadListLink(this) {
    mThreadRecord = &threadRecord;
    threadObject = this;
}
JKRThread::~JKRThread() = default;
void JKRAramPiece::sendCommand(JKRAMCommand*) { fail("Unexpected ARAM command"); }

extern "C" {
void OSInitMessageQueue(OSMessageQueue* queue, OSMessage* buffer, s32 capacity) {
    std::lock_guard lock(queueMutex);
    queue->msgArray = buffer;
    queue->msgCount = capacity;
    queue->firstIndex = 0;
    queue->usedCount = 0;
    if (queue == &JKRDecomp::sMessageQueue) ready = true;
    changed.notify_all();
}

BOOL OSSendMessage(OSMessageQueue* queue, OSMessage message, s32 flags) {
    std::unique_lock lock(queueMutex);
    const bool sentinel = queue == &JKRDecomp::sMessageQueue && message == nullptr;
    if (sentinel) {
        sentinelFlags = flags;
        drainAllowed = true;
        changed.notify_all();
    }
    if (queue->usedCount == queue->msgCount) {
        if (flags == OS_MESSAGE_NOBLOCK) return 0;
        if (!changed.wait_for(lock, 2s, [queue] { return queue->usedCount < queue->msgCount; }))
            fail("Blocking queue send timed out");
    }
    enqueue(queue, message);
    if (sentinel) sentinelAccepted = true;
    if (queue != &JKRDecomp::sMessageQueue) ++processed;
    changed.notify_all();
    return 1;
}

BOOL OSReceiveMessage(OSMessageQueue* queue, OSMessage* message, s32 flags) {
    std::unique_lock lock(queueMutex);
    if (queue == &JKRDecomp::sMessageQueue) {
        if (!changed.wait_for(lock, 3s, [] { return drainAllowed; })) fail("Worker gate timed out");
        if (queue->usedCount == 0 && processed == 8) {
            emptyAfterDrain = true;
            changed.notify_all();
        }
    }
    if (queue->usedCount == 0) {
        if (flags == OS_MESSAGE_NOBLOCK) return 0;
        if (!changed.wait_for(lock, 3s, [queue] { return queue->usedCount > 0; }))
            fail("Blocking queue receive timed out");
    }
    if (message) *message = queue->msgArray[queue->firstIndex];
    queue->firstIndex = (queue->firstIndex + 1) % queue->msgCount;
    --queue->usedCount;
    changed.notify_all();
    return 1;
}

s32 OSResumeThread(OSThread*) {
    worker = std::thread([] {
        threadObject->run();
        std::lock_guard lock(queueMutex);
        workerExited = true;
        changed.notify_all();
    });
    return 0;
}

BOOL OSJoinThread(OSThread*, void**) {
    std::unique_lock lock(queueMutex);
    if (!changed.wait_for(lock, 2s, [] { return workerExited || emptyAfterDrain; }))
        fail("Decompressor did not reach its bounded shutdown checkpoint");
    if (!workerExited && emptyAfterDrain && !sentinelAccepted) {
        // Recover only after the real worker drains every command and waits again.
        recoveryUsed = true;
        enqueue(&JKRDecomp::sMessageQueue, nullptr);
        changed.notify_all();
    }
    if (!changed.wait_for(lock, 2s, [] { return workerExited; })) fail("Recovery did not stop worker");
    lock.unlock();
    worker.join();
    return 1;
}
} // extern "C"

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "empty") && std::strcmp(argv[1], "full"))) return 2;
    const bool full = std::strcmp(argv[1], "full") == 0;
    JKRDecomp::create(1);
    {
        std::unique_lock lock(queueMutex);
        if (!changed.wait_for(lock, 2s, [] { return ready; })) fail("Queue initialization timed out");
    }
    std::array<JKRDecompCommand, 8> commands;
    std::array<u8, 4> source{};
    std::array<u8, 4> destination{};
    if (full) {
        for (auto& command : commands) {
            command.mSrcBuffer = source.data();
            command.mDstBuffer = destination.data();
            command.mSrcLength = source.size();
            command.mDstLength = destination.size();
            if (!OSSendMessage(&JKRDecomp::sMessageQueue, &command, OS_MESSAGE_NOBLOCK))
                fail("Could not prepare the full eight-slot queue");
        }
    }
    JKRDecomp::destroy();
    const bool pass = workerExited && sentinelAccepted && !recoveryUsed && processed == (full ? 8 : 0);
    std::printf("{\"case\":\"%s\",\"passed\":%s,\"commands_processed\":%d,"
                "\"sentinel_accepted\":%s,\"sentinel_flags\":%d,\"recovery_used\":%s,"
                "\"worker_exited\":%s}\n", argv[1], pass ? "true" : "false", processed,
                sentinelAccepted ? "true" : "false", sentinelFlags,
                recoveryUsed ? "true" : "false", workerExited ? "true" : "false");
    return pass ? 0 : 1;
}
