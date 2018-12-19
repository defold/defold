uniform sampler2D DIFFUSE_TEXTURE;
uniform vec4 uni_face_color;

void main()
{
    gl_FragColor = texture2D(DIFFUSE_TEXTURE, gl_TexCoord[0].st) * uni_face_color;
}
