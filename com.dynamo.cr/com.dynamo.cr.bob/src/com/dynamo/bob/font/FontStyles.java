// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.font;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.render.proto.Font.CompiledStyle;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.StyleDesc;
import com.dynamo.render.proto.Font.StyleEffect;

/** Shared source normalization and native style compilation for Bob and the editor. */
public final class FontStyles {
    private FontStyles() {}

    public static List<StyleDesc> getSourceStyles(FontDesc font) {
        List<StyleDesc> styles = new ArrayList<>(font.getStylesList());
        if (styles.isEmpty()) {
            styles.add(StyleDesc.newBuilder().setName("default").build());
            styles.add(StyleDesc.newBuilder().setName("link").setMarkup("<color=#1972e5><ul>").build());
            styles.add(StyleDesc.newBuilder().setName("link:hover").setMarkup("<color=#4ca5ff>").build());
            styles.add(StyleDesc.newBuilder().setName("link:active").setMarkup("<color=#0c4cb2>").build());
        }
        boolean hasDefault = false;
        for (StyleDesc style : styles)
            hasDefault |= style.getName().equals("default");
        if (!hasDefault)
            styles.add(0, StyleDesc.newBuilder().setName("default").build());
        return styles;
    }

    public static Set<String> readStyleNames(com.dynamo.bob.fs.IResource resource) throws java.io.IOException {
        FontDesc.Builder font = FontDesc.newBuilder();
        com.google.protobuf.TextFormat.merge(new String(resource.getContent(), java.nio.charset.StandardCharsets.UTF_8), font);
        Set<String> names = new HashSet<>();
        names.add("");
        for (StyleDesc style : getSourceStyles(font.build())) names.add(style.getName());
        return names;
    }

    public static boolean hasStyle(FontDesc font, String name) {
        if (name.isEmpty())
            return true;
        for (StyleDesc style : getSourceStyles(font))
            if (style.getName().equals(name))
                return true;
        return false;
    }

    public static List<CompiledStyle> compileStyles(FontDesc font) {
        List<CompiledStyle> result = new ArrayList<>();
        Set<String> names = new HashSet<>();
        for (StyleDesc source : getSourceStyles(font)) {
            if (source.getName().isBlank() || !names.add(source.getName()))
                throw new IllegalArgumentException("Font style names must be non-empty and unique: '" + source.getName() + "'");
            if (source.getName().equals("default") && !source.getMarkup().isEmpty())
                throw new IllegalArgumentException("The default font style is generated and cannot contain markup");
            FontRenderer.Style style;
            if (source.getName().equals("default")) {
                style = new FontRenderer.Style();
                if (!font.getFont().toLowerCase(java.util.Locale.ROOT).endsWith(".fnt")) {
                    if (font.getOutlineWidth() > 0 && font.getOutlineAlpha() > 0) {
                        style.flags |= FontRenderer.Style.FLAG_OUTLINE_WIDTH | FontRenderer.Style.FLAG_OUTLINE_ALPHA;
                        style.outlineWidth = font.getOutlineWidth();
                        style.outlineAlpha = font.getOutlineAlpha();
                    }
                    if (font.getShadowAlpha() > 0) {
                        style.flags |= FontRenderer.Style.FLAG_SHADOW_X | FontRenderer.Style.FLAG_SHADOW_Y
                                     | FontRenderer.Style.FLAG_SHADOW_BLUR | FontRenderer.Style.FLAG_SHADOW_ALPHA;
                        style.shadowX = font.getShadowX();
                        style.shadowY = font.getShadowY();
                        style.shadowBlur = font.getShadowBlur();
                        style.shadowAlpha = font.getShadowAlpha();
                    }
                }
            } else {
                try {
                    style = FontRenderer.compileStyle(source.getMarkup());
                } catch (IllegalArgumentException error) {
                    throw new IllegalArgumentException("Font style '" + source.getName() + "': " + error.getMessage(), error);
                }
            }
            result.add(toCompiledStyle(source.getName(), style));
        }
        return result;
    }

    private static CompiledStyle toCompiledStyle(String name, FontRenderer.Style style) {
        CompiledStyle.Builder output = CompiledStyle.newBuilder().setName(name).setNameHash(MurmurHash.hash64(name));
        output.setOutlineWidth(style.outlineWidth);
        output.setShadowX(style.shadowX);
        output.setShadowY(style.shadowY);
        output.setShadowBlur(style.shadowBlur);
        output.setOutlineAlpha(style.outlineAlpha);
        output.setShadowAlpha(style.shadowAlpha);
        output.setFlags(style.flags);
        output.setDecorationFlags(style.decorationFlags);
        output.setUnderlinePattern(style.underlinePattern);
        output.setStrikePattern(style.strikePattern);
        for (float value : style.faceColor) output.addFaceColor(value);
        for (float value : style.outlineColor) output.addOutlineColor(value);
        for (float value : style.shadowColor) output.addShadowColor(value);
        for (FontRenderer.StyleEffect effect : style.effects) {
            StyleEffect.Builder item = StyleEffect.newBuilder().setType(StyleEffect.Type.forNumber(effect.type));
            item.setAmplitude(effect.amplitude);
            item.setHz(effect.hz);
            item.setWavelength(effect.wavelength);
            item.setFit(effect.fit);
            item.setGradientMode(effect.gradientMode);
            for (float value : effect.colors) item.addColors(value);
            output.addEffects(item);
        }
        return output.build();
    }

    public static FontRenderer.Style toNativeStyle(CompiledStyle input) {
        FontRenderer.Style style = new FontRenderer.Style();
        style.outlineWidth = input.getOutlineWidth();
        style.shadowX = input.getShadowX();
        style.shadowY = input.getShadowY();
        style.shadowBlur = input.getShadowBlur();
        style.outlineAlpha = input.getOutlineAlpha();
        style.shadowAlpha = input.getShadowAlpha();
        style.flags = input.getFlags();
        style.decorationFlags = input.getDecorationFlags();
        style.underlinePattern = input.getUnderlinePattern();
        style.strikePattern = input.getStrikePattern();
        for (int i = 0; i < 4; ++i) {
            style.faceColor[i] = input.getFaceColor(i);
            style.outlineColor[i] = input.getOutlineColor(i);
            style.shadowColor[i] = input.getShadowColor(i);
        }
        style.effects = new FontRenderer.StyleEffect[input.getEffectsCount()];
        for (int i = 0; i < style.effects.length; ++i) {
            StyleEffect source = input.getEffects(i);
            FontRenderer.StyleEffect effect = new FontRenderer.StyleEffect();
            effect.type = source.getType().getNumber();
            effect.amplitude = source.getAmplitude();
            effect.hz = source.getHz();
            effect.wavelength = source.getWavelength();
            effect.fit = source.getFit();
            effect.gradientMode = source.getGradientMode();
            for (int j = 0; j < source.getColorsCount(); ++j) effect.colors[j] = source.getColors(j);
            style.effects[i] = effect;
        }
        return style;
    }
}
