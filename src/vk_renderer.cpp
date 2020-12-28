/*
    This file is part of spath.

    spath is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    spath is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with spath.  If not, see <https://www.gnu.org/licenses/>.
 * */

#include "basic_renderer.h"
#include "vk_renderer.h"
#include <vulkan/vulkan.hpp>
#include <fstream>
#include <iostream>
#include <chrono>
#include <memory>

namespace {
	struct vk_r : public basic_renderer {
		std::string	desc;
		vk::Instance 	instance;
		vk::Device 	device;
		vk::Queue	queue;
		vk::CommandPool	commandpool;

		void init(const int x, const int y) {
			// this gives a vector of command buffers
			auto cbuf = device.allocateCommandBuffers({commandpool, vk::CommandBufferLevel::ePrimary, 1});

			cbuf[0].begin({{}, nullptr});
			cbuf[0].end();

			vk::SubmitInfo si(0, nullptr, nullptr, 1, &cbuf[0], 0, nullptr);

			queue.submit(1, &si, vk::Fence());

			queue.waitIdle();

			device.freeCommandBuffers(commandpool, cbuf);
		}

	// interface here
	// if this was a class, the following would be public
	// methods
		vk_r(const int x, const int y) : basic_renderer(x, y) {
			// Vulkan instance
			vk::ApplicationInfo appInfo;
			appInfo.pApplicationName = "spath";
			appInfo.pEngineName = "spath_vk";
			appInfo.apiVersion = VK_API_VERSION_1_0;

			vk::InstanceCreateInfo instanceCreateInfo;
			instanceCreateInfo.pApplicationInfo = &appInfo;
			instance = vk::createInstance(instanceCreateInfo);

			// always use the first physical device
			auto physicalDevices = instance.enumeratePhysicalDevices();
			auto& physicalDevice = physicalDevices[0];

			// set desc
			desc = std::string("Vulkan renderer on [") + physicalDevice.getProperties().deviceName + ']';

			// get the queue families props
			auto qfProps = physicalDevice.getQueueFamilyProperties();
			// find the first with graphical capabilities
			int qIdx = -1;
			for(const auto& qf : qfProps) {
				if(qf.queueFlags & vk::QueueFlagBits::eGraphics) {
					qIdx = &qf - &qfProps[0];
					break;
				}
			}
			if(-1 == qIdx)
				throw std::runtime_error("Can't find graphics Vulkan queue");

			// create logical device, binding 1 queue
			// of the logical family found above
			vk::DeviceQueueCreateInfo 	dqci({}, qIdx, 1, nullptr);
			vk::DeviceCreateInfo		dci({}, 1, &dqci); 	
			device = physicalDevice.createDevice(dci);

			// after we got the logical device
			// get the queue!
			queue = device.getQueue(qIdx, 0);

			// then setup the command pool for this queue family
			commandpool = device.createCommandPool({{}, qIdx});
		}

		~vk_r() {
			device.destroy();
			instance.destroy();
		}

		virtual const char* get_description(void) const {
			return desc.c_str();
		}

		virtual void render_flat(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			init(vp.res_x, vp.res_y);
		}

		virtual void render(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			init(vp.res_x, vp.res_y);
		}

	};
}


namespace vk_renderer {
	scene::renderer* get(const int w, const int h) {
		return new vk_r(w, h);
	}
}

