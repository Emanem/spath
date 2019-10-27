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

#ifndef _BASIC_RENDERER_H_
#define _BASIC_RENDERER_H_

#include "renderer.h"

// This is the incomplete declaration that can
// be shared across different hw implementations
class basic_renderer : public scene::renderer {
protected:
	view::camera	vc;
public:
	basic_renderer(const int x, const int y) : vc(x, y) {
	}

	virtual void set_viewport_size(const int w, const int h) {
		vc.res_x = w;
		vc.res_y = h;
	}

	virtual void set_delta_mov(const geom::vec3& m) {
		vc.pos += vc.rel_move(m);
	}

	virtual void set_delta_rot(const geom::vec3& r) {
		vc.angle += r;
		// update the cached trigonometric values
		vc.update_angles_trig_values();
	}

	virtual void set_delta_focal(const real f) {
		vc.focal += f;
	}

	virtual void get_viewport(view::viewport& vp) {
		vc.get_viewport(vp);
	}
};

#endif //_BASIC_RENDERER_H_

