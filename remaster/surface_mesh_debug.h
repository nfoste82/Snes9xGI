#ifndef REMASTER_SURFACE_MESH_DEBUG_H
#define REMASTER_SURFACE_MESH_DEBUG_H

#include "surface_mesh.h"
#include <limits>

namespace RemasterSurfaceMesh
{
// Presentation-only oblique view of exactly the fans and closures intersected
// by the visibility shader. No welding or geometry inference happens here.
inline std::vector<uint8_t> debugView(unsigned width, unsigned height,
	const uint8_t *artwork, const std::vector<Sample> &samples,
	const std::vector<Cell> &mesh, unsigned scale = 4)
{
	const unsigned w = width * scale, h = height * scale;
	std::vector<uint8_t> image(size_t(w) * h * 4);
	if (!scale || mesh.size() != size_t(width)*height || samples.size() != mesh.size()) return image;
	std::vector<float> depth(size_t(w)*h, -std::numeric_limits<float>::infinity());
	for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++)
	{
		const size_t p = (size_t(y)*w+x)*4, source = (size_t(y/scale)*width+x/scale)*4;
		for (unsigned c = 0; c < 3; c++) image[p+c] = artwork ? artwork[source+c]/5 : 12;
		image[p+3] = 255;
	}
	struct Vertex { float x, y, z; };
	auto triangle = [&](Vertex a, Vertex b, Vertex c, const std::array<float,3> &color) {
		for (auto *v : {&a,&b,&c}) { v->x = (v->x + .35f*v->z)*scale; v->y = (v->y - .65f*v->z)*scale; }
		auto edge = [](Vertex p, Vertex q, float x, float y) { return (q.x-p.x)*(y-p.y)-(q.y-p.y)*(x-p.x); };
		const float area = edge(a,b,c.x,c.y);
		if (std::abs(area) < .00001f) return;
		const int x0 = std::max(0,int(std::floor(std::min({a.x,b.x,c.x})))), x1 = std::min(int(w)-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
		const int y0 = std::max(0,int(std::floor(std::min({a.y,b.y,c.y})))), y1 = std::min(int(h)-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
		const float ab = std::hypot(b.x-a.x,b.y-a.y), bc = std::hypot(c.x-b.x,c.y-b.y), ca = std::hypot(a.x-c.x,a.y-c.y);
		for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
		{
			const float u = edge(b,c,x+.5f,y+.5f)/area, v = edge(c,a,x+.5f,y+.5f)/area, t = 1-u-v;
			if (std::min({u,v,t}) < -.00001f) continue;
			const size_t p = size_t(y)*w+x;
			const float z = u*a.z+v*b.z+t*c.z;
			if (z < depth[p]-.00001f) continue;
			depth[p] = z;
			const bool wire = std::min({u*std::abs(area)/std::max(.001f,bc),v*std::abs(area)/std::max(.001f,ca),t*std::abs(area)/std::max(.001f,ab)}) < .32f;
			for (unsigned k = 0; k < 3; k++) image[p*4+k] = uint8_t(std::min(255.f,color[k]*(wire ? 1.f : .32f)));
		}
	};
	for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++)
	{
		const size_t p = size_t(y)*width+x;
		const auto &s = samples[p]; const auto &m = mesh[p];
		if (!s.known) continue;
		// Transparent receivers are shown dimly, but do not acquire blocker faces.
		std::array<float,3> color = s.fallbackGround ? std::array<float,3>{130,255,105} :
			m.solidWall > .5f ? std::array<float,3>{255,95,220} :
			m.thickness > 0 ? std::array<float,3>{255,180,65} : std::array<float,3>{70,220,255};
		if (s.coverage <= 0) for (float &c : color) c *= .35f;
		Vertex vertices[] = {{float(x),float(y),m.corners[0]}, {float(x+1),float(y),m.corners[1]},
			{float(x+1),float(y+1),m.corners[2]}, {float(x),float(y+1),m.corners[3]}};
		for (unsigned i = 0; i < 4; i++)
		{
			Vertex a = vertices[i], b = vertices[(i+1)%4], center = {x+.5f,y+.5f,s.height};
			const float half = m.thickness*.5f;
			a.z += half; b.z += half; center.z += half;
			triangle(center,a,b,color);
			if (half > 0)
			{
				Vertex lowA = a, lowB = b; lowA.z -= 2*half; lowB.z -= 2*half; center.z -= 2*half;
				triangle(center,lowB,lowA,color); triangle(lowA,lowB,b,color); triangle(lowA,b,a,color);
			}
			if (m.solidWall > .5f)
			{
				Vertex lowA = a, lowB = b; lowA.z = lowB.z = m.wallBase;
				triangle(lowA,lowB,b,color); triangle(lowA,b,a,color);
			}
		}
	}
	return image;
}
}
#endif
