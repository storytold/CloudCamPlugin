#pragma once

// This borrows heavily from the AzureKinectUnreal library

// Unreal Core
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

// ZeroMQ
#include <zmq.hpp>

// CloudCam
#include "RingBuffer.h"
#include "ZeroMqProtocol.h"

DECLARE_LOG_CATEGORY_EXTERN(ZeroMqPollingThreadLog, Log, All);

class CLOUDCAMPLUGIN_API FZeroMqPollingThread : public FRunnable
{
public:
	FZeroMqPollingThread();
	~FZeroMqPollingThread();

	/** Initializes the thread. */
	static FZeroMqPollingThread *StaticInitThread();

	/** Completes the execution of the thread and deletes the singleton instance. */
	static void StaticShutdown();
	
	/** Stops the thread and waits for its completion. */
	void EnsureCompletion();

	virtual bool Init();

	virtual uint32 Run();

	virtual void Stop();

	TSharedRef<RingBuffer> GetPointBuffer();

	uint32 GetCurrentCloudId();

private:
	static FZeroMqPollingThread* poller_instance_;

	/** Thread to run this Runnable object. */
	FRunnableThread* runnable_thread_;
	
	/** Used to stop the thread execution. */
	FThreadSafeCounter stop_thread_counter_;

	/** Ring buffer */
	TSharedRef<RingBuffer> point_buffer;

	/** Variable length point malloc buffer */
	CloudCamPointCloudDataPacket* malloc_point_buffer;

	TArray<CloudCamPoint> DoReadPointCloudData(zmq::socket_t* socket, bool* is_beginning);
};
