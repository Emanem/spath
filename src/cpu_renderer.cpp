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

#include "cpu_renderer.h"
#include "basic_renderer.h"
#include "frand.h"
#include <thread>
#include <atomic>
#include <cstdio>
#include <chrono>

namespace {
	const static real	MAX_VALUE_DIST = 1000000000000.0;

	template<typename Rf>
	geom::vec3 render_step(const geom::ray& r, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, Rf& rf, const int depth = 0) {
		using namespace geom;
		// just max 5 bounces for now...
		if(depth >= 5)
			return vec3();
		// try to hit something
		real	d = MAX_VALUE_DIST;
		ray	next_r;
		int	idx = -1;
		for(int i = 0; i < (int)n_tris; ++i) {
			geom::vec3	t_pos;
			const real	cur_d = geom::ray_intersect(r, tris[i], t_pos);
			if(cur_d > 0.0 && (cur_d < d)) {
				d = cur_d;
				next_r.pos = t_pos;
				idx = i;
			}
		}
		// if we haven't found an element
		if(idx < 0)
			return vec3();
		// get the next direction
		// make sure the normal is against the ray..
		vec3	adj_n = tris[idx].n;
		if(adj_n.dot(r.dir) > 0.0)
			adj_n *= -1.0;
		next_r.dir = rand_unit_vec(adj_n, rf);
		// probability of new ray
		const static real	p = 1.0/(PI*2.0);
		// BRDF
		const real	cos_theta = next_r.dir.dot(adj_n);
		const vec3	BRDF = mats[idx].reflectance_color * (1.0/PI);
		// recursive step
		const vec3	rec_color = render_step(next_r, tris, mats, n_tris, rf, depth+1);
		
		return mats[idx].emittance_color + (BRDF * rec_color * cos_theta * (1.0/p)); 
	}

	template<typename Rf>
	void render_core(const int idx, const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, Rf& rf, scene::bitmap& out) {
		out.values[idx] = scene::RGBA{0, 0, 0, 0};
		geom::vec3		accum;
		for(int j = 0; j < (int)n_samples; ++j) {
			accum += render_step(vp.rays[idx], tris, mats, n_tris, rf);
		}
		accum *= (1.0/n_samples);
		out.values[idx] = scene::vec3_RGBA(accum.clamp());
	}

	void render_test(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
		// first ensure that the bitmap is of correct size
		out.res_x = vp.res_x;
		out.res_y = vp.res_y;
		out.values.resize(out.res_x*out.res_y);
		// now, for each viewport point, do compute if we get a triangle
		// and if yes, write which one as a char
		for(int i = 0; i < (int)vp.rays.size(); ++i) {
			out.values[i] = scene::RGBA{0, 0, 0, 0};
			const geom::ray&	r = vp.rays[i];
			real			d = MAX_VALUE_DIST;
			for(int j = 0; j < (int)n_tris; ++j) {
				geom::vec3	unused;
				const real	cur_d = geom::ray_intersect(r, tris[j], unused);
				if(cur_d > 0.0 && (cur_d < d)) {
					d = cur_d;
					out.values[i] = scene::vec3_RGBA(mats[j].reflectance_color);
				}
			}
		}
	}

	// single threaded rendering
	// also useful for debugging
	void render_pt(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
		// first ensure that the bitmap is of correct size
		out.res_x = vp.res_x;
		out.res_y = vp.res_y;
		out.values.resize(out.res_x*out.res_y);
		// now, for each viewport point, do compute if we get a triangle
		// and if yes, write which one as a char
		frand::basic_dist	bd;
		for(int i = 0; i < (int)vp.rays.size(); ++i) {
			render_core(i, vp, tris, mats, n_tris, n_samples, bd, out);
		}
	}

	void render_pt_mt(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
		// first ensure that the bitmap is of correct size
		out.res_x = vp.res_x;
		out.res_y = vp.res_y;
		out.values.resize(out.res_x*out.res_y);
		// set the max number of threads
		const int			max_th = std::thread::hardware_concurrency(),
						chunk_sz = 16,
						n_chunks = out.values.size() / chunk_sz,
						chunks_per_th = n_chunks / max_th;
		if(1 == max_th) {
			render_pt(vp, tris, mats, n_tris, n_samples, out);
			return;
		}
		std::vector<std::thread>	th_vec;
		int				total = out.values.size();
		std::atomic<int>		done(0);
		//
		std::cout << "Set parallelism: " << max_th << std::endl;
		std::cout << "Samples: " << n_samples << std::endl;
		const auto			s_time = std::chrono::high_resolution_clock::now();
		// spawn a thread for each chunk
		for(int i = 0; i < max_th; ++i) {
			th_vec.push_back(
				std::thread(
					[&](const int s) -> void {
						// choose a randomizer
						//frand::basic_dist	d;
						//frand::vec_dist	d(1000);
						frand::seed_dist	d(s);
						for(int j = 0; j < chunks_per_th; ++j) {
							const int	cur_chunk = j*max_th + s;
							for(int r = cur_chunk*chunk_sz; r < (cur_chunk+1)*chunk_sz; ++r) {
								render_core(r, vp, tris, mats, n_tris, n_samples, d, out);
								++done;
							}
						}
						// if we're the last thread
						// finish off the remainder
						if(s == max_th-1) {
							const int	chunk_r_beg = chunks_per_th*max_th*chunk_sz,
									chunk_r_end = out.values.size();
							for(int r = chunk_r_beg; r < chunk_r_end; ++r) {
								render_core(r, vp, tris, mats, n_tris, n_samples, d, out);
								++done;
							}

						}
					},
					i
				)
			);
		}
		// print out progress
		while(done <= total) {
			std::printf("Progress:%7.2f%%\r", 100.0*done/total);
			std::fflush(stdout);
			if(done == total)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		const auto			e_time = std::chrono::high_resolution_clock::now();
		std::printf("\nDone (%.1fs)\n", std::chrono::duration_cast<std::chrono::milliseconds>(e_time - s_time).count()/1000.0);
		// wait for all of those to end
		for(auto& i : th_vec)
			i.join();
	}

	struct flat_r : public basic_renderer {
		flat_r(const int x, const int y) : basic_renderer(x, y) {
		}

		virtual const char* get_description(void) const {
			return "CPU - Flat";
		}

		virtual void render(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			render_test(vp, tris, mats, n_tris, n_samples, out);
		}

	};

	struct pt_r : public basic_renderer {
		pt_r(const int x, const int y) : basic_renderer(x, y) {
		}

		virtual const char* get_description(void) const {
			return "CPU - Path Tracing";
		}
		virtual void render(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) {
			render_pt_mt(vp, tris, mats, n_tris, n_samples, out);
		}

	};
}

namespace cpu_renderer {
	scene::renderer* get_flat(const int w, const int h) {
		return new flat_r(w, h);
	}

	scene::renderer* get_pt(const int w, const int h) {
		return new pt_r(w, h);
	}
}

