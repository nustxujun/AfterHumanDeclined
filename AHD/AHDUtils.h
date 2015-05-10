#ifndef _AHDUtils_H_
#define _AHDUtils_H_

#include <assert.h>

namespace AHD
{
	class Vector3
	{
	public:
		float x, y, z;

		inline Vector3()
		{
		}

		inline Vector3(const float fX, const float fY, const float fZ)
			: x(fX), y(fY), z(fZ)
		{
		}

		inline Vector3 operator - () const
		{
			return Vector3(-x, -y, -z);
		}
	
		inline Vector3 operator - (const Vector3& rkVector) const
		{
			return Vector3(
				x - rkVector.x,
				y - rkVector.y,
				z - rkVector.z);
		}

		inline Vector3 operator * (const float fScalar) const
		{
			return Vector3(
				x * fScalar,
				y * fScalar,
				z * fScalar);
		}

		inline Vector3 operator / (const float fScalar) const
		{
			assert(fScalar != 0.0);

			float fInv = 1.0f / fScalar;

			return Vector3(
				x * fInv,
				y * fInv,
				z * fInv);
		}

		// arithmetic updates
		inline Vector3& operator += (const Vector3& rkVector)
		{
			x += rkVector.x;
			y += rkVector.y;
			z += rkVector.z;

			return *this;
		}

		inline void makeFloor(const Vector3& cmp)
		{
			if (cmp.x < x) x = cmp.x;
			if (cmp.y < y) y = cmp.y;
			if (cmp.z < z) z = cmp.z;
		}

		inline void makeCeil(const Vector3& cmp)
		{
			if (cmp.x > x) x = cmp.x;
			if (cmp.y > y) y = cmp.y;
			if (cmp.z > z) z = cmp.z;
		}


		static const Vector3 ZERO;
		static const Vector3 UNIT_X;
		static const Vector3 UNIT_Y;
		static const Vector3 UNIT_Z;
		static const Vector3 NEGATIVE_UNIT_X;
		static const Vector3 NEGATIVE_UNIT_Y;
		static const Vector3 NEGATIVE_UNIT_Z;
		static const Vector3 UNIT_SCALE;

	
	};

	class AABB
	{
	public:
		enum Type
		{
			T_VALID,
			T_INVALID,
		};

	public:
		Vector3 getSize(void) const
		{
			switch (mType)
			{
				case T_INVALID:
					return Vector3::ZERO;

				case T_VALID:
					return (mMax - mMin);
				default: // shut up compiler
					assert(false && "Never reached");
					return Vector3::ZERO;
			}
		}

		inline const Vector3& getMin()const
		{
			return mMin;
		}

		inline const Vector3& getMax()const
		{
			return mMax;
		}

		inline bool isValid()const
		{
			return mType == T_VALID;
		}

		inline void setExtents(const Vector3& min, const Vector3& max)
		{
			assert((min.x <= max.x && min.y <= max.y && min.z <= max.z) &&
				   "The minimum corner of the box must be less than or equal to maximum corner");

			mType = T_VALID;
			mMin = min;
			mMax = max;
		}

		inline void merge(const Vector3& point)
		{
			if (isValid())
			{
				mMax.makeCeil(point);
				mMin.makeFloor(point);
			}
			else
			{
				setExtents(point, point);
			}
		}

	private:
		Vector3 mMin;
		Vector3 mMax;
		Type mType = T_INVALID;
	};
}

#endif