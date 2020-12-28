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
#include "cl_renderer.h"
#include "vk_renderer.h"
#include <iostream>
#include <GL/glut.h>
#include <memory>

namespace {
	// keep a vector of renderers
	std::vector<scene::renderer*>	all_renderers;
	int				cur_renderer = 0;

	void print_r_desc(void) {
		std::cout << "Current renderer: " << all_renderers[cur_renderer]->get_description() << std::endl;
	}

	void print_s_desc(const size_t s) {
		std::cout << "Sample per pixel (PT): " << s << std::endl;
	}
}

namespace gl {
	// maybe implement this: https://community.khronos.org/t/does-opengl-help-in-the-display-of-an-existing-image/69979
	// or just use glDrawPixels
	geom::triangle	*tris = 0;
	scene::material	*mats = 0;
	size_t		samples = 128;
	scene::bitmap	bmp;
	int		n_tris = -1;

	int		win_w = -1,
			win_h = -1,
			mouse_x = -1,
			mouse_y = -1;
	bool		mouse_lb_pressed = false,
			path_tracing = false;

	void reshapeFunc(int w, int h) {
		win_w = w;
		win_h = h;
		// update all renderers
		for(auto& i : all_renderers)
			i->set_viewport_size(w, h);
		// gl setup
		glViewport (0, 0, (GLsizei) w, (GLsizei) h);
		// these 2 lines are to flip the image...
		glRasterPos2f(-1.0, 1.0);
 		glPixelZoom(1.0, -1.0);

		glutPostRedisplay();
	}

	void displayFunc(void) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		view::viewport	vp;
		all_renderers[cur_renderer]->get_viewport(vp);
		if(path_tracing) {
			all_renderers[cur_renderer]->render(vp, tris, mats, n_tris, samples, bmp);
		} else {
			all_renderers[cur_renderer]->render_flat(vp, tris, mats, n_tris, samples, bmp);
		}

		glDrawPixels(win_w, win_h, GL_RGBA, GL_UNSIGNED_BYTE, &bmp.values[0]);
        	glutSwapBuffers();
	}

	void keyboardFunc(unsigned char key, int x, int y) {
		switch(key) {
			// camera pos
			case 'w':
				for(auto& i : all_renderers)
					i->set_delta_mov(geom::vec3(0.0, 0.0, 0.05));
				glutPostRedisplay();
				break;
			case 's':
				for(auto& i : all_renderers)
					i->set_delta_mov(geom::vec3(0.0, 0.0, -0.05));
				glutPostRedisplay();
				break;
			case 'a':
				for(auto& i : all_renderers)
					i->set_delta_mov(geom::vec3(0.05, 0.0, 0.0));
				glutPostRedisplay();
				break;
			case 'd':
				for(auto& i : all_renderers)
					i->set_delta_mov(geom::vec3(-0.05, 0.0, 0.0));
				glutPostRedisplay();
				break;
			// focal
			case 'f':
				for(auto& i : all_renderers)
					i->set_delta_focal(0.1);
				glutPostRedisplay();
				break;
			case 'g':
				for(auto& i : all_renderers)
					i->set_delta_focal(-0.1);
				glutPostRedisplay();
				break;
			// render type
			case 'r':
				cur_renderer = (cur_renderer + 1) % all_renderers.size();
				print_r_desc();
				reshapeFunc(win_w, win_h);
				break;
			// samples
			case '+':
				samples *= 2;
				print_s_desc(samples);
				glutPostRedisplay();
				break;
			case '-':
				samples /= 2;
				samples = (samples < 1) ? 1 : samples;
				print_s_desc(samples);
				glutPostRedisplay();
				break;
			// path tracing enabler
			case 'p':
				path_tracing = !path_tracing;
				glutPostRedisplay();
				break;
			// quit
			case 'q':
			case 27: //esc
				std::exit(0);
				break;
			default:
				break;
		}
	}

	void mouseFunc(int btn, int s, int x, int y) {
		// update old coordinates
		mouse_x = x;
		mouse_y = y;
		// manage the button press
		switch(btn) {
			case GLUT_LEFT_BUTTON:
				mouse_lb_pressed = (s == GLUT_DOWN);
				break;
			default:
				break;
		}
	}

	void motionFunc(int x, int y) {
		// update the view only if the LB is pressed
		if(mouse_lb_pressed) {
			const real	delta_angle_y = -1.0*(x - mouse_x)*(2.0*geom::PI)*0.0005,
					delta_angle_x = 1.0*(y - mouse_y)*(2.0*geom::PI)*0.0005;
			// set all renderers
			for(auto& i : all_renderers)
				i->set_delta_rot(geom::vec3(delta_angle_x, delta_angle_y, 0.0));
			// update old coordinates
			mouse_x = x;
			mouse_y = y;
			// redisplay
			glutPostRedisplay();
		}
	}
}

int main(int argc, char *argv[]) {
	try {
		geom::triangle	t[7];
		t[0].v0 = geom::vec3(0.0, 0.0, 1.0);
		t[0].v1 = geom::vec3(0.5, -0.5, 0.0);
		t[0].v2 = geom::vec3(-0.5, -0.5, 0.0);
		// t[1] and t[2] are the plane
		const float	p_size = 20.0;
		t[1].v0 = geom::vec3(p_size, -1.0, p_size);
		t[1].v1 = geom::vec3(-p_size, -1.0, -p_size);
		t[1].v2 = geom::vec3(-p_size, -1.0, p_size);
		t[2].v0 = geom::vec3(p_size, -1.0, p_size);
		t[2].v1 = geom::vec3(p_size, -1.0, -p_size);
		t[2].v2 = geom::vec3(-p_size, -1.0, -p_size);
		// t[3] and t[4] is the area light...
		const float	al_size = 0.75;
		t[3].v0 = geom::vec3(al_size, 0.75, al_size);
		t[3].v1 = geom::vec3(-al_size, 0.75, al_size);
		t[3].v2 = geom::vec3(al_size, 0.75, -al_size);
		t[4].v0 = geom::vec3(-al_size, 0.75, al_size);
		t[4].v1 = geom::vec3(-al_size, 0.75, -al_size);
		t[4].v2 = geom::vec3(al_size, 0.75, -al_size);
		// back wall t[5] and t[6]
		const float	wall_depth = 1.0;
		t[5].v0 = geom::vec3(1.25, 0.5, wall_depth);
		t[5].v1 = geom::vec3(1.25, -1.0, wall_depth);
		t[5].v2 = geom::vec3(-1.25, -1.0, wall_depth);
		t[6].v0 = geom::vec3(1.25, 0.5, wall_depth);
		t[6].v1 = geom::vec3(-1.25, -1.0, wall_depth);
		t[6].v2 = geom::vec3(-1.25, 0.5, wall_depth);
		// setup flat normals
		for(int i = 0; i < (int)(sizeof(t)/sizeof(geom::triangle)); ++i)
			geom::flat_normal(t[i]);
		// setup materials
		scene::material	m[7];
		m[0].reflectance_color = geom::vec3(1.0, 0.0, 0.0);
		//
		m[1].reflectance_color = geom::vec3(0.0, 1.0, 0.0);
		//m[1].emittance_color = geom::vec3(0.0, 0.1, 0.0);
		m[2].reflectance_color = geom::vec3(0.0, 0.0, 1.0);
		//m[2].emittance_color = geom::vec3(0.0, 0.0, 0.1);
		//
		m[3].reflectance_color = geom::vec3(1.0, 1.0, 1.0);
		m[3].emittance_color = geom::vec3(1.0, 1.0, 1.0);
		m[4].reflectance_color = geom::vec3(1.0, 1.0, 1.0);
		m[4].emittance_color = geom::vec3(1.0, 1.0, 1.0);
		//
		m[5].reflectance_color = geom::vec3(1.0, 1.0, 1.0);
		m[6].reflectance_color = geom::vec3(1.0, 1.0, 1.0);
		static_assert(sizeof(m)/sizeof(scene::material) == sizeof(t)/sizeof(geom::triangle));

		// set the global parameters
		gl::tris = t;
		gl::mats = m;
		gl::n_tris = sizeof(t)/sizeof(geom::triangle);
		gl::win_w = 640;
		gl::win_h = 480;

		// set the renderers
		std::unique_ptr<scene::renderer>	pt_r(cpu_renderer::get(gl::win_w, gl::win_h)),
							cl_r(cl_renderer::get(gl::win_w, gl::win_h)),
							vk_r(vk_renderer::get(gl::win_w, gl::win_h));
		// add to the vector
		all_renderers.push_back(&(*pt_r));
		all_renderers.push_back(&(*cl_r));
		all_renderers.push_back(&(*vk_r));
		// print description
		print_r_desc();

		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
		glutInitWindowPosition(100, 100);
		glutInitWindowSize(gl::win_w, gl::win_h);
		glutCreateWindow("spath");
		glutDisplayFunc(gl::displayFunc);
		glutReshapeFunc(gl::reshapeFunc);
		glutKeyboardUpFunc(gl::keyboardFunc);
		glutMouseFunc(gl::mouseFunc);
		glutMotionFunc(gl::motionFunc);
		glutMainLoop();
	} catch(const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	} catch(...) {
		std::cerr << "Unknown exception" << std::endl;
	}
}

