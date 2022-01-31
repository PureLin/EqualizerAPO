#pragma once
#include "Vector3.h"

class Quaternion
{
public:
	Quaternion(float yaw, float pitch, float roll);
	~Quaternion();
	void SetEulerAngle(float yaw, float pitch, float roll);

	static Quaternion identity;
	Vector3 Rotate(const Vector3& point);
private:

	float x;
	float y;
	float z;
	float w;
};