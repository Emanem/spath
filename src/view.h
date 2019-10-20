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

#ifndef _VIEW_H_
#define _VIEW_H_

#include "geom.h"
#include <vector>
#include <iostream>

namespace view {
	// a viewport is basically a large vector
	// of rays (postion and direction)
	struct viewport {
		size_t			res_x,
					res_y;
		std::vector<geom::ray>	rays;

		void print(std::ostream& ostr) const {
			for(int j = 0; j < (int)res_y; ++j) {
				for(int i = 0; i < (int)res_x; ++i) {
					const geom::ray&	cur_ray = rays[i + j*res_x];
					ostr << "[" << cur_ray.pos.x << "," << cur_ray.pos.y << "," << cur_ray.pos.z << "]";
					ostr << "(" << cur_ray.dir.x << "," << cur_ray.dir.y << "," << cur_ray.dir.z << ")\t";
				}
				ostr << '\n';
			}
			ostr << std::endl;
		}
	};	

	// use a camera to create a viewport
	struct camera {
		geom::vec3	pos,
				dir;
		real		focal;
		size_t		res_x,
				res_y;

		camera(const size_t x, const size_t y, const real f_ = 2.0) : pos(0.0, 0.0, -3.0), dir(1.0, 0.0, 0.0), focal(f_), res_x(x), res_y(y) {
		}

		void get_viewport(viewport& out) {
			using namespace geom;
			
			out.res_x = res_x;
			out.res_y = res_y;
			out.rays.resize(res_x*res_y);
			// set the positions (centered for each pixel)
			const real	x_size = 1.0*res_x/res_y,
			      		y_size = 1.0,
					x_max = x_size/2.0,
			      		x_step = x_size/res_x,
					h_x_step = x_step/2.0,
					y_max = y_size/2.0,
					y_step = y_size/res_y,
					h_y_step = y_step/2.0;
			for(int i = 0; i < (int)res_x; ++i) {
				for(int j = 0; j < (int)res_y; ++j) {
					const vec3	cur_pos(x_max - x_step*i - h_x_step, y_max - y_step*j - h_y_step, 0.0);
					out.rays[i + j*res_x].pos = cur_pos + pos;
					// now add the focal vector for direction
					out.rays[i + j*res_x].dir = (cur_pos + vec3(0.0, 0.0, focal)).unit();
				}
			}
			// we should rotate at this stage... but hey...
			// this may be useful https://math.stackexchange.com/questions/293116/rotating-one-3d-vector-to-another
			// this also https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula#Matrix_notation
		}
	};
}

#endif //_VIEW_H_
