#pragma once
#include "core/Constants.h"

#include <cmath>
#include <xmmintrin.h>

namespace dalia::math {

    inline float DbToGain(float db) {
        if (db <= VOLUME_DB_MIN) return GAIN_MIN;
        if (db >= VOLUME_DB_MAX) return GAIN_MAX;

        return powf(10.0f, db / 20.0f);
    }

    inline float GainToDb(float linear) {
        if (linear <= GAIN_MIN) return VOLUME_DB_MIN;
        if (linear >= GAIN_MAX) return VOLUME_DB_MAX;

        return 20.0f * log10f(linear);
    }

    inline bool NearlyEqual(float a, float b, float epsilon) {
        return std::abs(a - b) < epsilon;
    }

	 inline float CalculateInvSqrt(float sqrDist) {
    	if (sqrDist < EPSILON) return 0.0f;
    	float invSqrt = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(sqrDist)));
    	return invSqrt * (1.5f - (0.5f * sqrDist * invSqrt * invSqrt)); // Newton-Raphson iteration
    }

	struct alignas(16) Vector3 {
		float x, y, z;
		float padding; // Not used

		Vector3() : x(0.0f), y(0.0f), z(0.0f), padding(0.0f) {}
		Vector3(float xVal, float yVal, float zVal) : x(xVal), y(yVal), z(zVal), padding(0.0f) {}

		Vector3 operator+(const Vector3& other) const { return Vector3(x + other.x, y + other.y, z + other.z); }
		Vector3 operator-(const Vector3& other) const { return Vector3(x - other.x, y - other.y, z - other.z); }
		Vector3 operator*(float scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }

		float Dot(const Vector3& other) const { return (x * other.x) + (y * other.y) + (z * other.z); }

		Vector3 Cross(const Vector3& other) const {
			return Vector3(
				(y * other.z) - (z * other.y),
				(z * other.x) - (x * other.z),
				(x * other.y) - (y * other.x)
			);
		}

		float DistanceTo(const Vector3& other) const {
			float dx = x - other.x;
			float dy = y - other.y;
			float dz = z - other.z;
			float sqrDist = (dx * dx) + (dy * dy) + (dz * dz);

			return sqrDist * CalculateInvSqrt(sqrDist);;
		}

		void Normalize() {
			float sqrDist = (x * x) + (y * y) + (z * z);
			if (sqrDist > EPSILON) {
				float invSqrt = CalculateInvSqrt(sqrDist);
				x *= invSqrt;
				y *= invSqrt;
				z *= invSqrt;
			}
		}

		static Vector3 Normalize(const Vector3& vec) {
			Vector3 normalized;
			float sqrDist = (vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z);
			if (sqrDist > EPSILON) {
				float invSqrt = CalculateInvSqrt(sqrDist);
				normalized.x = vec.x * invSqrt;
				normalized.y = vec.y * invSqrt;
				normalized.z = vec.z * invSqrt;
			}

			return normalized;
		}
	};

	inline Vector3 AngleToVector3(float degrees) {
		float radians = degrees * DEG_TO_RAD;
		return Vector3(std::sinf(radians), 0.0f, cosf(radians));
	}

}