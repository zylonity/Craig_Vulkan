#pragma once

//idk why but it doesn't work unless defined right here.
#define VMA_IMPLEMENTATION
#include "Craig_Constants.hpp"

namespace Craig {
	
	//Forward declarations
	class Window;
	class Renderer;


	class Framework {

	public:
		CraigError init();
		CraigError update();
		CraigError terminate();
	private:

		Craig::Window* mp_Window			= nullptr;
		Craig::Renderer* mp_Renderer		= nullptr;


	};



}