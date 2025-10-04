#pragma once
#include "Craig_Constants.hpp"


namespace Craig {

	class ResourceManager {

	public:
		CraigError init();
		CraigError terminate();

		//===============================================================================
		// Singleton Implementations
		static ResourceManager& getInstance()
		{
			static ResourceManager instance; // Guaranteed to be destroyed.
			return instance;
		}
		// Make deleted functions public for nicer error messages (~ Scott Myers)
		ResourceManager(ResourceManager const&) = delete;	// Copy constructor
		void operator=(ResourceManager const&) = delete;	// Assignment Operator
		//===============================================================================
	private:


		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		ResourceManager() {}										// Default Constructor private so can only be called from within
		//===============================================================================
	};



}