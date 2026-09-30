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

kernel void remasterBuildVisibilityBlocks(
	texture2d<float, access::read> occlusion [[texture(0)]],
	texture2d<float, access::read> heightField [[texture(1)]],
	texture2d<float, access::read> surfaceField [[texture(2)]],
	texture2d<float, access::write> blocks [[texture(3)]],
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
				minimumHeight = min(minimumHeight, center - 0.5);
				maximumHeight = max(maximumHeight, center + 0.5);
			}
	blocks.write(float4(minimumHeight, maximumHeight, 0.0, 0.0), block);
}

// Transport positions describe voxel centers for shading. Visibility must
// originate on the outward face instead: otherwise a shallow ray immediately
// hits the adjacent voxels of its own continuous floor/wall/emitter sheet.
// Bias only represented opaque geometry; analytic lights keep exact endpoints.
static float3 remasterSurfaceRayEndpoint(uint2 pixel, float4 surface,
	texture2d<float, access::read> occlusion,
	texture2d<float, access::read> heightField)
{
	float3 point = float3(float2(pixel) + 0.5, surface.r);
	float dominant = max(abs(surface.g), max(abs(surface.b), abs(surface.a)));
	if (dominant > 0.0 && occlusion.read(pixel).r > 0.0 && heightField.read(pixel).g >= 0.5)
		point += surface.gba * (0.5001 / dominant);
	return point;
}

static float remasterVisibility(float3 from, float3 to,
	texture2d<float, access::read> occlusion,
	texture2d<float, access::read> heightField,
	texture2d<float, access::read> surfaceField,
	texture2d<float, access::read> visibilityBlocks)
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
		if (all(cell == endpoint))
			break;
		if (any(cell < 0) || cell.x >= int(occlusion.get_width()) || cell.y >= int(occlusion.get_height()))
			break;
		uint2 sample = uint2(cell);
		float coverage = occlusion.read(sample).r;
		if (coverage <= 0.0)
			continue;
		float exit = min(1.0, min(next.x, next.y));
		if (heightField.read(sample).g < 0.5)
			continue;
		// DDA supplies the segment's XY slab interval. Intersect it with the
		// unit Z slab centered on the effective physical height. Tangencies
		// have no positive-length intersection and do not attenuate.
		float center = surfaceField.read(sample).r;
		if (segment.z == 0.0)
		{
			if (from.z <= center - 0.5 || from.z >= center + 0.5)
				continue;
		}
		else
		{
			float bottom = (center - 0.5 - from.z) / segment.z;
			float top = (center + 0.5 - from.z) / segment.z;
			entry = max(entry, min(bottom, top));
			exit = min(exit, max(bottom, top));
		}
		if (exit - entry > 0.000001)
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
		float visibility = remasterVisibility(remasterSurfaceRayEndpoint(pixel, receiver, occlusion, heightField),
			remasterSurfaceRayEndpoint(sourcePixel, sourceSurface, occlusion, heightField), occlusion, heightField,
			surfaceField, visibilityBlocks);
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
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	constexpr uint distanceStep = 4;
	constexpr float angularStep = 2.0 * M_PI_F / 16.0;
	float4 receiverSurface = surfaceField.read(pixel);
	float3 normal = receiverSurface.gba;
	bool oppositeFacing = uniforms.passIndex == 0 && oppositeFacingField.read(pixel).r > 0.5;
	bool visibilityDiagnostic = uniforms.diagnosticStage == 4;
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
	bool directCollision = uniforms.passIndex == 0;
	// Cover the farthest frame corner, including the final jittered radial cell.
	float2 cornerDistance = max(float2(pixel) + 0.5,
		float2(uniforms.width, uniforms.height) - (float2(pixel) + 0.5));
	uint radialSteps = uint(ceil(length(cornerDistance) / float(distanceStep))) + 1;
	// Sample the sphere's subtended solid angle, avoiding missed tiny emitters
	// and unstable near-surface area/distance weights. Interior receivers see
	// an enclosing emissive boundary (the session light is two-sided).
	uint sphereSamples = directCollision && uniforms.debugColorIntensity.a > 0.0 && uniforms.debugPositionRadius.w > 0.0 ? 128 : 0;
	float3 receiverPosition = float3(float2(pixel) + 0.5, receiverSurface.r);
	float3 receiverRayEndpoint = remasterSurfaceRayEndpoint(pixel, receiverSurface, occlusion, heightField);
	float3 sphereDelta = uniforms.debugPositionRadius.xyz - receiverPosition;
	float sphereDistanceSquared = dot(sphereDelta, sphereDelta);
	float sphereRadiusSquared = uniforms.debugPositionRadius.w * uniforms.debugPositionRadius.w;
	bool insideSphere = sphereDistanceSquared < sphereRadiusSquared;
	float3 sphereAxis = sphereDistanceSquared > 0.000001 ? sphereDelta * rsqrt(sphereDistanceSquared) : float3(0.0, 0.0, 1.0);
	float3 sphereTangent = normalize(cross(abs(sphereAxis.z) < 0.9 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0), sphereAxis));
	float3 sphereBitangent = cross(sphereAxis, sphereTangent);
	float sinSquared = min(1.0, sphereRadiusSquared / max(sphereDistanceSquared, 0.000001));
	float coneExtent = insideSphere ? 2.0 : sinSquared / (1.0 + sqrt(max(0.0, 1.0 - sinSquared)));
	uint sampleTotal = directCollision ? emitterCount + sphereSamples : 16 * radialSteps * 3;
	uint directionStart = 0, directionEnd = 0, nextDirectionSample = 0, randomBase = 0;
	float2 direction = float2(0.0), perpendicular = float2(0.0);
	for (uint sampleIndex = 0; sampleIndex < sampleTotal; sampleIndex++)
	{
		float2 samplePoint;
		float3 debugPoint = 0.0;
		float3 debugDirection = 0.0;
		bool debugSample = directCollision && sampleIndex >= emitterCount;
		if (debugSample)
		{
			float index = float(sampleIndex - emitterCount) + 0.5;
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
		else if (directCollision)
		{
			uint emitter = emitterPixels[sampleIndex];
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
		if (!debugSample && (any(samplePoint < 0.0) || samplePoint.x >= uniforms.width || samplePoint.y >= uniforms.height))
			continue;
		uint2 samplePixel = debugSample ? pixel : uint2(samplePoint);
		if (!debugSample)
			samplePoint = float2(samplePixel) + 0.5;
		if (!debugSample && (all(samplePixel == pixel) || participationField.read(samplePixel).r < 0.5))
			continue;
		float4 sampleSurface = debugSample ? float4(debugPoint.z, 0.0, 0.0, 0.0) : surfaceField.read(samplePixel);
		float3 sourceRayEndpoint = debugSample ? debugPoint :
			remasterSurfaceRayEndpoint(samplePixel, sampleSurface, occlusion, heightField);
		float2 planarSegment = samplePoint - (float2(pixel) + 0.5);
		float planarDistance = length(planarSegment);
		float3 radiance = debugSample ? uniforms.debugColorIntensity.rgb * uniforms.debugColorIntensity.a * remasterEmissionRadiance : previousBounce.read(samplePixel).rgb;
		if (visibilityDiagnostic)
		{
			// Diagnose shadowing separately from facing, BRDF, and material absorption.
			if (any(radiance > 0.0))
				maximumVisibility = max(maximumVisibility, remasterVisibility(
					receiverRayEndpoint, sourceRayEndpoint, occlusion, heightField, surfaceField, visibilityBlocks));
			if (maximumVisibility == 1.0)
				break;
			continue;
		}
		float3 toSource = float3(planarSegment, sampleSurface.r - receiverSurface.r);
		float distanceSquared = dot(toSource, toSource);
		float3 segmentDirection = debugSample ? debugDirection : toSource * rsqrt(max(distanceSquared, 0.0001));
		float receiverResponse = saturate(dot(normal, segmentDirection));
		float sourceResponse = debugSample ? 1.0 : saturate(dot(sampleSurface.gba, -segmentDirection));
		float receiverScattering = directCollision && uniforms.padding > 0.5 ? receiverResponse :
			receiverResponse * remasterRoughDiffuse(normal, segmentDirection,
				viewDirection, uniforms.indirectRoughness);
		if (oppositeFacing)
		{
			// Reused wall artwork may receive direct light on either authored XY facing.
			float3 alternateNormal = float3(-normal.xy, normal.z);
			float alternateCosine = saturate(dot(alternateNormal, segmentDirection));
			receiverScattering = max(receiverScattering, directCollision && uniforms.padding > 0.5 ?
				alternateCosine : alternateCosine * remasterRoughDiffuse(alternateNormal,
					segmentDirection, viewDirection, uniforms.indirectRoughness));
		}
		float sampleArea = directCollision ? 1.0 : planarDistance * float(distanceStep) * angularStep / 3.0;
		// Sphere source cosine, area, and inverse-square distance are already
		// included in the solid-angle measure; do not apply them a second time.
		float distanceFormFactor = debugSample ? 2.0 * coneExtent / float(sphereSamples) :
			min(0.25, sampleArea / (M_PI_F * (distanceSquared + 1.0)));
		float formFactor = distanceFormFactor * receiverScattering * sourceResponse;
		if (uniforms.diagnosticStage != 0 && any(radiance > 0.0))
		{
			distanceIncoming += radiance * distanceFormFactor;
			cosineIncoming += radiance * formFactor;
		}
		if (formFactor > 0.0 && any(radiance > 0.0))
		{
			float visibility = remasterVisibility(receiverRayEndpoint, sourceRayEndpoint,
				occlusion, heightField, surfaceField, visibilityBlocks);
			incoming += radiance * formFactor * visibility;
		}
		totalFormFactor += formFactor;
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
	if (uniforms.diagnosticStage == 1)
		bounced = distanceIncoming;
	else if (uniforms.diagnosticStage == 2)
		bounced = cosineIncoming;
	else if (uniforms.diagnosticStage == 3)
		bounced = incoming;
	else if (visibilityDiagnostic)
		bounced = float3(maximumVisibility);
	nextBounce.write(float4(bounced, 1.0), pixel);
	float3 accumulated = uniforms.passIndex > 1 ? previousIndirect.read(pixel).rgb : float3(0.0);
	if (uniforms.passIndex > 0)
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
