#pragma once

namespace Eng
{
	class TimeMain
	{
	public:

		static constexpr double SimUpdateMs = 1000.0 / 50.0;
		static constexpr double SimUpdateInterval = SimUpdateMs / 1000.0;
		static constexpr double SimDeltaTime = SimUpdateInterval;

		TimeMain() = default;
		~TimeMain() = default;

		inline double DeltaTimeMain() const { return m_deltaTimeMain; }
		inline double TotalElapsedTime() const { return m_totalElapsedTime; }
		inline double RenderStallTime() const { return m_renderStallTime; }

	private:

		friend class Application;

		double m_deltaTimeMain = 0.0;
		double m_simStallTime = 0.0;
		double m_totalElapsedTime = 0.0;
		double m_renderStallTime = 0.0;
	};

	inline TimeMain g_timeMain{};
}