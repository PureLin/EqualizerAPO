#include"stdafx.h"
#include"DriectionCal.h"
#include"Quaternion.h"

float sin30 = sin(M_PI / 6);
float cos30 = cos(M_PI / 6);
float sin45 = sin(M_PI / 4);
float cos45 = cos(M_PI / 4);
float sin70 = sin(M_PI / 18 * 7);
float cos70 = cos(M_PI / 18 * 7);

Vector3 sourceDirection[8]{
	{cos30,sin30,0},//L
	{cos30,-sin30,0},//R
	{1.0,0.0,0.0},//C
	{1.0,0.0,0.0},//LFE
	{-sin45,cos45,0.0},//RL
	{-sin45,-cos45,0.0},//RR
	{cos70,sin70,0},//SL
	{cos70,-sin70,0}//SR
};
SoundDirection actualDirection[8];

void transferDirection(Vector3& v, SoundDirection& direction) {
	float length = v.Length();
	float length2 = sqrt(v.x * v.x + v.y * v.y);
	float hf = acos(v.x / length2) * 180 / M_PI;
	direction.horizon = int(round(hf));
	if (v.y > 0) {
		direction.horizon *= -1;
	}
	float vf = acos(length2 / length) * 180 / M_PI;
	direction.vertical = int(round(vf));
	if (v.z < 0) {
		direction.vertical *= -1;
	}
	direction.amp = 1 / length;
}

void calculateDirection(Position& position, int inputChannel)
{
	Quaternion q(position.yaw, position.pitch, position.roll);
	for (int ch = 0; ch != inputChannel; ++ch) {
		transferDirection(q.Rotate(sourceDirection[ch]), actualDirection[ch]);
	}
}
