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
#include "cl_renderer.h"
#include <CL/cl.hpp>
#include <fstream>
#include <iostream>
#include <chrono>
#include <memory>

namespace cl_data {
	// change this from cl_float --> cl_double
	// if you want to enable double precision
	typedef cl_float cl_real;

	struct vec3 {
		cl_real	x,
			y,
			z;

		vec3& operator=(const geom::vec3& rhs) {
			x = rhs.x;
			y = rhs.y;
			z = rhs.z;
			return *this;
		}
	};

	struct ray {
		vec3	pos,
			dir;

		ray& operator=(const geom::ray& rhs) {
			pos = rhs.pos;
			dir = rhs.dir;
			return *this;
		}
	};

	struct triangle {
		vec3	v0,
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

	struct material {
		vec3	reflectance_color,
			emittance_color;

		material& operator=(const scene::material& rhs) {
			reflectance_color = rhs.reflectance_color;
			emittance_color = rhs.emittance_color;
			return *this;
		}
	};

	struct RGBA {
		cl_uchar	r,
				g,
				b,
				a;
	};
}

namespace {
	// as per http://simpleopencl.blogspot.com/2013/06/tutorial-simple-start-with-opencl-and-c.html
	struct cl_r : public basic_renderer {
		// const declarations for OpenCL buffer access (less verbose)
		const static cl_mem_flags	WO = (CL_MEM_HOST_WRITE_ONLY|CL_MEM_READ_ONLY),
						RO = (CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY);
		// simple template to hold buffer and resize those
		// as and when needed
		template<typename T, cl_mem_flags Flags = WO>
		struct buf_holder {
			cl::Context&		ctx;
			cl::CommandQueue&	queue;
			cl::Buffer		buf;
			std::vector<T>		vec_data;

			buf_holder(cl::Context& c, cl::CommandQueue& q) : ctx(c), queue(q) {
			}

			void resize(const size_t num_el) {
				if(vec_data.size() < num_el) {
					vec_data.resize(num_el);
					buf = cl::Buffer(ctx, Flags, sizeof(T)*num_el);
				}
			}

			// write in the buffer - requires operator=
			// from --> to types
			template<typename U>
			void write(const U* pU, const size_t num_el, cl::Event& e) {
				resize(num_el);
				for(int i = 0; i < (int)num_el; ++i)
					vec_data[i] = pU[i];
				queue.enqueueWriteBuffer(buf, CL_FALSE, 0, sizeof(T)*num_el, &vec_data[0], 0, &e);
			}

			// read the OpenCL buffer content to an array
			// Requires the array to be already sized
			// correctly. Furthermore a lambda is needed
			// to copy from --> to types
			template<typename U, typename FnCopy>
			void read(U* pU, FnCopy&& fnCopy) {
				queue.enqueueReadBuffer(buf, CL_TRUE, 0, sizeof(T)*vec_data.size(), &vec_data[0]);
				for(int i = 0; i < (int)vec_data.size(); ++i)
					fnCopy(vec_data[i], pU[i]);
			}
		};

		std::string		desc;
		cl::Device		cl_dev;
		cl::Context		cl_ctx;
		cl::CommandQueue	cl_queue;
		cl::Program		cl_prog;
		// buf holders section
		std::unique_ptr<buf_holder<cl_data::ray> >	vp_buf_h;
		std::unique_ptr<buf_holder<cl_data::triangle> >	tris_buf_h;
		std::unique_ptr<buf_holder<cl_data::material> >	mats_buf_h;
		// output buffer
		std::unique_ptr<buf_holder<cl_data::RGBA, RO> >	out_buf_h;

		cl_r(const int x, const int y) : basic_renderer(x, y) {
			// initialize the OpenCL context
			// btw, we need to mess aroudn with strings because
			// Nvidia driver returns strings with 0x00 in it...
			std::vector<cl::Platform>	all_platforms;
			cl::Platform::get(&all_platforms);
			if(all_platforms.empty())
				throw std::runtime_error("Can't find OpenCL platforms");
			cl::Platform	cl_plat = all_platforms[0];
			desc = std::string("OpenCL (") + std::string(cl_plat.getInfo<CL_PLATFORM_NAME>()).c_str() + ')';
			// get the device
			std::vector<cl::Device>		all_devices;
			cl_plat.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
			if(all_devices.empty())
				throw std::runtime_error("Can't find OpenCL devices");
			cl_dev = all_devices[0];
			desc = desc + " [" + std::string(cl_dev.getInfo<CL_DEVICE_NAME>()).c_str() + ']';
			// create the context
			cl_ctx = cl::Context({cl_dev});
			// and the commands queue
			cl_queue = cl::CommandQueue(cl_ctx, cl_dev);
			// load up the sources
			std::string	src;
			{
				std::ifstream istr("src/render.cl");
				if(!istr)
					throw std::runtime_error("Can't open OpenCL kernel sources");
				istr.seekg(0, istr.end);
				const auto	len = istr.tellg();
				istr.seekg(0, istr.beg);
				src.resize(len);
				if(istr.read(&src[0], len).gcount() != len)
					throw std::runtime_error("Couldn't read whole kernel source file");
			}
			// compile and link
			cl::Program::Sources	s;
			s.push_back({src.c_str(), src.length()});
			cl_prog = cl::Program(cl_ctx, s);
			if(cl_prog.build({cl_dev}) != CL_SUCCESS) {
				throw std::runtime_error(std::string("Error building: ") + cl_prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl_dev));
			}
			// setup the buffers
			vp_buf_h = std::unique_ptr<buf_holder<cl_data::ray> >(new buf_holder<cl_data::ray>(cl_ctx, cl_queue));
			tris_buf_h = std::unique_ptr<buf_holder<cl_data::triangle> >(new buf_holder<cl_data::triangle>(cl_ctx, cl_queue));
			mats_buf_h = std::unique_ptr<buf_holder<cl_data::material> >(new buf_holder<cl_data::material>(cl_ctx, cl_queue));
			out_buf_h = std::unique_ptr<buf_holder<cl_data::RGBA, RO> >(new buf_holder<cl_data::RGBA, RO>(cl_ctx, cl_queue));
		}

		virtual const char* get_description(void) const {
			return desc.c_str();
		}

		virtual void render_core(const std::string& k_fn, const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			// first ensure that the bitmap is of correct size
			out.res_x = vp.res_x;
			out.res_y = vp.res_y;
			out.values.resize(out.res_x*out.res_y);
			// let's initialize the OpenCL buffers (non-blocking
			// with data conversion)
			// 0. create a std::vector of events for the buffers writes
			std::vector<cl::Event>	ev(3);
			// 1. vp rays
			vp_buf_h->write<geom::ray>(&vp.rays[0], vp.rays.size(), ev[0]);
			// 2. tris
			tris_buf_h->write<geom::triangle>(tris, n_tris, ev[1]),
			// 3. materials
			mats_buf_h->write<scene::material>(mats, n_tris, ev[2]);
			// 4. create the buffer for output (no need to fill it in)
			out_buf_h->resize(out.values.size());
			// run the kernel
			const auto	s_time = std::chrono::high_resolution_clock::now();
			cl::Kernel	k_render_flat(cl_prog, k_fn.c_str());
			k_render_flat.setArg(0, vp_buf_h->buf);
			k_render_flat.setArg(1, tris_buf_h->buf);
			k_render_flat.setArg(2, mats_buf_h->buf);
			k_render_flat.setArg(3, (cl_uint)n_tris);
			k_render_flat.setArg(4, (cl_uint)n_samples);
			k_render_flat.setArg(5, out_buf_h->buf);
			cl_queue.enqueueNDRangeKernel(k_render_flat, cl::NDRange(0), cl::NDRange(vp.rays.size()), cl::NullRange, &ev);
			cl_queue.finish();
			const auto	e_time = std::chrono::high_resolution_clock::now();
			std::printf("Done (%.1fs)\n", std::chrono::duration_cast<std::chrono::milliseconds>(e_time - s_time).count()/1000.0);
			// read back
			out_buf_h->read(&out.values[0],
				[](const cl_data::RGBA& in, scene::RGBA& out) -> void {
					out.r = in.r;
					out.g = in.g;
					out.b = in.b;
					out.a = in.a;
				}
			);
		}

		virtual void render_flat(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			render_core("render_flat", vp, tris, mats, n_tris, n_samples, out);
		}

		virtual void render(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			render_core("render", vp, tris, mats, n_tris, n_samples, out);
		}

	};
}

namespace cl_renderer {
	scene::renderer* get(const int w, const int h) {
		return new cl_r(w, h);
	}
}

