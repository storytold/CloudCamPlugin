#include "ZeroMqPollingThread.h"

#include "GenericPlatform/GenericPlatformProcess.h"

#include <stdlib.h>

DEFINE_LOG_CATEGORY(ZeroMqPollingThreadLog);

FZeroMqPollingThread* FZeroMqPollingThread::poller_instance_ = nullptr;

FZeroMqPollingThread::FZeroMqPollingThread() :
	runnable_thread_(nullptr),
	stop_thread_counter_(0),
	point_buffer(TSharedRef<RingBuffer>(new RingBuffer(10))),
	malloc_point_buffer(nullptr)
{
	//windows default = 8mb for thread, could specify more
	runnable_thread_ = FRunnableThread::Create(this, TEXT("FZeroMqPollingThread"), 0, TPri_BelowNormal); 

	if (!runnable_thread_) {
		UE_LOG(ZeroMqPollingThreadLog, Error, TEXT("Failure to create camera poller thread"));
	}

	size_t size = 16 * 100000;

	malloc_point_buffer = (CloudCamPointCloudDataPacket*)malloc(size);
}

FZeroMqPollingThread::~FZeroMqPollingThread()
{
	if (runnable_thread_) {
		delete runnable_thread_;
		runnable_thread_ = nullptr;
	}

	if (malloc_point_buffer != nullptr) {
		free(malloc_point_buffer);
		malloc_point_buffer = nullptr;
	}
}


FZeroMqPollingThread* FZeroMqPollingThread::StaticInitThread()
{
	// Create new instance of thread if it does not exist
	// and the platform supports multi threading!
	if (!poller_instance_ && FPlatformProcess::SupportsMultithreading()) {
		poller_instance_ = new FZeroMqPollingThread();
	}
	return poller_instance_;
}

void FZeroMqPollingThread::StaticShutdown()
{
	if (poller_instance_) {
		poller_instance_->EnsureCompletion();
		delete poller_instance_;
		poller_instance_ = nullptr;
	}
}

void FZeroMqPollingThread::EnsureCompletion()
{
	Stop();
	if (runnable_thread_)
	{
		runnable_thread_->WaitForCompletion();
	}
}

bool FZeroMqPollingThread::Init()
{
	//UE_LOG(CameraPollerThreadLog, Log, 
	//		TEXT("Camera poller thread started."));
	return true;
}

enum NiagaraReadState {
	NiagaraReadDataLength,
	NiagaraReadPointCloudData,
};

TOptional<int> DoReadNiagaraDataLength(zmq::socket_t* socket) {
	size_t size = 8;

	CloudCamDataLengthPacket* buffer = (CloudCamDataLengthPacket*)malloc(size);
	zmq::message_t request = zmq::message_t(&buffer, size);

	bool success = socket->recv(&request, 0); // TODO: Handle response code.

	if (!success) {
		free(buffer);
		return TOptional<int>();
	}

	CloudCamDataLengthPacket* typed_buffer = ((CloudCamDataLengthPacket*)(request.data()));

	auto length = typed_buffer->length;

	free(buffer);

	return TOptional<int>(length);
}

TArray<CloudCamPoint> FZeroMqPollingThread::DoReadPointCloudData(zmq::socket_t* socket, bool* is_beginning) {
	// Maximum possible points to be sent in this packet
	// The packet may contain fewer, and the actual number is encoded in the payload.
	auto num_points = 3000;

	size_t size = 8 + (16 * num_points);

	CloudCamPointCloudDataPacket* buffer = (CloudCamPointCloudDataPacket*)malloc(size);

	zmq::message_t request = zmq::message_t(&malloc_point_buffer, size);

	bool success = socket->recv(&request, 0); // TODO: Handle response code.

	CloudCamPointCloudDataPacket* typed_buffer = ((CloudCamPointCloudDataPacket*)(request.data()));

	if (typed_buffer->command == 2) {
		*is_beginning = true;
	}
	else {
		*is_beginning = false;
	}

	TArray<CloudCamPoint> point_array;

	for (auto i = 0; i < typed_buffer->length; i++) {
		auto point = typed_buffer->points[i];
		point_array.Push(point);
	}

	free(buffer);

	return point_array;
}

void ReplyNiagaraAck(zmq::socket_t* socket) {
	zmq::message_t reply(3);
	memcpy((void*)reply.data(), "ACK", 3);
	socket->send(reply);
}

void PrintZeroMqNiagaraReceiveError() {
	switch (errno) {
	case EAGAIN:
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket: EAGAIN"));
		break;
	case ENOTSUP:
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket: ENOTSUP"));
		break;
	case EFSM:
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket: EFSM"));
		break;
	case ETERM:
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket: ETERM"));
		break;
	case ENOTSOCK:
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket: ENOTSOCK"));
		break;
	case EINTR:
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket: EINTR"));
		break;
	case EFAULT:
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket: EFAULT"));
		break;
	default:
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket: Unknown!?"));
		break;
	}
	auto reason = strerror(errno);
	UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("Error receiving data from socket (%d): %s"), errno, reason);
}


uint32 FZeroMqPollingThread::Run()
{
	int major, minor, patch;
	zmq::version(&major, &minor, &patch);

	UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("ZeroMQ version: v%d.%d.%d"), major, minor, patch);

	zmq::context_t context(1);

	zmq::socket_t socket(context, ZMQ_PULL);
	//zmq::socket_t socket(context, ZMQ_REP); // "Server" is REP, "Client" is REQ

	try {
		// Whoa, in-process is probably really fast: sock.bind("inproc://test");
		//socket.bind("tcp://*:5555");
		socket.bind("tcp://*:8888");
	}
	catch (zmq::error_t& err) {
		(void)err; // https://stackoverflow.com/a/24428637
		UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("ERROR BINDING SOCKET! C++ Exception (%d) : %s"), err.num(), err.what());
		Sleep(1.0);
	}
	
	UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("ZeroMQ Socket Bound!"));

	NiagaraReadState read_state = NiagaraReadState::NiagaraReadDataLength;
	int data_length = 0;
	bool is_cloud_start = true;

	while (stop_thread_counter_.GetValue() == 0)
	{
		TOptional<int> maybe_data_length = TOptional<int>();

		try {
			switch (read_state) {
			case NiagaraReadState::NiagaraReadDataLength:
			case NiagaraReadState::NiagaraReadPointCloudData:
				TArray<CloudCamPoint> points = DoReadPointCloudData(&socket, &is_cloud_start);

				if (is_cloud_start) {
					point_buffer->NewPointCloudFrame(points);
				}
				else {
					point_buffer->AppendPointCloudFrame(points);
				}

				//ReplyNiagaraAck(&socket);

				read_state = NiagaraReadState::NiagaraReadDataLength;
				continue;
			}

		} catch (zmq::error_t& err) {
			(void)err; // https://stackoverflow.com/a/24428637
			UE_LOG(ZeroMqPollingThreadLog, Log, TEXT("ERROR! C++ Exception (%d) : %s"), err.num(), err.what());
			Sleep(1.0);
			continue;
		}
	}

	return 0;
}

uint32 FZeroMqPollingThread::GetCurrentCloudId() {
	return point_buffer->CurrentReadIndex();
}

TSharedRef<RingBuffer> FZeroMqPollingThread::GetPointBuffer() {
	TSharedRef<RingBuffer> copy = point_buffer;
	return copy;
}

void FZeroMqPollingThread::Stop()
{
	stop_thread_counter_.Increment();
}
