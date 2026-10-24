#pragma once

#include "Craig/Craig_Game.hpp"

namespace Game {

	class SoundDemo : public Craig::Game {
	public:
		CraigError init(Craig::GameServices* pServices) override;
		CraigError terminate() override;
		void onPlayStarted(bool startedFromEditor) override;
		void onPlayStopped() override;
		CraigError update(const float& deltaTime) override;
		bool isWorldRunning() const override { return true; }
		void drawImGui() override;

	private:
		Craig::GameServices* mp_services = nullptr;
		bool m_animate = true;
		bool m_probeReady = false;
		float m_time = 0.0f;
	};

}
