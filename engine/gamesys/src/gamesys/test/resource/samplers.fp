varying vec2 var_uv;

uniform sampler2D tex_diffuse;
uniform sampler2D tex_normal;

void main()
{
    gl_FragColor = texture2D(tex_diffuse, var_uv) + texture2D(tex_normal, var_uv);
}

