#pragma once
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"

namespace Craig {
	class Device;

	class SyncManager {
	public:
		struct SyncManagerInitInfo
		{


		};

		CraigError init(const SyncManagerInitInfo& info);
		CraigError update();
		CraigError terminate();

	private:


	};



}