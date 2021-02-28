#include "RingBuffer.h"

DEFINE_LOG_CATEGORY(RingBufferLog);

RingBuffer::RingBuffer() :
	RingBuffer(10)
{
}

RingBuffer::RingBuffer(uint32 capacity) :
	point_buffer(capacity, TArray<CloudCamPoint>()),
	current_read_index(0),
	current_write_index(0),
	buffer_capacity(capacity)
{
}

RingBuffer::~RingBuffer()
{
}

void RingBuffer::NewPointCloudFrame(TArray<CloudCamPoint> points)
{
	current_read_index = current_write_index;

	uint32 next_index = point_buffer.GetNextIndex(current_write_index);

	// Same as empty, but doesn't change memory allocations, unless the new size is larger than the current array.
	point_buffer[next_index].Reset(0); 

	// TODO: Does this cause a shift / reallocation?
	point_buffer[next_index].Insert(points, 0);

	current_write_index = next_index;
	
}

void RingBuffer::AppendPointCloudFrame(TArray<CloudCamPoint> points)
{
	point_buffer[current_write_index].Append(points);
}

bool RingBuffer::CurrentlyHasPoints() {
	return point_buffer[current_read_index].Num() != 0;
}

TArray<CloudCamPoint> RingBuffer::GetPoints()
{
	return point_buffer[current_read_index];
}

uint32 RingBuffer::CurrentReadIndex() 
{
	return current_read_index;
}

uint32 RingBuffer::GetNextIndex(uint32 index) {
	return point_buffer.GetNextIndex(index);
}

uint32 RingBuffer::GetBufferCapacity()
{
	return buffer_capacity;
}

