static float4 outColor;
static float2 var_texcoord;

struct SPIRV_Cross_Input
{
    float2 var_texcoord : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 outColor : SV_Target0;
};

void frag_main()
{
    outColor = float4(var_texcoord, 0.0f, 1.0f);
}

[RootSignature(" ")]
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    var_texcoord = stage_input.var_texcoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outColor = outColor;
    return stage_output;
}
