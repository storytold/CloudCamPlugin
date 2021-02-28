#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"

#include "ZeroMqProtocol.h"

DECLARE_LOG_CATEGORY_EXTERN(RingBufferLog, Log, All);

class CLOUDCAMPLUGIN_API RingBuffer {
public:
	RingBuffer();
	RingBuffer(uint32 capacity);

	~RingBuffer();

	void NewPointCloudFrame(TArray<CloudCamPoint> points);

	void AppendPointCloudFrame(TArray<CloudCamPoint> points);

	bool CurrentlyHasPoints();

	TArray<CloudCamPoint> GetPoints();

	// External callers to this can see if we're consuming a new point cloud
	uint32 CurrentReadIndex();

	uint32 GetBufferCapacity();

private:
	TCircularBuffer<TArray<CloudCamPoint>> point_buffer;

	uint32 current_read_index;
	uint32 current_write_index;

	uint32 buffer_capacity;

	uint32 GetNextIndex(uint32 index);
};
