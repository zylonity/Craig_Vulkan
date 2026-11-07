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
		enum class ReferenceCase { MovingDoor, Clear, Half, Blocked, Reflection };
		CraigError selectReferenceCase(ReferenceCase referenceCase);
		Craig::GameServices* mp_services = nullptr;
		ReferenceCase m_referenceCase = ReferenceCase::MovingDoor;
		bool m_referenceFailed = false;
		bool m_animate = true;
		bool m_probeReady = false;
		float m_time = 0.0f;
		float m_reflectorDistance = 15.0f; //Metres from the listener/source line to the wall's face
		int m_sound = 0;
	};

}
