// -------------------------------------------------------------------------------------------------------------------------------
// Vertex Shader
// -------------------------------------------------------------------------------------------------------------------------------

const WindowVShader = ShaderShared + `

uniform Viewport inViewport;

uniform float minX;
uniform float minY;
uniform float maxX;
uniform float maxY;

void main()
{
    vec2 position = QuadPosition(gl_VertexID, minX, minY, maxX, maxY);
    gl_Position = UVToNDC(inViewport, position);
}
`;

// -------------------------------------------------------------------------------------------------------------------------------
// Fragment Shader
// -------------------------------------------------------------------------------------------------------------------------------

const WindowFShader = ShaderShared + `

out vec4 outColour;

void main()
{
    outColour = vec4(1.0, 1.0, 1.0, 0.0);
}
`;
