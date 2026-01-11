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

package com.defold.extension.pipeline.texture;
import java.util.HashMap;

/**
 * Interface for compressing a texture using settings
 * NOTE: Public API for ITextureCompressor
 * We will scan for public non-abstract classes that implement this interface inside the com.defold.extension.pipeline
 * package and instantiate them to transform Lua source code. Implementors must provide a no-argument constructor.
 */
public class TextureCompressorPreset {

    private String                  name;           // The strict name used for lookups (i.e. the "api")
    private String                  displayName;    // For nicer sorting and displaying in editor etc
    private String                  compressorName;
    private HashMap<String, Object> options = new HashMap<>();

    public TextureCompressorPreset(String name, String displayName, String compressorName) {
        this.name = name;
        this.displayName = displayName;
        this.compressorName = compressorName;
    }

    public String getName() {
        return name;
    }

    public String getDisplayName() {
        return displayName;
    }

    public String getCompressorName() {
        return compressorName;
    }

    public HashMap<String, Object> getOptions() {
        return options;
    }

    public String getOptionString(String name) {
        Object o = options.getOrDefault(name, null);
        if (o != null && o instanceof String)
            return (String)o;
        return null;
    }

    public Integer getOptionInt(String name) {
        Object o = options.getOrDefault(name, null);
        if (o != null && o instanceof Integer)
            return (Integer)o;
        return null;
    }

    public Float getOptionFloat(String name) {
        Object o = options.getOrDefault(name, null);
        if (o != null && o instanceof Float)
            return (Float)o;
        return null;
    }

    public void setOptionString(String name, String o) {
        options.put(name, o);
    }

    public void setOptionInt(String name, int o) {
        options.put(name, o);
    }

    public void setOptionFloat(String name, float o) {
        options.put(name, o);
    }

    public String toString() {
        String s = String.format("Preset:\n  name: '%s'\n  displayName: '%s',\n  compressorName: '%s',\n  options: {", getName(), getDisplayName(), getCompressorName());
        for (String key : options.keySet()) {
            Object value = options.get(key);
            s += String.format("    '%s' = '%s',\n", key, value);
        }
        s += "  }\n";
        return s;
    }
}
