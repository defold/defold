
uniform sampler2D tex0;
uniform sampler2D tex1;

varying vec2 var_texcoord0;
varying vec2 var_texcoord1;

void main()
{
    vec4 tex0_color = texture2D(tex0, var_texcoord0.st);
    vec4 tex1_color = texture2D(tex1, var_texcoord1.st);
    gl_FragColor = tex0_color * tex1_color;
}
