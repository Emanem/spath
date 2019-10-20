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

#ifndef _GEOM_H_
#define _GEOM_H_

#include <cmath>
#include <cstdlib>

typedef double	real;

namespace geom {
	struct vec3 {
		real	x,
			y,
			z;

		vec3() : x(0.0), y(0.0), z(0.0) {
		}

		vec3(const real x_, const real y_, const real z_) : x(x_), y(y_), z(z_) {
		}

		vec3 operator+(const vec3& rhs) const {
			return vec3(x + rhs.x, y + rhs.y, z + rhs.z);
		}

		vec3 operator-(const vec3& rhs) const {
			return vec3(x - rhs.x, y - rhs.y, z - rhs.z);
		}

		vec3 operator*(const vec3& rhs) const {
			return vec3(x * rhs.x, y * rhs.y, z * rhs.z);
		}

		vec3 operator/(const vec3& rhs) const {
			return vec3(x / rhs.x, y / rhs.y, z / rhs.z);
		}

		vec3 operator+(const real rhs) const {
			return vec3(x + rhs, y + rhs, z + rhs);
		}

		vec3 operator-(const real rhs) const {
			return vec3(x - rhs, y - rhs, z - rhs);
		}

		vec3 operator*(const real rhs) const {
			return vec3(x * rhs, y * rhs, z * rhs);
		}

		vec3 operator/(const real rhs) const {
			return vec3(x / rhs, y / rhs, z / rhs);
		}

		vec3& operator+=(const vec3& rhs) {
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return *this;
		}

		vec3& operator-=(const vec3& rhs) {
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
			return *this;
		}

		vec3& operator*=(const vec3& rhs) {
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;
			return *this;
		}

		vec3& operator/=(const vec3& rhs) {
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;
			return *this;
		}

		vec3& operator+=(const real rhs) {
			x += rhs;
			y += rhs;
			z += rhs;
			return *this;
		}

		vec3& operator-=(const real rhs) {
			x -= rhs;
			y -= rhs;
			z -= rhs;
			return *this;
		}

		vec3& operator*=(const real rhs) {
			x *= rhs;
			y *= rhs;
			z *= rhs;
			return *this;
		}

		vec3& operator/=(const real rhs) {
			x /= rhs;
			y /= rhs;
			z /= rhs;
			return *this;
		}

		real dot(const vec3& rhs) const {
			return x*rhs.x + y*rhs.y + z*rhs.z;
		}

		real sq_length(void) const {
			return x*x + y*y + z*z;
		}

		real length(void) const {
			return std::sqrt(sq_length());
		}

		vec3 unit(void) const {
			const real l = length();
			return (*this)/l;
		}

		vec3 cross(const vec3& rhs) const {
			return vec3(y*rhs.z - z*rhs.y, z*rhs.x - x*rhs.z, x*rhs.y - y*rhs.x);
		}

		vec3 invert(void) const {
			return (*this)*-1.0;
		}

		vec3 clamp(const real min = 0.0, const real max = 1.0) const {
			return vec3(
				(x > max) ? max : ((x < min) ? min : x),
				(y > max) ? max : ((y < min) ? min : y),
				(z > max) ? max : ((z < min) ? min : z)
			);
		}
	};

	const double PI = std::acos(-1.0);

	vec3 rand_unit_vec(const vec3& in) {
		//return in;

		const real	rv_xz = 1.0*std::rand()/RAND_MAX*PI*2.0,
		      		rv_y = 1.0*std::rand()/RAND_MAX*PI*0.5,
				f_x = std::cos(rv_y),
				f_y = std::sin(rv_y);
		// directions
		vec3		out(std::cos(rv_xz)*f_x, f_y, std::sin(rv_xz)*f_x);
		if(in.dot(out) < 0.0)
			return out*-1.0;
		return out;
	}

	struct ray {
		vec3	pos,
			dir;
	};

	// vertex and normals
	struct triangle {
		vec3	v0,
			v1,
			v2,
			n;
	};

	void flat_normal(triangle& inout) {
		const auto	dir = (inout.v1 - inout.v0).cross(inout.v2 - inout.v0);
		inout.n = dir.unit();
	}

	real ray_intersect(const ray& r, const triangle& t, vec3& point) {
		static const real	EPSILON = 0.00000000000001;

		const vec3	edge1 = t.v1 - t.v0,
		      		edge2 = t.v2 - t.v0,
				h = r.dir.cross(edge2);
		const real	a = edge1.dot(h);
		if(a > -EPSILON && a < EPSILON)
			return -1.0;
		const real	f = 1.0/a;
		const vec3	s = r.pos - t.v0;
		const real	u = f * s.dot(h);
		if(u < 0.0 || u > 1.0)
			return -1.0;
		const vec3	q = s.cross(edge1);
		const real	v = f * r.dir.dot(q);
		if(v < 0.0 || (u+v)> 1.0)
			return -1.0;
		// compute intersection
		const real	d = f * edge2.dot(q);
		if(d > EPSILON && d < 1.0/EPSILON) {
			point = r.pos + (r.dir*d);
			return d;
		}
		return -1.0;
	}
}

#endif //_GEOM_H_

