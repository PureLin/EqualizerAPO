#pragma once
#include<math.h>
extern const double uZero;

class Vector3
{
public:
	float x, y, z;
	Vector3() :x(0), y(0), z(0) {  }
	Vector3(float _x, float _y, float _z) :x(_x), y(_y), z(_z) {  }
	explicit Vector3(float value) :x(value), y(value), z(value) {  }
	Vector3(const Vector3& other) :x(other.x), y(other.y), z(other.z) {  }
	float Length()const
	{
		return ::sqrtf(x * x + y * y + z * z);
	}
	void Normalize()
	{
		float scale = 1 / Length();
		x *= scale;
		y *= scale;
		z *= scale;
	}
	void Zero()
	{
		x = y = z = 0;
	}
	int Size()
	{
		return 3;
	}

	Vector3 operator+(const Vector3& other)const
	{
		return Vector3(x + other.x, y + other.y, z + other.z);
	}

	Vector3 operator*(const float value)const
	{
		return Vector3(x * value, y * value, z * value);
	}
	
	Vector3 operator-(const Vector3& other)const
	{
		return Vector3(x - other.x, y - other.y, z - other.z);
	}

	friend Vector3 operator*(const float value, const Vector3& other)
	{
		return other * value;
	}

	static Vector3 Cross(const Vector3& left, const Vector3& right)
	{
		float x = left.y * right.z - left.z * right.y;
		float y = left.z * right.x - left.x * right.z;
		float z = left.x * right.y - left.y * right.x;
		return Vector3(x, y, z);
	}
	static float Dot(const Vector3& left, const Vector3& right) {
		return left.x * right.x + left.y * right.y + left.z * right.z;
	}
};