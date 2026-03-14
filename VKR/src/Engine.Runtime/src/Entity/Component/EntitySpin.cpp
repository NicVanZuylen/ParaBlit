#include "EntitySpin.h"
#include "Engine.Math/Scalar.h"
#include "Entity/Component/Transform.h"

#include "Input.h"
#include "TimeMain.h"

namespace Eng
{
	void EntitySpin::OnInitialize()
	{
		m_initialPosition = m_host->GetComponent<Transform>()->GetPosition();
	}

	void EntitySpin::OnSimUpdate()
	{
		Transform* transform = m_host->GetComponent<Transform>();

		float dtf(TimeMain::SimDeltaTime);
		if (m_translate)
		{
			m_sinX += dtf;
			transform->SetPosition(m_initialPosition + Vector3f(0.0f, 0.0f, Math::Sin(m_sinX) * 20.0f));
		}

		if (m_scale)
		{
			transform->SetScale(Vector3f(Math::Abs(Math::Sin(m_sinX))));
		}

		if (m_spin)
		{
			transform->RotateEulerY(Math::ToRadians(90.0f) * dtf);
		}
	}
}