#include "remaster-normal-picker.h"
#include <cstdlib>
#include <iostream>

static void check(bool condition)
{
	if (!condition)
	{
		std::cerr << "Normal picker regression failed\n";
		std::exit(1);
	}
}

static bool near(float a, float b)
{
	return std::abs(a - b) < 0.00001f;
}

int main()
{
	for (bool back : {false, true})
	{
		const auto initial = RemasterNormalPickerTilt(-3, 4, 0.6f, 0, back);
		check(near(initial[0], -0.48f) && near(initial[1], 0.64f));
		check(near(initial[2], back ? -0.6f : 0.6f));
		for (float delta : {-2.0f, -0.3f, 0.0f, 0.2f, 2.0f})
		{
			const auto normal = RemasterNormalPickerTilt(-3, 4, initial[2], delta, back);
			check(near(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2], 1));
			check(near(normal[0] * 4, normal[1] * -3));
			check(normal[0] <= 0 && normal[1] >= 0);
			check(back ? normal[2] <= 0 : normal[2] >= 0);
		}
		const auto pole = RemasterNormalPickerTilt(-3, 4, initial[2], 2, back);
		check(near(pole[0], 0) && near(pole[1], 0));
		const auto restored = RemasterNormalPickerTilt(-3, 4, pole[2], -0.4f, back);
		check(near(restored[0], initial[0]) && near(restored[1], initial[1]));
		const auto rim = RemasterNormalPickerTilt(-3, 4, initial[2], -2, back);
		check(near(rim[0], -0.6f) && near(rim[1], 0.8f) && near(rim[2], 0));
		for (bool lockX : {false, true})
		{
			for (float delta : {-2.0f, -0.2f, 0.0f, 0.2f, 2.0f})
			{
				const auto normal = RemasterNormalPickerAxisLock(-0.48f, 0.64f,
					delta, delta, lockX, !lockX, back);
				check(near(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2], 1));
				check(lockX ? normal[0] == -0.48f : normal[1] == 0.64f);
				check(back ? normal[2] <= 0 : normal[2] >= 0);
				if (delta == 0)
					check(near(normal[0], initial[0]) && near(normal[1], initial[1]) && near(normal[2], initial[2]));
			}
		}
		const auto both = RemasterNormalPickerAxisLock(-0.48f, 0.64f, 2, -2, true, true, back);
		check(near(both[0], initial[0]) && near(both[1], initial[1]) && near(both[2], initial[2]));
		const auto axis = RemasterNormalPickerAxisLock(1, 0, -2, 2, true, false, back);
		check(axis[0] == 1 && axis[1] == 0 && near(axis[2], 0));
	}
	const auto fromPole = RemasterNormalPickerTilt(0, 0, 1, -0.4f, false);
	check(near(fromPole[0], 0.8f) && near(fromPole[1], 0) && near(fromPole[2], 0.6f));
	std::cout << "Normal picker regressions passed\n";
}
