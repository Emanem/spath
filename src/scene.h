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

#ifndef _SCENE_H_
#define _SCENE_H_

#include "geom.h"
#include "view.h"
#include <thread>
#include <atomic>
#include <cstdio>

namespace scene {
	struct RGBA {
		uint8_t	r,
			g,
			b,
			a;
	};

	RGBA vec3_RGBA(const geom::vec3& in) {
		const geom::vec3	c_v = in.clamp();
		return RGBA { c_v.x*255, c_v.y*255, c_v.z*255, 0};
	}

	struct bitmap {
		size_t			res_x,
					res_y;
		std::vector<RGBA>	values;
	};

	struct material {
		geom::vec3	reflectance_color,
				emittance_color;
	};

	void render_test(const view::viewport& vp, const geom::triangle* tris, const material* mats, const size_t n_tris, bitmap& out) {
		// first ensure that the bitmap is of correct size
		out.res_x = vp.res_x;
		out.res_y = vp.res_y;
		out.values.resize(out.res_x*out.res_y);
		// now, for each viewport point, do compute if we get a triangle
		// and if yes, write which one as a char
		for(int i = 0; i < (int)vp.rays.size(); ++i) {
			out.values[i] = RGBA{0, 0, 0, 0};
			const geom::ray&	r = vp.rays[i];
			real			d = 1000000000000;
			for(int j = 0; j < (int)n_tris; ++j) {
				geom::vec3	unused;
				const real	cur_d = geom::ray_intersect(r, tris[j], unused);
			       	if(cur_d > 0.0 && (cur_d < d)) {
					d = cur_d;
					out.values[i] = vec3_RGBA(mats[j].reflectance_color);
				}
			}
		}
	}

	geom::vec3 render_step(const geom::ray& r, const geom::triangle* tris, const material* mats, const size_t n_tris, const int depth = 0) {
		using namespace geom;
		// just max 5 bounces for now...
		if(depth >= 5)
			return vec3();
		// try to hit something
		real	d = 1000000000000;
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
		next_r.dir = rand_unit_vec(adj_n);
		// probability of new ray
		const static real	p = 1.0/(PI*2.0);
		// BRDF
		const real	cos_theta = next_r.dir.dot(adj_n);
		const vec3	BRDF = mats[idx].reflectance_color * (1.0/PI);
		// recursive step
		const vec3	rec_color = render_step(next_r, tris, mats, n_tris, depth+1);
		
		return mats[idx].emittance_color + (BRDF * rec_color * cos_theta * (1.0/p)); 
	}

	void render_core(const int idx, const view::viewport& vp, const geom::triangle* tris, const material* mats, const size_t n_tris, bitmap& out) {
		out.values[idx] = RGBA{0, 0, 0, 0};
		const static int	SAMPLES = 200;
		geom::vec3		accum;
		for(int j = 0; j < SAMPLES; ++j) {
			accum += render_step(vp.rays[idx], tris, mats, n_tris);
		}
		accum *= (1.0/SAMPLES);
		out.values[idx] = vec3_RGBA(accum.clamp());
	}

	void render_pt(const view::viewport& vp, const geom::triangle* tris, const material* mats, const size_t n_tris, bitmap& out) {
		// first ensure that the bitmap is of correct size
		out.res_x = vp.res_x;
		out.res_y = vp.res_y;
		out.values.resize(out.res_x*out.res_y);
		// now, for each viewport point, do compute if we get a triangle
		// and if yes, write which one as a char
		for(int i = 0; i < (int)vp.rays.size(); ++i) {
			render_core(i, vp, tris, mats, n_tris, out);
		}
	}

	void render_pt_mt(const view::viewport& vp, const geom::triangle* tris, const material* mats, const size_t n_tris, bitmap& out) {
		// first ensure that the bitmap is of correct size
		out.res_x = vp.res_x;
		out.res_y = vp.res_y;
		out.values.resize(out.res_x*out.res_y);
		// set the max number of threads
		const int			max_th = std::thread::hardware_concurrency(),
		      				th_sz = out.values.size() / max_th,
						th_rem = out.values.size()%th_sz;
		std::vector<std::thread>	th_vec;
		int				total = out.values.size();
		std::atomic<int>		done(0);
		//
		std::cout << "Set parallelism: " << max_th << std::endl;
		// spawn a thread for each chunk
		for(int i = 0; i < max_th; ++i) {
			th_vec.push_back(
				std::thread(
					[&out, &vp, &tris, &mats, &n_tris, &done](const int s, const int e) -> void {
						for(int r = s; r < e; ++r) {
							render_core(r, vp, tris, mats, n_tris, out);
							++done;
						}
					},
					i*th_sz,
					(i+1)*th_sz + ((i != max_th-1) ? 0 : th_rem) 
				)
			);
		}
		// print out progress
		while(done <= total) {
			std::printf("Progress:%7.2f%%\r", 6, 100.0*done/total);
			std::fflush(stdout);
			if(done == total)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		std::printf("\nDone\n");
		// wait for all of those to end
		for(auto& i : th_vec)
			i.join();
	}
}

#endif //_SCENE_H_

