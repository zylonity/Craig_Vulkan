#pragma once
#include "StateMachine/Craig_StateMachine.hpp"
#include "../External/json.hpp"

#include <string>

// careful what goes in here, Craig_Framework.hpp includes it and that defines VMA_IMPLEMENTATION
// pull the renderer in from here and main.cpp gets a second copy of VMA and the linker loses its shit

namespace Craig {

	//Forward declarations
	class Renderer;
	class SceneManager;
	class Game;

	// new mode = one here + a row in buildEngineModes()
	enum class EngineModeId {
		Edit,		// nothing ticks, you edit the scene
		Play,		// everything runs
		Pause,		// frozen mid play, look but don't touch
		Step,		// one frame of play then straight back to Pause
		Simulate,	// physics runs but gameplay doesn't, for watching things settle
	};

	// what each mode lets run
	// the framework only asks these, never which mode it is
	struct EngineModeFlags {
		bool stepPhysics = false;
		bool runGameplay = false;
		float timeScale = 1.0f;
		bool singleTick = false; // delta time becomes exactly one physics step
	};

	// everything the modes get to touch
	struct EngineContext {
		Craig::Renderer* pRenderer = nullptr;
		Craig::SceneManager* pSceneManager = nullptr;
		Craig::Game* pGame = nullptr; // can be null, the engine runs fine without a game
		bool gameSessionActive = false; // between the game's onPlayStarted and onPlayStopped
	};

	class EngineMode : public State<EngineContext, EngineModeId> {

	public:
		EngineMode(const char* name, EngineModeFlags flags) : mp_name(name), m_flags(flags) {}

		const char* getName() const override { return mp_name; }
		const EngineModeFlags& getFlags() const { return m_flags; }

		// starts the game's session if this mode runs gameplay
		void onEnter(EngineContext& context, const State* pFrom) override;
		// only does anything on shutdown, ends the game's session
		void onExit(EngineContext& context, const State* pTo) override;

	private:
		const char* mp_name;
		EngineModeFlags m_flags;
	};

	// snapshots the scene on the way out and puts it back on the way in, so Stop undoes play
	class EditMode : public EngineMode {

	public:
		EditMode();

		void onEnter(EngineContext& context, const State* pFrom) override;
		void onExit(EngineContext& context, const State* pTo) override;

	private:
		nlohmann::json m_sceneSnapshot;
		std::string m_snapshotScenePath; // the game might load other scenes, this is the one to come back to
		bool m_hasSnapshot = false;
	};

	// one frame then back to Pause
	class StepMode : public EngineMode {

	public:
		StepMode();

		void update(EngineContext& context, const float& deltaTime) override;
	};

	using EngineModeMachine = StateMachine<EngineContext, EngineModeId>;

	// adds every mode and which ones can go where
	void buildEngineModes(EngineModeMachine& machine);

	const EngineModeFlags& getCurrentModeFlags(const EngineModeMachine& machine);

}
