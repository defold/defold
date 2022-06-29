const GridShaderShared = ShaderShared + `

#define RowHeight 15.0

struct Grid
{
    float minX;
    float minY;
    float maxX;
    float maxY;
    float pixelOffsetX;
    float pixelOffsetY;
};

uniform Viewport inViewport;
uniform Grid inGrid;

float Row(vec2 pixel_position)
{
    return floor(pixel_position.y / RowHeight);
}

vec3 RowColour(float row)
{
    float row_grey = (int(row) & 1) == 0 ? 0.25 : 0.23;
    return vec3(row_grey);
}

`;

// -------------------------------------------------------------------------------------------------------------------------------
// Vertex Shader
// -------------------------------------------------------------------------------------------------------------------------------

const GridVShader = GridShaderShared + `

out vec2 varPixelPosition;

void main()
{
    vec2 position = QuadPosition(gl_VertexID, inGrid.minX, inGrid.minY, inGrid.maxX, inGrid.maxY);
    vec4 ndc_pos = UVToNDC(inViewport, position);
    
    gl_Position = ndc_pos;
    varPixelPosition = position - vec2(inGrid.minX, inGrid.minY) + vec2(inGrid.pixelOffsetX, inGrid.pixelOffsetY);
}
`;

const GridNumberVShader = GridShaderShared + `

out vec2 varPixelPosition;

void main()
{
    vec2 position = QuadPosition(gl_VertexID, inGrid.minX, inGrid.minY, inGrid.maxX, inGrid.maxY);
    vec4 ndc_pos = UVToNDC(inViewport, position);
    
    gl_Position = ndc_pos;
    varPixelPosition = position - vec2(inGrid.minX, inGrid.minY) + vec2(inGrid.pixelOffsetX, inGrid.pixelOffsetY);
}
`;

// -------------------------------------------------------------------------------------------------------------------------------
// Fragment Shader
// -------------------------------------------------------------------------------------------------------------------------------

const GridFShader = GridShaderShared + `

// Array of samples
uniform sampler2D inSamples;
uniform float inSamplesLength;
uniform float inFloatsPerSample;
uniform float inNbSamples;

in vec2 varPixelPosition;

out vec4 outColour;

void main()
{
    // Font description
    float font_width_px = inTextBufferDesc.fontWidth;
    float font_height_px = inTextBufferDesc.fontHeight;
    float text_buffer_length = inTextBufferDesc.textBufferLength;

    // Which row are we on?
    float row = Row(varPixelPosition);
    vec3 row_colour = RowColour(row);

    float text_weight = 0.0;
    vec3 text_colour = vec3(0.0);
    if (row < inNbSamples)
    {
        // Unpack colour and depth
        int colour_depth = floatBitsToInt(TextureBufferLookup(inSamples, row * inFloatsPerSample + 2.0, inSamplesLength).r);
        text_colour.r = float(colour_depth & 255) / 255.0;
        text_colour.g = float((colour_depth >> 8) & 255) / 255.0;
        text_colour.b = float((colour_depth >> 16) & 255) / 255.0;
        float depth = float(colour_depth >> 24);

        float text_buffer_offset = TextureBufferLookup(inSamples, row * inFloatsPerSample + 0.0, inSamplesLength).r;
        float text_length_chars = TextureBufferLookup(inSamples, row * inFloatsPerSample + 1.0, inSamplesLength).r;
        float text_length_px = text_length_chars * font_width_px;

        // Pixel position within the row
        vec2 pos_in_box_px;
        pos_in_box_px.x = varPixelPosition.x;
        pos_in_box_px.y = varPixelPosition.y - row * RowHeight;

        // Get text at this position
        vec2 text_start_px = vec2(4.0 + depth * 10.0, 3.0);
        text_weight = LookupText(pos_in_box_px - text_start_px, text_buffer_offset, text_length_chars);
    }
    
    outColour = vec4(mix(row_colour, text_colour, text_weight), 1.0);
}
`;

const GridNumberFShader = GridShaderShared + `

// Array of samples
uniform sampler2D inSamples;
uniform float inSamplesLength;
uniform float inFloatsPerSample;
uniform float inNbSamples;

// Offset within the sample
uniform float inNumberOffset;

uniform float inNbFloatChars;

in vec2 varPixelPosition;

out vec4 outColour;

void main()
{
    // Font description
    float font_width_px = inTextBufferDesc.fontWidth;
    float font_height_px = inTextBufferDesc.fontHeight;
    float text_buffer_length = inTextBufferDesc.textBufferLength;

    // Which row are we on?
    float row = Row(varPixelPosition);
    vec3 row_colour = RowColour(row);
    float text_weight = 0.0;
    if (row < inNbSamples)
    {
        // Pixel position within the row
        vec2 pos_in_box_px;
        pos_in_box_px.x = varPixelPosition.x;
        pos_in_box_px.y = varPixelPosition.y - row * RowHeight;

        // Get the number at this pixel
        const vec2 text_start_px = vec2(4.0, 3.0);
        float number = TextureBufferLookup(inSamples, row * inFloatsPerSample + inNumberOffset, inSamplesLength).r;
        text_weight = LookupNumber(pos_in_box_px - text_start_px, number, inNbFloatChars);
    }
    
    outColour = vec4(mix(row_colour, vec3(1.0), text_weight), 1.0);
}
`;
