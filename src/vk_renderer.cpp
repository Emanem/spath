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
	struct rgba {
		float	r,
			g,
			b,
			a;
	};

	static_assert(sizeof(rgba) == 16, "struct rgba is not aligned to 4 bytes");

	struct vk_r : public basic_renderer {
		std::string				desc;
		vk::Instance 				instance;
		vk::PhysicalDeviceMemoryProperties	memprops;
		vk::Device 				device;
		vk::Queue				queue;
		vk::DescriptorSetLayout			descsetlayout;
		vk::DescriptorPool			descpool;
		vk::DescriptorSet			descset;
		vk::ShaderModule			computeshader;
		vk::PipelineLayout			pipelinelayout;
		vk::Pipeline				pipeline;
		vk::CommandPool				commandpool;

		struct buf_holder {
			vk::Device&					dev_;
			const vk::PhysicalDeviceMemoryProperties&	mprops_;
			size_t						sz_;
			vk::Buffer					buf_;
			vk::DeviceMemory				bufmem_;

			uint32_t find_memory_type(uint32_t mbits, const vk::MemoryPropertyFlags props) {
				for(uint32_t i = 0; i < mprops_.memoryTypeCount; ++i) {
					if ((mbits & (1 << i)) && ((mprops_.memoryTypes[i].propertyFlags & props) == props))
						return i;
				}
				throw std::runtime_error("Can't find required memory type");
			}

			void cleanup(void) {
				dev_.freeMemory(bufmem_);
				dev_.destroyBuffer(buf_);
			}

			void resize(const size_t sz) {
				if(sz > sz_) {
					if(sz_)
						cleanup();
					buf_ = dev_.createBuffer({{}, sz, vk::BufferUsageFlagBits::eStorageBuffer});
					auto memReq = dev_.getBufferMemoryRequirements(buf_);
					vk::MemoryAllocateInfo	mai;
					mai.allocationSize = memReq.size;
					mai.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
					bufmem_ = dev_.allocateMemory(mai);
					dev_.bindBufferMemory(buf_, bufmem_, 0);
					sz_ = sz;
				}
			}

			buf_holder(vk::Device& d, const vk::PhysicalDeviceMemoryProperties& p) : dev_(d), mprops_(p), sz_(0) {
			}

			~buf_holder() {
				cleanup();
			}
		};

		// buffers
		std::unique_ptr<buf_holder>	outbuf;

		void init_descset(void) {
			// first layout
			vk::DescriptorSetLayoutBinding	dslb[1];
			dslb[0].binding = 0;
			dslb[0].descriptorType = vk::DescriptorType::eStorageBuffer;
			dslb[0].descriptorCount = 1;
			dslb[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

			descsetlayout = device.createDescriptorSetLayout({{}, sizeof(dslb)/sizeof(dslb[0]), dslb });

			// then pool
			vk::DescriptorPoolSize	dps;
			dps.descriptorCount = 1;

			descpool = device.createDescriptorPool({{}, 1 /*max set*/, 1, &dps});

			// then get the desc set
			// please note this returns a vector of vk::DescriptionSet
			// hence only fecth element 0
			vk::DescriptorSetAllocateInfo	dsai(descpool, 1, &descsetlayout);
			descset = device.allocateDescriptorSets(dsai)[0];
		}

		void init_pipeline(void) {
			// load the shader
			std::vector<char>	spv_bin;
			{
				std::ifstream	istr("comp.spv", std::ios_base::binary);
				if(!istr)
					throw std::runtime_error("Can't load 'comp.spv'!");
				const auto	spv_sz = istr.seekg(0, std::ios_base::end).tellg();
				if(spv_sz <= 0)
					throw std::runtime_error("Invalid 'comp.spv' file, size 0 or less!");
				if(spv_sz%4)
					throw std::runtime_error("Invalid 'comp.spv' file, size is not a multiple of 4 bytes!");
				istr.seekg(0, std::ios_base::beg);
				spv_bin.resize(spv_sz);
				if(istr.read(&spv_bin[0], spv_sz).gcount() != spv_sz)
					throw std::runtime_error("Couldn't read whole 'comp.spv'!");
			}
			// compute shader
			vk::ShaderModuleCreateInfo	smci;
			smci.codeSize = spv_bin.size();
			smci.pCode = (const uint32_t*)&spv_bin[0];
			computeshader = device.createShaderModule(smci);
			// pipeline layout
			vk::PipelineLayoutCreateInfo		plci;
			plci.setLayoutCount = 1;
			plci.pSetLayouts = &descsetlayout;
			pipelinelayout = device.createPipelineLayout(plci);
			// pipeline creation
			vk::PipelineShaderStageCreateInfo	ssci;
			ssci.stage = vk::ShaderStageFlagBits::eCompute;
			ssci.module = computeshader;
			ssci.pName = "main";
			vk::ComputePipelineCreateInfo	pci;
			pci.stage = ssci;
			pci.layout = pipelinelayout;
			const auto res = device.createComputePipelines(nullptr, 1, &pci, nullptr, &pipeline);
			if(res != vk::Result::eSuccess)
				throw std::runtime_error("Can't create computet pipeline!");
		}

		void update_bufs(const int x, const int y) {
			outbuf->resize(x*y*sizeof(rgba));
			// update the desc set
			vk::DescriptorBufferInfo	dbi[1];
			dbi[0].buffer = outbuf->buf_;
			dbi[0].offset = 0;
			dbi[0].range = outbuf->sz_;

			vk::WriteDescriptorSet	wds;
			wds.dstSet = descset;
			wds.dstBinding = 0;
			wds.descriptorCount = sizeof(dbi)/sizeof(dbi[0]);
			wds.descriptorType = vk::DescriptorType::eStorageBuffer;
			wds.pBufferInfo = dbi;

			device.updateDescriptorSets(1, &wds, 0, nullptr);
		}

		void update(const int x, const int y) {
			// update all buffers and 
			update_bufs(x, y);

			// this gives a vector of command buffers
			auto cbuf = device.allocateCommandBuffers({commandpool, vk::CommandBufferLevel::ePrimary, 1});

			cbuf[0].begin({{}, nullptr});
			cbuf[0].bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
			cbuf[0].bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelinelayout, 0, 1, &descset, 0, nullptr);
			cbuf[0].dispatch(x*y, 1, 1);
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
			auto& physDev = physicalDevices[0];

			// set desc
			desc = std::string("Vulkan compute renderer on [") + physDev.getProperties().deviceName + ']';

			// get the queue families props
			auto qfProps = physDev.getQueueFamilyProperties();
			// find the first with compute capabilities
			int qIdx = -1;
			for(const auto& qf : qfProps) {
				if((qf.queueFlags & vk::QueueFlagBits::eCompute) == vk::QueueFlagBits::eCompute) {
					qIdx = &qf - &qfProps[0];
					break;
				}
			}
			if(-1 == qIdx)
				throw std::runtime_error("Can't find compute Vulkan queue");

			// get memory props
			memprops = physDev.getMemoryProperties();

			// create logical device, binding 1 queue
			// of the logical family found above
			vk::DeviceQueueCreateInfo 	dqci({}, qIdx, 1, nullptr);
			vk::DeviceCreateInfo		dci({}, 1, &dqci); 	
			device = physDev.createDevice(dci);

			// after we got the logical device
			// get the queue!
			queue = device.getQueue(qIdx, 0);

			// then setup the command pool for this queue family
			commandpool = device.createCommandPool({{}, qIdx});

			// initialize once
			init_descset();
			init_pipeline();

			// setup all the buffers
			outbuf = std::unique_ptr<buf_holder>(new buf_holder(device, memprops));

			// update the buffers
			update(x, y);
		}

		~vk_r() {
			device.destroyShaderModule(computeshader);
			device.destroyDescriptorPool(descpool);
			device.destroyDescriptorSetLayout(descsetlayout);
			device.destroy();
			instance.destroy();
		}

		void get_output(scene::bitmap& out) {
			auto mapmem = device.mapMemory(outbuf->bufmem_, 0, outbuf->sz_);
			const size_t	n_els = outbuf->sz_/sizeof(rgba);
			const rgba*	m_buf = (const rgba*)mapmem;
			out.values.resize(n_els);
			for(size_t i = 0; i < n_els; ++i) {
				out.values[i].r = 255.0*m_buf[i].r;
				out.values[i].g = 255.0*m_buf[i].g;
				out.values[i].b = 255.0*m_buf[i].b;
				out.values[i].a = 255;
			}
			device.unmapMemory(outbuf->bufmem_);
		}

		virtual const char* get_description(void) const {
			return desc.c_str();
		}

		virtual void render_flat(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			const auto	s_time = std::chrono::high_resolution_clock::now();
			update(vp.res_x, vp.res_y);
			const auto	e_time = std::chrono::high_resolution_clock::now();
			std::printf("Done (%.1fs)\n", std::chrono::duration_cast<std::chrono::milliseconds>(e_time - s_time).count()/1000.0);
			get_output(out);
		}

		virtual void render(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			const auto	s_time = std::chrono::high_resolution_clock::now();
			update(vp.res_x, vp.res_y);
			const auto	e_time = std::chrono::high_resolution_clock::now();
			std::printf("Done (%.1fs)\n", std::chrono::duration_cast<std::chrono::milliseconds>(e_time - s_time).count()/1000.0);
			get_output(out);
		}

	};
}


namespace vk_renderer {
	scene::renderer* get(const int w, const int h) {
		return new vk_r(w, h);
	}
}

