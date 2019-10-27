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

#ifndef _RENDERER_H_
#define _RENDERER_H_

#include "scene.h"

namespace scene {
	class renderer {
	public:
		virtual const char* get_description(void) const = 0;
		virtual void set_viewport_size(const int w, const int h) = 0;
		virtual void set_delta_mov(const geom::vec3& m) = 0;
		virtual void set_delta_rot(const geom::vec3& r) = 0;
		virtual void set_delta_focal(const real f) = 0;
		virtual void get_viewport(view::viewport& vp) = 0;
		virtual void render_flat(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) = 0;
		virtual void render(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, const size_t n_samples, scene::bitmap& out) = 0;
		// inline declaration
		virtual ~renderer() {}
	};
}

#endif //_RENDERER_H_

