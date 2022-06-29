const ShaderShared = `#version 300 es

precision mediump float;

struct Viewport
{
    float width;
    float height;
};

vec2 QuadPosition(int vertex_id, float min_x, float min_y, float max_x, float max_y)
{
    // Quad indices are:
    //
    //  2        3
    //    +----+
    //    |    |
    //    +----+
    //  0        1
    //
    vec2 position;
    position.x = (vertex_id & 1) == 0 ? min_x : max_x;
    position.y = (vertex_id & 2) == 0 ? min_y : max_y;
    return position;
}

vec4 UVToNDC(Viewport viewport, vec2 uv)
{
    //
    // NDC is:
    //    -1 to 1, left to right
    //    -1 to 1, bottom to top
    //
    vec4 ndc_pos;
    ndc_pos.x = (uv.x / viewport.width) * 2.0 - 1.0;
    ndc_pos.y = 1.0 - (uv.y / viewport.height) * 2.0;
    ndc_pos.z = 0.0;
    ndc_pos.w = 1.0;
    return ndc_pos;
}

vec4 TextureBufferLookup(sampler2D sampler, float index, float length)
{
    vec2 uv = vec2((index + 0.5) / length, 0.5);
    return texture(sampler, uv);
}

struct TextBufferDesc
{
    float fontWidth;
    float fontHeight;
    float textBufferLength;
};

uniform sampler2D inFontAtlasTexture;
uniform sampler2D inTextBuffer;
uniform TextBufferDesc inTextBufferDesc;

float LookupCharacter(float char_ascii, float pos_x, float pos_y, float font_width_px, float font_height_px)
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

    // Strip colour and return alpha only
    return texture(inFontAtlasTexture, uv).a;
}

float LookupText(vec2 render_pos_px, float text_buffer_offset, float text_length_chars)
{
    // Font description
    float font_width_px = inTextBufferDesc.fontWidth;
    float font_height_px = inTextBufferDesc.fontHeight;
    float text_buffer_length = inTextBufferDesc.textBufferLength;
    float text_length_px = text_length_chars * font_width_px;

    // Text pixel position clamped to the bounds of the full word, allowing leakage to neighbouring NULL characters to pad zeroes
    vec2 text_pixel_pos;
    text_pixel_pos.x = max(min(render_pos_px.x, text_length_px), -1.0);
    text_pixel_pos.y = max(min(render_pos_px.y, font_height_px - 1.0), 0.0);

    // Index of the current character in the text buffer
    float text_index = text_buffer_offset + floor(text_pixel_pos.x / font_width_px);

    // Sample the 1D text buffer to get the ASCII character index
    float char_ascii = TextureBufferLookup(inTextBuffer, text_index, text_buffer_length).a * 255.0;

    return LookupCharacter(char_ascii,
        text_pixel_pos.x - (text_index - text_buffer_offset) * font_width_px,
        text_pixel_pos.y,
        font_width_px, font_height_px);
}

float NbIntegerCharsForNumber(float number)
{
    float number_int = floor(number);
    return number_int == 0.0 ? 1.0 : floor(log(number_int) / 2.302585092994046) + 1.0;
}

// Base-10 lookup table for shifting digits of a float to the range 0-9 where they can be rendered
const float g_Multipliers[14] = float[14](
    // Decimal part multipliers
    1000.0, 100.0, 10.0,
    // Zero entry for maintaining the ASCII "." base when rendering the period
    0.0,
    // Integer part multipliers
    1.0, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001, 0.00000001, 0.000000001 );

float LookupNumber(vec2 render_pos_px, float number, float nb_float_chars)
{
    // Font description
    float font_width_px = inTextBufferDesc.fontWidth;
    float font_height_px = inTextBufferDesc.fontHeight;
    float text_buffer_length = inTextBufferDesc.textBufferLength;

    float number_integer_chars = NbIntegerCharsForNumber(number);

    // Clip
    render_pos_px.y = max(min(render_pos_px.y, font_height_px - 1.0), 0.0);

    float number_index = floor(render_pos_px.x / font_width_px);

    if (number_index >= 0.0 && number_index < number_integer_chars + nb_float_chars)
    {
        // When we are indexing the period separating integer and decimal, set the base to ASCII "."
        // The lookup table stores zero for this entry, multipying with the addend to produce no shift from this base
        float base = (number_index == number_integer_chars) ? 46.0 : 48.0;

        // Calculate digit using the current number index base-10 shift
        float multiplier = g_Multipliers[int(number_integer_chars - number_index) + 3];
        float number_shifted_int = floor(number * multiplier);
        float number_digit = floor(mod(number_shifted_int, 10.0));

        return LookupCharacter(base + number_digit,
            render_pos_px.x - number_index * font_width_px,
            render_pos_px.y,
            font_width_px, font_height_px);
    }

    return 0.0;
}
`;
