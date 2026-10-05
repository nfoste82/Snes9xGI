// Portable authoring of an explicit held-lamp source into a replay packet.
// Input must supply recovered pre-cone scene color/ownership for faithful GI.
#include "../../remaster/held_lamp.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
	if (argc != 6 && argc != 10 && argc != 13) {
		std::fprintf(stderr, "Usage: author-held-lamp INPUT OUTPUT LINK_X LINK_Y FACING [HEIGHT RADIUS INTENSITY FLOOR_HEIGHT [RED GREEN BLUE]]\n"
			"Coordinates and dimensions are source pixels; facing 0/2/4/6 = north/south/west/east; color is sRGB 0..1.\n");
		return 2;
	}
	auto number = [](const char *text, float &value) {
		char *end = nullptr; errno = 0; value = std::strtof(text, &end);
		return !errno && end != text && *end == 0 && std::isfinite(value);
	};
	RemasterFrame frame;
	if (!S9xReadRemasterFrame(argv[1], frame)) return 1;
	RemasterHeldLampState state;
	state.verifiedRom = frame.profileRomSha256 == "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
	state.darkWithLantern = true; // explicit authoring, never automatic inference
	float facing = 0;
	if (!number(argv[3], state.linkScreenX) || !number(argv[4], state.linkScreenY) ||
		!number(argv[5], facing) || facing < 0 || facing > 6 || facing != std::floor(facing)) return 2;
	state.facing = static_cast<uint8_t>(facing);
	RemasterHeldLampSettings settings;
	if (argc >= 10 && (!number(argv[6], settings.height) || !number(argv[7], settings.radius) ||
		!number(argv[8], settings.intensity) || !number(argv[9], state.floorHeight))) return 2;
	if (argc == 13) for (unsigned c = 0; c < 3; c++) if (!number(argv[10+c], settings.color[c])) return 2;
	RemasterFrameLight light;
	if (!S9xRemasterHeldLampLight(state, settings, light)) return 2;
	frame.lights.push_back(light);
	frame.schemaVersion = REMASTER_FRAME_SCHEMA_VERSION;
	if (!S9xWriteRemasterFrame(frame, argv[2])) return 1;
	std::printf("Authored omnidirectional lamp: center=(%.3f,%.3f,%.3f) radius=%.3f intensity=%.3f color=(%.3f,%.3f,%.3f)\n",
		light.x, light.y, light.z, light.radius, light.intensity, light.red, light.green, light.blue);
	return 0;
}
