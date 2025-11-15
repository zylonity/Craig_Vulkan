#pragma once
#include "Craig_Constants.hpp"


namespace Craig {

	class ImguiEditor {

	public:
		CraigError editorMain();
		CraigError editorInit();
		CraigError terminate();

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

		void showRenderProperties();
		bool m_ShowRendererProperties = false;

		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		ImguiEditor() {}										// Default Constructor private so can only be called from within
		//===============================================================================
	};



}