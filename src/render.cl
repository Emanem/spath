/* declare types */

typedef float cl_real;

typedef struct vec3_ {
	cl_real	x,
		y,
		z;
} vec3;

typedef struct ray_ {
	vec3	pos,
		dir;
} ray;

typedef struct triangle_ {
	vec3	v0,
		v1,
		v2,
		n;
} triangle;

typedef struct material_ {
	vec3	reflectance_color,
		emittance_color;
} material;

typedef struct RGBA_ {
	uchar	r,
		g,
		b,
		a;
} RGBA;

constant cl_real	EPSILON = 0.00000000000001;
constant cl_real	MAX_VALUE_DIST = 1000000000000.0;

vec3 v_add(const vec3* lhs, const vec3* rhs) {
	vec3	rv;
	rv.x = lhs->x + rhs->x;
	rv.y = lhs->y + rhs->y;
	rv.z = lhs->z + rhs->z;
	return rv;
}

vec3 v_diff(const vec3* lhs, const vec3* rhs) {
	vec3	rv;
	rv.x = lhs->x - rhs->x;
	rv.y = lhs->y - rhs->y;
	rv.z = lhs->z - rhs->z;
	return rv;
}

vec3 v_mult(const vec3* lhs, const cl_real rhs) {
	vec3	rv;
	rv.x = lhs->x*rhs;
	rv.y = lhs->y*rhs;
	rv.z = lhs->z*rhs;
	return rv;
}

vec3 v_cross(const vec3* lhs, const vec3* rhs) {
	vec3	rv;
	rv.x = lhs->y*rhs->z - lhs->z*rhs->y; 
	rv.y = lhs->z*rhs->x - lhs->x*rhs->z;
	rv.z = lhs->x*rhs->y - lhs->y*rhs->x;
	return rv;
}

cl_real v_dot(const vec3* lhs, const vec3* rhs) {
	return lhs->x*rhs->x + lhs->y*rhs->y + lhs->z*rhs->z;
}

cl_real ray_intersect(const ray* r, const triangle* t, vec3* point) {
	const vec3	edge1 = v_diff(&t->v1, &t->v0),
			edge2 = v_diff(&t->v2, &t->v0),
			h = v_cross(&r->dir, &edge2);
	const cl_real	a = v_dot(&edge1, &h);
	if(a > -EPSILON && a < EPSILON)
		return -1.0;
	const cl_real	f = 1.0/a;
	const vec3	s = v_diff(&r->pos, &t->v0);
	const cl_real	u = f * v_dot(&s, &h);
	if(u < 0.0 || u > 1.0)
		return -1.0;
	const vec3	q = v_cross(&s, &edge1);
	const cl_real	v = f * v_dot(&r->dir, &q);
	if(v < 0.0 || (u+v)> 1.0)
		return -1.0;
	// compute intersection
	const cl_real	d = f * v_dot(&edge2, &q);
	if(d > EPSILON && d < 1.0/EPSILON) {
		const vec3 vd = v_mult(&r->dir, d);
		*point = v_add(&r->pos, &vd);
		return d;
	}
	return -1.0;
}

uchar clampcnv(const cl_real v) {
	const cl_real	r = 255*v + 0.5;
	uchar	rv = (r < 0) ? 0 : ((r > 255) ? 255 : (uchar)r);
	return rv;
}

RGBA vec3_RGBA(const vec3 color) {
	RGBA	rv;
	rv.r = clampcnv(color.x);
	rv.g = clampcnv(color.y);
	rv.b = clampcnv(color.z);
	rv.a = 0;
	return rv;
}

void kernel render_flat(global ray* rays, global triangle* tris, global material* mats, const unsigned int n_tris, const unsigned int n_samples, global RGBA* out) {
	size_t g_id = get_global_id(0);

	out[g_id].r = 0;
	out[g_id].g = 0;
	out[g_id].b = 0;
	out[g_id].a = 0;

	const ray	r = rays[g_id];
	cl_real		d = MAX_VALUE_DIST;
	for(int i = 0; i < n_tris; ++i) {
		vec3		unused;
		triangle	cur_tri = tris[i];
		const cl_real	cur_d = ray_intersect(&r, &cur_tri, &unused);
		if((cur_d > 0.0) && (cur_d < d)) {
			d = cur_d;
			out[g_id] = vec3_RGBA(mats[i].reflectance_color);
		}
	}
}

