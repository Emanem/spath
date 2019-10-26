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

#include "geom.h"
#include "view.h"
#include "scene.h"
#include <iostream>
#include <GL/glut.h>

namespace gl {
	typedef void (*render_func)(const view::viewport& vp, const geom::triangle* tris, const scene::material* mats, const size_t n_tris, scene::bitmap& out);
	// maybe implement this: https://community.khronos.org/t/does-opengl-help-in-the-display-of-an-existing-image/69979
	// or just use glDrawPixels
	view::camera	*vc = 0;
	geom::triangle	*tris = 0;
	scene::material	*mats = 0;
	scene::bitmap	bmp;
	render_func	r_func = scene::render_test;
	int		n_tris = -1;

	int		win_w = -1,
			win_h = -1;

	void reshapeFunc(int w, int h) {
		win_w = w;
		win_h = h;
		vc->res_x = win_w;
		vc->res_y = win_h;
		glViewport (0, 0, (GLsizei) w, (GLsizei) h);
		// these 2 lines are to flip the image...
		glRasterPos2f(-1.0, 1.0);
 		glPixelZoom(1.0, -1.0);

		glutPostRedisplay();
	}

	void displayFunc(void) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		view::viewport	vp;
		vc->get_viewport(vp);

		r_func(vp, tris, mats, n_tris, bmp);

		glDrawPixels(win_w, win_h, GL_RGBA, GL_UNSIGNED_BYTE, &bmp.values[0]);
        	glutSwapBuffers();
	}

	void keyboardFunc(unsigned char key, int x, int y) {
		switch(key) {
			// camera pos
			case 'w':
				vc->pos += vc->rel_move(geom::vec3(0.0, 0.0, 0.05));
				glutPostRedisplay();
				break;
			case 's':
				vc->pos += vc->rel_move(geom::vec3(0.0, 0.0, -0.05));
				glutPostRedisplay();
				break;
			case 'a':
				vc->pos += vc->rel_move(geom::vec3(0.05, 0.0, 0.0));
				glutPostRedisplay();
				break;
			case 'd':
				vc->pos += vc->rel_move(geom::vec3(-0.05, 0.0, 0.0));
				glutPostRedisplay();
				break;
			// camera angle
			case 'q':
				vc->angle.y += 2*geom::PI/128.0;
				glutPostRedisplay();
				break;
			case 'e':
				vc->angle.y -= 2*geom::PI/128.0;
				glutPostRedisplay();
				break;
			case 27: //esc
				std::exit(0);
				break;
			case 'f':
				vc->focal += 0.1;
				glutPostRedisplay();
				break;
			case 'g':
				vc->focal -= 0.1;
				glutPostRedisplay();
				break;
			case 'r':
				if(r_func == scene::render_test) r_func = scene::render_pt_mt;
				else r_func = scene::render_test;
				glutPostRedisplay();
			default:
				break;
		}
	}
}

int main(int argc, char *argv[]) {
	try {
		view::camera	c(640, 480);
		view::viewport	vp;
		c.get_viewport(vp);

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
		gl::vc = &c;
		gl::tris = t;
		gl::mats = m;
		gl::n_tris = sizeof(t)/sizeof(geom::triangle);
		gl::win_w = 640;
		gl::win_h = 480;

		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
		glutInitWindowPosition(100, 100);
		glutInitWindowSize(gl::win_w, gl::win_h);
		glutCreateWindow("spath");
		glutDisplayFunc(gl::displayFunc);
		glutReshapeFunc(gl::reshapeFunc);
		glutKeyboardUpFunc(gl::keyboardFunc);
		glutMainLoop();
	} catch(const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	} catch(...) {
		std::cerr << "Unknown exception" << std::endl;
	}
}

