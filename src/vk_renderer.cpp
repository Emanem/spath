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

namespace vk_data {
	typedef float cl_real;

	struct vec4 {
		cl_real	x,
			y,
			z,
			__padding;

		vec4& operator=(const geom::vec3& rhs) {
			x = rhs.x;
			y = rhs.y;
			z = rhs.z;
			return *this;
		}
	};

	static_assert(sizeof(vec4) == 16, "struct vec4 is not aligned to 16 bytes");

	struct ray {
		vec4	pos,
			dir;

		ray& operator=(const geom::ray& rhs) {
			pos = rhs.pos;
			dir = rhs.dir;
			return *this;
		}
	};

	static_assert(!(sizeof(ray)%16), "struct ray is not aligned to 16 bytes");

	struct triangle {
		vec4	v0,
			v1,
			v2,
			n;

		triangle& operator=(const geom::triangle& rhs) {
			v0 = rhs.v0;
			v1 = rhs.v1;
			v2 = rhs.v2;
			n = rhs.n;
			return *this;
		}
	};

	static_assert(!(sizeof(triangle)%16), "struct triangle is not aligned to 16 bytes");

	struct material {
		vec4	reflectance_color,
			emittance_color;

		material& operator=(const scene::material& rhs) {
			reflectance_color = rhs.reflectance_color;
			emittance_color = rhs.emittance_color;
			return *this;
		}
	};

	static_assert(!(sizeof(material)%16), "struct material is not aligned to 16 bytes");

	struct rgba {
		float	r,
			g,
			b,
			a;
	};

	static_assert(sizeof(rgba) == 16, "struct rgba is not aligned to 16 bytes");
}

namespace {
	uint32_t ceil(const uint32_t a, const uint32_t b) {
		return (a > b) ? a : b;
	}

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
		vk::CommandBuffer			commandbuffer;

		template<typename T>
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
					const size_t	r_size = sizeof(T)*sz;
					buf_ = dev_.createBuffer({{}, r_size, vk::BufferUsageFlagBits::eStorageBuffer});
					auto memReq = dev_.getBufferMemoryRequirements(buf_);
					vk::MemoryAllocateInfo	mai;
					mai.allocationSize = memReq.size;
					mai.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
					bufmem_ = dev_.allocateMemory(mai);
					dev_.bindBufferMemory(buf_, bufmem_, 0);
				}
				sz_ = sz;
			}

			buf_holder(vk::Device& d, const vk::PhysicalDeviceMemoryProperties& p) : dev_(d), mprops_(p), sz_(0) {
			}

			~buf_holder() {
				cleanup();
			}

			template<typename Fn>
			void read(Fn&& f) {
				const T* m_buf = (const T*)dev_.mapMemory(bufmem_, 0, sz_*sizeof(T));
				for(size_t i = 0; i < sz_; ++i) {
					f(i, m_buf[i]);
				}
				dev_.unmapMemory(bufmem_);
			}

			template<typename Fn>
			void write(Fn&& f) {
				T* m_buf = (T*)dev_.mapMemory(bufmem_, 0, sz_*sizeof(T));
				for(size_t i = 0; i < sz_; ++i) {
					f(i, &m_buf[i]);
				}
				dev_.unmapMemory(bufmem_);
			}
		};

		// input
		std::unique_ptr<buf_holder<vk_data::ray>>	raybuf;
		std::unique_ptr<buf_holder<vk_data::triangle>>	tribuf;
		std::unique_ptr<buf_holder<vk_data::material>>	matbuf;

		// buffers
		std::unique_ptr<buf_holder<vk_data::rgba>>	outbuf;

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

		void update_bufs(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples) {
			outbuf->resize(vp.res_x*vp.res_y);
			// update the desc set
			vk::DescriptorBufferInfo	dbi[1];
			dbi[0].buffer = outbuf->buf_;
			dbi[0].offset = 0;
			dbi[0].range = outbuf->sz_*sizeof(vk_data::rgba);

			vk::WriteDescriptorSet	wds;
			wds.dstSet = descset;
			wds.dstBinding = 0;
			wds.descriptorCount = sizeof(dbi)/sizeof(dbi[0]);
			wds.descriptorType = vk::DescriptorType::eStorageBuffer;
			wds.pBufferInfo = dbi;

			device.updateDescriptorSets(1, &wds, 0, nullptr);
		}

		void update(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples) {
			// update all buffers and 
			update_bufs(vp, tris, mats, n_tris, n_samples);

			commandbuffer.begin({{}, nullptr});
			commandbuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
			commandbuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelinelayout, 0, 1, &descset, 0, nullptr);
			commandbuffer.dispatch(ceil(vp.res_x*vp.res_y, 1), 1, 1);
			commandbuffer.end();

			vk::SubmitInfo si(0, nullptr, nullptr, 1, &commandbuffer, 0, nullptr);

			queue.submit(1, &si, vk::Fence());

			queue.waitIdle();

			commandbuffer.reset({});
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

			// this gives a vector of command buffers
			commandbuffer = device.allocateCommandBuffers({commandpool, vk::CommandBufferLevel::ePrimary, 1})[0];

			// initialize once
			init_descset();
			init_pipeline();

			// setup all the buffers
			raybuf = std::unique_ptr<buf_holder<vk_data::ray>>(new buf_holder<vk_data::ray>(device, memprops));
			tribuf = std::unique_ptr<buf_holder<vk_data::triangle>>(new buf_holder<vk_data::triangle>(device, memprops));
			matbuf = std::unique_ptr<buf_holder<vk_data::material>>(new buf_holder<vk_data::material>(device, memprops));
			outbuf = std::unique_ptr<buf_holder<vk_data::rgba>>(new buf_holder<vk_data::rgba>(device, memprops));
		}

		~vk_r() {
			device.destroyShaderModule(computeshader);
			device.destroyDescriptorPool(descpool);
			device.destroyDescriptorSetLayout(descsetlayout);
			device.freeCommandBuffers(commandpool, commandbuffer);
			device.destroyCommandPool(commandpool);
			device.destroy();
			instance.destroy();
		}

		void get_output(scene::bitmap& out) {
			out.values.resize(outbuf->sz_);
			outbuf->read([&out](const size_t i, const vk_data::rgba& v) {
				out.values[i].r = 255.0*v.r;
				out.values[i].g = 255.0*v.g;
				out.values[i].b = 255.0*v.b;
				out.values[i].a = 255;
			});
		}

		virtual const char* get_description(void) const {
			return desc.c_str();
		}

		virtual void render_flat(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			const auto	s_time = std::chrono::high_resolution_clock::now();
			update(vp, tris, mats, n_samples, n_samples);
			const auto	e_time = std::chrono::high_resolution_clock::now();
			std::printf("Done (%.1fs)\n", std::chrono::duration_cast<std::chrono::milliseconds>(e_time - s_time).count()/1000.0);
			get_output(out);
		}

		virtual void render(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			const auto	s_time = std::chrono::high_resolution_clock::now();
			update(vp, tris, mats, n_samples, n_samples);
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

