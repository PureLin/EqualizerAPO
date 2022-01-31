#pragma once
#include <math.h>
struct Position {
	int yaw;
	int pitch;
	int roll;
	int x;
	int y;
	int z;

	Position(double* data) {
		x = data[0];
		y = data[1];
		z = data[2];
		yaw = data[3];
		pitch = data[4];
		roll = data[5];
		if (abs(yaw) > 45) {
			yaw = yaw > 0 ? 45 : -45;
		}
		if (abs(pitch) > 25) {
			pitch = pitch > 0 ? 25 : -25;
		}
		if (abs(roll) > 45) {
			roll = roll > 0 ? 45 : -45;
		}
	}
};

struct SoundDirection {
	int horizon;
	int vertical;
	float amp;
};

extern SoundDirection actualDirection[8];

void calculateDirection(Position& position,int inputChannel);