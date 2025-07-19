/*
 * Vulkan Windowed Program
 *
 * Copyright (C) 2016, 2018 Valve Corporation
 * Copyright (C) 2016, 2018 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Enable the WSI extensions
#if defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

// Tell SDL not to mess with main()
#define SDL_MAIN_HANDLED


#include <cassert>

#include "Craig/Craig_Framework.hpp"

//===============================================================================
// Framework is constructed as Global
Craig::Framework g_framework;
//===============================================================================

int main() {

    CraigError ret = CRAIG_SUCCESS;

	ret = g_framework.init();
	assert(ret == CRAIG_SUCCESS && "Framework failed to init");

    while (true) {
		ret = g_framework.update();
		assert((ret == CRAIG_SUCCESS || ret == CRAIG_CLOSED) && "Framework failed to update");

        //Exit loop if we're not ok
        if (ret != CRAIG_SUCCESS) {
            break;
		}
    }

    ret = g_framework.terminate();
    assert(ret == CRAIG_SUCCESS);

    return 0;


}