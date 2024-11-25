#version 140

out vec4 _DMENGINE_GENERATED_gl_FragColor_0;
struct Light
{
    int type;
    vec3 position;
    vec4 color;
};

layout(set=1) uniform _DMENGINE_GENERATED_UB_FS_0 { Light u_lights[4]  ; };
void main()
{
    _DMENGINE_GENERATED_gl_FragColor_0 = u_lights[0].color + u_lights[3].color;
}
