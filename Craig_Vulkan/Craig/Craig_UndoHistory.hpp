#pragma once
#include "Craig_Constants.hpp"
#include "../External/json.hpp"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace Craig {

	//Forward declarations
	class Renderer;
	class SceneManager;

	// ctrl z / ctrl y for the editor
	// whenever you let go of a widget it diffs every game object's json against last time
	// anything different becomes one undo step
	class UndoHistory {

	public:
		void init(Craig::Renderer* pRenderer, Craig::SceneManager* pSceneManager);
		// once a frame, after all the editor widgets
		void update(bool isEditing);

		// false if there was nothing to do
		bool undo();
		bool redo();
		bool canUndo() const { return !m_undoStack.empty(); }
		bool canRedo() const { return !m_redoStack.empty(); }

		// for edits that aren't a widget (keyboard shortcuts), scans like you just let go of one
		void scanSoon() { m_framesToScan = 2; }

		// bins the history, the next update takes a fresh baseline
		void clear();

	private:
		// null json = the object didn't exist on that side
		struct ObjectChange {
			std::string name;
			nlohmann::json before;
			nlohmann::json after;
		};
		using Entry = std::vector<ObjectChange>;
		// name -> that object's json, names are unique so they work as keys
		using SceneState = std::map<std::string, nlohmann::json>;

		SceneState captureState() const;
		void scan();
		void applyEntry(const Entry& entry, bool useBefore);
		static std::string describe(const Entry& entry);

		std::deque<Entry> m_undoStack; // deque so the oldest can fall off the front
		std::vector<Entry> m_redoStack;

		SceneState m_baseline;
		bool m_hasBaseline = false;
		std::string m_scenePath; // whose history this is, a different scene means start over

		bool m_wasBusy = false;
		int m_framesToScan = 0; // some edits land a frame late (the model browser), so scan twice

		Craig::Renderer* mp_renderer = nullptr;
		Craig::SceneManager* mp_sceneManager = nullptr;
	};



}
