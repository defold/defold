package com.defold.editor.pipeline;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;


public class BMFont
{

    static public class BMFontFormatException extends Exception {
        private static final long serialVersionUID = -3244090862970667930L;

        public BMFontFormatException(String reason) {
        super(reason);
        }
    }


    public enum ChannelData {
        GLYPH, OUTLINE,
        GLYPH_AND_OUTLINE,
        ZERO, ONE
    }

    public enum ChannelID {
        BLUE, GREEN, RED, ALPHA, ALL
    }

    // info
    public String face;
    public float size;
    public Boolean bold;
    public Boolean italics;
    public String charset;
    public Boolean unicode;
    public float stretchH;
    public Boolean smooth;
    public int aa;
    public float padding[];
    public float spacing[];
    public int outline;

    // common
    public int lineHeight;
    public int base;
    public int scaleW;
    public int scaleH;
    public int pages;
    public Boolean packed;
    public ChannelData alphaChnl;
    public ChannelData redChnl;
    public ChannelData greenChnl;
    public ChannelData blueChnl;

    // page
    public ArrayList<String> page;

    // char
    public class Char {
        public int id;
        public int x;
        public int y;
        public int width;
        public int height;
        public float xoffset;
        public float yoffset;
        public float xadvance;
        public int page;
        public ChannelID chnl;
    }

    public int chars;
    public ArrayList<Char> charArray;

    static public class DefaultHashMap<K,V> extends HashMap<K,V> {
        private static final long serialVersionUID = 1473053429689898384L;
        public DefaultHashMap() {}
        public V get(Object k, V defaultValue) {
            return containsKey(k) ? super.get(k) : defaultValue;
        }
    }

    private enum ParseState
    {
        PARSE_STATE_KEY, PARSE_STATE_VALUE, PARSE_STATE_VALUE_QUOTED
    }

    static public DefaultHashMap<String,String> splitEntries(String line) throws BMFontFormatException
    {
        DefaultHashMap<String,String> entries = new DefaultHashMap<String,String>();

        ParseState state = ParseState.PARSE_STATE_KEY;
        String cur_key = "";
        String cur_value = "";

        // Simple state machine like parsing
        for (int i = 0; i < line.length(); i++) {

            char c = line.charAt(i);

            switch (state) {
                case PARSE_STATE_KEY:
                {

                    if (c == '=') {
                        state = ParseState.PARSE_STATE_VALUE;
                        cur_value = "";
                    } else {
                        cur_key = cur_key + c;
                    }

                } break;

                case PARSE_STATE_VALUE:
                {
                    if (c == '"') {
                        state = ParseState.PARSE_STATE_VALUE_QUOTED;
                        cur_value = "";
                    } else if (c == ' ') {

                        if (!cur_key.isEmpty() && !cur_value.isEmpty()) {
                            entries.put(cur_key.trim(), cur_value);
                        }
                        state = ParseState.PARSE_STATE_KEY;
                        cur_key = "";
                        cur_value = "";

                    } else {
                        cur_value = cur_value + c;
                    }
                } break;

                case PARSE_STATE_VALUE_QUOTED:
                {
                    if (c == '"') {

                        if (!cur_key.isEmpty() && !cur_value.isEmpty()) {
                            entries.put(cur_key.trim(), cur_value);
                        }
                        state = ParseState.PARSE_STATE_KEY;
                        cur_key = "";
                        cur_value = "";
                        i+=1;

                    } else {
                        cur_value = cur_value + c;
                    }
                } break;

            }

        }

        if (state == ParseState.PARSE_STATE_VALUE_QUOTED)
        {
            throw new BMFontFormatException("Quoted string entry not closed!");
        }

        // add if there is any entry lingering
        if (!cur_key.isEmpty() && !cur_value.isEmpty()) {
            entries.put(cur_key.trim(), cur_value);
        }

        return entries;
    }

    public BMFont()
    {

    }

    public Boolean parse(InputStream data) throws BMFontFormatException, IOException
    {
        BufferedInputStream buff_data = new BufferedInputStream(data);
        buff_data.mark(3);

        // check if binary
        if (buff_data.read() == 66 && buff_data.read() == 77 && buff_data.read() == 70) {
            return parseBinary(buff_data);
        }
        buff_data.reset();
        return parseText(buff_data);
    }

    private Boolean parseBinary(BufferedInputStream data) throws BMFontFormatException
    {
        throw new BMFontFormatException("Binary BMFont files not supported!");
    }

    static private Boolean parseIntBoolean( String val, Boolean defaultValue )
    {
        int v;
        try {
            v = Integer.parseInt(val);
        } catch (NumberFormatException e) {
            return defaultValue;
        }

        return (v > 1) ? true : false;
    }

    static private float[] parseFloatArray( String val )
    {
        String s[] = val.split(",");
        float f[] = new float[s.length];

        for (int i = 0; i < s.length; i++)
        {
            f[i] = Float.parseFloat(s[i]);
        }

        return f;
    }

    static private ChannelData parseIntChannelData( String val, ChannelData defaultValue )
    {
        int v;
        try {
            v = Integer.parseInt(val);
        } catch (NumberFormatException e) {
            return defaultValue;
        }

        switch (v) {
            case 0:
                return ChannelData.GLYPH;
            case 1:
                return ChannelData.OUTLINE;
            case 2:
                return ChannelData.GLYPH_AND_OUTLINE;
            case 3:
                return ChannelData.ZERO;
            case 4:
                return ChannelData.ONE;
            default:
                return defaultValue;
        }
    }

    static private ChannelID parseIntChannelID( String val, ChannelID defaultValue )
    {
        int v;
        try {
            v = Integer.parseInt(val);
        } catch (NumberFormatException e) {
            return defaultValue;
        }

        switch (v) {
            case 1:
                return ChannelID.BLUE;
            case 2:
                return ChannelID.GREEN;
            case 4:
                return ChannelID.RED;
            case 8:
                return ChannelID.ALPHA;
            case 15:
                return ChannelID.ALL;
            default:
                return defaultValue;
        }
    }

    static private int parseInt( String val, String typeId, String entryId ) throws BMFontFormatException
    {
        int v;
        try {
            v = Integer.parseInt(val);
        } catch (NumberFormatException e) {
            throw new BMFontFormatException("Expected int as value for '" + typeId + "." + entryId + "'.");
        }

        return v;
    }

    static private float parseFloat( String val, String typeId, String entryId ) throws BMFontFormatException
    {
        float v;
        try {
            v = Float.parseFloat(val);
        } catch (NumberFormatException e) {
            throw new BMFontFormatException("Expected float as value for '" + typeId + "." + entryId + "'.");
        }

        return v;
    }

    public Boolean parseText(BufferedInputStream data) throws BMFontFormatException, IOException
    {
        BufferedReader in = new BufferedReader(new InputStreamReader(data));
        String line = null;
        this.charArray = new ArrayList<Char>();
        this.page = new ArrayList<String>();
        String entry_id = "";
        boolean foundInfoEntry = false;

        while ((line = in.readLine()) != null) {

            String[] line_parts = line.split(" ", 2);
            entry_id = line_parts[0].toLowerCase();
            DefaultHashMap<String,String> entries = null;

            if (line_parts.length > 1)
            {
                entries = splitEntries(line_parts[1]);
            }

            switch (entry_id) {
                case "info":
                {
                    this.face     = entries.get("face", "");
                    this.size     = parseFloat(entries.get("size", "0.0"), "info", "size");
                    this.bold     = parseIntBoolean(entries.get("bold", "0"), false);
                    this.italics  = parseIntBoolean(entries.get("italics", "0"), false);
                    this.charset  = entries.get("charset", "");
                    this.unicode  = parseIntBoolean(entries.get("unicode", "0"), false);
                    this.stretchH = parseFloat(entries.get("stretchH", "100"), "info", "stretchH");
                    this.smooth   = parseIntBoolean(entries.get("smooth", "0"), false);
                    this.aa       = parseInt(entries.get("aa", "1"), "info", "aa");
                    this.padding  = parseFloatArray(entries.get("padding", ""));
                    this.spacing  = parseFloatArray(entries.get("spacing", ""));
                    this.outline  = parseInt(entries.get("outline", "0"), "info", "outline");

                    foundInfoEntry = true;

                } break;
                case "common":
                {
                    this.lineHeight = parseInt(entries.get("lineHeight", "0"), "common", "lineHeight");
                    this.base       = parseInt(entries.get("base", "0"), "common", "base");
                    this.scaleW     = parseInt(entries.get("scaleW", "0"), "common", "scaleW");
                    this.scaleH     = parseInt(entries.get("scaleH", "0"), "common", "scaleH");
                    this.pages      = parseInt(entries.get("pages", "0"), "common", "pages");
                    this.packed     = parseIntBoolean(entries.get("packed", "0"), false);
                    this.alphaChnl  = parseIntChannelData(entries.get("alphaChnl", "0"), ChannelData.GLYPH);
                    this.redChnl    = parseIntChannelData(entries.get("redChnl", "0"), ChannelData.GLYPH);
                    this.greenChnl  = parseIntChannelData(entries.get("greenChnl", "0"), ChannelData.GLYPH);
                    this.blueChnl   = parseIntChannelData(entries.get("blueChnl", "0"), ChannelData.GLYPH);

                } break;
                case "chars":
                {
                    this.chars = parseInt(entries.get("count", "0"), "chars", "count");

                } break;
                case "char":
                {
                    assert(this.chars <= this.charArray.size() + 1 );

                    Char c = new Char();
                    c.id       = parseInt(entries.get("id", "0"), "char", "id");
                    c.x        = parseInt(entries.get("x", "0"), "char", "x");
                    c.y        = parseInt(entries.get("y", "0"), "char", "y");
                    c.width    = parseInt(entries.get("width", "0"), "char", "width");
                    c.height   = parseInt(entries.get("height", "0"), "char", "height");
                    c.xoffset  = parseFloat(entries.get("xoffset", "0.0"), "char", "xoffset");
                    c.yoffset  = parseFloat(entries.get("yoffset", "0.0"), "char", "yoffset");
                    c.xadvance = parseFloat(entries.get("xadvance", "0.0"), "char", "xadvance");
                    c.page     = parseInt(entries.get("page", "0"), "char", "page");
                    c.chnl     = parseIntChannelID(entries.get("chnl", "0"), ChannelID.ALL);

                    this.charArray.add(c);

                } break;
                case "page":
                {
                    this.page.add(entries.get("file", ""));

                } break;
                case "": // empty rows
                case "kerning":
                case "kernings":
                    // ignored/not supported in defold
                    break;
                default:
                    throw new BMFontFormatException("Unknown BMFont entry: " + entry_id);
            }

        }

        if (!foundInfoEntry) {
            throw new BMFontFormatException("Invalid BMFont file, missing info entry.");
        }

        return true;
    }

}
