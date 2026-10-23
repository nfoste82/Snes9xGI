#include "../runtime/lighting_field_cache.h"
#include "../runtime/surface_mesh_debug.h"
#include <cstdio>
#include <memory>
#include <map>

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 4) {
		std::fprintf(stderr,"usage: surface-mesh-capture-probe capture [profile [output-prefix]]\n");
		return 2;
	}
	auto frame = std::make_unique<RemasterFrame>();
	if (!S9xReadRemasterFrame(argv[1], *frame)) return 2;
	if (argc >= 3) {
		RemasterProfile profile; std::vector<RemasterProfileDiagnostic> diagnostics;
		if (!S9xRemasterLoadProfile(argv[2], profile, diagnostics)) return 2;
		S9xRemasterApplyProfileToFrame(profile, *frame);
	}
	RemasterLightingFieldCache fields;
	fields.update(*frame, frame->scenePixels.size() == frame->mainPixels.size());
	auto mesh = RemasterSurfaceMesh::build(frame->width, frame->height, fields.samples, frame->lightingCoordinateScale / 255);
	std::printf("frame %ux%u schema %u scale %g\n", frame->width, frame->height, frame->schemaVersion, frame->lightingCoordinateScale);
	std::printf("instances %zu main %zu scene %zu sceneRGB %zu\n",frame->tileInstances.size(),frame->mainPixels.size(),frame->scenePixels.size(),frame->sceneRgb555.size());
	unsigned groundSheets = 0, groundShells = 0, groundRaised = 0;
	std::map<std::string, unsigned> types;
	unsigned reported = 0;
	for (size_t i = 0; i < mesh.size(); i++)
	{
		const auto &s = fields.samples[i];
		if (!s.known || s.height != 0 || s.coverage <= 0) continue;
		if (s.sheet) groundSheets++; else groundShells++;
		bool raised = false; for (float z : mesh[i].corners) raised |= std::abs(z) > 0.0001f;
		groundRaised += raised;
		if (raised && reported++ < 12) std::printf("nonflat ground %zu,%zu domain %llu corners %g,%g,%g,%g\n",i%frame->width,i/frame->width,(unsigned long long)s.domain,mesh[i].corners[0],mesh[i].corners[1],mesh[i].corners[2],mesh[i].corners[3]);
		const auto &pixels = frame->scenePixels.size() == frame->mainPixels.size() ? frame->scenePixels : frame->mainPixels;
		const auto &pixel = pixels[i].instanceId ? pixels[i] : frame->subPixels[i];
		if (pixel.instanceId) {
			const auto &t = frame->tileInstances[pixel.instanceId-1];
			types[t.material + "/" + t.assetGroup + (s.sheet ? "/sheet" : "/shell") + (raised ? "/nonflat" : "/flat")]++;
		}
	}
	std::printf("opaque zero-height ground: %u sheets, %u shells, %u nonflat patches\n", groundSheets, groundShells, groundRaised);
	for (const auto &t : types) std::printf("%s %u\n", t.first.c_str(), t.second);
	if (argc == 4)
	{
		std::vector<uint8_t> colors(mesh.size()*4);
		for (size_t i = 0; i < mesh.size(); i++) {
			unsigned rgb = frame->originalRgb555[i];
			colors[i*4] = ((rgb>>10)&31)*255/31; colors[i*4+1] = ((rgb>>5)&31)*255/31;
			colors[i*4+2] = (rgb & 31)*255/31; colors[i*4+3] = 255;
		}
		auto write = [](const std::string &path, unsigned w, unsigned h, const std::vector<uint8_t> &rgba) {
			std::ofstream out(path, std::ios::binary); out << "P6\n" << w << " " << h << "\n255\n";
			for (size_t i = 0; i < rgba.size(); i += 4) out.write(reinterpret_cast<const char *>(rgba.data()+i),3);
		};
		write(std::string(argv[3])+"-original.ppm",frame->width,frame->height,colors);
		auto debug = RemasterSurfaceMesh::debugView(frame->width,frame->height,colors.data(),fields.samples,mesh);
		write(std::string(argv[3])+"-mesh.ppm",frame->width*4,frame->height*4,debug);
		std::ofstream csv(std::string(argv[3])+"-mesh.csv");
		csv << "x,y,known,coverage,height,thickness,solidWall,wallBase,nw,ne,se,sw,fallbackGround\n";
		for (size_t i = 0; i < mesh.size(); i++)
		{
			const auto &s = fields.samples[i]; const auto &m = mesh[i];
			csv << i%frame->width << ',' << i/frame->width << ',' << s.known << ',' << s.coverage << ',' << s.height << ',' << m.thickness << ',' << m.solidWall << ',' << m.wallBase;
			for (float z : m.corners) csv << ',' << z;
			csv << ',' << s.fallbackGround << '\n';
		}
	}
}
