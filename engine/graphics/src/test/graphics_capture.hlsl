struct Input { float3 position : TEXCOORD0; float3 color : TEXCOORD1; };
struct Output { float4 position : SV_Position; float3 color : TEXCOORD0; };
#if CAPTURE_CUBEMAP
#define CAPTURE_ROOT_SIGNATURE "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_PIXEL), DescriptorTable(Sampler(s1), visibility=SHADER_VISIBILITY_PIXEL)"
TextureCube<float4> cubemap : register(t0);
SamplerState cube_sampler : register(s1);
#else
#define CAPTURE_ROOT_SIGNATURE "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)"
#endif
Output vertex_main(Input input) {
    Output output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    return output;
}
float4 fragment_main(Output input) : SV_Target0 {
#if CAPTURE_CUBEMAP
    return cubemap.SampleLevel(cube_sampler, input.color, 0.0);
#else
    return float4(input.color, 1.0);
#endif
}
