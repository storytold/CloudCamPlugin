#pragma once

/// Read over the wire from ZeroMq
#pragma pack(push, 1)
struct CloudCamPoint {
	// Position
	float x;
	float y;
	float z;

	// Color
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;

	CloudCamPoint();
	~CloudCamPoint();
};
#pragma pack(pop)


/// Read over the wire from ZeroMq
#pragma pack(push, 1)
struct CloudCamDataLengthPacket {
	int command;
	int length;
};
#pragma pack(pop)


/// Read over the wire from ZeroMq
#pragma pack(push, 1)
struct CloudCamPointCloudDataPacket {
	int command;
	int length;
	CloudCamPoint points[];
};
#pragma pack(pop)
