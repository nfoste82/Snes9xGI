#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

typedef struct
{
    vector_float2 position;
    vector_float2 textureCoordinate;
} MetalVertex;

typedef struct
{
    // The [[position]] attribute qualifier of this member indicates this value is
    // the clip space position of the vertex when this structure is returned from
    // the vertex shader
    float4 position [[position]];

    // Since this member does not have a special attribute qualifier, the rasterizer
    // will interpolate its value with values of other vertices making up the triangle
    // and pass that interpolated value to the fragment shader for each fragment in
    // that triangle.
    float2 textureCoordinate;

} RasterizerData;

typedef struct
{
	float3 position;
	float radius;
	float intensity;
	uint sampleCount;
	uint firstSample;
	uint hideSource;
	float3 color;
} RemasterGpuLight;

typedef struct
{
	uint width;
	uint height;
	uint view;
	uint lightCount;
	uint passIndex;
	uint diagnosticStage;
	float indirectRoughness;
	float originalSceneContribution;
	float heightPreviewMultiplier;
	float padding;
	uint sampleIndex;
	uint sampleCount;
	uint randomSeed;
	float reflectanceBoost;
	float4 cameraDirection;
	float4 debugPositionRadius;
	float4 debugColorIntensity;
	float4 heightPreviewRange;
	float4 directionalDirection;
	float4 directionalColorBrightness;
} RemasterLightingUniforms;

// Radiance calibration: byte 25 is 100 linear radiance units, not display white.
// A small emitter must be much brighter than a reflecting surface at room distances.
constant float remasterEmissionRadiance = 100.0;

static float remasterRoughDiffuse(float3 normal, float3 incoming, float3 outgoing, float roughness)
{
	float cosineIncoming = saturate(dot(normal, incoming));
	float outgoingDot = dot(normal, outgoing);
	float cosineOutgoing = saturate(outgoingDot);
	if (cosineIncoming <= 0.0 || outgoingDot < 0.0)
		return 0.0;
	float sigma = roughness * M_PI_F * 0.5;
	float sigmaSquared = sigma * sigma;
	float a = 1.0 - 0.5 * sigmaSquared / (sigmaSquared + 0.33);
	float b = 0.45 * sigmaSquared / (sigmaSquared + 0.09);
	float3 incomingTangent = incoming - normal * cosineIncoming;
	float3 outgoingTangent = outgoing - normal * cosineOutgoing;
	float tangentProduct = length(incomingTangent) * length(outgoingTangent);
	float azimuth = tangentProduct > 0.0001 ? max(0.0, dot(incomingTangent, outgoingTangent) / tangentProduct) : 0.0;
	float sineAlpha = sqrt(max(0.0, 1.0 - min(cosineIncoming, cosineOutgoing) * min(cosineIncoming, cosineOutgoing)));
	float tangentBeta = sqrt(max(0.0, 1.0 - max(cosineIncoming, cosineOutgoing) *
		max(cosineIncoming, cosineOutgoing))) / max(max(cosineIncoming, cosineOutgoing), 0.0001);
	return a + b * azimuth * sineAlpha * tangentBeta;
}

static uint remasterHash(uint value)
{
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	return value ^ (value >> 16);
}

static float remasterRandom(uint value)
{
	return float(remasterHash(value) & 0x00ffffffu) / 16777216.0;
}

static float3 remasterToLinear(float3 color)
{
	return pow(max(color, 0.0), float3(2.2));
}

static float3 remasterToDisplay(float3 color)
{
	return pow(max(color, 0.0), float3(1.0 / 2.2));
}

// Keep transport radiance HDR, but show the emitter in artwork-scale units.
// Intensity 25 reproduces the authored RGB; higher power can light the room
// without multiplying the visible flame by the 100x transport calibration.
static float3 remasterEmissionAppearance(float3 radiance)
{
	return radiance / remasterEmissionRadiance;
}

static float3 remasterCompositeToDisplay(float3 color)
{
	color = max(color, 0.0);
	float peak = max(color.r, max(color.g, color.b));
	// A shared RGB scale preserves hue; channel-by-channel clipping makes warm
	// highlights white. Leave darker artwork and reflected light unchanged.
	if (peak > 0.75)
	{
		float shoulder = 0.75 + 0.25 * (1.0 - exp(-(peak - 0.75) / 0.25));
		color *= shoulder / peak;
	}
	return remasterToDisplay(color);
}

static float3 remasterAlbedo(float3 paintedColor)
{
	// Preserve artwork value and hue. Baked-shadow removal belongs in authored material data.
	return saturate(remasterToLinear(paintedColor) * 0.9);
}

static float3 remasterBoostReflectance(float3 albedo, float strength)
{
	// A single scale preserves each painted pixel's hue. Darker pixels receive
	// more lift, while black stays black and no channel reflects all incoming light.
	float brightness = dot(albedo, float3(0.2126, 0.7152, 0.0722));
	float darkness = 1.0 - saturate(brightness);
	float scale = 1.0 + strength * darkness * darkness;
	float largest = max(albedo.r, max(albedo.g, albedo.b));
	return albedo * min(scale, 0.95 / max(largest, 0.000001));
}

static float remasterRayExitDistance(float2 origin, float2 ray, float2 bounds)
{
	float xDistance = ray.x > 0.0 ? (bounds.x - origin.x) / ray.x :
		(ray.x < 0.0 ? -origin.x / ray.x : INFINITY);
	float yDistance = ray.y > 0.0 ? (bounds.y - origin.y) / ray.y :
		(ray.y < 0.0 ? -origin.y / ray.y : INFINITY);
	return min(xDistance, yDistance);
}

// Orthographic camera-facing sphere surface. Presentation never seeds transport.
static bool remasterDebugSphereVisible(float2 point, float height, float4 sphere)
{
	float radialSquared = dot(point - sphere.xy, point - sphere.xy);
	return sphere.w > 0.0 && radialSquared <= sphere.w * sphere.w &&
		height <= sphere.z + sqrt(max(0.0, sphere.w * sphere.w - radialSquared));
}

struct RemasterMeshCell
{
	float4 corners;
	packed_float3 normal;
	float thickness;
	float wallBase;
	float solidWall;
	float emissionDepth;
	float emissionOmnidirectional;
};

kernel void remasterBuildVisibilityBlocks(
	texture2d<float, access::read> occlusion [[texture(0)]],
	texture2d<float, access::read> heightField [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::write> blocks [[texture(3)]],
	const device RemasterMeshCell *mesh [[buffer(0)]],
	uint2 block [[thread_position_in_grid]])
{
	if (block.x >= (occlusion.get_width() + 3) / 4 || block.y >= (occlusion.get_height() + 3) / 4)
		return;
	float minimumHeight = INFINITY;
	float maximumHeight = -INFINITY;
	for (uint y = block.y * 4; y < min((block.y + 1) * 4, occlusion.get_height()); y++)
		for (uint x = block.x * 4; x < min((block.x + 1) * 4, occlusion.get_width()); x++)
			{
				uint2 pixel = uint2(x, y);
				float center = surfaceField.read(pixel).r;
				float coverage = heightField.read(pixel).g >= 0.5 ? occlusion.read(pixel).r : 0.0;
				float low = INFINITY, high = -INFINITY;
				if (coverage > 0.0)
				{
					RemasterMeshCell patch = mesh[y * occlusion.get_width() + x];
					low = min(center, min(min(patch.corners.x, patch.corners.y), min(patch.corners.z, patch.corners.w)));
					if (patch.solidWall > 0.5) low = min(low, patch.wallBase);
					high = max(center, max(max(patch.corners.x, patch.corners.y), max(patch.corners.z, patch.corners.w)));
					low -= patch.thickness * 0.5;
					high += patch.thickness * 0.5;
					minimumHeight = min(minimumHeight, low);
					maximumHeight = max(maximumHeight, high);
				}
				// Exact center/known-opacity plus conservative finite-mesh bounds.
				// Empty/unknown pixels retain an inverted interval, not a ground column.
				blocks.write(float4(low, high, center, coverage), pixel);
			}
	// Keep the 4x4 envelopes in a separate bottom strip of the same texture.
	blocks.write(float4(minimumHeight, maximumHeight, 0.0, 0.0),
		uint2(block.x, block.y + occlusion.get_height()));
}

// Shading positions lie on the resolved mesh at pixel centers. Visibility starts outside
// the represented face so shallow rays do not hit coplanar neighboring shells.
// Known surfaces share this bias regardless of opacity; analytic lights keep
// exact endpoints.
static float3 remasterSurfaceRayEndpoint(uint2 pixel, float4 surface,
	texture2d<float, access::read> occlusion,
	texture2d<float, access::read> heightField,
	const device RemasterMeshCell *mesh)
{
	float3 point = float3(float2(pixel) + 0.5, surface.r);
	RemasterMeshCell patch = mesh[pixel.y * occlusion.get_width() + pixel.x];
	// Opacity controls ray attenuation, not where a known receiving/emitting
	// surface lies. Transparent top patches must use the same shell face as
	// coplanar opaque patches or their rays enter the neighbors' thickness.
	if (heightField.read(pixel).g >= 0.5)
	{
		if (patch.thickness == 0.0)
		{
			float3 geometricNormal = float3(patch.normal);
			point += geometricNormal * (patch.solidWall <= 0.5 && dot(geometricNormal, surface.gba) < 0.0 ? -0.0001 : 0.0001);
		}
		else
		{
			// Shells are extruded in Z, not axis-aligned unit voxels. Artwork
			// normals may tilt strongly on a flat top; a voxel-face offset can
			// leave the endpoint inside this shell and hit coplanar neighbors.
			// Keep XY at the authored center and reach its actual top/bottom
			// face. Shading normals select the side, never the geometry.
			float side = dot(float3(patch.normal), surface.gba) < 0.0 ? -1.0 : 1.0;
			point.z += side * (patch.thickness * 0.5 + 0.0001);
		}
	}
	return point;
}

// A zero-depth omnidirectional emitter has no shading-selected face. Keep its
// prepared endpoint at the center, then bias toward the receiver for visibility.
static float3 remasterEmissionRayEndpoint(uint2 pixel, float4 surface,
	texture2d<float, access::read> occlusion, texture2d<float, access::read> heightField,
	const device RemasterMeshCell *mesh)
{
	RemasterMeshCell patch = mesh[pixel.y * occlusion.get_width() + pixel.x];
	if (patch.emissionOmnidirectional > 0.5 && patch.emissionDepth <= 0.0)
		return float3(float2(pixel) + 0.5, surface.r);
	return remasterSurfaceRayEndpoint(pixel, surface, occlusion, heightField, mesh);
}

static float3 remasterOmnidirectionalRayEndpoint(float3 point, uint2 pixel, float3 receiver,
	texture2d<float, access::read> heightField, const device RemasterMeshCell *mesh)
{
	if (heightField.read(pixel).g < 0.5) return point;
	RemasterMeshCell patch = mesh[pixel.y * heightField.get_width() + pixel.x];
	float3 geometricNormal = float3(patch.normal);
	float side = dot(geometricNormal, receiver - point) < 0.0 ? -1.0 : 1.0;
	if (patch.thickness == 0.0) point += geometricNormal * (side * 0.0001);
	else point.z += side * (patch.thickness * 0.5 + 0.0001);
	return point;
}

static bool remasterTriangleHit(float3 from, float3 ray, float3 a, float3 b, float3 c, float entry, float exit)
{
	float3 e1 = b - a, e2 = c - a;
	float3 p = cross(ray, e2);
	float determinant = dot(e1, p);
	if (abs(determinant) <= 0.0000001) return false;
	float inverse = 1.0 / determinant;
	float3 s = from - a;
	float u = dot(s, p) * inverse;
#ifndef REMASTER_REFERENCE_VISIBILITY
	// Preserve the original predicate/arithmetic, but do not finish a triangle
	// already outside its first barycentric half-space. Negation retains NaN
	// rejection too. The reference shader evaluates the complete predicate.
	if (!(u >= -0.000001)) return false;
#endif
	float3 q = cross(s, e1);
	float v = dot(ray, q) * inverse;
#ifndef REMASTER_REFERENCE_VISIBILITY
	if (!(v >= -0.000001 && u + v <= 1.000001)) return false;
#endif
	float t = dot(e2, q) * inverse;
	return u >= -0.000001 && v >= -0.000001 && u + v <= 1.000001 &&
		t > max(0.000001, entry - 0.000001) && t < min(0.999999, exit + 0.000001);
}

static bool remasterMeshHit(float3 from, float3 ray, int2 cell, float center,
	RemasterMeshCell patch, float entry, float exit)
{
	float low = min(center, min(min(patch.corners.x, patch.corners.y), min(patch.corners.z, patch.corners.w))) - patch.thickness * 0.5;
	if (patch.solidWall > 0.5) low = min(low, patch.wallBase);
	float high = max(center, max(max(patch.corners.x, patch.corners.y), max(patch.corners.z, patch.corners.w))) + patch.thickness * 0.5;
	float z0 = from.z + ray.z * entry, z1 = from.z + ray.z * exit;
	if (min(z0, z1) > high || max(z0, z1) < low) return false;
	if (patch.thickness > 0.0 && ray.z == 0.0 && all(patch.corners == center) &&
		(from.z <= center - patch.thickness * 0.5 || from.z >= center + patch.thickness * 0.5)) return false;
#ifdef REMASTER_REFERENCE_VISIBILITY
	// Keep the original array-based geometry expansion as an independent oracle.
	float3 vertices[4] = {float3(float2(cell), patch.corners.x),
		float3(float2(cell) + float2(1, 0), patch.corners.y),
		float3(float2(cell) + 1.0, patch.corners.z),
		float3(float2(cell) + float2(0, 1), patch.corners.w)};
#else
	// Stream successive edges rather than dynamically indexing a local vertex
	// array in this hot loop. Triangle coordinates and test order are unchanged.
	float3 edgeVertex = float3(float2(cell), patch.corners.x);
#endif
	float3 middle = float3(float2(cell) + 0.5, center);
	float3 halfThickness = float3(0, 0, patch.thickness * 0.5);
	for (uint i = 0; i < 4; i++)
	{
#ifdef REMASTER_REFERENCE_VISIBILITY
		float3 edgeVertex = vertices[i];
		float3 nextVertex = vertices[(i + 1) & 3];
#else
		float3 nextVertex = i == 0 ? float3(float2(cell) + float2(1, 0), patch.corners.y) :
			i == 1 ? float3(float2(cell) + 1.0, patch.corners.z) :
			i == 2 ? float3(float2(cell) + float2(0, 1), patch.corners.w) : float3(float2(cell), patch.corners.x);
#endif
		if (remasterTriangleHit(from, ray, middle + halfThickness, edgeVertex + halfThickness,
			nextVertex + halfThickness, entry, exit)) return true;
		if (patch.thickness > 0.0)
		{
			if (remasterTriangleHit(from, ray, middle - halfThickness, nextVertex - halfThickness,
				edgeVertex - halfThickness, entry, exit)) return true;
		}
#ifndef REMASTER_REFERENCE_VISIBILITY
		// Default side-face filter, visually accepted: faces lie on an XY plane. This
		// fixed margin passed bounded reference probes, but is not a universal
		// floating-point guarantee for extreme coordinates/near-parallel rays.
		bool verticalEdge = (i & 1) != 0;
		float originAxis = verticalEdge ? from.x : from.y;
		float rayAxis = verticalEdge ? ray.x : ray.y;
		float edgeAxis = verticalEdge ? edgeVertex.x : edgeVertex.y;
		float axis0 = originAxis + rayAxis * entry;
		float axis1 = originAxis + rayAxis * exit;
		float margin = 0.001 + abs(rayAxis) * 0.000002;
		bool sideCandidate = edgeAxis >= min(axis0, axis1) - margin &&
			edgeAxis <= max(axis0, axis1) + margin;
#else
		bool sideCandidate = true;
#endif
		if (sideCandidate && patch.thickness > 0.0)
		{
			if (remasterTriangleHit(from, ray, edgeVertex - halfThickness, nextVertex - halfThickness,
				nextVertex + halfThickness, entry, exit) ||
				remasterTriangleHit(from, ray, edgeVertex - halfThickness, nextVertex + halfThickness,
				edgeVertex + halfThickness, entry, exit)) return true;
		}
		if (sideCandidate && patch.solidWall > 0.5)
		{
			float3 bottomI = float3(edgeVertex.xy, patch.wallBase);
			float3 bottomJ = float3(nextVertex.xy, patch.wallBase);
			if (remasterTriangleHit(from, ray, bottomI, bottomJ, nextVertex, entry, exit) ||
				remasterTriangleHit(from, ray, bottomI, nextVertex, edgeVertex, entry, exit)) return true;
		}
#ifndef REMASTER_REFERENCE_VISIBILITY
		edgeVertex = nextVertex;
#endif
	}
	return false;
}

static float remasterVisibility(float3 from, float3 to,
	texture2d<float, access::read> occlusion,
	texture2d<float, access::read> heightField,
	texture2d<float, access::read> surfaceField,
	texture2d<float, access::read> visibilityBlocks,
	const device RemasterMeshCell *mesh)
{
	float3 segment = to - from;
	int2 cell = int2(floor(from.xy));
	int2 endpoint = int2(floor(to.xy));
	int2 step = int2(sign(segment.xy));
	float2 delta = float2(segment.x != 0.0 ? abs(1.0 / segment.x) : INFINITY,
		segment.y != 0.0 ? abs(1.0 / segment.y) : INFINITY);
	float2 edge = float2(cell) + float2(step.x > 0 ? 1.0 : 0.0, step.y > 0 ? 1.0 : 0.0);
	float2 next = float2(segment.x != 0.0 ? (edge.x - from.x) / segment.x : INFINITY,
		segment.y != 0.0 ? (edge.y - from.y) / segment.y : INFINITY);
	float visibility = 1.0;
	// Exclude only endpoint contact, not a proper crossing near the receiver.
	// Shell endpoints now lie outside their faces too: a below-top source must
	// not leak through a crossing wholly inside the receiver's first XY cell.
	if (all(cell >= 0) && cell.x < int(occlusion.get_width()) && cell.y < int(occlusion.get_height()))
	{
		uint2 originPixel = uint2(cell);
		float coverage = occlusion.read(originPixel).r;
		if (coverage > 0.0 && heightField.read(originPixel).g >= 0.5 &&
			remasterMeshHit(from, segment, cell, surfaceField.read(originPixel).r,
				mesh[originPixel.y * occlusion.get_width() + originPixel.x], 0.0, min(1.0, min(next.x, next.y))))
			visibility *= 1.0 - coverage;
		if (visibility < 0.01) return 0.0;
	}
	int2 testedBlock = int2(-1);
	bool emptyBlock = false;
	float blockExit = 0.0;
	// Retain pixel-exact traversal wherever a block might contain a blocker.
	while (any(cell != endpoint))
	{
#ifndef REMASTER_REFERENCE_VISIBILITY
		if (emptyBlock)
		{
			if (blockExit >= 1.0)
				break;
			// Stop just before the block boundary, leaving boundary/corner handling
			// to the same pixel DDA below.
			int2 remaining = int2(step.x > 0 ? 3 - (cell.x & 3) : (cell.x & 3),
				step.y > 0 ? 3 - (cell.y & 3) : (cell.y & 3));
			// Preserve the reference DDA's rounding at pixel corners.
			#pragma unroll
			for (int i = 0; i < 3; i++)
			{
				if (i < remaining.x && next.x < blockExit) { cell.x += step.x; next.x += delta.x; }
				if (i < remaining.y && next.y < blockExit) { cell.y += step.y; next.y += delta.y; }
			}
		}
#endif
		float entry = min(next.x, next.y);
		if (entry >= 1.0)
			break;
		bool crossX = next.x <= next.y;
		bool crossY = next.y <= next.x;
		if (crossX) { cell.x += step.x; next.x += delta.x; }
		if (crossY) { cell.y += step.y; next.y += delta.y; }
		if (any(cell < 0) || cell.x >= int(occlusion.get_width()) || cell.y >= int(occlusion.get_height()))
			break;
		uint2 sample = uint2(cell);
		float exit = min(1.0, min(next.x, next.y));
#ifndef REMASTER_REFERENCE_VISIBILITY
		// Test a newly entered block before fetching even its first pixel. The
		// envelope covers this pixel's entry through the block exit, so a rejection
		// also proves that the first per-pixel height read would reject. It does
		// not imply solid columns beneath the finite authored geometry.
		int2 block = cell / 4;
		if (any(block != testedBlock))
		{
			testedBlock = block;
			int2 remaining = int2(step.x > 0 ? 3 - (cell.x & 3) : (cell.x & 3),
				step.y > 0 ? 3 - (cell.y & 3) : (cell.y & 3));
			float2 blockNext = next;
			// Retain the original pixel additions, including corner rounding.
			#pragma unroll
			for (int i = 0; i < 3; i++)
			{
				if (i < remaining.x && step.x) blockNext.x += delta.x;
				if (i < remaining.y && step.y) blockNext.y += delta.y;
			}
			blockExit = min(1.0, min(blockNext.x, blockNext.y));
			float minimumHeight = min(from.z + segment.z * entry, from.z + segment.z * blockExit);
			float maximumHeight = max(from.z + segment.z * entry, from.z + segment.z * blockExit);
			float2 bounds = visibilityBlocks.read(uint2(block.x, block.y + occlusion.get_height())).rg;
			emptyBlock = bounds.x > maximumHeight || bounds.y < minimumHeight;
		}
		if (emptyBlock)
			continue;
		// This is the same conservative Z rejection as remasterMeshHit, before
		// fetching the 48-byte patch. DDA rounding, triangle tests and attenuation
		// order are unchanged. The origin-cell proper-crossing test stays above.
		float4 bounds = visibilityBlocks.read(sample);
		float z0 = from.z + segment.z * entry, z1 = from.z + segment.z * exit;
		if (bounds.x > max(z0, z1) || bounds.y < min(z0, z1))
			continue;
		float coverage = bounds.a;
		float center = bounds.b;
#else
		float coverage = occlusion.read(sample).r;
		float center = surfaceField.read(sample).r;
#endif
		if (coverage <= 0.0)
			continue;
#ifdef REMASTER_REFERENCE_VISIBILITY
		if (heightField.read(sample).g < 0.5)
			continue;
#endif
		RemasterMeshCell patch = mesh[sample.y * occlusion.get_width() + sample.x];
		// Grid traversal bins triangle candidates; it does not define geometry.
		// Count a patch once even if both triangles/shared edges are hit.
		if (exit - entry > 0.000001 && remasterMeshHit(from, segment, cell, center,
			patch, entry, exit))
		{
			visibility *= 1.0 - coverage;
			if (visibility < 0.01)
				return 0.0;
		}
	}
	return visibility;
}

// Complete binary sum tree. Leaves start at leafCount; padding leaves have zero
// power. The broad proposal below still reaches every represented pixel.
kernel void remasterBuildSourcePowerLeaves(
	texture2d<float, access::read> radiance [[texture(0)]],
	texture2d<float, access::read> participation [[texture(1)]],
	device float *tree [[buffer(0)]],
	constant uint &leafCount [[buffer(1)]],
	uint index [[thread_position_in_grid]])
{
	if (index >= leafCount)
		return;
	uint width = radiance.get_width();
	uint count = width * radiance.get_height();
	float power = 0.0;
	if (index < count)
	{
		uint2 pixel = uint2(index % width, index / width);
		if (participation.read(pixel).r > 0.5)
		{
			float3 value = max(radiance.read(pixel).rgb, 0.0);
			power = max(value.r, max(value.g, value.b));
		}
	}
	tree[leafCount + index] = isfinite(power) ? power : 0.0;
}

kernel void remasterReduceSourcePower(
	device float *tree [[buffer(0)]],
	constant uint &firstNode [[buffer(1)]],
	uint index [[thread_position_in_grid]])
{
	uint node = firstNode + index;
	if (node >= firstNode * 2)
		return;
	tree[node] = tree[node * 2] + tree[node * 2 + 1];
}

constant bool remasterBatchedIndirectPass [[function_constant(2)]];

kernel void remasterSampledIndirectBounce(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> occlusion [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::read> heightField [[texture(3)]],
	texture2d<float, access::read> participationField [[texture(4)]],
	texture2d<float, access::read> previousBounce [[texture(5)]],
	texture2d<float, access::read> previousIndirect [[texture(6)]],
	texture2d<float, access::write> nextBounce [[texture(7)]],
	texture2d<float, access::write> nextIndirect [[texture(8)]],
	texture2d<float, access::read> visibilityBlocks [[texture(10)]],
	texture2d<float, access::read> reflectanceField [[texture(11)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	const device float *sourcePower [[buffer(1)]],
	constant uint &leafCount [[buffer(2)]],
	const device RemasterMeshCell *mesh [[buffer(3)]],
	device float4 *partials [[buffer(5)]],
	uint3 grid [[thread_position_in_grid]])
{
	uint2 pixel = grid.xy;
	bool batched = is_function_constant_defined(remasterBatchedIndirectPass) ? remasterBatchedIndirectPass : false;
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	float3 accumulated = uniforms.passIndex > 1 ? previousIndirect.read(pixel).rgb : float3(0.0);
	float2 participation = participationField.read(pixel).rg;
	if (participation.r < 0.5 || participation.g < 0.5 || sourcePower[1] <= 0.0)
	{
		if (batched) { partials[(grid.z * uniforms.height + pixel.y) * uniforms.width + pixel.x] = float4(0.0); return; }
		nextBounce.write(float4(0.0), pixel);
		nextIndirect.write(float4(accumulated, 1.0), pixel);
		return;
	}
	float4 receiver = surfaceField.read(pixel);
	float4 authoredReflectance = reflectanceField.read(pixel);
	float3 albedo = authoredReflectance.a > 0.5 ? saturate(authoredReflectance.rgb) :
		remasterAlbedo(source.read(pixel).rgb);
	albedo = remasterBoostReflectance(albedo, uniforms.reflectanceBoost);
	if (all(albedo <= 0.0))
	{
		if (batched) { partials[(grid.z * uniforms.height + pixel.y) * uniforms.width + pixel.x] = float4(0.0); return; }
		nextBounce.write(float4(0.0), pixel);
		nextIndirect.write(float4(accumulated, 1.0), pixel);
		return;
	}
	uint count = uniforms.width * uniforms.height;
	int2 low = max(int2(pixel) - 16, int2(0));
	int2 high = min(int2(pixel) + 16, int2(uniforms.width - 1, uniforms.height - 1));
	uint localCount = uint(high.x - low.x + 1) * uint(high.y - low.y + 1);
	float3 incoming = 0.0;
	uint randomBase = uniforms.randomSeed ^ (pixel.x * 0x9e3779b9u) ^
		(pixel.y * 0x85ebca6bu) ^ (uniforms.passIndex * 0xc2b2ae35u);
	uint connectionCount = max(1u, uniforms.sampleCount);
	// Preserve each connection's seed/proposal exactly, but shorten divergent
	// traversal loops by assigning independent eight-connection batches.
	uint firstConnection = batched ? grid.z * 8 : 0;
	uint endConnection = batched ? min(connectionCount, firstConnection + 8) : connectionCount;
	float3 receiverEndpoint = remasterSurfaceRayEndpoint(pixel, receiver, occlusion, heightField, mesh);
	for (uint connection = firstConnection; connection < endConnection; connection++)
	{
		uint random = remasterHash(randomBase ^ (connection * 0x27d4eb2du));
		uint choice = random % 3;
		uint sourceIndex;
		if (choice == 0)
		{
			// Descend the power tree using one uniform variate. Every positive
			// leaf is selected with exactly its power / root probability.
			float target = remasterRandom(random ^ 0x68bc21ebu) * sourcePower[1];
			uint node = 1;
			while (node < leafCount)
			{
				float left = sourcePower[node * 2];
				node = node * 2 + uint(target >= left);
				if (target >= left)
					target -= left;
			}
			sourceIndex = node - leafCount;
		}
		else if (choice == 1)
			sourceIndex = min(uint(remasterRandom(random ^ 0x02e5be93u) * float(count)), count - 1);
		else
		{
			uint localIndex = min(uint(remasterRandom(random ^ 0x9d6ef916u) * float(localCount)), localCount - 1);
			uint localWidth = uint(high.x - low.x + 1);
			uint2 localPixel = uint2(low) + uint2(localIndex % localWidth, localIndex / localWidth);
			sourceIndex = localPixel.y * uniforms.width + localPixel.x;
		}
		if (sourceIndex >= count)
			continue;
		uint2 sourcePixel = uint2(sourceIndex % uniforms.width, sourceIndex / uniforms.width);
		if (all(sourcePixel == pixel) || participationField.read(sourcePixel).r < 0.5)
			continue;
		float power = sourcePower[leafCount + sourceIndex];
		if (power <= 0.0)
			continue;
		float localDensity = all(int2(sourcePixel) >= low) && all(int2(sourcePixel) <= high) ?
			1.0 / float(localCount) : 0.0;
		float probability = (power / sourcePower[1] + 1.0 / float(count) + localDensity) / 3.0;
		float4 sourceSurface = surfaceField.read(sourcePixel);
		float3 toSource = float3(float2(sourcePixel) - float2(pixel), sourceSurface.r - receiver.r);
		float distanceSquared = dot(toSource, toSource);
		if (distanceSquared <= 0.0)
			continue;
		float3 direction = toSource * rsqrt(distanceSquared);
		float receiverCosine = saturate(dot(receiver.gba, direction));
		float sourceCosine = saturate(dot(sourceSurface.gba, -direction));
		if (receiverCosine <= 0.0 || sourceCosine <= 0.0)
			continue;
		float visibility = remasterVisibility(receiverEndpoint,
			remasterSurfaceRayEndpoint(sourcePixel, sourceSurface, occlusion, heightField, mesh), occlusion, heightField,
			surfaceField, visibilityBlocks, mesh);
		float transfer = receiverCosine * sourceCosine * visibility / (M_PI_F * distanceSquared);
		incoming += previousBounce.read(sourcePixel).rgb * (transfer / probability);
	}
	float3 bounced = albedo * (incoming / float(connectionCount));
	if (batched) { partials[(grid.z * uniforms.height + pixel.y) * uniforms.width + pixel.x] = float4(incoming, 0.0); return; }
	nextBounce.write(float4(bounced, 1.0), pixel);
	nextIndirect.write(float4(accumulated + bounced, 1.0), pixel);
}

// Dedicated data-only presentation: no GI/normal/local-height passes required.
kernel void remasterPlacementOffsets(texture2d<float,access::read> saved [[texture(0)]],
	texture2d<float,access::write> output [[texture(1)]], uint2 p [[thread_position_in_grid]]) {
	if(p.x>=output.get_width() || p.y>=output.get_height())return;
	float2 v=saved.read(p).rg;
	float gray=saturate((v.r+128.0)/256.0);
	output.write(float4(v.g>0.5?float3(gray):float3(0.22,0.0,0.28),1.0),p);
}

kernel void remasterDirectLighting(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> occlusion [[texture(1)]],
	texture2d<float, access::write> output [[texture(2)]],
	texture2d<float, access::read> emission [[texture(3)]],
	texture2d<float, access::read> heightField [[texture(4)]],
	texture2d<float, access::read> surfaceField [[texture(5)]],
	texture2d<float, access::write> directField [[texture(6)]],
	texture2d<float, access::read> participationField [[texture(7)]],
	texture2d<float, access::read> oppositeFacingField [[texture(8)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	constant RemasterGpuLight *lights [[buffer(1)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	float4 surface = surfaceField.read(pixel);
	float3 normal = surface.gba;
	float3 original = source.read(pixel).rgb;
	float2 participation = participationField.read(pixel).rg;
	if (participation.r < 0.5)
	{
		directField.write(float4(0.0), pixel);
		output.write(float4(original, 1.0), pixel);
		return;
	}
	float3 direct = 0.0;
	float3 ambient = remasterToLinear(original) * uniforms.originalSceneContribution;
	float4 authoredEmission = emission.read(pixel);
	float3 selfEmission = remasterToLinear(authoredEmission.rgb) * authoredEmission.a * (255.0 / 25.0) * remasterEmissionRadiance;
	// Authored glow replaces the flame's baked artwork brightness. Adding both
	// washes low-power orange emission out when the original highlight is white.
	if (any(selfEmission > 0.0)) ambient = 0.0;
	float3 composite = ambient + direct + remasterEmissionAppearance(selfEmission);
	directField.write(float4(direct + selfEmission, 1.0), pixel);
	if (uniforms.view == 5)
	{
		float2 field = heightField.read(pixel).rg;
		// RG32Float v2 retains signed effective heights; local profile bytes are unchanged.
		float signedHeight = field.r * 255.0;
		float heightValue = saturate((signedHeight - uniforms.heightPreviewRange.x) /
			max(1.0, uniforms.heightPreviewRange.y - uniforms.heightPreviewRange.x) *
			uniforms.heightPreviewMultiplier / 8.0);
		output.write(float4(field.g > 0.5 ? float3(heightValue) :
			float3(0.22, 0.0, 0.28), 1.0), pixel);
	}
	else if (uniforms.view == 1)
	{
		float2 field = occlusion.read(pixel).rg;
		float3 coverage = field.g > 0.5 ? mix(float3(0.0, 0.35, 0.05), float3(1.0, 0.05, 0.0), field.r) :
			float3(0.22, 0.0, 0.28);
		output.write(float4(coverage, 1.0), pixel);
	}
	else if (uniforms.view == 3 || uniforms.view == 8)
		output.write(float4(remasterCompositeToDisplay(direct + remasterEmissionAppearance(selfEmission)), 1.0), pixel);
	else if (uniforms.view == 6)
		output.write(float4(0.0, 0.0, 0.0, 1.0), pixel);
	else if (uniforms.view == 7)
		output.write(float4(normal * 0.5 + 0.5, 1.0), pixel);
	else if (uniforms.view == 9)
	{
		float3 debugEmission = remasterDebugSphereVisible(float2(pixel) + 0.5, surface.r, uniforms.debugPositionRadius) ?
			uniforms.debugColorIntensity.rgb * uniforms.debugColorIntensity.a * remasterEmissionRadiance : float3(0.0);
		for (uint i = 0; i < uniforms.lightCount; i++) {
			RemasterGpuLight light = lights[i];
			if (!light.hideSource && remasterDebugSphereVisible(float2(pixel) + 0.5, surface.r, float4(light.position, light.radius)))
				debugEmission += light.color * light.intensity * remasterEmissionRadiance;
		}
		output.write(float4(remasterCompositeToDisplay(remasterEmissionAppearance(selfEmission + debugEmission)), 1.0), pixel);
	}
	else
		output.write(float4(remasterCompositeToDisplay(composite), 1.0), pixel);
}

// Sources are invariant across receivers. Prepare only the actual planar/depth
// samples once, rather than reconstructing them for every screen pixel.
struct RemasterDirectSample
{
	float4 position;
	float4 endpoint;
	float4 normal;
	float4 radiance;
};

struct RemasterEmitterPatch { uint count; uint mode; uint members[256]; };
// High bit marks the sampled-only base record; low bits retain patch/stratum ABI.
constant uint remasterCollapsedDepthRecord = 0x80000000u;

static RemasterDirectSample remasterRepresentativeSample(uint2 source, uint excluded,
	texture2d<float, access::read> occlusion, texture2d<float, access::read> heightField,
	texture2d<float, access::read> surfaceField, texture2d<float, access::read> emissionSeed,
	const device RemasterMeshCell *mesh, const device RemasterEmitterPatch *patches)
{
	bool collapsed = (source.y & remasterCollapsedDepthRecord) != 0;
	source.y &= ~remasterCollapsedDepthRecord;
	const device RemasterEmitterPatch &patch = patches[source.y >> 2];
	float3 centroid = 0, radiance = 0; float weight = 0; uint count = 0;
	for (uint j = 0; j < patch.count; j++) {
		uint p = patch.members[j]; if (p == excluded) continue;
		uint2 pixel = uint2(p % surfaceField.get_width(), p / surfaceField.get_width());
		float3 value = emissionSeed.read(pixel).rgb;
		float power = max(value.r,max(value.g,value.b));
		centroid += float3(float2(pixel)+0.5,surfaceField.read(pixel).r)*power;
		radiance += value; weight += power; count++;
	}
	RemasterDirectSample sample = {};
	if (!count || weight <= 0) return sample;
	centroid /= weight;
	uint representative = source.x; float nearest = INFINITY;
	for (uint j = 0; j < patch.count; j++) {
		uint p = patch.members[j]; if (p == excluded) continue;
		uint2 pixel = uint2(p % surfaceField.get_width(), p / surfaceField.get_width());
		float3 position = float3(float2(pixel)+0.5,surfaceField.read(pixel).r);
		float d = dot(position-centroid,position-centroid);
		if (d < nearest) { nearest = d; representative = p; }
	}
	uint2 pixel = uint2(representative % surfaceField.get_width(), representative / surfaceField.get_width());
	float4 surface = surfaceField.read(pixel); float depth = mesh[representative].emissionDepth;
	// Layout stays fixed even if receiver exclusion changes the representative's depth.
	float fraction = !collapsed && (patch.mode & 2u) ? 0.25 : 1.0;
	float3 offset = !collapsed && depth > 0 ? surface.gba*depth*(float(source.y & 3)+0.5)/4 : float3(0);
	sample.position = float4(float3(float2(pixel)+0.5,surface.r)+offset,
		depth > 0 ? 1.0 : mesh[representative].emissionOmnidirectional > 0.5 ? 2.0 : 0.0);
	sample.endpoint = float4(remasterEmissionRayEndpoint(pixel,surface,occlusion,heightField,mesh)+offset,weight);
	sample.normal = float4(surface.gba,collapsed ? depth : -depth);
	sample.radiance = float4(radiance*fraction,float(count)*fraction);
	return sample;
}

kernel void remasterPrepareEmitterPatches(
	texture2d<float, access::read> occlusion [[texture(0)]],
	texture2d<float, access::read> heightField [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::read> emissionSeed [[texture(3)]],
	const device uint2 *sources [[buffer(0)]],
	device RemasterDirectSample *samples [[buffer(1)]],
	const device RemasterMeshCell *mesh [[buffer(2)]],
	constant uint &count [[buffer(3)]],
	const device RemasterEmitterPatch *patches [[buffer(4)]],
	uint index [[thread_position_in_grid]])
{
	if (index >= count) return;
	uint2 source = sources[index];
	if (patches[(source.y & ~remasterCollapsedDepthRecord) >> 2].mode & 1u) {
		samples[index] = remasterRepresentativeSample(source, 0xffffffffu, occlusion, heightField,
			surfaceField, emissionSeed, mesh, patches); return;
	}
	bool collapsed = (source.y & remasterCollapsedDepthRecord) != 0;
	source.y &= ~remasterCollapsedDepthRecord;
	RemasterEmitterPatch patch = patches[source.y >> 2];
	float3 position = 0, endpoint = 0, normal = 0, radiance = 0;
	float weight = 0;
	float depth = mesh[source.x].emissionDepth;
	for (uint i = 0; i < patch.count; i++) {
		uint p = patch.members[i];
		uint2 pixel = uint2(p % surfaceField.get_width(), p / surfaceField.get_width());
		float4 surface = surfaceField.read(pixel);
		float3 value = emissionSeed.read(pixel).rgb;
		float power = max(value.r, max(value.g, value.b));
		float3 offset = depth > 0 && !collapsed ? surface.gba * depth * (float(source.y & 3) + 0.5) / 4.0 : float3(0);
		position += (float3(float2(pixel) + 0.5, surface.r) + offset) * power;
		endpoint += (remasterEmissionRayEndpoint(pixel, surface, occlusion, heightField, mesh) + offset) * power;
		normal += surface.gba * power;
		radiance += value;
		weight += power;
	}
	float fraction = depth > 0 && !collapsed ? 0.25 : 1.0;
	float area = float(patch.count) * fraction;
	RemasterDirectSample sample;
	sample.position = float4(position / max(weight, 1e-20),
		depth > 0 ? 1.0 : mesh[source.x].emissionOmnidirectional > 0.5 ? 2.0 : 0.0);
	sample.endpoint = float4(endpoint / max(weight, 1e-20), weight);
	sample.normal = float4(normal / max(weight, 1e-20), collapsed ? depth : -depth);
	sample.radiance = float4(radiance * fraction, area);
	samples[index] = sample;
}

// Remove only the receiving member from an aggregated source. Cluster members
// share normal/height/depth, so removing its energy and centroid weight is cheap.
static RemasterDirectSample remasterExcludeEmitterReceiver(RemasterDirectSample sample,
	uint2 source, uint receiverIndex, float4 receiverSurface,
	texture2d<float, access::read> emissionSeed, texture2d<float, access::read> surfaceField,
	texture2d<float, access::read> occlusion, texture2d<float, access::read> heightField,
	const device RemasterMeshCell *mesh, const device RemasterEmitterPatch *patches)
{
	uint2 receiverPixel = uint2(receiverIndex % emissionSeed.get_width(), receiverIndex / emissionSeed.get_width());
	if (all(emissionSeed.read(receiverPixel).rgb <= 0)) return sample;
	bool collapsed = (source.y & remasterCollapsedDepthRecord) != 0;
	source.y &= ~remasterCollapsedDepthRecord;
	const device RemasterEmitterPatch &patch = patches[source.y >> 2];
	for (uint i = 0; i < patch.count; i++) if (patch.members[i] == receiverIndex) {
		if (patch.mode & 1u) return remasterRepresentativeSample(
			uint2(source.x, source.y | (collapsed ? remasterCollapsedDepthRecord : 0u)), receiverIndex,
			occlusion, heightField, surfaceField, emissionSeed, mesh, patches);
		uint2 pixel = uint2(receiverIndex % emissionSeed.get_width(), receiverIndex / emissionSeed.get_width());
		float3 value = emissionSeed.read(pixel).rgb;
		float power = max(value.r, max(value.g, value.b));
		float remaining = sample.endpoint.w - power;
		float depth = mesh[receiverIndex].emissionDepth;
		float fraction = depth > 0 && !collapsed ? 0.25 : 1.0;
		if (remaining <= 0 || patch.count == 1) { sample.radiance = 0; return sample; }
		float3 offset = depth > 0 && !collapsed ? receiverSurface.gba * depth * (float(source.y & 3) + 0.5) / 4.0 : float3(0);
		sample.position.xyz = (sample.position.xyz * sample.endpoint.w -
			(float3(float2(pixel)+0.5, receiverSurface.r)+offset)*power) / remaining;
		sample.endpoint.xyz = (sample.endpoint.xyz * sample.endpoint.w -
			(remasterEmissionRayEndpoint(pixel, receiverSurface, occlusion, heightField, mesh)+offset)*power) / remaining;
		sample.endpoint.w = remaining;
		sample.radiance.rgb = max(float3(0), sample.radiance.rgb - value*fraction);
		sample.radiance.w -= fraction;
		break;
	}
	return sample;
}

// The app specializes its normal direct pass; the unspecialized function is
// retained for reference transport and diagnostics. Compile-time branches keep
// radial indirect sampling/diagnostics out of the production direct shader.
constant bool remasterPreparedDirectPass [[function_constant(0)]];
constant bool remasterBatchedDirectPass [[function_constant(1)]];
constant bool remasterClusteredDirectPass [[function_constant(3)]];
constant bool remasterAdaptiveSpherePass [[function_constant(4)]];
constant bool remasterAdaptiveDepthPass [[function_constant(6)]];
constant bool remasterHoistReceiverEmission [[function_constant(7)]];

kernel void remasterPrepareDirectSamples(
	texture2d<float, access::read> occlusion [[texture(0)]],
	texture2d<float, access::read> heightField [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::read> emissionSeed [[texture(3)]],
	const device uint2 *sources [[buffer(0)]],
	device RemasterDirectSample *samples [[buffer(1)]],
	const device RemasterMeshCell *mesh [[buffer(2)]],
	constant uint &count [[buffer(3)]],
	uint index [[thread_position_in_grid]])
{
	if (index >= count) return;
	uint2 source = sources[index];
	bool collapsed = (source.y & remasterCollapsedDepthRecord) != 0;
	source.y &= ~remasterCollapsedDepthRecord;
	uint2 pixel = uint2(source.x % surfaceField.get_width(), source.x / surfaceField.get_width());
	float4 surface = surfaceField.read(pixel);
	float depth = mesh[source.x].emissionDepth;
	float3 offset = depth > 0.0 && !collapsed ? surface.gba * depth * (float(source.y) + 0.5) / 4.0 : float3(0.0);
	RemasterDirectSample sample;
	sample.position = float4(float3(float2(pixel) + 0.5, surface.r) + offset,
		depth > 0.0 ? 1.0 : mesh[source.x].emissionOmnidirectional > 0.5 ? 2.0 : 0.0);
	sample.endpoint = float4(remasterEmissionRayEndpoint(pixel, surface, occlusion, heightField, mesh) + offset, 0.0);
	sample.normal = float4(surface.gba, collapsed ? depth : 0.0);
	sample.radiance = float4(emissionSeed.read(pixel).rgb * (depth > 0.0 && !collapsed ? 0.25 : 1.0), 0.0);
	samples[index] = sample;
}

// Cheap unoccluded weight shared by the two sampled-Direct scans. The exact
// normalization includes every source, including depth samples' quarter area.
static float remasterDirectFormFactor(float3 receiver, float3 normal,
	bool oppositeFacing, RemasterDirectSample sample)
{
	float3 delta = sample.position.xyz - receiver;
	float distanceSquared = dot(delta, delta);
	float3 direction = delta * rsqrt(max(distanceSquared, 0.0001));
	float response = saturate(dot(normal, direction));
	if (oppositeFacing) response = max(response, saturate(dot(float3(-normal.xy, normal.z), direction)));
	float sourceResponse = sample.position.w > 0.5 ? 1.0 : saturate(dot(sample.normal.xyz, -direction));
	return (sample.position.w == 3.0 ? sample.endpoint.w :
		min(0.25, 1.0 / (M_PI_F * (distanceSquared + 1.0)))) * response * sourceResponse;
}

// Receiver-specific sphere quadrature joins the same importance CDF as authored
// pixels. Only selected entries trace visibility; energy/solid-angle weights match
// exhaustive adaptive sphere transport, including receivers inside the sphere.
static uint remasterSampledSphereBudget(RemasterGpuLight light, float3 receiver, bool full)
{
	if (full) return 128;
	float3 delta = light.position - receiver;
	float angular = light.radius * light.radius / max(dot(delta, delta), 0.000001);
	uint samples = angular < 0.0001 ? 8 : angular < 0.000625 ? 16 :
		angular < 0.0025 ? 32 : angular < 0.01 ? 64 : 128;
	return min(light.sampleCount, samples);
}

struct RemasterSampledSphereState
{
	uint first, end, nextLight;
	float3 delta, axis, tangent, bitangent, radiance;
	float distanceSquared, radiusSquared, extent;
	bool inside;
};

static RemasterDirectSample remasterSampledSphere(uint index, float3 receiver,
	constant RemasterLightingUniforms &uniforms, const device RemasterGpuLight *lights,
	thread RemasterSampledSphereState &state)
{
	RemasterDirectSample sample = {};
	bool full = uniforms.heightPreviewRange.w > 0.5;
	// Each CDF scan visits indices in order. Cache the cone/basis once per light,
	// rather than rebuilding it and searching the light list for every stratum.
	if (index >= state.end) {
		RemasterGpuLight light = lights[state.nextLight++];
		state.first = state.end;
		state.end += remasterSampledSphereBudget(light, receiver, full);
		state.delta = light.position - receiver;
		state.distanceSquared = dot(state.delta, state.delta);
		state.radiusSquared = light.radius * light.radius;
		state.inside = state.distanceSquared < state.radiusSquared;
		state.axis = state.distanceSquared > 0.000001 ? state.delta * rsqrt(state.distanceSquared) : float3(0,0,1);
		state.tangent = normalize(cross(abs(state.axis.z) < 0.9 ? float3(0,0,1) : float3(0,1,0), state.axis));
		state.bitangent = cross(state.axis, state.tangent);
		float sinSquared = min(1.0, state.radiusSquared / max(state.distanceSquared, 0.000001));
		state.extent = state.inside ? 2.0 : sinSquared / (1.0 + sqrt(max(0.0, 1.0 - sinSquared)));
		state.radiance = light.color * light.intensity * remasterEmissionRadiance;
	}
	uint local = index - state.first, samples = state.end - state.first;
	float offset = state.extent * (float(local) + 0.5) / float(samples);
	float cosine = 1.0 - offset, sineSquared = offset * (2.0 - offset);
	float angle = (float(local) + 0.5) * 2.39996323;
	float3 ray = state.axis * cosine + sqrt(max(0.0, sineSquared)) * (state.tangent * cos(angle) + state.bitangent * sin(angle));
	float projected = dot(state.delta, ray);
	float root = sqrt(max(0.0, state.radiusSquared - state.distanceSquared * sineSquared));
	float distance = state.inside ? projected + root : (state.distanceSquared - state.radiusSquared) / max(projected + root, 0.000001);
	sample.position = float4(receiver + ray * max(distance, 0.00001), 3.0);
	sample.endpoint = float4(sample.position.xyz, 2.0 * state.extent / float(samples));
	sample.radiance = float4(state.radiance, 0);
	return sample;
}

static float4 remasterSampledDepthFactors(float3 receiver, float3 normal,
	bool oppositeFacing, RemasterDirectSample sample)
{
	if (sample.normal.w <= 0) return float4(remasterDirectFormFactor(receiver, normal, oppositeFacing, sample));
	// Evaluate the four cheap strata together without fetching four source records.
	float4 depths = float4(0.125, 0.375, 0.625, 0.875) * sample.normal.w;
	float4 dx = sample.position.x + sample.normal.x * depths - receiver.x;
	float4 dy = sample.position.y + sample.normal.y * depths - receiver.y;
	float4 dz = sample.position.z + sample.normal.z * depths - receiver.z;
	float4 distanceSquared = dx*dx + dy*dy + dz*dz;
	float4 inverseDistance = rsqrt(max(distanceSquared, float4(0.0001)));
	float4 xy = normal.x * dx + normal.y * dy;
	float4 z = normal.z * dz;
	float4 response = saturate((xy + z) * inverseDistance);
	if (oppositeFacing) response = max(response, saturate((-xy + z) * inverseDistance));
	return min(float4(0.25), 1.0 / (M_PI_F * (distanceSquared + 1.0))) * response;
}

kernel void remasterSampledDirect(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> occlusion [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::read> heightField [[texture(3)]],
	texture2d<float, access::read> participationField [[texture(4)]],
	texture2d<float, access::read> emissionSeed [[texture(5)]],
	texture2d<float, access::write> direct [[texture(7)]],
	texture2d<float, access::write> indirect [[texture(8)]],
	texture2d<float, access::read> oppositeFacingField [[texture(9)]],
	texture2d<float, access::read> visibilityBlocks [[texture(10)]],
	texture2d<float, access::read> reflectanceField [[texture(11)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	const device uint2 *sources [[buffer(1)]],
	constant uint &count [[buffer(2)]],
	const device RemasterMeshCell *mesh [[buffer(3)]],
	const device RemasterDirectSample *samples [[buffer(4)]],
	const device RemasterEmitterPatch *patches [[buffer(6)]],
	const device RemasterGpuLight *sceneLights [[buffer(8)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height) return;
	float2 participation = participationField.read(pixel).rg;
	if (participation.r < 0.5 || participation.g < 0.5 || (count == 0 && uniforms.lightCount == 0))
	{
		direct.write(float4(0.0), pixel);
		indirect.write(float4(0.0, 0.0, 0.0, 1.0), pixel);
		return;
	}
	float4 surface = surfaceField.read(pixel);
	float3 receiver = float3(float2(pixel) + 0.5, surface.r);
	uint totalCount = count;
	for (uint i = 0; i < uniforms.lightCount; i++)
		totalCount += remasterSampledSphereBudget(sceneLights[i], receiver, uniforms.heightPreviewRange.w > 0.5);
	bool oppositeFacing = oppositeFacingField.read(pixel).r > 0.5;
	uint receiverIndex = pixel.y * uniforms.width + pixel.x;
	float totalFormFactor = 0.0, totalWeight = 0.0;
	RemasterSampledSphereState sphereState = {};
	for (uint i = 0; i < totalCount; i++)
	{
		RemasterDirectSample sample = i < count ? samples[i] : remasterSampledSphere(i-count, receiver, uniforms, sceneLights, sphereState);
		if (i < count && sample.radiance.w > 0 && sources[i].x != 0xffffffffu) sample = remasterExcludeEmitterReceiver(sample, sources[i], receiverIndex,
			surface, emissionSeed, surfaceField, occlusion, heightField, mesh, patches);
		else if (i < count && sources[i].x == receiverIndex) continue;
		if (all(sample.radiance.rgb <= 0)) continue;
		float factor = dot(remasterSampledDepthFactors(receiver, surface.gba, oppositeFacing, sample), float4(0.25));
		totalFormFactor += factor * (sample.radiance.w > 0 ? sample.radiance.w : sample.normal.w > 0 ? 1.0 : sample.position.w == 1.0 ? 0.25 : 1.0);
		totalWeight += factor * max(sample.radiance.r, max(sample.radiance.g, sample.radiance.b));
	}
	float3 incoming = 0.0;
	if (totalWeight > 0.0 && isfinite(totalWeight))
	{
		uint connections = max(1u, uniforms.sampleCount);
		uint randomBase = uniforms.randomSeed ^ (pixel.x * 0x9e3779b9u) ^ (pixel.y * 0x85ebca6bu);
		uint connection = 0;
		// Stratified CDF targets are sorted: scan emitters once, not once per ray.
		float target = remasterRandom(remasterHash(randomBase)) * totalWeight / float(connections);
		float cumulative = 0.0;
		float3 endpoint = remasterSurfaceRayEndpoint(pixel, surface, occlusion, heightField, mesh);
		sphereState = {};
		for (uint i = 0; i < totalCount && connection < connections; i++)
		{
			RemasterDirectSample sample = i < count ? samples[i] : remasterSampledSphere(i-count, receiver, uniforms, sceneLights, sphereState);
			if (i < count && sample.radiance.w > 0 && sources[i].x != 0xffffffffu) sample = remasterExcludeEmitterReceiver(sample, sources[i], receiverIndex,
				surface, emissionSeed, surfaceField, occlusion, heightField, mesh, patches);
			else if (i < count && sources[i].x == receiverIndex) continue;
			if (all(sample.radiance.rgb <= 0)) continue;
			float4 factors = remasterSampledDepthFactors(receiver, surface.gba, oppositeFacing, sample);
			float factor = dot(factors, float4(0.25));
			float power = max(sample.radiance.r, max(sample.radiance.g, sample.radiance.b));
			float weight = factor * power;
			cumulative += weight;
			if (weight <= 0.0 || target >= cumulative) continue;
			uint4 multiplicities = uint4(0);
			while (connection < connections && target < cumulative)
			{
				uint stratum = 0;
				if (sample.normal.w > 0) {
					float depthTarget = remasterRandom(remasterHash(randomBase ^ 0xa511e9b3u ^
						(connection * 0x63d83595u))) * (factor * 4.0);
					float depthCumulative = factors[0];
					while (stratum < 3 && depthTarget >= depthCumulative) depthCumulative += factors[++stratum];
				}
				multiplicities[stratum]++;
				connection++;
				uint random = remasterHash(randomBase ^ (connection * 0x27d4eb2du));
				target = (float(connection) + remasterRandom(random)) * totalWeight / float(connections);
			}
			for (uint k = 0; k < 4; k++) if (multiplicities[k]) {
				float3 depthOffset = sample.normal.xyz * max(0.0, sample.normal.w) * (float(k) + 0.5) / 4.0;
				uint2 sourcePixel = uint2(clamp(floor(sample.endpoint.xy), float2(0.0),
					float2(uniforms.width - 1, uniforms.height - 1)));
				float3 sourceEndpoint = sample.endpoint.xyz + depthOffset;
				if (sample.position.w == 2.0) sourceEndpoint = remasterOmnidirectionalRayEndpoint(
					sourceEndpoint, sourcePixel, receiver, heightField, mesh);
				float visibility = remasterVisibility(endpoint, sourceEndpoint,
					occlusion, heightField, surfaceField, visibilityBlocks, mesh);
				// Joint source/stratum probability cancels its form factor.
				incoming += (sample.radiance.rgb / power) * visibility *
					(totalWeight * float(multiplicities[k]) / float(connections));
			}
		}
	}
	if (totalFormFactor > 0.95) incoming *= 0.95 / totalFormFactor;
	float4 authored = reflectanceField.read(pixel);
	float3 albedo = authored.a > 0.5 ? saturate(authored.rgb) : remasterAlbedo(source.read(pixel).rgb);
	albedo = remasterBoostReflectance(albedo, uniforms.reflectanceBoost);
	direct.write(float4(albedo * incoming, 1.0), pixel);
	indirect.write(float4(0.0, 0.0, 0.0, 1.0), pixel);
}

struct PRVEntry { float visibility; uint receiverVersion, sampleVersion, frame; };
struct PRVEndpoint { float4 point; uint4 version; };
struct PRVSample { float4 position, endpoint, normal; uint4 version; };
struct PRVBox { float4 low, high; };
static bool prvSegment(float3 a,float3 b, PRVBox box) {
    float3 d=b-a; float lo=0,hi=1;
    for(uint k=0;k<3;k++) {
        if(d[k]==0) {if(a[k]<box.low[k] || a[k]>box.high[k])return false;}
        else {float x=(box.low[k]-a[k])/d[k],y=(box.high[k]-a[k])/d[k];lo=max(lo,min(x,y));hi=min(hi,max(x,y));}
    }
    return lo<=hi;
}
// SAT between convex hull of two XY boxes and an axis-aligned changed box.
// Testing extra (non-edge) axes cannot reject an actual intersection.
static bool prvHull(float3 al,float3 ah,float3 bl,float3 bh,PRVBox c) {
    if(min(al.z,bl.z)>c.high.z || max(ah.z,bh.z)<c.low.z)return false;
    float2 p[8]={al.xy,float2(ah.x,al.y),ah.xy,float2(al.x,ah.y),
        bl.xy,float2(bh.x,bl.y),bh.xy,float2(bl.x,bh.y)};
    if(min(al.x,bl.x)>c.high.x || max(ah.x,bh.x)<c.low.x ||
       min(al.y,bl.y)>c.high.y || max(ah.y,bh.y)<c.low.y)return false;
    for(uint i=0;i<4;i++)for(uint j=4;j<8;j++) {
        float2 d=p[j]-p[i],n=float2(-d.y,d.x);
        float low=INFINITY,high=-INFINITY;
        for(uint k=0;k<8;k++){float v=dot(n,p[k]);low=min(low,v);high=max(high,v);}
        float2 center=(c.low.xy+c.high.xy)*0.5, extent=(c.high.xy-c.low.xy)*0.5;
        float middle=dot(n,center),radius=dot(abs(n),extent);
        float pad=0.001*(abs(n.x)+abs(n.y)+1);
        if(low>middle+radius+pad || high<middle-radius-pad)return false;
    }
    return true;
}
kernel void prvUpdateReceivers(texture2d<float,access::read> surface [[texture(0)]],
    texture2d<float,access::read> occlusion [[texture(1)]], texture2d<float,access::read> height [[texture(2)]],
    const device RemasterMeshCell *mesh [[buffer(0)]], device PRVEndpoint *endpoints [[buffer(1)]],
    constant uint4 &config [[buffer(2)]],constant int4 &room [[buffer(3)]],const device uint *pixelMap [[buffer(4)]],uint2 p [[thread_position_in_grid]]) {
    if(p.x>=surface.get_width() || p.y>=surface.get_height())return;
    uint index=p.y*surface.get_width()+p.x; float4 s=surface.read(p);
    float4 point=float4(remasterSurfaceRayEndpoint(p,s,occlusion,height,mesh),s.r);
    if(room.z){point.xy+=float2(room.xy);index=pixelMap[index];if(index==0xffffffffu)return;}
    PRVEndpoint old=endpoints[index];
    if(config.y || any(as_type<uint4>(point)!=as_type<uint4>(old.point))) {
        old.point=point; old.version.x++; if(old.version.x==0)old.version.x=1;
        endpoints[index]=old;
    }
}
kernel void prvUpdateSamples(const device RemasterDirectSample *samples [[buffer(0)]],
    const device uint *columns [[buffer(1)]],device PRVSample *versions [[buffer(2)]],
    constant uint4 &config [[buffer(3)]],constant int4 &room [[buffer(4)]],uint i [[thread_position_in_grid]]) {
    if(i>=config.z)return;uint column=columns[i];if(column==0xffffffffu)return;PRVSample old=versions[column];RemasterDirectSample s=samples[i];
    if(room.z){s.position.xy+=float2(room.xy);s.endpoint.xy+=float2(room.xy);}
    if(config.y || any(as_type<uint4>(s.position)!=as_type<uint4>(old.position)) ||
        any(as_type<uint4>(s.endpoint)!=as_type<uint4>(old.endpoint)) || any(as_type<uint4>(s.normal)!=as_type<uint4>(old.normal))) {
        old.position=s.position;old.endpoint=s.endpoint;old.normal=s.normal;
        old.version.x++;if(!old.version.x)old.version.x=1;versions[column]=old;
    }
}
kernel void prvGroups(const device PRVEndpoint *receivers [[buffer(0)]],
    const device RemasterDirectSample *samples [[buffer(1)]],const device PRVBox *changes [[buffer(2)]],
    device uint *dirty [[buffer(3)]],constant uint4 &config [[buffer(4)]],constant uint2 &dims [[buffer(5)]],
    uint2 index [[thread_position_in_grid]]) {
    uint tilesX=(dims.x+7)/8,tilesY=(dims.y+7)/8;
    if(index.x>=tilesX*tilesY || index.y>=config.z)return;
    uint tile=index.x; float3 low=INFINITY,high=-INFINITY;
    for(uint y=tile/tilesX*8;y<min(dims.y,tile/tilesX*8+8);y++)
        for(uint x=tile%tilesX*8;x<min(dims.x,tile%tilesX*8+8);x++) {
            float3 p=receivers[y*dims.x+x].point.xyz;low=min(low,p);high=max(high,p);
        }
    // The receiver box contains all exact biased endpoints, not just raster XY.
    // One-cell XY padding covers DDA corner/rounding and origin-cell evaluation.
    low-=float3(1,1,0.001);high+=float3(1,1,0.001);
    RemasterDirectSample s=samples[index.y];float3 sl=s.endpoint.xyz,sh=sl;
    // Adaptive depth moves a stratum anywhere inside the original source volume.
    if(s.normal.w<0) {float3 delta=s.normal.xyz*(-s.normal.w);sl-=abs(delta);sh+=abs(delta);}
    sl-=float3(1,1,0.001);sh+=float3(1,1,0.001);
    uint touched=0;
    for(uint c=0;c<config.w;c++)if(prvHull(low,high,sl,sh,changes[c])){touched=1;break;}
    dirty[index.y*tilesX*tilesY+tile]=touched;
}

kernel void prvChunkGroups(const device PRVSample *samples [[buffer(0)]],const device uint *columns [[buffer(1)]],
    const device PRVBox *changes [[buffer(2)]],device uint2 *stamps [[buffer(3)]],
    const device int4 *chunks [[buffer(4)]],constant uint4 &config [[buffer(5)]],
    constant uint4 &layout [[buffer(6)]],uint2 p [[thread_position_in_grid]]) {
    if(p.x>=layout.x || p.y>=config.z)return;
    int4 chunk=chunks[p.x];if(!chunk.w)return;uint col=columns[p.y];if(col>=layout.y)return;
    uint at=col*layout.x+p.x;uint2 stamp=stamps[at];
    if(config.y || stamp.x<uint(chunk.z))stamp=uint2(0,uint(chunk.z));
    float3 a=float3(float2(chunk.xy),-1e20f),b=float3(float2(chunk.xy+8),1e20f);
    PRVSample source=samples[col];float3 endpoint=source.endpoint.xyz;
    float2 depth=source.normal.w<0?abs(source.normal.xy*(-source.normal.w)):float2(0);
    float2 lo=min(a.xy-2,endpoint.xy-depth-2),hi=max(b.xy+2,endpoint.xy+depth+2);
    uint low=0,high=config.w;
    while(low<high){uint mid=(low+high)/2;if(changes[mid].low.w<=float(stamp.x))low=mid+1;else high=mid;}
    for(uint i=config.w;i>low;) {
        PRVBox c=changes[--i];
        if(all(hi>=c.low.xy) && all(lo<=c.high.xy) &&
            prvHull(a-float3(2,2,0),b+float3(2,2,0),endpoint-float3(depth+2,0),endpoint+float3(depth+2,0),c)) {
            stamp.y=max(stamp.y,uint(c.low.w));break;
        }
    }
    stamp.x=layout.z;stamps[at]=stamp;
}

// The prepared sample endpoint may be receiver-adapted, but only as a pure
// function of the versioned receiver/sample geometry. Exceptional endpoints trace.
static float remasterCachedVisibility(float3 from, float3 to, uint receiver,
 uint sampleIndex, bool eligible, constant uint4 &config, constant uint2 &generation,
 device PRVEntry *entries, const device PRVEndpoint *receivers,
 const device PRVSample *samples, const device uint *columns,
 const device PRVBox *changes, const device uint *dirty, device atomic_uint *counters,
 const device uint *pixelMap,const device uint *births,constant int4 &room,constant uint2 &layout,
 constant RemasterLightingUniforms &uniforms,
 texture2d<float, access::read> occlusion, texture2d<float, access::read> height,
 texture2d<float, access::read> surface, texture2d<float, access::read> blocks,
 const device RemasterMeshCell *mesh)
{
 if (!config.x || !eligible)
  return remasterVisibility(from,to,occlusion,height,surface,blocks,mesh);
 uint column=columns[sampleIndex];
 if(room.z){receiver=pixelMap[receiver];if(receiver==0xffffffffu || column==0xffffffffu)
  return remasterVisibility(from,to,occlusion,height,surface,blocks,mesh);}
 uint address=column*(room.z?layout.x:uniforms.width*uniforms.height)+receiver;
 PRVEntry entry=entries[address];
 uint rv=receivers[receiver].version.x, sv=samples[column].version.x;
 bool valid=!config.y && entry.receiverVersion==rv && entry.sampleVersion==sv &&
  (room.z?entry.frame>=max(uint(room.w),births[receiver/64]):(entry.frame==generation.x || entry.frame==generation.y));
 if(room.z && valid) {
  uint through=((const device uint2*)dirty)[column*(layout.x/64)+receiver/64].y;
  if(entry.frame<through) {
   float3 a=from,b=to;a.xy+=float2(room.xy);b.xy+=float2(room.xy);
   uint low=0,high=config.w;
   while(low<high){uint mid=(low+high)/2;if(changes[mid].low.w<=float(entry.frame))low=mid+1;else high=mid;}
   for(uint c=low;c<config.w;c++)if(prvSegment(a,b,changes[c])){valid=false;break;}
  }
 }
 if (!room.z && valid && entry.frame!=generation.x && config.w) {
  uint tilesX=(uniforms.width+7)/8, tilesY=(uniforms.height+7)/8;
  uint tile=(receiver/uniforms.width/8)*tilesX+(receiver%uniforms.width/8);
  if (dirty[sampleIndex*tilesX*tilesY+tile]) {
   for (uint c=0;c<config.w;c++) if (prvSegment(from,to,changes[c])) {valid=false;break;}
  }
 }
 if (!valid) entry.visibility=remasterVisibility(from,to,occlusion,height,surface,blocks,mesh);
 if (config.x & 256u) {
  // Stripe atomics by receiver: diagnostics must not serialize millions of rays
  // through one global cache-line pair. Each receiver still has one writer/batch.
  uint stripe=(receiver&255u)*6;
  atomic_fetch_add_explicit(counters+stripe,1u,memory_order_relaxed);
  if (!valid) {
   atomic_fetch_add_explicit(counters+stripe+1,1u,memory_order_relaxed);
   uint cause=entry.receiverVersion!=rv ? 2u : entry.sampleVersion!=sv ? 3u :
    !(entry.frame==generation.x || entry.frame==generation.y) ? 4u : 5u;
   atomic_fetch_add_explicit(counters+stripe+cause,1u,memory_order_relaxed);
  }
 }
 entry.receiverVersion=rv;entry.sampleVersion=sv;entry.frame=generation.x;
 entries[address]=entry;
 return entry.visibility;
}

// Expanded records match exhaustive receiver-adaptive Direct. The exact subset
// is visibility-independent; stratified samples estimate only the weak tail.
struct RemasterHybridConfig {
 float tau; uint tailSamples, seed, clustered;
 uint counts; float exactRadius; uint reserved0, reserved1;
};

static RemasterDirectSample remasterHybridSample(uint i, uint count, float3 receiver,
 uint receiverIndex, float4 surface, bool emittingReceiver,
 constant RemasterHybridConfig &config, constant RemasterLightingUniforms &uniforms,
 const device uint2 *sources, const device RemasterDirectSample *samples,
 const device RemasterEmitterPatch *patches, const device RemasterGpuLight *lights,
 texture2d<float, access::read> emission, texture2d<float, access::read> surfaces,
 texture2d<float, access::read> occlusion, texture2d<float, access::read> heights,
 const device RemasterMeshCell *mesh, thread RemasterSampledSphereState &sphereState)
{
 if (i >= count) return remasterSampledSphere(i-count, receiver, uniforms, lights, sphereState);
 RemasterDirectSample sample = samples[i];
 uint2 record = sources[i];
 if (config.clustered && emittingReceiver && record.x != 0xffffffffu)
  sample = remasterExcludeEmitterReceiver(sample, record, receiverIndex,
   surface, emission, surfaces, occlusion, heights, mesh, patches);
 if (!config.clustered && record.x == receiverIndex) { sample.radiance = 0; return sample; }
 if ((config.clustered || record.x == 0xffffffffu) && sample.normal.w < 0) {
  float depth = -sample.normal.w;
  uint stratum = record.y & 3u;
  float3 axis = sample.normal.xyz * depth;
  float3 center = sample.position.xyz - axis * (float(stratum)+0.5)/4.0 + axis*0.5;
  float ratio = depth*depth/max(dot(center-receiver, center-receiver), 0.0001);
  uint n = ratio < 0.000625 ? 1u : ratio < 0.0025 ? 2u : 4u;
  if (stratum >= n) { sample.radiance = 0; return sample; }
  float3 shift = axis*((float(stratum)+0.5)/float(n)-(float(stratum)+0.5)/4.0);
  sample.position.xyz += shift; sample.endpoint.xyz += shift;
  sample.radiance *= 4.0/float(n);
 }
 return sample;
}

kernel void remasterHybridDirect(
 texture2d<float, access::read> source [[texture(0)]],
 texture2d<float, access::read> occlusion [[texture(1)]],
 texture2d<float, access::read> surfaceField [[texture(2)]],
 texture2d<float, access::read> heightField [[texture(3)]],
 texture2d<float, access::read> participationField [[texture(4)]],
 texture2d<float, access::read> emissionSeed [[texture(5)]],
 texture2d<float, access::write> direct [[texture(7)]],
 texture2d<float, access::write> indirect [[texture(8)]],
 texture2d<float, access::read> oppositeFacingField [[texture(9)]],
 texture2d<float, access::read> visibilityBlocks [[texture(10)]],
 texture2d<float, access::read> reflectanceField [[texture(11)]],
 constant RemasterLightingUniforms &uniforms [[buffer(0)]],
 const device uint2 *sources [[buffer(1)]], constant uint &count [[buffer(2)]],
 const device RemasterMeshCell *mesh [[buffer(3)]],
 const device RemasterDirectSample *samples [[buffer(4)]],
 const device RemasterEmitterPatch *patches [[buffer(6)]],
 const device RemasterGpuLight *sceneLights [[buffer(8)]],
 constant RemasterHybridConfig &config [[buffer(9)]],
 device uint4 *statistics [[buffer(10)]],
 device PRVEntry *prvEntries [[buffer(11)]],
 const device PRVEndpoint *prvReceivers [[buffer(12)]],
 const device PRVSample *prvSamples [[buffer(13)]],
 const device uint *prvColumns [[buffer(14)]],
 const device PRVBox *prvChanges [[buffer(15)]],
 const device uint *prvDirty [[buffer(16)]],
 constant uint4 &prvConfig [[buffer(17)]],
 constant uint2 &prvGeneration [[buffer(18)]],
 device atomic_uint *prvCounters [[buffer(19)]],
 const device uint *prvPixelMap [[buffer(20)]],const device uint *prvBirths [[buffer(21)]],
 constant int4 &prvRoom [[buffer(22)]],constant uint2 &prvLayout [[buffer(23)]],
 uint2 pixel [[thread_position_in_grid]])
{
 if (pixel.x >= uniforms.width || pixel.y >= uniforms.height) return;
 uint index = pixel.y*uniforms.width+pixel.x;
 if (config.counts) statistics[index] = uint4(0);
 float2 participation = participationField.read(pixel).rg;
 if (participation.r < 0.5 || participation.g < 0.5) {
  direct.write(float4(0), pixel); indirect.write(float4(0,0,0,1), pixel); return;
 }
 float4 surface = surfaceField.read(pixel);
 float3 receiver = float3(float2(pixel)+0.5,surface.r);
 bool opposite = oppositeFacingField.read(pixel).r > 0.5;
 bool emitting = any(emissionSeed.read(pixel).rgb > 0);
 uint totalCount=count;
 for(uint j=0;j<uniforms.lightCount;j++)
  totalCount+=remasterSampledSphereBudget(sceneLights[j],receiver,uniforms.heightPreviewRange.w>0.5);
 float W=0, normalization=0, tailWeight=0;
 float3 incoming=0;
 uint eligible=0, exact=0, tailRays=0;
 float3 endpoint=remasterSurfaceRayEndpoint(pixel,surface,occlusion,heightField,mesh);
 // Pass 0 gets W and the exact exhaustive normalization. Pass 1 traces E and
 // sums T directly (no subtractive cancellation). Pass 2 samples only T.
 for(uint pass=0;pass<3;pass++) {
  RemasterSampledSphereState state={};
  uint connection=0;
  uint randomBase=config.seed ^ (pixel.x*0x9e3779b9u) ^ (pixel.y*0x85ebca6bu);
  float target=remasterRandom(remasterHash(randomBase))*tailWeight/float(config.tailSamples);
  float cumulative=0;
  if(pass==2 && !(tailWeight>0)) break;
  for(uint i=0;i<totalCount;i++) {
   RemasterDirectSample sample=remasterHybridSample(i,count,receiver,index,surface,emitting,
    config,uniforms,sources,samples,patches,sceneLights,emissionSeed,surfaceField,
    occlusion,heightField,mesh,state);
   if(all(sample.radiance.rgb<=0)) continue;
   float factor=remasterDirectFormFactor(receiver,surface.gba,opposite,sample);
   float power=max(sample.radiance.r,max(sample.radiance.g,sample.radiance.b));
   float weight=factor*power;
   if(pass==0) {
    normalization+=factor*(sample.radiance.w>0 ? sample.radiance.w : sample.position.w==1 ? 0.25 : 1.0);
    W+=weight; eligible+=weight>0; continue;
   }
   if(!(weight>0)) continue;
   bool isExact=weight>=config.tau*W || (config.exactRadius>0 &&
    distance(sample.position.xyz,receiver)<=config.exactRadius);
   float multiplier=1;
   if(pass==1) {
    if(!isExact) {tailWeight+=weight;continue;}
    exact++;
   } else {
    if(isExact) continue;
    cumulative+=weight;
    if(connection>=config.tailSamples || target>=cumulative) continue;
    uint multiplicity=0;
    while(connection<config.tailSamples && target<cumulative) {
     multiplicity++; connection++;
     target=(float(connection)+remasterRandom(remasterHash(randomBase ^
      (connection*0x27d4eb2du))))*tailWeight/float(config.tailSamples);
    }
    multiplier=tailWeight*float(multiplicity)/(float(config.tailSamples)*weight);
    tailRays++;
   }
   float3 sourceEndpoint=sample.endpoint.xyz;
   if(sample.position.w==2.0) sourceEndpoint=remasterOmnidirectionalRayEndpoint(
    sourceEndpoint,uint2(clamp(floor(sourceEndpoint.xy),float2(0),
     float2(uniforms.width-1,uniforms.height-1))),receiver,heightField,mesh);
   float visibility=config.reserved1 ? 1.0 : remasterCachedVisibility(endpoint,sourceEndpoint,index,i,
     i<count && !emitting && sample.position.w!=2.0,prvConfig,prvGeneration,
     prvEntries,prvReceivers,prvSamples,prvColumns,prvChanges,prvDirty,prvCounters,
     prvPixelMap,prvBirths,prvRoom,prvLayout,
     uniforms,occlusion,heightField,surfaceField,visibilityBlocks,mesh);
   incoming+=sample.radiance.rgb*factor*visibility*multiplier;
  }
 }
 if(normalization>0.95) incoming*=0.95/normalization;
 float4 authored=reflectanceField.read(pixel);
 float3 albedo=authored.a>0.5 ? saturate(authored.rgb) : remasterAlbedo(source.read(pixel).rgb);
 albedo=remasterBoostReflectance(albedo,uniforms.reflectanceBoost);
 direct.write(float4(albedo*incoming,1),pixel);
 indirect.write(float4(0,0,0,1),pixel);
 if(config.counts) statistics[index]=uint4(eligible,exact,tailRays,1);
}

// Independent of emitter budgets/batches; runs before the first indirect bounce.
kernel void remasterDirectionalLight(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> occlusion [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::read> heightField [[texture(3)]],
	texture2d<float, access::read> participationField [[texture(4)]],
	texture2d<float, access::read_write> direct [[texture(7)]],
	texture2d<float, access::read> oppositeFacingField [[texture(9)]],
	texture2d<float, access::read> visibilityBlocks [[texture(10)]],
	texture2d<float, access::read> reflectanceField [[texture(11)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	const device RemasterMeshCell *mesh [[buffer(3)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height) return;
	float2 participation = participationField.read(pixel).rg;
	if (participation.r < 0.5 || participation.g < 0.5) return;
	float4 surface = surfaceField.read(pixel);
	bool oppositeFacing = oppositeFacingField.read(pixel).r > 0.5;
	float3 incoming = 0;
	if (uniforms.directionalColorBrightness.w > 0)
	{
		float3 towardLight = -uniforms.directionalDirection.xyz;
		float cosine = saturate(dot(surface.gba, towardLight));
		if (oppositeFacing) cosine = max(cosine, saturate(dot(surface.gba * float3(-1,-1,1), towardLight)));
		if (cosine > 0)
		{
			float visibility = 1.0;
			// A vertical ray cannot cross another XY cell. Oblique rays use the mesh DDA.
			if (any(abs(towardLight.xy) > float2(0.000001)))
			{
				float3 origin = remasterSurfaceRayEndpoint(pixel, surface, occlusion, heightField, mesh);
				float distance = 1e7;
				if (towardLight.x > 0.000001) distance = min(distance, (float(uniforms.width) - origin.x) / towardLight.x);
				if (towardLight.x < -0.000001) distance = min(distance, -origin.x / towardLight.x);
				if (towardLight.y > 0.000001) distance = min(distance, (float(uniforms.height) - origin.y) / towardLight.y);
				if (towardLight.y < -0.000001) distance = min(distance, -origin.y / towardLight.y);
				visibility = remasterVisibility(origin, origin + towardLight * max(0.0, distance),
					occlusion, heightField, surfaceField, visibilityBlocks, mesh);
			}
			incoming += uniforms.directionalColorBrightness.rgb *
				(uniforms.directionalColorBrightness.w * cosine * visibility);
		}
	}
	float4 authored = reflectanceField.read(pixel);
	float3 albedo = authored.a > 0.5 ? saturate(authored.rgb) : remasterAlbedo(source.read(pixel).rgb);
	albedo = remasterBoostReflectance(albedo, uniforms.reflectanceBoost);
	direct.write(float4(direct.read(pixel).rgb + albedo * incoming, 1.0), pixel);
}

kernel void remasterIndirectBounce(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> occlusion [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::read> heightField [[texture(3)]],
	texture2d<float, access::read> participationField [[texture(4)]],
	texture2d<float, access::read> previousBounce [[texture(5)]],
	texture2d<float, access::read> previousIndirect [[texture(6)]],
	texture2d<float, access::write> nextBounce [[texture(7)]],
	texture2d<float, access::write> nextIndirect [[texture(8)]],
	texture2d<float, access::read> oppositeFacingField [[texture(9)]],
	texture2d<float, access::read> visibilityBlocks [[texture(10)]],
	texture2d<float, access::read> reflectanceField [[texture(11)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	const device uint *emitterPixels [[buffer(1)]],
	constant uint &emitterCount [[buffer(2)]],
	const device RemasterMeshCell *mesh [[buffer(3)]],
	const device RemasterDirectSample *directSamples [[buffer(4)]],
	device float4 *directPartials [[buffer(5)]],
	const device RemasterEmitterPatch *emitterPatches [[buffer(7)]],
	const device RemasterGpuLight *sceneLights [[buffer(8)]],

 device PRVEntry *prvEntries [[buffer(11)]],
 const device PRVEndpoint *prvReceivers [[buffer(12)]],
 const device PRVSample *prvSamples [[buffer(13)]],
 const device uint *prvColumns [[buffer(14)]],
 const device PRVBox *prvChanges [[buffer(15)]],
 const device uint *prvDirty [[buffer(16)]],
 constant uint4 &prvConfig [[buffer(17)]],
 constant uint2 &prvGeneration [[buffer(18)]],
 device atomic_uint *prvCounters [[buffer(19)]],
 const device uint *prvPixelMap [[buffer(20)]],const device uint *prvBirths [[buffer(21)]],
 constant int4 &prvRoom [[buffer(22)]],constant uint2 &prvLayout [[buffer(23)]],
 uint3 grid [[thread_position_in_grid]])
{
	uint2 pixel = grid.xy;
	bool batched = is_function_constant_defined(remasterBatchedDirectPass) ? remasterBatchedDirectPass : false;
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	constexpr uint distanceStep = 4;
	constexpr float angularStep = 2.0 * M_PI_F / 16.0;
	float4 receiverSurface = surfaceField.read(pixel);
	float3 normal = receiverSurface.gba;
	bool specializedDirect = is_function_constant_defined(remasterPreparedDirectPass) ? remasterPreparedDirectPass : false;
	bool clusteredDirect = is_function_constant_defined(remasterClusteredDirectPass) ? remasterClusteredDirectPass : false;
	bool oppositeFacing = (specializedDirect || uniforms.passIndex == 0) && oppositeFacingField.read(pixel).r > 0.5;
	bool visibilityDiagnostic = !specializedDirect && uniforms.diagnosticStage == 4;
	float2 receiverParticipation = participationField.read(pixel).rg;
	if (receiverParticipation.r < 0.5 || (!visibilityDiagnostic && receiverParticipation.g < 0.5))
	{
		if (batched)
		{
			directPartials[(grid.z * uniforms.height + pixel.y) * uniforms.width + pixel.x] = float4(0.0);
			return;
		}
		nextBounce.write(float4(0.0), pixel);
		float3 accumulated = uniforms.passIndex > 1 ? previousIndirect.read(pixel).rgb : float3(0.0);
		nextIndirect.write(float4(accumulated, 1.0), pixel);
		return;
	}
	float3 incoming = 0.0;
	float3 distanceIncoming = 0.0;
	float3 cosineIncoming = 0.0;
	float totalFormFactor = 0.0;
	float maximumVisibility = 0.0;
	float3 viewDirection = normalize(-uniforms.cameraDirection.xyz);
	// Direct enumerates visible emissive pixels, not receiver-local quadrature cells.
	bool directCollision = specializedDirect || uniforms.passIndex == 0;
	// Cover the farthest frame corner, including the final jittered radial cell.
	float2 cornerDistance = max(float2(pixel) + 0.5,
		float2(uniforms.width, uniforms.height) - (float2(pixel) + 0.5));
	uint radialSteps = uint(ceil(length(cornerDistance) / float(distanceStep))) + 1;
	// Sample the sphere's subtended solid angle, avoiding missed tiny emitters
	// and unstable near-surface area/distance weights. Interior receivers see
	// an enclosing emissive boundary (the session light is two-sided).
	bool sessionSphere = uniforms.debugColorIntensity.a > 0.0 && uniforms.debugPositionRadius.w > 0.0;
	bool adaptiveSpheres = is_function_constant_defined(remasterAdaptiveSpherePass) ? remasterAdaptiveSpherePass : false;
	uint sphereSamples = directCollision ? (adaptiveSpheres ? uint(uniforms.heightPreviewRange.z) :
		(uniforms.lightCount + uint(sessionSphere)) * 128) : 0;
	float3 receiverPosition = float3(float2(pixel) + 0.5, receiverSurface.r);
	float3 receiverRayEndpoint = remasterSurfaceRayEndpoint(pixel, receiverSurface, occlusion, heightField, mesh);
	bool preparedDirect = specializedDirect || (directCollision && abs(uniforms.padding) > 1.5);
	uint authoredSamples = preparedDirect ? emitterCount : emitterCount * 4;
	uint sampleTotal = directCollision ? authoredSamples + sphereSamples : 16 * radialSteps * 3;
	uint directionStart = 0, directionEnd = 0, nextDirectionSample = 0, randomBase = 0;
	float2 direction = float2(0.0), perpendicular = float2(0.0);
	// Exhaustive direct transport: batches partition all samples, never subsample.
	uint batchSize = max(8u, uniforms.sampleCount);
	uint firstSample = batched ? grid.z * batchSize : 0;
	uint endSample = batched ? min(sampleTotal, firstSample + batchSize) : sampleTotal;
	bool adaptiveDepth = is_function_constant_defined(remasterAdaptiveDepthPass) ? remasterAdaptiveDepthPass : false;
	// Receiver membership cannot change while enumerating this frame's sources.
	// Most receivers emit nothing, so avoid entering the exclusion helper for
	// every source (and repeating its emission texture read).
	bool hoistReceiver = is_function_constant_defined(remasterHoistReceiverEmission) ? remasterHoistReceiverEmission : true;
	bool emittingReceiver = !hoistReceiver || (clusteredDirect && any(previousBounce.read(pixel).rgb > 0.0));
	for (uint sampleIndex = firstSample; sampleIndex < endSample; sampleIndex++)
	{
		float2 samplePoint;
		float3 debugPoint = 0.0;
		float3 debugDirection = 0.0;
		float4 sphereColor = 0.0;
		float coneExtent = 0.0;
		uint sphereSampleCount = 128;
		bool debugSample = directCollision && sampleIndex >= authoredSamples;
		float3 emissionOffset = 0.0;
		RemasterDirectSample preparedSample;
		if (debugSample)
		{
			uint sphereIndex = (sampleIndex - authoredSamples) / 128;
			uint sphereLocalSample = (sampleIndex - authoredSamples) % 128;
			if (adaptiveSpheres) {
				uint sphereSample = sampleIndex - authoredSamples;
				sphereIndex = uniforms.lightCount;
				uint sceneSamples = sphereSamples - uint(sessionSphere) * 128;
				sphereLocalSample = sphereSample - min(sphereSample, sceneSamples);
				for (uint i = 0; i < uniforms.lightCount; i++) {
					RemasterGpuLight light = sceneLights[i];
					if (sphereSample >= light.firstSample && sphereSample - light.firstSample < light.sampleCount) {
						sphereIndex = i;
						sphereSampleCount = light.sampleCount;
						sphereLocalSample = sphereSample - light.firstSample;
						break;
					}
				}
			}
			float4 sphere = uniforms.debugPositionRadius;
			sphereColor = uniforms.debugColorIntensity;
			if (sphereIndex < uniforms.lightCount) {
				RemasterGpuLight light = sceneLights[sphereIndex];
				sphere = float4(light.position, light.radius);
				sphereColor = float4(light.color, light.intensity);
			}
			float3 sphereDelta = sphere.xyz - receiverPosition;
			float sphereDistanceSquared = dot(sphereDelta, sphereDelta);
			float sphereRadiusSquared = sphere.w * sphere.w;
			// A viewport-edge light can be near one receiver but tiny for most of
			// the frame. Refine the conservative CPU budget per receiver, retaining
			// deterministic quadrature and its solid-angle energy normalization.
			if (adaptiveSpheres && sphereIndex < uniforms.lightCount) {
				float angularSquared = sphereRadiusSquared / max(sphereDistanceSquared, 0.000001);
				uint receiverSamples = angularSquared < 0.0001 ? 8 : angularSquared < 0.000625 ? 16 :
					angularSquared < 0.0025 ? 32 : angularSquared < 0.01 ? 64 : 128;
				sphereSampleCount = min(sphereSampleCount, receiverSamples);
				if (sphereLocalSample >= sphereSampleCount) continue;
			}
			bool insideSphere = sphereDistanceSquared < sphereRadiusSquared;
			float3 sphereAxis = sphereDistanceSquared > 0.000001 ? sphereDelta * rsqrt(sphereDistanceSquared) : float3(0.0, 0.0, 1.0);
			float3 sphereTangent = normalize(cross(abs(sphereAxis.z) < 0.9 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0), sphereAxis));
			float3 sphereBitangent = cross(sphereAxis, sphereTangent);
			float sinSquared = min(1.0, sphereRadiusSquared / max(sphereDistanceSquared, 0.000001));
			coneExtent = insideSphere ? 2.0 : sinSquared / (1.0 + sqrt(max(0.0, 1.0 - sinSquared)));
			float index = float(sphereLocalSample) + 0.5;
			float cosineOffset = coneExtent * index / float(sphereSampleCount);
			float cosine = 1.0 - cosineOffset;
			float sineSquared = cosineOffset * (2.0 - cosineOffset);
			float sine = sqrt(max(0.0, sineSquared));
			float angle = index * 2.39996323;
			float3 ray = sphereAxis * cosine + sine * (sphereTangent * cos(angle) + sphereBitangent * sin(angle));
			debugDirection = ray;
			float projectedCenter = dot(sphereDelta, ray);
			float root = sqrt(max(0.0, sphereRadiusSquared - sphereDistanceSquared * sineSquared));
			float hitDistance = insideSphere ? projectedCenter + root :
				(sphereDistanceSquared - sphereRadiusSquared) / max(projectedCenter + root, 0.000001);
			debugPoint = receiverPosition + ray * max(hitDistance, 0.00001);
			samplePoint = debugPoint.xy;
		}
		else if (preparedDirect)
		{
			preparedSample = directSamples[sampleIndex];
			uint2 sourceRecord = uint2(emitterPixels[sampleIndex*2], emitterPixels[sampleIndex*2+1]);
			if (clusteredDirect) {
				if (emittingReceiver && sourceRecord.x != 0xffffffffu)
					preparedSample = remasterExcludeEmitterReceiver(preparedSample, sourceRecord,
						pixel.y*uniforms.width+pixel.x, receiverSurface, previousBounce, surfaceField, occlusion, heightField, mesh, emitterPatches);
				if (all(preparedSample.radiance.rgb <= 0)) continue;
			}
			// A hidden representative is independent of visible clustering. Exact
			// visible samples are excluded even if a malformed sample has a depth tag.
			if ((clusteredDirect || sourceRecord.x == 0xffffffffu) && adaptiveDepth && preparedSample.normal.w < 0.0) {
				float depth = -preparedSample.normal.w;
				uint stratum = sourceRecord.y & 3u;
				float3 axis = preparedSample.normal.xyz * depth;
				float3 base = preparedSample.position.xyz - axis * (float(stratum) + 0.5) / 4.0;
				float3 center = base + axis * 0.5;
				float ratioSquared = depth * depth / max(dot(center-receiverPosition, center-receiverPosition), 0.0001);
				// A thin distant volume is effectively one midpoint source. Preserve
				// its full radiance/area below, without dropping spatial representatives.
				// Near-field depth/shadows retain the existing two/four strata.
				uint depthSamples = ratioSquared < 0.000625 ? 1u : ratioSquared < 0.0025 ? 2u : 4u;
				if (stratum >= depthSamples) continue;
				float3 shift = axis * ((float(stratum)+0.5)/float(depthSamples) - (float(stratum)+0.5)/4.0);
				preparedSample.position.xyz += shift;
				preparedSample.endpoint.xyz += shift;
				preparedSample.radiance *= 4.0 / float(depthSamples);
			}
			samplePoint = preparedSample.position.xy;
		}
		else if (directCollision)
		{
			uint emitter = emitterPixels[sampleIndex / 4];
			RemasterMeshCell emitterPatch = mesh[emitter];
			// Zero depth keeps the original one-sample planar emission exactly.
			if (emitterPatch.emissionDepth <= 0.0 && (sampleIndex & 3) != 0) continue;
			if (emitterPatch.emissionDepth > 0.0)
			{
				float3 emitterNormal = surfaceField.read(uint2(emitter % uniforms.width, emitter / uniforms.width)).gba;
				emissionOffset = emitterNormal * emitterPatch.emissionDepth * (float(sampleIndex & 3) + 0.5) / 4.0;
			}
			samplePoint = float2(emitter % uniforms.width, emitter / uniforms.width) + 0.5;
		}
		else
		{
			// Angular setup is shared by every radial cell and lane of this direction.
			if (sampleIndex == nextDirectionSample)
			{
				uint directionIndex = sampleIndex / (radialSteps * 3);
				directionStart = sampleIndex;
				nextDirectionSample += radialSteps * 3;
				randomBase = uniforms.randomSeed ^ (pixel.x * 0x9e3779b9u) ^ (pixel.y * 0x85ebca6bu) ^
					(uniforms.passIndex * 0xc2b2ae35u) ^ (uniforms.sampleIndex * 0x27d4eb2du) ^ directionIndex;
				float angularJitter = uniforms.sampleCount > 1 ? remasterRandom(randomBase) - 0.5 : 0.0;
				float angle = (float(directionIndex) + angularJitter) * angularStep;
				direction = float2(cos(angle), sin(angle));
				perpendicular = float2(-direction.y, direction.x);
				float maximumDistance = 0.0;
				for (uint lane = 0; lane < 3; lane++)
				{
					float lateralScale = (float(lane) - 1.0) * angularStep / 3.0;
					maximumDistance = max(maximumDistance, remasterRayExitDistance(float2(pixel) + 0.5,
						direction + perpendicular * lateralScale, float2(uniforms.width, uniforms.height)));
				}
				// Radial jitter is at least -distanceStep / 2, so later cells cannot re-enter the frame.
				uint directionRadialSteps = min(radialSteps, uint(floor(maximumDistance / float(distanceStep))) + 1);
				directionEnd = directionStart + directionRadialSteps * 3;
			}
			if (sampleIndex >= directionEnd)
			{
				sampleIndex = nextDirectionSample - 1;
				continue;
			}
			uint distance = 2 + ((sampleIndex - directionStart) / 3) * distanceStep;
			uint lane = sampleIndex % 3;
			float radialJitter = uniforms.sampleCount > 1 ?
				(remasterRandom(randomBase ^ (distance * 0x165667b1u) ^ lane) - 0.5) * float(distanceStep) : 0.0;
			float sampleDistance = max(0.5, float(distance) + radialJitter);
			float lateralOffset = (float(lane) - 1.0) * sampleDistance * angularStep / 3.0;
			// Sample about the receiver center so opposite directions select mirrored pixels.
			samplePoint = float2(pixel) + 0.5 + direction * sampleDistance + perpendicular * lateralOffset;
		}
		if (!debugSample && !preparedDirect && (any(samplePoint < 0.0) || samplePoint.x >= uniforms.width || samplePoint.y >= uniforms.height))
			continue;
		uint2 samplePixel = debugSample ? pixel : preparedDirect ?
			uint2(emitterPixels[sampleIndex * 2] % uniforms.width, emitterPixels[sampleIndex * 2] / uniforms.width) : uint2(samplePoint);
		if (!debugSample && !preparedDirect)
			samplePoint = float2(samplePixel) + 0.5;
		if (!debugSample && !preparedDirect && (all(samplePixel == pixel) || participationField.read(samplePixel).r < 0.5))
			continue;
		if (!debugSample && preparedDirect && !clusteredDirect && all(samplePixel == pixel)) continue;
		float4 sampleSurface = debugSample ? float4(debugPoint.z, 0.0, 0.0, 0.0) :
			preparedDirect ? float4(preparedSample.position.z, preparedSample.normal.xyz) : surfaceField.read(samplePixel);
		float3 sourceRayEndpoint = debugSample ? debugPoint :
			preparedDirect ? preparedSample.endpoint.xyz : remasterSurfaceRayEndpoint(samplePixel, sampleSurface, occlusion, heightField, mesh);
		if (!debugSample && directCollision && !preparedDirect)
		{
			sourceRayEndpoint += emissionOffset;
			samplePoint += emissionOffset.xy;
			sampleSurface.r += emissionOffset.z;
		}
		float2 planarSegment = samplePoint - (float2(pixel) + 0.5);
		float planarDistance = length(planarSegment);
		float3 radiance = debugSample ? sphereColor.rgb * sphereColor.a * remasterEmissionRadiance :
			preparedDirect ? preparedSample.radiance.rgb : previousBounce.read(samplePixel).rgb;
		bool volumeSample = !debugSample && directCollision && (preparedDirect ? preparedSample.position.w == 1.0 :
			mesh[samplePixel.y * uniforms.width + samplePixel.x].emissionDepth > 0.0);
		bool omnidirectionalSample = !debugSample && directCollision && (preparedDirect ? preparedSample.position.w > 0.5 :
			volumeSample || mesh[samplePixel.y * uniforms.width + samplePixel.x].emissionOmnidirectional > 0.5);
		if (volumeSample && !preparedDirect) radiance *= 0.25;
		if (omnidirectionalSample && !volumeSample)
		{
			if (!preparedDirect) sourceRayEndpoint = float3(samplePoint, sampleSurface.r);
			sourceRayEndpoint = remasterOmnidirectionalRayEndpoint(sourceRayEndpoint,
				uint2(clamp(floor(sourceRayEndpoint.xy), float2(0.0),
					float2(uniforms.width - 1, uniforms.height - 1))), receiverPosition, heightField, mesh);
		}
		if (visibilityDiagnostic)
		{
			// Diagnose shadowing separately from facing, BRDF, and material absorption.
			if (any(radiance > 0.0))
				maximumVisibility = max(maximumVisibility, remasterVisibility(
					receiverRayEndpoint, sourceRayEndpoint, occlusion, heightField, surfaceField, visibilityBlocks, mesh));
			if (maximumVisibility == 1.0)
				break;
			continue;
		}
		float3 toSource = float3(planarSegment, sampleSurface.r - receiverSurface.r);
		float distanceSquared = dot(toSource, toSource);
		float3 segmentDirection = debugSample ? debugDirection : toSource * rsqrt(max(distanceSquared, 0.0001));
		float receiverResponse = saturate(dot(normal, segmentDirection));
		float sourceResponse = debugSample || omnidirectionalSample ? 1.0 : saturate(dot(sampleSurface.gba, -segmentDirection));
		float receiverScattering = specializedDirect || (directCollision && uniforms.padding > 0.5) ? receiverResponse :
			receiverResponse * remasterRoughDiffuse(normal, segmentDirection,
				viewDirection, uniforms.indirectRoughness);
		if (oppositeFacing)
		{
			// Reused wall artwork may receive direct light on either authored XY facing.
			float3 alternateNormal = float3(-normal.xy, normal.z);
			float alternateCosine = saturate(dot(alternateNormal, segmentDirection));
			receiverScattering = max(receiverScattering, specializedDirect || (directCollision && uniforms.padding > 0.5) ?
				alternateCosine : alternateCosine * remasterRoughDiffuse(alternateNormal,
					segmentDirection, viewDirection, uniforms.indirectRoughness));
		}
		float sampleArea = directCollision ? 1.0 : planarDistance * float(distanceStep) * angularStep / 3.0;
		// Sphere source cosine, area, and inverse-square distance are already
		// included in the solid-angle measure; do not apply them a second time.
		float distanceFormFactor = debugSample ? 2.0 * coneExtent / float(sphereSampleCount) :
			min(0.25, sampleArea / (M_PI_F * (distanceSquared + 1.0)));
		float formFactor = distanceFormFactor * receiverScattering * sourceResponse;
		if (!specializedDirect && uniforms.diagnosticStage != 0 && any(radiance > 0.0))
		{
			distanceIncoming += radiance * distanceFormFactor;
			cosineIncoming += radiance * formFactor;
		}
		if (formFactor > 0.0 && any(radiance > 0.0))
		{
			float visibility = remasterCachedVisibility(receiverRayEndpoint,sourceRayEndpoint,
                pixel.y*uniforms.width+pixel.x,sampleIndex,
                specializedDirect && !debugSample && !emittingReceiver && preparedSample.position.w!=2.0,
                prvConfig,prvGeneration,prvEntries,prvReceivers,prvSamples,prvColumns,
                prvChanges,prvDirty,prvCounters,prvPixelMap,prvBirths,prvRoom,prvLayout,uniforms,
                occlusion,heightField,surfaceField,visibilityBlocks,mesh);
			incoming += radiance * formFactor * visibility;
		}
		// Hidden representatives can coexist with exact visible sources. Their
		// emitting area is independent of the visible clustered pipeline choice.
		totalFormFactor += preparedDirect && !debugSample && preparedSample.radiance.w > 0 &&
			(clusteredDirect || emitterPixels[sampleIndex * 2] == 0xffffffffu) ?
			formFactor * preparedSample.radiance.w : volumeSample ? formFactor * 0.25 : formFactor;
	}
	if (!batched && totalFormFactor > 0.95)
		incoming *= 0.95 / totalFormFactor;
	if (batched)
	{
		directPartials[(grid.z * uniforms.height + pixel.y) * uniforms.width + pixel.x] = float4(incoming, totalFormFactor);
		return;
	}
	// Previous radiance is already linear and includes the source's reflection.
	// Apply only this receiver's RGB albedo, once for this collision.
	float4 authoredReflectance = reflectanceField.read(pixel);
	float3 albedo = authoredReflectance.a > 0.5 ? saturate(authoredReflectance.rgb) :
		remasterAlbedo(source.read(pixel).rgb);
	albedo = remasterBoostReflectance(albedo, uniforms.reflectanceBoost);
	float3 bounced = albedo * incoming;
	if (!specializedDirect && uniforms.diagnosticStage == 1)
		bounced = distanceIncoming;
	else if (!specializedDirect && uniforms.diagnosticStage == 2)
		bounced = cosineIncoming;
	else if (!specializedDirect && uniforms.diagnosticStage == 3)
		bounced = incoming;
	else if (visibilityDiagnostic)
		bounced = float3(maximumVisibility);
	nextBounce.write(float4(bounced, 1.0), pixel);
	float3 accumulated = !specializedDirect && uniforms.passIndex > 1 ? previousIndirect.read(pixel).rgb : float3(0.0);
	if (!specializedDirect && uniforms.passIndex > 0)
		accumulated += bounced;
	nextIndirect.write(float4(accumulated, 1.0), pixel);
}

kernel void remasterClearIndirectSeed(
	texture2d<float, access::write> indirect [[texture(0)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x < indirect.get_width() && pixel.y < indirect.get_height())
		indirect.write(float4(0.0, 0.0, 0.0, 1.0), pixel);
}

kernel void remasterReduceDirectPartials(
	const device float4 *partials [[buffer(0)]],
	constant uint &batchCount [[buffer(1)]],
	constant RemasterLightingUniforms &uniforms [[buffer(2)]],
	texture2d<float, access::write> direct [[texture(0)]],
	texture2d<float, access::write> indirect [[texture(1)]],
	texture2d<float, access::read> source [[texture(2)]],
	texture2d<float, access::read> reflectance [[texture(3)]],
	texture2d<float, access::read> previousIndirect [[texture(4)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= direct.get_width() || pixel.y >= direct.get_height()) return;
	float4 sum = 0.0;
	uint stride = direct.get_width() * direct.get_height();
	uint index = pixel.y * direct.get_width() + pixel.x;
	for (uint batch = 0; batch < batchCount; batch++) sum += partials[batch * stride + index];
	if (uniforms.passIndex == 0 && sum.a > 0.95) sum.rgb *= 0.95 / sum.a;
	if (uniforms.passIndex > 0) sum.rgb /= float(max(1u, uniforms.sampleCount));
	float4 authored = reflectance.read(pixel);
	float3 albedo = authored.a > 0.5 ? saturate(authored.rgb) : remasterAlbedo(source.read(pixel).rgb);
	albedo = remasterBoostReflectance(albedo, uniforms.reflectanceBoost);
	direct.write(float4(albedo * sum.rgb, 1.0), pixel);
	float3 accumulated = uniforms.passIndex > 1 ? previousIndirect.read(pixel).rgb : float3(0.0);
	if (uniforms.passIndex > 0) accumulated += albedo * sum.rgb;
	indirect.write(float4(accumulated, 1.0), pixel);
}

kernel void remasterCompositeLighting(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> emissionField [[texture(1)]],
	texture2d<float, access::read> directField [[texture(2)]],
	texture2d<float, access::read> indirectField [[texture(3)]],
	texture2d<float, access::write> output [[texture(4)]],
	texture2d<float, access::read> surfaceField [[texture(5)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	const device RemasterGpuLight *sceneLights [[buffer(1)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	float3 original = source.read(pixel).rgb;
	float4 emissionSample = emissionField.read(pixel);
	if (emissionSample.a < 0.5)
	{
		output.write(float4(original, 1.0), pixel);
		return;
	}
	float3 emission = emissionSample.rgb;
	float3 direct = directField.read(pixel).rgb;
	float3 indirect = indirectField.read(pixel).rgb;
	float3 composite = (any(emission > 0.0) ? float3(0.0) : remasterToLinear(original) * uniforms.originalSceneContribution) +
		remasterEmissionAppearance(emission) + direct + indirect;
	// The sphere's visible glow is presentation-only; its samples seed Direct above.
	if (remasterDebugSphereVisible(float2(pixel) + 0.5, surfaceField.read(pixel).r, uniforms.debugPositionRadius))
		composite += uniforms.debugColorIntensity.rgb * uniforms.debugColorIntensity.a;
	for (uint i = 0; i < uniforms.lightCount; i++) {
		RemasterGpuLight light = sceneLights[i];
		if (!light.hideSource && remasterDebugSphereVisible(float2(pixel) + 0.5, surfaceField.read(pixel).r, float4(light.position, light.radius)))
			composite += light.color * light.intensity;
	}
	if (uniforms.view == 2)
		output.write(float4(float3(0.0, 0.85, 1.0) * direct.r, 1.0), pixel);
	else if (uniforms.view == 3)
		output.write(float4(remasterCompositeToDisplay(direct + remasterEmissionAppearance(emission)), 1.0), pixel);
	else if (uniforms.view == 6)
		output.write(float4(remasterToDisplay(indirect * 4.0), 1.0), pixel);
	else if (uniforms.view == 8)
		output.write(float4(remasterCompositeToDisplay(direct + indirect + remasterEmissionAppearance(emission)), 1.0), pixel);
	else
		output.write(float4(remasterCompositeToDisplay(composite), 1.0), pixel);
}

kernel void remasterSelectedTileHighlight(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> mask [[texture(1)]],
	texture2d<float, access::write> output [[texture(2)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= output.get_width() || pixel.y >= output.get_height())
		return;
	float4 color = source.read(pixel);
	if (mask.read(pixel).r > 0.0)
		color.rgb = mix(color.rgb, float3(1.0, 1.0, 0.0), 0.5);
	output.write(color, pixel);
}

// Vertex Function
vertex RasterizerData
vertexShader(uint vertexID [[ vertex_id ]], constant MetalVertex *vertexArray [[ buffer(0) ]], constant vector_uint2 *viewportSizePointer  [[ buffer(1) ]])
{

    RasterizerData out;

    float2 pixelSpacePosition = vertexArray[vertexID].position.xy;

    float2 viewportSize = float2(*viewportSizePointer);

    out.position = vector_float4(0.0, 0.0, 0.0, 1.0);
    out.position.xy = pixelSpacePosition / (viewportSize / 2.0);
    out.textureCoordinate = vertexArray[vertexID].textureCoordinate;

    return out;
}

fragment float4
fragmentShader(RasterizerData in [[stage_in]], texture2d<half> colorTexture [[ texture(0) ]],
	constant int *videoModePointer [[ buffer(1) ]], texture2d<half> notificationTexture [[ texture(1) ]],
	constant int &hasNotification [[ buffer(2) ]])
{
	int videoMode = int(*videoModePointer);
	// Host text is display-space UI, not a lit scene surface. Nearest sampling
	// keeps its stock pixel font crisp in every lighting/debug view.
	if (hasNotification)
	{
		constexpr sampler notificationSampler (mag_filter::nearest, min_filter::nearest);
		const half4 notification = notificationTexture.sample(notificationSampler, in.textureCoordinate);
		if (notification.a > 0.0h) return float4(notification);
	}

	if ( videoMode == 0)
	{
		constexpr sampler textureSampler (mag_filter::nearest, min_filter::nearest);
		const half4 colorSample = colorTexture.sample(textureSampler, in.textureCoordinate);
		return float4(colorSample);
	}
	else
	{
		constexpr sampler textureSampler (mag_filter::linear, min_filter::linear);
		const half4 colorSample = colorTexture.sample(textureSampler, in.textureCoordinate);
		return float4(colorSample);
	}
}
