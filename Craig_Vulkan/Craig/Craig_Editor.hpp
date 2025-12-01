#pragma once
#include "Craig_Constants.hpp"


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

		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		ImguiEditor() {}										// Default Constructor private so can only be called from within
		//===============================================================================
	};



}