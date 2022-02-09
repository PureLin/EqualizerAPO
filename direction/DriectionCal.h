#pragma once
#include <math.h>
struct Position {
	int yaw;
	int pitch;
	int roll;

	Position(double* data) {
		yaw = data[3] / 4;
		pitch = data[4] / 4;
		roll = data[5] / 4;
	}
};

struct SoundDirection {
	int horizon;
	int vertical;
};

extern SoundDirection actualDirection[8];

void calculateDirection(Position& position, int inputChannel);