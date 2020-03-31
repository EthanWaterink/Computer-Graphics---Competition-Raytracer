#ifndef LIGHT_H_
#define LIGHT_H_

#include "triple.h"

// Declare LightPtr for use in Scene class
#include <memory>
class Light;
typedef std::shared_ptr<Light> LightPtr;

class Light
{
    public:
        Point const position;
        Color const color;
		int cells_x, cells_y;
		double width, height;

        Light(Point const &pos, Color const &c, int c_x, int c_y, int w, int h)
        :
            position(pos),
            color(c),
			cells_x(c_x),
			cells_y(c_y),
			width(w),
			height(h)
        {}
};

#endif
