const TimelineShaderShared = ShaderShared + `

#define SAMPLE_HEIGHT 16.0
#define SAMPLE_BORDER 2.0
#define SAMPLE_Y_SPACING (SAMPLE_HEIGHT + SAMPLE_BORDER * 2.0)

#define PIXEL_ROUNDED_OFFSETS

struct Container
{
    float x0;
    float y0;
    float x1;
    float y1;
};

struct TimeRange
{
    float msStart;
    float msPerPixel;
};

struct Row
{
    float yOffset;
};

uniform Viewport inViewport;
uniform TimeRange inTimeRange;
uniform Container inContainer;
uniform Row inRow;

float PixelOffset(float time_ms)
{
    float offset = (time_ms - inTimeRange.msStart) * inTimeRange.msPerPixel;
    #ifdef PIXEL_ROUNDED_OFFSETS
        return floor(offset);
    #else
        return offset;
    #endif
}

float PixelSize(float time_ms)
{
    float size = time_ms * inTimeRange.msPerPixel;
    #ifdef PIXEL_ROUNDED_OFFSETS
        return floor(size);
    #else
        return size;
    #endif
}

vec4 SampleQuad(int vertex_id, vec4 in_sample_textoffset, float padding, out vec4 out_quad_pos_size_px)
{
    // Unpack input data
    float ms_start = in_sample_textoffset.x;
    float ms_length = in_sample_textoffset.y;
    float depth = in_sample_textoffset.z;

    // Determine pixel range of the sample
    float x0 = PixelOffset(ms_start);
    float x1 = x0 + PixelSize(ms_length);

    // Calculate box to render
    // Ensure no sample is less than one pixel in length and so is always visible
    float offset_x = inContainer.x0 + x0 - padding;
    float offset_y = inRow.yOffset + (depth - 1.0) * SAMPLE_Y_SPACING + SAMPLE_BORDER - padding;
    float size_x = max(x1 - x0, 1.0) + padding * 2.0;
    float size_y = SAMPLE_HEIGHT + padding * 2.0;

    // Box range clipped to container bounds
    float min_x = min(max(offset_x, inContainer.x0), inContainer.x1);
    float min_y = min(max(offset_y, inContainer.y0), inContainer.y1);
    float max_x = min(max(offset_x + size_x, inContainer.x0), inContainer.x1);
    float max_y = min(max(offset_y + size_y, inContainer.y0), inContainer.y1);

    // Box quad position in NDC
    vec2 position = QuadPosition(vertex_id, min_x, min_y, max_x, max_y);
    vec4 ndc_pos = UVToNDC(inViewport, position);

    out_quad_pos_size_px.xy = vec2(position.x - offset_x, position.y - offset_y);
    out_quad_pos_size_px.zw = vec2(max_x - min_x, max_y - min_y);

    return ndc_pos;
}

`;

// -------------------------------------------------------------------------------------------------------------------------------
// Sample Rendering
// -------------------------------------------------------------------------------------------------------------------------------

const TimelineVShader = TimelineShaderShared + `

in vec4 inSample_TextOffset;
in vec4 inColour_TextLength;

out vec4 varColour_TimeMs;
out vec4 varPosInBoxPx_TextEntry;
out float varTimeChars;

void main()
{
    // Unpack input data
    float ms_length = inSample_TextOffset.y;
    float text_buffer_offset = inSample_TextOffset.w;
    vec3 box_colour = inColour_TextLength.rgb;
    float text_length_chars = inColour_TextLength.w;

    // Calculate number of characters required to display the millisecond time
    float time_chars = NbIntegerCharsForNumber(ms_length);

    // Calculate sample quad vertex positions
    vec4 quad_pos_size_px;
    gl_Position = SampleQuad(gl_VertexID, inSample_TextOffset, 0.0, quad_pos_size_px);

    // Pack for fragment shader
    varColour_TimeMs = vec4(box_colour / 255.0, ms_length);
    varPosInBoxPx_TextEntry = vec4(quad_pos_size_px.x, quad_pos_size_px.y, text_buffer_offset, text_length_chars);
    varTimeChars = time_chars;
}
`;

const TimelineFShader = TimelineShaderShared + `

in vec4 varColour_TimeMs;
in vec4 varPosInBoxPx_TextEntry;
in float varTimeChars;

out vec4 outColour;

void main()
{
    // Font description
    float font_width_px = inTextBufferDesc.fontWidth;
    float font_height_px = inTextBufferDesc.fontHeight;

    // Text range in the text buffer
    vec2 pos_in_box_px = varPosInBoxPx_TextEntry.xy; 
    float text_buffer_offset = varPosInBoxPx_TextEntry.z;
    float text_length_chars = varPosInBoxPx_TextEntry.w;
    float text_length_px = text_length_chars * font_width_px;

    // Text placement offset within the box
    const vec2 text_start_px = vec2(10.0, 3.0);

    vec3 box_colour = varColour_TimeMs.rgb;

    // Add a subtle border to the box so that you can visually separate samples when they are next to each other
    vec2 top_left = min(pos_in_box_px.xy, 2.0);
    float both = min(top_left.x, top_left.y);
    box_colour *= (0.8 + both * 0.1);

    float text_weight = 0.0;

    // Are we over the time number or the text?
    float text_end_px = text_start_px.x + text_length_px;
    float number_start_px = text_end_px + font_width_px * 2.0;
    if (pos_in_box_px.x > number_start_px)
    {
        vec2 time_pixel_pos;
        time_pixel_pos.x = pos_in_box_px.x - number_start_px;
        time_pixel_pos.y = max(min(pos_in_box_px.y - text_start_px.y, font_height_px - 1.0), 0.0);

        // Time number
        float time_ms = varColour_TimeMs.w;
        float time_index = floor(time_pixel_pos.x / font_width_px);
        if (time_index < varTimeChars + 4.0)
        {
            text_weight = LookupNumber(time_pixel_pos, time_ms, 4.0);
        }

        // " ms" label at the end of the time
        else if (time_index < varTimeChars + 7.0)
        {
            const float ms[3] = float[3] ( 32.0, 109.0, 115.0 );
            float char = ms[int(time_index - (varTimeChars + 4.0))];
            text_weight = LookupCharacter(char, time_pixel_pos.x - time_index * font_width_px, time_pixel_pos.y, font_width_px, font_height_px);
        }
    }
    else
    {
        text_weight = LookupText(pos_in_box_px - text_start_px, text_buffer_offset, text_length_chars);
    }

    // Blend text onto the box
    vec3 text_colour = vec3(0.0, 0.0, 0.0);
    outColour = vec4(mix(box_colour, text_colour, text_weight), 1.0);
}
`;

// -------------------------------------------------------------------------------------------------------------------------------
// Sample Highlights
// -------------------------------------------------------------------------------------------------------------------------------

const TimelineHighlightVShader = TimelineShaderShared + `

uniform float inStartMs;
uniform float inLengthMs;
uniform float inDepth;

out vec4 varPosSize;

void main()
{
    // Calculate sample quad vertex positions
    gl_Position = SampleQuad(gl_VertexID, vec4(inStartMs, inLengthMs, inDepth, 0.0), 1.0, varPosSize);
}
`;

const TimelineHighlightFShader = TimelineShaderShared + `

// TODO(don): Vector uniforms, please!
uniform float inColourR;
uniform float inColourG;
uniform float inColourB;

in vec4 varPosSize;

out vec4 outColour;

void main()
{
    // Rounded pixel co-ordinates interpolating across the sample
    vec2 pos = floor(varPosSize.xy);

    // Sample size in pixel co-ordinates
    vec2 size = floor(varPosSize.zw);

    // Highlight thickness
    float t = 2.0;

    // Distance along axes to highlight edges
    vec2 dmin = abs(pos - 0.0);
    vec2 dmax = abs(pos - (size - 1.0));

    // Take the closest distance
    float dx = min(dmin.x, dmax.x);
    float dy = min(dmin.y, dmax.y);
    float d = min(dx, dy);

    // Invert the distance and clamp to thickness
    d = (t + 1.0) - min(d, t + 1.0);

    // Scale with thickness for uniform intensity
    d = d / (t + 1.0);
    outColour = vec4(inColourR * d, inColourG * d, inColourB * d, d);
}
`;

// -------------------------------------------------------------------------------------------------------------------------------
// GPU->CPU Sample Sources
// -------------------------------------------------------------------------------------------------------------------------------

const TimelineGpuToCpuVShader = TimelineShaderShared + `

uniform float inStartMs;
uniform float inLengthMs;
uniform float inDepth;

out vec4 varPosSize;

void main()
{
    // Calculate sample quad vertex positions
    gl_Position = SampleQuad(gl_VertexID, vec4(inStartMs, inLengthMs, inDepth, 0.0), 1.0, varPosSize);
}
`;


const TimelineGpuToCpuFShader = TimelineShaderShared + `

in vec4 varPosSize;

out vec4 outColour;

void main()
{
    // Rounded pixel co-ordinates interpolating across the sample
    vec2 pos = floor(varPosSize.xy);

    // Sample size in pixel co-ordinates
    vec2 size = floor(varPosSize.zw);

    // Distance to centre line, bumped out every period to create a dash
    float dc = abs(pos.y - size.y / 2.0);
    dc += (int(pos.x / 3.0) & 1) == 0 ? 100.0 : 0.0;

    // Min with the start line
    float ds = abs(pos.x - 0.0);
    float d = min(dc, ds);

    // Invert the distance for highlight
    d = 1.0 - min(d, 1.0);

    outColour = vec4(d, d, d, d);
}

`;

// -------------------------------------------------------------------------------------------------------------------------------
// Background
// -------------------------------------------------------------------------------------------------------------------------------

const TimelineBackgroundVShader = TimelineShaderShared + `

uniform float inYOffset;

out vec2 varPosition;

void main()
{

    // Container quad position in NDC
    vec2 position = QuadPosition(gl_VertexID, inContainer.x0, inContainer.y0, inContainer.x1, inContainer.y1);
    gl_Position = UVToNDC(inViewport, position);

    // Offset Y with scroll position
    varPosition = vec2(position.x, position.y - inYOffset);
}

`;

const TimelineBackgroundFShader = TimelineShaderShared + `

in vec2 varPosition;

out vec4 outColour;

void main()
{
    vec2 pos = floor(varPosition);
    float f = round(fract(pos.y / SAMPLE_Y_SPACING) * SAMPLE_Y_SPACING);
    float g = f >= 1.0 && f <= (SAMPLE_Y_SPACING - 2.0) ? 0.30 : 0.23;
    outColour = vec4(g, g, g, 1.0);
}
`;