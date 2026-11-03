"""Portable numeric contracts for assembled terrain normal authoring."""
import unittest

import numpy as np

from alttp_author_village_normals import carrier_rotation, gradient, height_normal, rotate
from alttp_author_village_terrain import floor_residual


class VillageNormalAuthoringTest(unittest.TestCase):
    def test_only_explicit_floor_may_receive_pixel_heights(self):
        delta = np.arange(64).reshape(8, 8)*.125
        for role in ('wall', 'wall-corner', 'obstacle', 'door', 'doorway', None, 'unknown'):
            np.testing.assert_equal(floor_residual(role, delta, 4), np.zeros((8, 8)))
        np.testing.assert_equal(floor_residual('floor', delta, 4), delta-4)

    def test_plane_crosses_tile_boundaries(self):
        y, x = np.mgrid[:24, :32]
        z = .125*x - .25*y + 40
        gx, gy = gradient(z, np.ones(z.shape, dtype=int))
        np.testing.assert_allclose(gx, .125)
        np.testing.assert_allclose(gy, -.25)
        n = height_normal(gx, gy)
        np.testing.assert_allclose(n[:, 7], n[:, 8])
        np.testing.assert_allclose(n[:, 15], n[:, 16])

    def test_cliff_not_smoothed_into_ground(self):
        z = np.zeros((16, 24))
        z[:, :8] = 40
        z[:, 8:16] = np.linspace(40, 0, 8)
        labels = np.ones(z.shape, dtype=int)
        labels[:, 8:16] = 2
        labels[:, 16:] = 3
        gx, gy = gradient(z, labels)
        np.testing.assert_equal(gx[:, :8], 0)
        np.testing.assert_equal(gx[:, 16:], 0)
        np.testing.assert_equal(gy, 0)
        np.testing.assert_allclose(gx[:, 8:16], -40/7)

    def test_flat_fractional_base_is_identity(self):
        z = np.full((16, 16), 19.125)
        gx, gy = gradient(z, np.ones(z.shape))
        n = height_normal(gx, gy)
        np.testing.assert_equal(n, np.broadcast_to([0, 0, 1], n.shape))

    def test_structural_carrier_not_double_tilted(self):
        rng = np.random.default_rng(42)
        before = height_normal(rng.normal(size=(100,)), rng.normal(size=(100,)))
        after = height_normal(rng.normal(size=(100,)), rng.normal(size=(100,)))
        n = carrier_rotation(before, after)
        np.testing.assert_allclose(rotate(n, before), after, atol=1e-14)
        np.testing.assert_allclose(np.linalg.norm(n, axis=-1), 1, atol=1e-14)
        detail = height_normal(rng.normal(size=(100,)), rng.normal(size=(100,)))
        result = rotate(n, detail)
        np.testing.assert_allclose(np.linalg.norm(result, axis=-1), 1, atol=1e-14)
        np.testing.assert_allclose((result*after).sum(-1), (detail*before).sum(-1), atol=1e-14)

    def test_equal_carrier_requires_no_rotation(self):
        n = height_normal(np.array([.2, -.4]), np.array([.7, .9]))
        np.testing.assert_equal(carrier_rotation(n, n), [[0, 0, 1], [0, 0, 1]])


if __name__ == '__main__':
    unittest.main()
