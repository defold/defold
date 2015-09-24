uniform sampler2D texture;
uniform vec4 uni_face_color;

void main()
{
    gl_FragColor = texture2D(texture, gl_TexCoord[0].st) * uni_face_color;
}
