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
} RemasterLightingUniforms;

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

static float remasterVisibility(float3 from, float3 to,
	texture2d<float, access::read> occlusion,
	texture2d<float, access::read> heightField,
	texture2d<float, access::read> surfaceField)
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
	// Traverse every crossed pixel once; sparse radiance samples must not skip blockers.
	while (any(cell != endpoint))
	{
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
		float rayHeight = min(from.z + segment.z * entry, from.z + segment.z * exit);
		if (heightField.read(sample).g < 0.5 || surfaceField.read(sample).r > rayHeight + 0.05)
		{
			visibility *= 1.0 - coverage;
			if (visibility < 0.01)
				return 0.0;
		}
	}
	return visibility;
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
	float3 position = float3(float2(pixel) + 0.5, surface.r);
	float3 normal = surface.gba;
	float3 original = source.read(pixel).rgb;
	float2 participation = participationField.read(pixel).rg;
	if (participation.r < 0.5)
	{
		directField.write(float4(0.0), pixel);
		output.write(float4(original, 1.0), pixel);
		return;
	}
	float3 albedo = remasterAlbedo(original);
	float3 direct = 0.0;
	float maximumVisibility = uniforms.lightCount ? 0.0 : 1.0;
	for (uint lightIndex = 0; lightIndex < uniforms.lightCount; lightIndex++)
	{
		RemasterGpuLight light = lights[lightIndex];
		float3 toLight = light.position - position;
		float distanceToLight = length(toLight);
		float visibility = remasterVisibility(position, light.position, occlusion, heightField, surfaceField);
		float normalizedDistance = distanceToLight / max(light.radius, 1.0);
		// Radius is the authored influence scale, not a hard cutoff.
		float attenuation = 1.0 / (1.0 + 6.0 * normalizedDistance * normalizedDistance);
		float normalResponse = 1.0;
		if (distanceToLight > 0.0)
		{
			float3 lightDirection = toLight / distanceToLight;
			float response = dot(normal, lightDirection);
			if (oppositeFacingField.read(pixel).r > 0.5)
				response = max(response, dot(float3(-normal.xy, normal.z), lightDirection));
			normalResponse = saturate(response);
		}
		float3 irradiance = light.color * attenuation * light.intensity * visibility * normalResponse;
		direct += albedo * irradiance;
		maximumVisibility = max(maximumVisibility, visibility);
	}
	float3 ambient = remasterToLinear(original) * 0.65;
	float4 authoredEmission = emission.read(pixel);
	float3 selfEmission = remasterToLinear(authoredEmission.rgb) * authoredEmission.a * (255.0 / 25.0);
	float3 composite = ambient + direct + selfEmission;
	directField.write(float4(direct + selfEmission, 1.0), pixel);
	if (uniforms.view == 5)
	{
		float2 field = heightField.read(pixel).rg;
		float heightValue = saturate(field.r * 8.0);
		output.write(float4(field.g > 0.5 ? mix(float3(0.0, 0.12, 0.35), float3(1.0, 0.9, 0.1), heightValue) :
			float3(0.22, 0.0, 0.28), 1.0), pixel);
	}
	else if (uniforms.view == 1)
	{
		float2 field = occlusion.read(pixel).rg;
		float3 coverage = field.g > 0.5 ? mix(float3(0.0, 0.35, 0.05), float3(1.0, 0.05, 0.0), field.r) :
			float3(0.22, 0.0, 0.28);
		output.write(float4(coverage, 1.0), pixel);
	}
	else if (uniforms.view == 2)
		output.write(float4(float3(0.0, 0.85, 1.0) * maximumVisibility, 1.0), pixel);
	else if (uniforms.view == 3)
		output.write(float4(remasterToDisplay(direct * 4.0), 1.0), pixel);
	else if (uniforms.view == 4)
		output.write(float4(saturate(remasterToDisplay(abs(composite - remasterToLinear(original))) * 3.0), 1.0), pixel);
	else if (uniforms.view == 6)
		output.write(float4(0.0, 0.0, 0.0, 1.0), pixel);
	else if (uniforms.view == 7)
		output.write(float4(normal * 0.5 + 0.5, 1.0), pixel);
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
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	constexpr float2 directions[16] = {
		float2(1.0, 0.0), float2(0.9239, 0.3827), float2(0.7071, 0.7071), float2(0.3827, 0.9239),
		float2(0.0, 1.0), float2(-0.3827, 0.9239), float2(-0.7071, 0.7071), float2(-0.9239, 0.3827),
		float2(-1.0, 0.0), float2(-0.9239, -0.3827), float2(-0.7071, -0.7071), float2(-0.3827, -0.9239),
		float2(0.0, -1.0), float2(0.3827, -0.9239), float2(0.7071, -0.7071), float2(0.9239, -0.3827)
	};
	constexpr uint maximumDistance = 64;
	constexpr uint distanceStep = 4;
	constexpr float angularStep = 2.0 * M_PI_F / 16.0;
	float4 receiverSurface = surfaceField.read(pixel);
	float3 normal = receiverSurface.gba;
	float2 receiverParticipation = participationField.read(pixel).rg;
	if (receiverParticipation.r < 0.5 || receiverParticipation.g < 0.5)
	{
		nextBounce.write(float4(0.0), pixel);
		float3 accumulated = uniforms.passIndex == 0 ? float3(0.0) : previousIndirect.read(pixel).rgb;
		nextIndirect.write(float4(accumulated, 1.0), pixel);
		return;
	}
	float3 incoming = 0.0;
	float totalFormFactor = 0.0;
	for (uint directionIndex = 0; directionIndex < 16; directionIndex++)
	{
		float2 direction = directions[directionIndex];
		float2 perpendicular = float2(-direction.y, direction.x);
		for (uint distance = 2; distance <= maximumDistance; distance += distanceStep)
		{
			for (uint lane = 0; lane < 3; lane++)
			{
				float lateralOffset = (float(lane) - 1.0) * float(distance) * angularStep / 3.0;
				// Sample about the receiver center so opposite directions select mirrored pixels.
				float2 samplePoint = float2(pixel) + 0.5 + direction * float(distance) + perpendicular * lateralOffset;
				if (any(samplePoint < 0.0) || samplePoint.x >= uniforms.width || samplePoint.y >= uniforms.height)
					continue;
				uint2 samplePixel = uint2(samplePoint);
				float4 sampleSurface = surfaceField.read(samplePixel);
				float2 planarSegment = (float2(samplePixel) + 0.5) - (float2(pixel) + 0.5);
				float planarDistance = length(planarSegment);
				if (participationField.read(samplePixel).r > 0.5)
				{
					float3 toSource = float3(planarSegment, sampleSurface.r - receiverSurface.r);
					float distanceSquared = dot(toSource, toSource);
					float3 segmentDirection = toSource * rsqrt(max(distanceSquared, 0.0001));
					float receiverResponse = saturate(dot(normal, segmentDirection));
					float sourceResponse = saturate(dot(sampleSurface.gba, -segmentDirection));
					float sampleArea = planarDistance * float(distanceStep) * angularStep / 3.0;
					float formFactor = min(0.25, receiverResponse * sourceResponse * sampleArea /
						(M_PI_F * (distanceSquared + 1.0)));
					float3 radiance = previousBounce.read(samplePixel).rgb;
					if (formFactor > 0.0 && any(radiance > 0.0))
					{
						float visibility = remasterVisibility(float3(float2(pixel) + 0.5, receiverSurface.r),
							float3(float2(samplePixel) + 0.5, sampleSurface.r), occlusion, heightField, surfaceField);
						incoming += radiance * formFactor * visibility;
					}
					totalFormFactor += formFactor;
				}
			}
		}
	}
	if (totalFormFactor > 0.95)
		incoming *= 0.95 / totalFormFactor;
	float3 albedo = remasterAlbedo(source.read(pixel).rgb);
	float3 bounced = albedo * incoming;
	nextBounce.write(float4(bounced, 1.0), pixel);
	float3 accumulated = uniforms.passIndex == 0 ? float3(0.0) : previousIndirect.read(pixel).rgb;
	nextIndirect.write(float4(accumulated + bounced, 1.0), pixel);
}

kernel void remasterCompositeLighting(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> directField [[texture(1)]],
	texture2d<float, access::read> indirectField [[texture(2)]],
	texture2d<float, access::write> output [[texture(3)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	float3 original = source.read(pixel).rgb;
	float4 directSample = directField.read(pixel);
	if (directSample.a < 0.5)
	{
		output.write(float4(original, 1.0), pixel);
		return;
	}
	float3 direct = directSample.rgb;
	float3 indirect = indirectField.read(pixel).rgb;
	float3 composite = remasterToLinear(original) * 0.65 + direct + indirect;
	if (uniforms.view == 3)
		output.write(float4(remasterToDisplay(direct * 4.0), 1.0), pixel);
	else if (uniforms.view == 4)
		output.write(float4(saturate(remasterToDisplay(abs(composite - remasterToLinear(original))) * 3.0), 1.0), pixel);
	else if (uniforms.view == 6)
		output.write(float4(remasterToDisplay(indirect * 4.0), 1.0), pixel);
	else
		output.write(float4(remasterToDisplay(composite), 1.0), pixel);
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
