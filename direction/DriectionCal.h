#pragma once
#include <math.h>
struct Position {
	double x;
	double y;
	double z;
	int yaw;
	int pitch;
	int roll;

	Position(double* data) {
		x = data[0];
		y = data[1];
		z = data[2];
		yaw = data[3];
		pitch = data[4];
		roll = data[5];
	}
};

struct SoundDirection {
	int horizon;
	int vertical;
	double volume;
};

extern SoundDirection actualDirection[8];

void calculateDirection(Position& position, int inputChannel);