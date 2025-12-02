#pragma once
#include "Craig_Constants.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace Craig {

	//Forward declarations
	class Renderer;
	class Camera;

	class ImguiEditor {

	public:
		CraigError editorMain(const float& deltaTime);
		CraigError editorInit();
		CraigError terminate();

		void setRenderer(Craig::Renderer* pRenderer) { mp_renderer = pRenderer; };
		void setCamera(Craig::Camera* pCamera) { mp_camera = pCamera; };

		//===============================================================================
		// Singleton Implementations
		static ImguiEditor& getInstance()
		{
			static ImguiEditor instance; // Guaranteed to be destroyed.
			return instance;
		}
		// Make deleted functions public for nicer error messages (~ Scott Myers)
		ImguiEditor(ImguiEditor const&) = delete;	// Copy constructor
		void operator=(ImguiEditor const&) = delete;	// Assignment Operator
		//===============================================================================
	private:
		bool m_initialised = false;

		void showRenderProperties(const float& deltaTime);
		bool m_ShowRendererProperties = false;

		Craig::Renderer* mp_renderer;
		Craig::Camera* mp_camera;

		int m_currentMipLevel = 0;
		int m_currentMSAALevel = 0;
		int m_MSAADropdownIndex = 0;

		std::vector<const char*> mv_MSAADropdownOptions = { "Off", "x2", "x4", "x8", "x16", "x32", "x64" };
		std::unordered_map<int, int> m_MSAAEquivalents = { {0, 1}, {1, 2}, {2, 4}, {3, 8}, {4, 16}, {5, 32}, {6, 64} };
		std::unordered_map<int, int> m_MSAAIndexes = { {1, 0}, {2, 1}, {4, 2}, {8, 3}, {16, 4}, {32, 5}, {64, 6} };

		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		ImguiEditor() {}										// Default Constructor private so can only be called from within
		//===============================================================================
	};



}