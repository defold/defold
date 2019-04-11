uniform sampler2D texture_sampler;
uniform vec4 uni_face_color;

void main()
{
    gl_FragColor = texture2D(texture_sampler, gl_TexCoord[0].st) * uni_face_color;
}
