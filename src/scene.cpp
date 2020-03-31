#include "scene.h"

#include "hit.h"
#include "image.h"
#include "material.h"
#include "ray.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>

using namespace std;

pair<ObjectPtr, Hit> Scene::castRay (Ray const &ray) const {
	// Find hit object and distance
	Hit min_hit (numeric_limits<double>::infinity (), Vector ());
	ObjectPtr obj = nullptr;
	for (unsigned idx = 0; idx != objects.size (); ++idx) {
		Hit hit (objects[idx]->intersect (ray));
		if (hit.t < min_hit.t) {
			min_hit = hit;
			obj = objects[idx];
		}
	}

	return pair<ObjectPtr, Hit> (obj, min_hit);
}

Color Scene::trace (Ray const &ray, unsigned depth) {
	pair<ObjectPtr, Hit> mainhit = castRay (ray);
	ObjectPtr obj = mainhit.first;
	Hit min_hit = mainhit.second;

	// No hit? Return background color.
	if (!obj)
		return Color (0.0, 0.0, 0.0);

	Material const &material = obj->material;
	Point hit = ray.at (min_hit.t);
	Vector V = -ray.D;

	// Pre-condition: For closed objects, N points outwards.
	Vector N = min_hit.N;

	// The shading normal always points in the direction of the view,
	// as required by the Phong illumination model.
	Vector shadingN;
	if (N.dot (V) >= 0.0)
		shadingN = N;
	else
		shadingN = -N;

	Color matColor;
	if (material.hasTexture) {
		Vector UV = obj->toUV (hit);
		matColor = material.texture.colorAt (UV.x, 1 - UV.y);
	} else {
		matColor = material.color;
	}

	// Add ambient once, regardless of the number of lights.
	Color color = material.ka * matColor;

	// Add diffuse and specular components.
	for (auto const &light : lights) {
		// Color for diffuse and specular component
		Color diffSpecColor = Color ();
		// The corner of the light surface (light->position is the center)
		Point lightCorner = light->position - Vector (light->width / 2, light->height / 2, 0);

		// If the shadow type is hard, we assume that there is only 1 cell. In that case it would simply a point light.
		// Otherwise we keep the number of cells
		int cells_x = shadowType == HARD ? 1 : light->cells_x;
		int cells_y = shadowType == HARD ? 1 : light->cells_y;
		// Total number of cells in the light surface
		int num_cells = cells_x * cells_y;

		// Add diffuse and specular components for every cell
		for (int x = 0; x < cells_x; x++) {
			for (int y = 0; y < cells_y; y++) {
				// If the shadow type is soft (banding), the light emits from the center of the cell. 
				// Otherwise, it is jittered, i.e. we get a random position in the cell
				double p_x = shadowType == SOFT_JITTERED
					? static_cast <float> (rand ()) / static_cast <float> (RAND_MAX)
					: 0.5;
				double p_y = shadowType == SOFT_JITTERED
					? static_cast <float> (rand ()) / static_cast <float> (RAND_MAX)
					: 0.5;
				// Get the position within the cell
				Vector subLightPos (
					lightCorner.x + light->width * (p_x + x) / cells_x,
					lightCorner.y + light->height * (p_y + y) / cells_y,
					lightCorner.z
				);
				Vector L = (subLightPos - hit).normalized ();

				// Add shadow
				bool inShadow = false;
				Ray shadowRay (hit + epsilon * shadingN, L);
				pair<ObjectPtr, Hit> shadowHit = castRay (shadowRay);
				if (shadowHit.first) { // hit!
					inShadow = true;
					Point shadowPoint = shadowRay.at (shadowHit.second.t);
					// Check if the light is between the object and its shadow
					if ((subLightPos - hit).length_2 () <= (shadowPoint - hit).length_2 ()) {
						inShadow = false;
					} else if (shadowHit.first->material.isTransparent) {
						//cout << "Shadow behind transparent object" << endl;
						inShadow = false;
					}
				}

				if (!inShadow) {
					// Add diffuse.
					double diffuse = std::max (shadingN.dot (L), 0.0);
					diffSpecColor += diffuse * material.kd * light->color * matColor;

					// Add specular.
					Vector reflectDir = reflect (-L, shadingN);
					double specAngle = std::max (reflectDir.dot (V), 0.0);
					double specular = std::pow (specAngle, material.n);

					diffSpecColor += specular * material.ks * light->color;
				}
			}
		}
		// Divide by the number of cells to average
		color += (diffSpecColor / num_cells);
	}

	if (depth > 0 and material.isTransparent) {
		// The object is transparent, and thus refracts and reflects light.
		// Use Schlick's approximation to determine the ratio between the two.

		bool isGoingIn = V.dot (N) >= 0.0;

		double n_i, n_t;
		if (isGoingIn) {
			n_i = 1.0;
			n_t = material.nt;
		} else {
			n_i = material.nt;
			n_t = 1.0;
		}

		double kr0 = pow ((n_i - n_t) / (n_i + n_t), 2);
		double cos_phi_i = shadingN.dot (V);
		double kr = kr0 + (1 - kr0) * pow (1 - cos_phi_i, 5);
		double kt = 1 - kr;

		Vector R = reflect (-V, shadingN);
		Ray reflectRay (hit + epsilon * shadingN, R);
		Color reflectColor = trace (reflectRay, depth - 1);
		color += reflectColor * kr;

		Vector T = refract (-V, shadingN, n_i, n_t);
		Ray refractRay (hit - epsilon * shadingN, T); // we're moving inside, so -epsilon
		Color refractColor = trace (refractRay, depth - 1);
		color += refractColor * kt;
	} else if (depth > 0 and material.ks > 0.0) {
		// The object is not transparent, but opaque.
		Vector R = reflect (-V, shadingN);
		Ray reflectRay (hit + epsilon * shadingN, R);
		Color reflectColor = trace (reflectRay, depth - 1);
		color += reflectColor * material.ks;
	}

	return color;
}

void Scene::render (Image &img) {
	unsigned w = img.width ();
	unsigned h = img.height ();

	for (unsigned y = 0; y < h; ++y) {
		for (unsigned x = 0; x < w; ++x) {
			Color col = Color ();
			// Divide into sub-pixels, trace through their centers
			for (unsigned s_x = 0; s_x < supersamplingFactor; s_x++) {
				for (unsigned s_y = 0; s_y < supersamplingFactor; s_y++) {
					Point pixel (
						x + (0.5 + s_x) / supersamplingFactor,
						h - 1 - y + (0.5 + s_y) / supersamplingFactor,
						0
					);
					Ray ray (eye, (pixel - eye).normalized ());
					col += trace (ray, recursionDepth).clamp ();
				}
			}
			img (x, y) = col / (supersamplingFactor * supersamplingFactor); // average
		}
	}
}

// --- Misc functions ----------------------------------------------------------

// Defaults
Scene::Scene ()
	:
	objects (),
	lights (),
	eye (),
	renderShadows (false),
	shadowType (HARD),
	recursionDepth (0),
	supersamplingFactor (1) {
}

void Scene::addObject (ObjectPtr obj) {
	objects.push_back (obj);
}

void Scene::addLight (Light const &light) {
	lights.push_back (LightPtr (new Light (light)));
}

void Scene::setEye (Triple const &position) {
	eye = position;
}

unsigned Scene::getNumObject () {
	return objects.size ();
}

unsigned Scene::getNumLights () {
	return lights.size ();
}

void Scene::setRenderShadows (bool shadows) {
	renderShadows = shadows;
}

void Scene::setShadowType (ShadowType sType) {
	shadowType = sType;
}

void Scene::setRecursionDepth (unsigned depth) {
	recursionDepth = depth;
}

void Scene::setSuperSample (unsigned factor) {
	supersamplingFactor = factor;
}
