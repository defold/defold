static float4 gl_Position;
static float2 pos;
static float2 var_texcoord;
static float2 texcoord;

struct SPIRV_Cross_Input
{
    float2 pos : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float2 var_texcoord : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    gl_Position = float4(pos, 0.0f, 1.0f);
    var_texcoord = texcoord;
}

[RootSignature(" ")]
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    pos = stage_input.pos;
    texcoord = stage_input.texcoord;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.var_texcoord = var_texcoord;
    return stage_output;
}
