#pragma once
#include "Craig/Craig_Constants.hpp"


namespace Craig {

	namespace Components
	{
		class Sun {

		public:
			CraigError init();
			CraigError update();
			CraigError terminate();
		private:


		};
	}

}