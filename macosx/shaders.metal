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
	float2 position;
	float radius;
	float intensity;
	float3 color;
	float padding;
} RemasterGpuLight;

typedef struct
{
	uint width;
	uint height;
	uint view;
	uint lightCount;
} RemasterLightingUniforms;

kernel void remasterDirectLighting(
	texture2d<float, access::read> source [[texture(0)]],
	texture2d<float, access::read> occlusion [[texture(1)]],
	texture2d<float, access::write> output [[texture(2)]],
	texture2d<float, access::read> emission [[texture(3)]],
	constant RemasterLightingUniforms &uniforms [[buffer(0)]],
	constant RemasterGpuLight *lights [[buffer(1)]],
	uint2 pixel [[thread_position_in_grid]])
{
	if (pixel.x >= uniforms.width || pixel.y >= uniforms.height)
		return;
	float2 position = float2(pixel) + 0.5;
	float3 original = source.read(pixel).rgb;
	float3 direct = 0.0;
	float maximumVisibility = uniforms.lightCount ? 0.0 : 1.0;
	for (uint lightIndex = 0; lightIndex < uniforms.lightCount; lightIndex++)
	{
		RemasterGpuLight light = lights[lightIndex];
		float2 toLight = light.position - position;
		float distanceToLight = length(toLight);
		float visibility = 1.0;
		if (distanceToLight > 1.0)
		{
			float2 direction = toLight / distanceToLight;
			uint steps = min(uint(ceil(distanceToLight)), 320u);
			for (uint step = 2; step < steps; step++)
			{
				uint2 samplePosition = uint2(clamp(position + direction * float(step),
					float2(0.0), float2(uniforms.width - 1, uniforms.height - 1)));
				if (occlusion.read(samplePosition).r > 0.5)
				{
					visibility = 0.12;
					break;
				}
			}
		}
		float attenuation = saturate(1.0 - distanceToLight / light.radius);
		attenuation *= attenuation;
		direct += original * light.color * attenuation * light.intensity * visibility;
		maximumVisibility = max(maximumVisibility, visibility);
	}
	float3 ambient = original * 0.32;
	float4 authoredEmission = emission.read(pixel);
	float3 selfEmission = authoredEmission.rgb * authoredEmission.a * (255.0 / 25.0);
	float3 composite = ambient + direct + selfEmission;
	if (uniforms.view == 1)
	{
		float2 field = occlusion.read(pixel).rg;
		float3 coverage = field.g > 0.5 ? mix(float3(0.0, 0.35, 0.05), float3(1.0, 0.05, 0.0), field.r) :
			float3(0.22, 0.0, 0.28);
		output.write(float4(coverage, 1.0), pixel);
	}
	else if (uniforms.view == 2)
		output.write(float4(float3(0.0, 0.85, 1.0) * maximumVisibility, 1.0), pixel);
	else if (uniforms.view == 3)
		output.write(float4(direct, 1.0), pixel);
	else if (uniforms.view == 4)
		output.write(float4(saturate(abs(composite - original) * 3.0), 1.0), pixel);
	else
		output.write(float4(composite, 1.0), pixel);
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
