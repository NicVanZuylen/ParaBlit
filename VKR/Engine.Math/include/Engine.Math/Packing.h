#pragma once
#include "Engine.Math/MathLib.h"
#include "Engine.Math/Vectors.h"

namespace Eng::Math
{
	uint32_t PackHalf2x16(const Vector2f& vec)
	{
		return glm::packHalf2x16((const Vector2f::GLMT&)vec);
	}
}