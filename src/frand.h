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

#ifndef _FRAND_H_
#define _FRAND_H_

#include <vector>
#include <algorithm>
#include <cstdlib>

namespace frand {
	struct basic_dist {
		double operator()(void) {
			return 1.0*std::rand()/RAND_MAX;
		}
	};

	struct vec_dist {
		std::vector<double>	r_values;
		int			r_next;

		vec_dist(const int max_values) : r_next(0) {
			const double	step = 1.0/max_values;
			r_values.reserve(max_values+1);
			for(int i = 0; i < max_values; ++i)
				r_values.push_back(step*i);
			r_values.push_back(1.0);
			std::random_shuffle(r_values.begin(), r_values.end());
		}

		double operator()(void) {
			const double	rv = r_values[r_next++];
			r_next = (r_next >= (int)r_values.size()) ? 0 : r_next;
			return rv;
		}
	};

	// taken from https://stackoverflow.com/a/26237777/159094
	struct seed_dist {
		uint32_t	seed;

		seed_dist(const uint32_t s_) : seed(s_) {
		}

		double operator()(void) {
			seed = (214013*seed+2531011);
    			return 1.0*((seed>>16)&0x7FFF)/32767.0;
		}
	};
}

#endif //_FRAND_H_

