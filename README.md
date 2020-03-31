# Computer-Graphics - Competition - Raytracer

This is a raytracer with soft shadows.

Point light sources have been replaced with flat rectangular light sources. The light source contains a number of cells, which emits light themselves. We now have an area light, which allows us to create soft shadows.

We distinguish two types of soft shadows: banding and jitter:
* Banding soft shadows have distinct step-like differences in shadow intensity. Each cell emits light from its center.
* Jitter soft shadows are more noisy/granier. Instead of emitting light from its center, a random position in each cell is chosen.

This raytracer also does anti-aliasing, reflaction and refraction. Note that transparent objects don't have any shadows for simplicity.


Controls:
* ShadowType: a field for the Scene: specifies what type of shadow is used: 0 = hard shadow, 1 = soft shadow (banding), 2 = soft shadow (jitter)
* cells_x: a field for Light: the number of cells in the x direction
* cells_y: a field for Light: the number of cells in the y direction
* width: a field for Light: the width of the light area
* height: a field for Light: the height of the light area
* Light->position is the **center** of the light area. It lives in the z-plane


Some pictures with the different soft shadows, cell sizes, scenes can be found in the Screenshots folder.
