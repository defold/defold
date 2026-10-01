varying vec4 var_crash_attr;
uniform vec4 tint;
void main()
{
    gl_FragColor = var_crash_attr * tint;
}
