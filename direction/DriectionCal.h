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
		if (abs(yaw) > 45) {
			yaw = yaw > 0 ? 45 : -45;
		}
		if (abs(pitch) > 30) {
			pitch = pitch > 0 ? 30 : -30;
		}
		if (abs(roll) > 30) {
			roll = roll > 0 ? 30 : -30;
		}
	}
};

struct SoundDirection {
	int horizon;
	int vertical;
	float amp;
};

extern SoundDirection actualDirection[8];

void calculateDirection(Position& position, int inputChannel);