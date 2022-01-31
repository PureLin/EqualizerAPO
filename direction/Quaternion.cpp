#pragma once
#include "stdafx.h"
#include "Quaternion.h"
#include <cmath>

Quaternion::Quaternion(float yaw, float pitch, float roll)
{
	this->SetEulerAngle(yaw, pitch, roll);
}

Quaternion::~Quaternion()
{

}

void Quaternion::SetEulerAngle(float yaw, float pitch, float roll)
{
	float  angle;
	float  sinRoll, sinPitch, sinYaw, cosRoll, cosPitch, cosYaw;

	angle = yaw / 180 * M_PI_2;
	sinYaw = sin(angle);
	cosYaw = cos(angle);

	angle = pitch / 180 * M_PI_2;
	sinPitch = sin(angle);
	cosPitch = cos(angle);

	angle = roll / 180 * M_PI_2;
	sinRoll = sin(angle);
	cosRoll = cos(angle);

	w = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;
	x = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
	y = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
	z = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;
	float mag = x * x + y * y + z * z + w * w;
	x /= mag;
	y /= mag;
	z /= mag;
	w /= mag;
}

Vector3 Quaternion::Rotate(const Vector3& v)
{
	// Extract the vector part of the quaternion
	Vector3 u(x, y, z);

	// Extract the scalar part of the quaternion
	float s = w;

	// Do the math
	return 2.0f * Vector3::Dot(u, v) * u
		+ (s * s - Vector3::Dot(u, u)) * v
		+ 2.0f * s * Vector3::Cross(u, v);
}