#include <gtest/gtest.h>
#include "core/Math.h"

using namespace dalia;
using namespace dalia::math;

TEST(MathTest, DecibelAndGainConversion) {
    // Known exact conversions
    EXPECT_NEAR(DbToGain(0.0f), 1.0f, 0.001f);
    EXPECT_NEAR(DbToGain(-6.0f), 0.501187f, 0.001f); // ~Half volume
    EXPECT_NEAR(DbToGain(-20.0f), 0.1f, 0.001f);

    EXPECT_NEAR(GainToDb(1.0f), 0.0f, 0.001f);
    EXPECT_NEAR(GainToDb(0.1f), -20.0f, 0.001f);

    // Boundaries
    EXPECT_EQ(DbToGain(VOLUME_DB_MIN - 10.0f), GAIN_MIN);
    EXPECT_EQ(DbToGain(VOLUME_DB_MAX + 10.0f), GAIN_MAX);
    EXPECT_EQ(GainToDb(GAIN_MIN - 1.0f), VOLUME_DB_MIN);
    EXPECT_EQ(GainToDb(GAIN_MAX + 1.0f), VOLUME_DB_MAX);
}

TEST(MathTest, InverseSquareRootSSE) {
    // 1 / sqrt(4) = 0.5
    EXPECT_NEAR(CalculateInvSqrt(4.0f), 0.5f, 0.001f);
    // 1 / sqrt(16) = 0.25
    EXPECT_NEAR(CalculateInvSqrt(16.0f), 0.25f, 0.001f);

    // Safety check (less than EPSILON returns 0 to prevent div by zero)
    EXPECT_EQ(CalculateInvSqrt(0.0f), 0.0f);
}

TEST(MathTest, Vector3BasicOperations) {
    Vector3 v1(1.0f, 2.0f, 3.0f);
    Vector3 v2(4.0f, 5.0f, 6.0f);

    Vector3 add = v1 + v2;
    EXPECT_FLOAT_EQ(add.x, 5.0f);
    EXPECT_FLOAT_EQ(add.y, 7.0f);
    EXPECT_FLOAT_EQ(add.z, 9.0f);

    Vector3 sub = v2 - v1;
    EXPECT_FLOAT_EQ(sub.x, 3.0f);
    EXPECT_FLOAT_EQ(sub.y, 3.0f);
    EXPECT_FLOAT_EQ(sub.z, 3.0f);

    Vector3 mult = v1 * 2.0f;
    EXPECT_FLOAT_EQ(mult.x, 2.0f);
    EXPECT_FLOAT_EQ(mult.y, 4.0f);
    EXPECT_FLOAT_EQ(mult.z, 6.0f);
}

TEST(MathTest, Vector3DotAndCross) {
    Vector3 v1(1.0f, 0.0f, 0.0f); // X Axis
    Vector3 v2(0.0f, 1.0f, 0.0f); // Y Axis

    // Perpendicular vectors have a dot product of 0
    EXPECT_FLOAT_EQ(v1.Dot(v2), 0.0f);

    // Cross product of X and Y is Z (in a right-handed system)
    Vector3 cross = v1.Cross(v2);
    EXPECT_FLOAT_EQ(cross.x, 0.0f);
    EXPECT_FLOAT_EQ(cross.y, 0.0f);
    EXPECT_FLOAT_EQ(cross.z, 1.0f);
}

TEST(MathTest, Vector3LengthAndDistance) {
    Vector3 v(0.0f, 3.0f, 4.0f); // 3-4-5 triangle

    // Length utilizes the SSE InvSqrt, so we use EXPECT_NEAR
    EXPECT_NEAR(v.Length(), 5.0f, 0.001f);

    Vector3 p1(1.0f, 1.0f, 1.0f);
    Vector3 p2(1.0f, 4.0f, 5.0f);
    EXPECT_NEAR(p1.DistanceTo(p2), 5.0f, 0.001f);
}

TEST(MathTest, Vector3Normalization) {
    Vector3 v(10.0f, 0.0f, 0.0f);
    v.Normalize();

    EXPECT_NEAR(v.x, 1.0f, 0.001f);
    EXPECT_NEAR(v.y, 0.0f, 0.001f);
    EXPECT_NEAR(v.z, 0.0f, 0.001f);
    EXPECT_NEAR(v.Length(), 1.0f, 0.001f);

    // Normalizing a zero vector shouldn't produce NaNs
    Vector3 zero(0.0f, 0.0f, 0.0f);
    zero.Normalize();
    EXPECT_FLOAT_EQ(zero.x, 0.0f);
    EXPECT_FLOAT_EQ(zero.y, 0.0f);
    EXPECT_FLOAT_EQ(zero.z, 0.0f);
}

TEST(MathTest, AngleToVector3) {
    // 0 degrees should point straight down the Z axis
    Vector3 v0 = AngleToVector3(0.0f);
    EXPECT_NEAR(v0.x, 0.0f, 0.001f);
    EXPECT_NEAR(v0.z, 1.0f, 0.001f);

    // 90 degrees should point down the X axis
    Vector3 v90 = AngleToVector3(90.0f);
    EXPECT_NEAR(v90.x, 1.0f, 0.001f);
    EXPECT_NEAR(v90.z, 0.0f, 0.001f);
}
