const TimelineVShader =`#version 300 es

#define CANVAS_BORDER 1.0
#define SAMPLE_HEIGHT 16.0
#define SAMPLE_BORDER 1.0
#define SAMPLE_Y_SPACING (SAMPLE_HEIGHT + SAMPLE_BORDER * 2.0)
#define SAMPLE_Y_OFFSET (CANVAS_BORDER + 1.0)

struct Viewport
{
    float width;
    float height;
};

struct TimeRange
{
    float usStart;
    float usPerPixel;
};

struct Row
{
    float yOffset;
};

uniform Viewport inViewport;
uniform TimeRange inTimeRange;
uniform Row inRow;

in vec4 inSample_TextOffset;
in vec4 inColour_TextLength;

out vec4 varColour_TimeMs;
out vec4 varPosInBoxPx_TextEntry;
out float varTimeChars;

//#define PIXEL_ROUNDED_OFFSETS

float PixelOffset(float time_us)
{
    float offset = (time_us - inTimeRange.usStart) * inTimeRange.usPerPixel;
    #ifdef PIXEL_ROUNDED_OFFSETS
        return floor(offset);
    #else
        return offset;
    #endif
}

float PixelSize(float time_us)
{
    float size = time_us * inTimeRange.usPerPixel;
    #ifdef PIXEL_ROUNDED_OFFSETS
        return floor(size);
    #else
        return size;
    #endif
}

void main()
{
    // Unpack input data
    float us_start = inSample_TextOffset.x;
    float us_length = inSample_TextOffset.y;
    float depth = inSample_TextOffset.z;
    float text_buffer_offset = inSample_TextOffset.w;
    vec3 box_colour = inColour_TextLength.rgb;
    float text_length_chars = inColour_TextLength.w;

    // Determine pixel range of the sample
    float x0 = PixelOffset(us_start);
    float x1 = x0 + PixelSize(us_length);

    // Calculate box to render
    float offset_x = x0;
    float offset_y = inRow.yOffset + SAMPLE_Y_OFFSET + (depth - 1.0) * SAMPLE_Y_SPACING;
    float size_x = max(x1 - x0, 1.0);
    float size_y = SAMPLE_HEIGHT;

    // Box range
    float min_x = offset_x;
    float min_y = offset_y;
    float max_x = offset_x + size_x;
    float max_y = offset_y + size_y;

    // Quad indices are:
    //
    //  2        3
    //    +----+
    //    |    |
    //    +----+
    //  0        1
    //
    vec2 position;
    position.x = (gl_VertexID & 1) == 0 ? min_x : max_x;
    position.y = (gl_VertexID & 2) == 0 ? min_y : max_y;

    //
    // NDC is:
    //    -1 to 1, left to right
    //    -1 to 1, bottom to top
    //
    vec4 ndc_pos;
    ndc_pos.x = (position.x / inViewport.width) * 2.0 - 1.0;
    ndc_pos.y = 1.0 - (position.y / inViewport.height) * 2.0;
    ndc_pos.z = 0.0;
    ndc_pos.w = 1.0;

    // Calculate number of characters required to display the millisecond time
    float time_ms = us_length / 1000.0;
    float time_ms_int = floor(time_ms);
    float time_chars = time_ms_int == 0.0 ? 1.0 : floor(log(time_ms_int) / 2.302585092994046) + 1.0;

    gl_Position = ndc_pos;

    varColour_TimeMs = vec4(box_colour / 255.0, time_ms);
    varPosInBoxPx_TextEntry = vec4(position.x - offset_x, position.y - offset_y, text_buffer_offset, text_length_chars);
    varTimeChars = time_chars;
}
`;

const TimelineFShader = `#version 300 es

precision mediump float;

#define SAMPLE_HEIGHT 16.0

struct TextBufferDesc
{
    float fontWidth;
    float fontHeight;
    float textBufferLength;
};

uniform sampler2D inFontAtlasTextre;
uniform sampler2D inTextBuffer;
uniform TextBufferDesc inTextBufferDesc;

in vec4 varColour_TimeMs;
in vec4 varPosInBoxPx_TextEntry;
in float varTimeChars;

out vec4 outColour;

vec4 LookupCharacter(float char_ascii, float pos_x, float pos_y, float font_width_px, float font_height_px)
{
    // 2D index of the ASCII character in the font atlas
    float char_index_y = floor(char_ascii / 16.0);
    float char_index_x = char_ascii - char_index_y * 16.0;

    // Start UV of the character in the font atlas
    float char_base_uv_x = char_index_x / 16.0;
    float char_base_uv_y = char_index_y / 16.0;

    // UV within the character itself, scaled to the font atlas
    float char_uv_x = pos_x / (font_width_px * 16.0);
    float char_uv_y = pos_y / (font_height_px * 16.0);

    vec2 uv;
    uv.x = char_base_uv_x + char_uv_x;
    uv.y = char_base_uv_y + char_uv_y;

    // Apply colour to the text in premultiplied alpha space
    vec4 t = texture(inFontAtlasTextre, uv);
    vec3 colour = vec3(1.0, 1.0, 1.0) * 0.25;
    return vec4(colour * t.a, t.a);
}

void main()
{
    // Font description
    float font_width_px = inTextBufferDesc.fontWidth;
    float font_height_px = inTextBufferDesc.fontHeight;
    float text_buffer_length = inTextBufferDesc.textBufferLength;

    // Text range in the text buffer
    vec2 pos_in_box_px = varPosInBoxPx_TextEntry.xy; 
    float text_buffer_offset = varPosInBoxPx_TextEntry.z;
    float text_length_chars = varPosInBoxPx_TextEntry.w;
    float text_length_px = text_length_chars * font_width_px;

    // Text placement offset within the box
    const vec2 text_offset_px = vec2(4.0, 3.0);

    vec4 box_colour = vec4(varColour_TimeMs.rgb, 1.0);

    // Add a subtle border to the box so that you can visually separate samples when they are next to each other
    vec2 top_left = min(pos_in_box_px.xy, 2.0);
    float both = min(top_left.x, top_left.y);
    box_colour.rgb *= (0.8 + both * 0.1);

    vec4 text_colour = vec4(0.0);

    float text_end_px = text_length_px + text_offset_px.x + font_width_px;
    float time_length_px = (varTimeChars + 4.0) * font_width_px;
    if (pos_in_box_px.x > text_end_px && pos_in_box_px.x < text_end_px + time_length_px)
    {
        float time_ms = varColour_TimeMs.w;

        vec2 time_pixel_pos;
        time_pixel_pos.x = max(min(pos_in_box_px.x - text_end_px, time_length_px), 0.0);
        time_pixel_pos.y = max(min(pos_in_box_px.y - text_offset_px.y, font_height_px - 1.0), 0.0);

        float time_index = floor(time_pixel_pos.x / font_width_px);
        if (time_index < varTimeChars)
        {
            // Use base-10 integer digit counting to calculate the divisor needed to bring this digit below 10
            float time_divisor = 1.0;
            for (int i = 0; i < int(varTimeChars - time_index - 1.0); i++)
            {
                time_divisor *= 10.0;
            }

            // Calculate digit
            float time_shifted_int = floor(time_ms / time_divisor);
            float time_digit = floor(mod(time_shifted_int, 10.0));

            text_colour = LookupCharacter(48.0 + time_digit,
                time_pixel_pos.x - time_index * font_width_px,
                time_pixel_pos.y,
                font_width_px, font_height_px);
        }
        else if (time_index == varTimeChars)
        {
            text_colour = LookupCharacter(46.0,
                time_pixel_pos.x - time_index * font_width_px,
                time_pixel_pos.y,
                font_width_px, font_height_px);
        }
        else if (time_index == varTimeChars + 1.0)
        {
            float time_digit = floor(mod(time_ms * 10.0, 10.0));
            text_colour = LookupCharacter(48.0 + time_digit,
                time_pixel_pos.x - time_index * font_width_px,
                time_pixel_pos.y,
                font_width_px, font_height_px);
        }
        else if (time_index == varTimeChars + 2.0)
        {
            float time_digit = floor(mod(time_ms * 10.0, 10.0));
            text_colour = LookupCharacter(109.0,
                time_pixel_pos.x - time_index * font_width_px,
                time_pixel_pos.y,
                font_width_px, font_height_px);
        }
        else if (time_index == varTimeChars + 3.0)
        {
            float time_digit = floor(mod(time_ms * 10.0, 10.0));
            text_colour = LookupCharacter(115.0, time_pixel_pos.x - time_index * font_width_px, time_pixel_pos.y, font_width_px, font_height_px);
        }
    }
    else
    {
        // Text pixel position clamped to the bounds of the full word, allowing leakage to neighbouring NULL characters to pad zeroes
        vec2 text_pixel_pos;
        text_pixel_pos.x = max(min(pos_in_box_px.x - text_offset_px.x, text_length_px), -1.0);
        text_pixel_pos.y = max(min(pos_in_box_px.y - text_offset_px.y, font_height_px - 1.0), 0.0);

        // Index of the current character in the text buffer
        float text_index = text_buffer_offset + floor(text_pixel_pos.x / font_width_px);

        // Sample the 1D text buffer to get the ASCII character index
        vec2 char_uv = vec2((text_index + 0.5) / text_buffer_length, 0.5);
        float char_ascii = texture(inTextBuffer, char_uv).a * 255.0;

        text_colour = LookupCharacter(char_ascii,
            text_pixel_pos.x - (text_index - text_buffer_offset) * font_width_px,
            text_pixel_pos.y,
            font_width_px, font_height_px);
    }

    // Bring out of premultiplied alpha space and lerp with the box colour
    float inv_alpha = text_colour.a == 0.0 ? 1.0 : 1.0 / text_colour.a;
    outColour = mix(box_colour, vec4(text_colour.rgb * inv_alpha, 1.0), text_colour.a);
}
`;
