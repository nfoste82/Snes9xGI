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
	float reserved;
};

kernel void remasterBuildVisibilityBlocks(
	texture2d<float, access::read> occlusion [[texture(0)]],
	texture2d<float, access::read> heightField [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::write> blocks [[texture(3)]],
	const device RemasterMeshCell *mesh [[buffer(0)]],
	uint2 block [[thread_position_in_grid]])
{
	if (block.x >= blocks.get_width() || block.y >= blocks.get_height())
		return;
	float minimumHeight = INFINITY;
	float maximumHeight = -INFINITY;
	for (uint y = block.y * 8; y < min((block.y + 1) * 8, occlusion.get_height()); y++)
		for (uint x = block.x * 8; x < min((block.x + 1) * 8, occlusion.get_width()); x++)
			if (occlusion.read(uint2(x, y)).r > 0.0 && heightField.read(uint2(x, y)).g >= 0.5)
			{
				float center = surfaceField.read(uint2(x, y)).r;
				RemasterMeshCell patch = mesh[y * occlusion.get_width() + x];
				float low = min(center, min(min(patch.corners.x, patch.corners.y), min(patch.corners.z, patch.corners.w)));
				if (patch.solidWall > 0.5) low = min(low, patch.wallBase);
				float high = max(center, max(max(patch.corners.x, patch.corners.y), max(patch.corners.z, patch.corners.w)));
				minimumHeight = min(minimumHeight, low - patch.thickness * 0.5);
				maximumHeight = max(maximumHeight, high + patch.thickness * 0.5);
			}
	blocks.write(float4(minimumHeight, maximumHeight, 0.0, 0.0), block);
}

// Shading positions lie at authored surface centers. Visibility starts outside
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

static bool remasterTriangleHit(float3 from, float3 ray, float3 a, float3 b, float3 c, float entry, float exit)
{
	float3 e1 = b - a, e2 = c - a;
	float3 p = cross(ray, e2);
	float determinant = dot(e1, p);
	if (abs(determinant) <= 0.0000001) return false;
	float inverse = 1.0 / determinant;
	float3 s = from - a;
	float u = dot(s, p) * inverse;
	float3 q = cross(s, e1);
	float v = dot(ray, q) * inverse;
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
	float3 vertices[4] = {float3(float2(cell), patch.corners.x),
		float3(float2(cell) + float2(1, 0), patch.corners.y),
		float3(float2(cell) + 1.0, patch.corners.z),
		float3(float2(cell) + float2(0, 1), patch.corners.w)};
	float3 middle = float3(float2(cell) + 0.5, center);
	float3 halfThickness = float3(0, 0, patch.thickness * 0.5);
	for (uint i = 0; i < 4; i++)
	{
		uint j = (i + 1) & 3;
		if (remasterTriangleHit(from, ray, middle + halfThickness, vertices[i] + halfThickness,
			vertices[j] + halfThickness, entry, exit)) return true;
		if (patch.thickness > 0.0)
		{
			if (remasterTriangleHit(from, ray, middle - halfThickness, vertices[j] - halfThickness,
				vertices[i] - halfThickness, entry, exit) ||
				remasterTriangleHit(from, ray, vertices[i] - halfThickness, vertices[j] - halfThickness,
				vertices[j] + halfThickness, entry, exit) ||
				remasterTriangleHit(from, ray, vertices[i] - halfThickness, vertices[j] + halfThickness,
				vertices[i] + halfThickness, entry, exit)) return true;
		}
		if (patch.solidWall > 0.5)
		{
			float3 bottomI = float3(vertices[i].xy, patch.wallBase);
			float3 bottomJ = float3(vertices[j].xy, patch.wallBase);
			if (remasterTriangleHit(from, ray, bottomI, bottomJ, vertices[j], entry, exit) ||
				remasterTriangleHit(from, ray, bottomI, vertices[j], vertices[i], entry, exit)) return true;
		}
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
		// The envelope is acceleration only: pixels occupy finite unit voxels,
		// not solid columns beneath their authored height.
		int2 block = cell / 8;
		if (any(block != testedBlock))
		{
			testedBlock = block;
			int2 remaining = int2(step.x > 0 ? 7 - (cell.x & 7) : (cell.x & 7),
				step.y > 0 ? 7 - (cell.y & 7) : (cell.y & 7));
			float2 blockNext = next;
			// Match repeated pixel steps even when a boundary is almost at t=1.
			#pragma unroll
			for (int i = 0; i < 7; i++)
			{
				if (i < remaining.x && step.x) blockNext.x += delta.x;
				if (i < remaining.y && step.y) blockNext.y += delta.y;
			}
			blockExit = min(1.0, min(blockNext.x, blockNext.y));
			float blockEntry = max(0.0, min(next.x, next.y));
			float minimumHeight = min(from.z + segment.z * blockEntry, from.z + segment.z * blockExit);
			float maximumHeight = max(from.z + segment.z * blockEntry, from.z + segment.z * blockExit);
			float2 bounds = visibilityBlocks.read(uint2(block)).rg;
			emptyBlock = bounds.x > maximumHeight || bounds.y < minimumHeight;
		}
		if (emptyBlock)
		{
			if (blockExit >= 1.0)
				break;
			// Stop just before the block boundary, leaving boundary/corner handling
			// to the same pixel DDA below.
			int2 remaining = int2(step.x > 0 ? 7 - (cell.x & 7) : (cell.x & 7),
				step.y > 0 ? 7 - (cell.y & 7) : (cell.y & 7));
			// Preserve the reference DDA's rounding at pixel corners.
			#pragma unroll
			for (int i = 0; i < 7; i++)
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
		float coverage = occlusion.read(sample).r;
		if (coverage <= 0.0)
			continue;
		float exit = min(1.0, min(next.x, next.y));
		if (heightField.read(sample).g < 0.5)
			continue;
		RemasterMeshCell patch = mesh[sample.y * occlusion.get_width() + sample.x];
		// Grid traversal bins triangle candidates; it does not define geometry.
		// Count a patch once even if both triangles/shared edges are hit.
		if (exit - entry > 0.000001 && remasterMeshHit(from, segment, cell, surfaceField.read(sample).r,
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
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	float3 accumulated = uniforms.passIndex > 1 ? previousIndirect.read(pixel).rgb : float3(0.0);
	float2 participation = participationField.read(pixel).rg;
	if (participation.r < 0.5 || participation.g < 0.5 || sourcePower[1] <= 0.0)
	{
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
	for (uint connection = 0; connection < connectionCount; connection++)
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
		float visibility = remasterVisibility(remasterSurfaceRayEndpoint(pixel, receiver, occlusion, heightField, mesh),
			remasterSurfaceRayEndpoint(sourcePixel, sourceSurface, occlusion, heightField, mesh), occlusion, heightField,
			surfaceField, visibilityBlocks, mesh);
		float transfer = receiverCosine * sourceCosine * visibility / (M_PI_F * distanceSquared);
		incoming += previousBounce.read(sourcePixel).rgb * (transfer / probability);
	}
	float3 bounced = albedo * (incoming / float(connectionCount));
	nextBounce.write(float4(bounced, 1.0), pixel);
	nextIndirect.write(float4(accumulated + bounced, 1.0), pixel);
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
	float3 composite = ambient + direct + selfEmission;
	directField.write(float4(direct + selfEmission, 1.0), pixel);
	if (uniforms.view == 5)
	{
		float2 field = heightField.read(pixel).rg;
		float heightByte = field.r * 255.0;
		float heightValue = saturate((heightByte - uniforms.heightPreviewRange.x) /
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
		output.write(float4(remasterToDisplay(direct * 4.0), 1.0), pixel);
	else if (uniforms.view == 4)
		output.write(float4(saturate(remasterToDisplay(abs(composite - remasterToLinear(original))) * 3.0), 1.0), pixel);
	else if (uniforms.view == 6)
		output.write(float4(0.0, 0.0, 0.0, 1.0), pixel);
	else if (uniforms.view == 7)
		output.write(float4(normal * 0.5 + 0.5, 1.0), pixel);
	else if (uniforms.view == 9)
	{
		float3 debugEmission = remasterDebugSphereVisible(float2(pixel) + 0.5, surface.r, uniforms.debugPositionRadius) ?
			uniforms.debugColorIntensity.rgb * uniforms.debugColorIntensity.a * remasterEmissionRadiance : float3(0.0);
		output.write(float4(remasterToDisplay(selfEmission + debugEmission), 1.0), pixel);
	}
	else
		output.write(float4(remasterToDisplay(composite), 1.0), pixel);
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

// The app specializes its normal direct pass; the unspecialized function is
// retained for reference transport and diagnostics. Compile-time branches keep
// radial indirect sampling/diagnostics out of the production direct shader.
constant bool remasterPreparedDirectPass [[function_constant(0)]];

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
	uint2 pixel = uint2(source.x % surfaceField.get_width(), source.x / surfaceField.get_width());
	float4 surface = surfaceField.read(pixel);
	float depth = mesh[source.x].emissionDepth;
	float3 offset = depth > 0.0 ? surface.gba * depth * (float(source.y) + 0.5) / 4.0 : float3(0.0);
	RemasterDirectSample sample;
	sample.position = float4(float3(float2(pixel) + 0.5, surface.r) + offset, depth > 0.0 ? 1.0 : 0.0);
	sample.endpoint = float4(remasterSurfaceRayEndpoint(pixel, surface, occlusion, heightField, mesh) + offset, 0.0);
	sample.normal = float4(surface.gba, 0.0);
	sample.radiance = float4(emissionSeed.read(pixel).rgb * (depth > 0.0 ? 0.25 : 1.0), 0.0);
	samples[index] = sample;
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
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	constexpr uint distanceStep = 4;
	constexpr float angularStep = 2.0 * M_PI_F / 16.0;
	float4 receiverSurface = surfaceField.read(pixel);
	float3 normal = receiverSurface.gba;
	bool specializedDirect = is_function_constant_defined(remasterPreparedDirectPass) ? remasterPreparedDirectPass : false;
	bool oppositeFacing = (specializedDirect || uniforms.passIndex == 0) && oppositeFacingField.read(pixel).r > 0.5;
	bool visibilityDiagnostic = !specializedDirect && uniforms.diagnosticStage == 4;
	float2 receiverParticipation = participationField.read(pixel).rg;
	if (receiverParticipation.r < 0.5 || (!visibilityDiagnostic && receiverParticipation.g < 0.5))
	{
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
	uint sphereSamples = directCollision && uniforms.debugColorIntensity.a > 0.0 && uniforms.debugPositionRadius.w > 0.0 ? 128 : 0;
	float3 receiverPosition = float3(float2(pixel) + 0.5, receiverSurface.r);
	float3 receiverRayEndpoint = remasterSurfaceRayEndpoint(pixel, receiverSurface, occlusion, heightField, mesh);
	float3 sphereDelta = uniforms.debugPositionRadius.xyz - receiverPosition;
	float sphereDistanceSquared = dot(sphereDelta, sphereDelta);
	float sphereRadiusSquared = uniforms.debugPositionRadius.w * uniforms.debugPositionRadius.w;
	bool insideSphere = sphereDistanceSquared < sphereRadiusSquared;
	float3 sphereAxis = sphereDistanceSquared > 0.000001 ? sphereDelta * rsqrt(sphereDistanceSquared) : float3(0.0, 0.0, 1.0);
	float3 sphereTangent = normalize(cross(abs(sphereAxis.z) < 0.9 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0), sphereAxis));
	float3 sphereBitangent = cross(sphereAxis, sphereTangent);
	float sinSquared = min(1.0, sphereRadiusSquared / max(sphereDistanceSquared, 0.000001));
	float coneExtent = insideSphere ? 2.0 : sinSquared / (1.0 + sqrt(max(0.0, 1.0 - sinSquared)));
	bool preparedDirect = specializedDirect || (directCollision && abs(uniforms.padding) > 1.5);
	uint authoredSamples = preparedDirect ? emitterCount : emitterCount * 4;
	uint sampleTotal = directCollision ? authoredSamples + sphereSamples : 16 * radialSteps * 3;
	uint directionStart = 0, directionEnd = 0, nextDirectionSample = 0, randomBase = 0;
	float2 direction = float2(0.0), perpendicular = float2(0.0);
	for (uint sampleIndex = 0; sampleIndex < sampleTotal; sampleIndex++)
	{
		float2 samplePoint;
		float3 debugPoint = 0.0;
		float3 debugDirection = 0.0;
		bool debugSample = directCollision && sampleIndex >= authoredSamples;
		float3 emissionOffset = 0.0;
		RemasterDirectSample preparedSample;
		if (debugSample)
		{
			float index = float(sampleIndex - authoredSamples) + 0.5;
			float cosineOffset = coneExtent * index / float(sphereSamples);
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
		if (!debugSample && preparedDirect && all(samplePixel == pixel)) continue;
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
		float3 radiance = debugSample ? uniforms.debugColorIntensity.rgb * uniforms.debugColorIntensity.a * remasterEmissionRadiance :
			preparedDirect ? preparedSample.radiance.rgb : previousBounce.read(samplePixel).rgb;
		bool volumeSample = !debugSample && directCollision && (preparedDirect ? preparedSample.position.w > 0.5 :
			mesh[samplePixel.y * uniforms.width + samplePixel.x].emissionDepth > 0.0);
		if (volumeSample && !preparedDirect) radiance *= 0.25;
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
		float sourceResponse = debugSample || volumeSample ? 1.0 : saturate(dot(sampleSurface.gba, -segmentDirection));
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
		float distanceFormFactor = debugSample ? 2.0 * coneExtent / float(sphereSamples) :
			min(0.25, sampleArea / (M_PI_F * (distanceSquared + 1.0)));
		float formFactor = distanceFormFactor * receiverScattering * sourceResponse;
		if (!specializedDirect && uniforms.diagnosticStage != 0 && any(radiance > 0.0))
		{
			distanceIncoming += radiance * distanceFormFactor;
			cosineIncoming += radiance * formFactor;
		}
		if (formFactor > 0.0 && any(radiance > 0.0))
		{
			float visibility = remasterVisibility(receiverRayEndpoint, sourceRayEndpoint,
				occlusion, heightField, surfaceField, visibilityBlocks, mesh);
			incoming += radiance * formFactor * visibility;
		}
		totalFormFactor += volumeSample ? formFactor * 0.25 : formFactor;
	}
	if (totalFormFactor > 0.95)
		incoming *= 0.95 / totalFormFactor;
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

kernel void remasterCompositeLighting(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> emissionField [[texture(1)]],
	texture2d<float, access::read> directField [[texture(2)]],
	texture2d<float, access::read> indirectField [[texture(3)]],
	texture2d<float, access::write> output [[texture(4)]],
	texture2d<float, access::read> surfaceField [[texture(5)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
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
	float3 composite = remasterToLinear(original) * uniforms.originalSceneContribution + emission + direct + indirect;
	// The sphere's visible glow is presentation-only; its samples seed Direct above.
	if (remasterDebugSphereVisible(float2(pixel) + 0.5, surfaceField.read(pixel).r, uniforms.debugPositionRadius))
		composite += uniforms.debugColorIntensity.rgb * uniforms.debugColorIntensity.a * remasterEmissionRadiance;
	if (uniforms.view == 2)
		output.write(float4(float3(0.0, 0.85, 1.0) * direct.r, 1.0), pixel);
	else if (uniforms.view == 3)
		output.write(float4(remasterToDisplay(direct * 4.0), 1.0), pixel);
	else if (uniforms.view == 4)
		output.write(float4(saturate(remasterToDisplay(abs(composite - remasterToLinear(original))) * 3.0), 1.0), pixel);
	else if (uniforms.view == 6)
		output.write(float4(remasterToDisplay(indirect * 4.0), 1.0), pixel);
	else if (uniforms.view == 8)
		output.write(float4(remasterToDisplay((direct + indirect) * 4.0), 1.0), pixel);
	else
		output.write(float4(remasterToDisplay(composite), 1.0), pixel);
}

kernel void remasterAccumulateSamples(
	texture2d<float, access::read> sample [[texture(0)]],
	texture2d<float, access::read> previous [[texture(1)]],
	texture2d<float, access::write> output [[texture(2)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	float3 value = sample.read(pixel).rgb;
	if (uniforms.sampleIndex > 0)
		value = previous.read(pixel).rgb + (value - previous.read(pixel).rgb) / float(uniforms.sampleIndex + 1);
	output.write(float4(value, 1.0), pixel);
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
fragmentShader(RasterizerData in [[stage_in]], texture2d<half> colorTexture [[ texture(0) ]], constant int *videoModePointer [[ buffer(1) ]])
{
	int videoMode = int(*videoModePointer);
	
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
