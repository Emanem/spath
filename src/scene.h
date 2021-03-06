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

namespace scene {
	struct RGBA {
		uint8_t	r,
			g,
			b,
			a;
	};

	inline RGBA vec3_RGBA(const geom::vec3& in) {
		const geom::vec3	c_v = in.clamp() * 255.0 + 0.5;
		return RGBA {
			(c_v.x < 0.0) ? 0 : ((c_v.x > 255.0) ? 255 : (uint8_t)c_v.x),
			(c_v.y < 0.0) ? 0 : ((c_v.y > 255.0) ? 255 : (uint8_t)c_v.y),
			(c_v.z < 0.0) ? 0 : ((c_v.z > 255.0) ? 255 : (uint8_t)c_v.z),
			0};
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
}

#endif //_SCENE_H_

