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

package com.dynamo.bob.pipeline;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Shared utility for converting SDL gamecontrollerdb.txt format to Defold's .gamepads TextFormat.
 */
public final class GamepadConverter {

    private static final String SDL_TYPE_AXIS = "a";
    private static final String SDL_TYPE_BUTTON = "b";
    private static final String SDL_TYPE_HAT = "h";

    public static final String TYPE_AXIS_DDF = "GAMEPAD_TYPE_AXIS";
    public static final String TYPE_BUTTON_DDF = "GAMEPAD_TYPE_BUTTON";
    public static final String TYPE_HAT_DDF = "GAMEPAD_TYPE_HAT";

    // SDL logical button names to Defold input names.
    public static final Map<String, String> SDL_BUTTON_TO_INPUT = new HashMap<>();
    static {
        SDL_BUTTON_TO_INPUT.put("a", "GAMEPAD_RPAD_DOWN");
        SDL_BUTTON_TO_INPUT.put("b", "GAMEPAD_RPAD_RIGHT");
        SDL_BUTTON_TO_INPUT.put("x", "GAMEPAD_RPAD_LEFT");
        SDL_BUTTON_TO_INPUT.put("y", "GAMEPAD_RPAD_UP");
        SDL_BUTTON_TO_INPUT.put("back", "GAMEPAD_BACK");
        SDL_BUTTON_TO_INPUT.put("start", "GAMEPAD_START");
        SDL_BUTTON_TO_INPUT.put("guide", "GAMEPAD_GUIDE");
        SDL_BUTTON_TO_INPUT.put("leftshoulder", "GAMEPAD_LSHOULDER");
        SDL_BUTTON_TO_INPUT.put("rightshoulder", "GAMEPAD_RSHOULDER");
        SDL_BUTTON_TO_INPUT.put("leftstick", "GAMEPAD_LSTICK_CLICK");
        SDL_BUTTON_TO_INPUT.put("rightstick", "GAMEPAD_RSTICK_CLICK");
    }

    // SDL logical axis names to Defold negative/positive input names.
    public static final Map<String, String> SDL_AXIS_TO_INPUT = new HashMap<>();
    static {
        SDL_AXIS_TO_INPUT.put("-leftx", "GAMEPAD_LSTICK_LEFT");
        SDL_AXIS_TO_INPUT.put("+leftx", "GAMEPAD_LSTICK_RIGHT");
        SDL_AXIS_TO_INPUT.put("-lefty", "GAMEPAD_LSTICK_UP");
        SDL_AXIS_TO_INPUT.put("+lefty", "GAMEPAD_LSTICK_DOWN");
        SDL_AXIS_TO_INPUT.put("-rightx", "GAMEPAD_RSTICK_LEFT");
        SDL_AXIS_TO_INPUT.put("+rightx", "GAMEPAD_RSTICK_RIGHT");
        SDL_AXIS_TO_INPUT.put("-righty", "GAMEPAD_RSTICK_UP");
        SDL_AXIS_TO_INPUT.put("+righty", "GAMEPAD_RSTICK_DOWN");
        SDL_AXIS_TO_INPUT.put("lefttrigger", "GAMEPAD_LTRIGGER");
        SDL_AXIS_TO_INPUT.put("righttrigger", "GAMEPAD_RTRIGGER");
    }

    // SDL logical dpad names and physical hat bindings to Defold input names.
    public static final Map<String, String> SDL_HAT_TO_INPUT = new HashMap<>();
    static {
        SDL_HAT_TO_INPUT.put("dpup", "GAMEPAD_LPAD_UP");
        SDL_HAT_TO_INPUT.put("dpright", "GAMEPAD_LPAD_RIGHT");
        SDL_HAT_TO_INPUT.put("dpdown", "GAMEPAD_LPAD_DOWN");
        SDL_HAT_TO_INPUT.put("dpleft", "GAMEPAD_LPAD_LEFT");
        SDL_HAT_TO_INPUT.put("h0.1", "GAMEPAD_LPAD_UP");
        SDL_HAT_TO_INPUT.put("h0.2", "GAMEPAD_LPAD_RIGHT");
        SDL_HAT_TO_INPUT.put("h0.4", "GAMEPAD_LPAD_DOWN");
        SDL_HAT_TO_INPUT.put("h0.8", "GAMEPAD_LPAD_LEFT");
        SDL_HAT_TO_INPUT.put("h1.1", "GAMEPAD_RPAD_UP");
        SDL_HAT_TO_INPUT.put("h1.2", "GAMEPAD_RPAD_RIGHT");
        SDL_HAT_TO_INPUT.put("h1.4", "GAMEPAD_RPAD_DOWN");
        SDL_HAT_TO_INPUT.put("h1.8", "GAMEPAD_RPAD_LEFT");
    }

    private static final String HEX_GUID_PATTERN = "^[0-9a-fA-F]{32}";

    private GamepadConverter() {}

    private static final class SdlMapping {
        final String guid;
        final String name;
        final String mappings;
        final String platform;

        SdlMapping(String guid, String name, String mappings, String platform) {
            this.guid = guid;
            this.name = name;
            this.mappings = mappings;
            this.platform = platform;
        }
    }

    private static final class GamepadGuidFields {
        final long busCrc;
        final long vendorProduct;
        final long versionDriverSignatureDriverData;

        GamepadGuidFields(long busCrc, long vendorProduct, long versionDriverSignatureDriverData) {
            this.busCrc = busCrc;
            this.vendorProduct = vendorProduct;
            this.versionDriverSignatureDriverData = versionDriverSignatureDriverData;
        }
    }

    /**
     * Get the SDL axis type identifier.
     */
    public static String getSDL_TYPE_AXIS() { return SDL_TYPE_AXIS; }

    /**
     * Get the SDL button type identifier.
     */
    public static String getSDL_TYPE_BUTTON() { return SDL_TYPE_BUTTON; }

    /**
     * Get the SDL hat type identifier.
     */
    public static String getSDL_TYPE_HAT() { return SDL_TYPE_HAT; }

    /**
     * Detect whether the given text content is in SDL gamecontrollerdb.txt format.
     */
    public static boolean isSdlFormat(String content) {
        if (content == null || content.isEmpty()) {
            return false;
        }

        try (BufferedReader reader = new BufferedReader(new StringReader(content))) {
            String line;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }
                if (line.matches(HEX_GUID_PATTERN + ".*")) {
                    return true;
                }
            }
        } catch (IOException e) {
            return false;
        }

        return content.contains("platform:");
    }

    /**
     * Convert SDL gamecontrollerdb.txt content to Defold's .gamepads TextFormat.
     */
    public static String convertToDefoldFormat(String sdlContent, String platform) {
        String defoldPlatform = normalizePlatform(platform);
        List<SdlMapping> mappings = parseSdlMappings(sdlContent, defoldPlatform);

        StringBuilder sb = new StringBuilder();
        for (SdlMapping mapping : mappings) {
            sb.append("driver\n");
            sb.append("{\n");
            sb.append("    device: \"").append(escapeDefoldString(mapping.name)).append("\"\n");
            sb.append("    platform: \"").append(escapeDefoldString(defoldPlatform)).append("\"\n");
            GamepadGuidFields guid = parseGuidFields(mapping.guid);
            if (guid != null) {
                sb.append("    guid { bus_crc: ").append(guid.busCrc)
                        .append(" vendor_product: ").append(guid.vendorProduct)
                        .append(" version_driversignature_driverdata: ").append(guid.versionDriverSignatureDriverData)
                        .append(" }\n");
            }

            List<String> mapEntries = convertSdlMappingToDefoldMap(mapping.mappings);
            for (String mapEntry : mapEntries) {
                sb.append("    ").append(mapEntry).append("\n");
            }

            sb.append("}\n\n");
        }

        return sb.toString();
    }

    private static List<SdlMapping> parseSdlMappings(String content, String platform) {
        List<SdlMapping> result = new ArrayList<>();

        try (BufferedReader reader = new BufferedReader(new StringReader(content))) {
            String line;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }

                SdlMapping parsed = parseSdlLine(line);
                if (parsed != null && platformMatches(parsed.platform, platform)) {
                    result.add(parsed);
                }
            }
        } catch (IOException e) {
            // silently skip parse errors for individual lines
        }

        return result;
    }

    private static SdlMapping parseSdlLine(String line) {
        int firstComma = line.indexOf(',');
        if (firstComma == -1) {
            return null;
        }

        String guid = line.substring(0, firstComma).trim();
        if (!isValidGuid(guid)) {
            return null;
        }

        String rest = line.substring(firstComma + 1);

        int secondComma = rest.indexOf(',');
        if (secondComma == -1) {
            return null;
        }

        String name = rest.substring(0, secondComma).trim();
        String mappings = rest.substring(secondComma + 1);

        return new SdlMapping(guid, name, mappings, extractPlatform(mappings));
    }

    private static String extractPlatform(String mappings) {
        String[] parts = mappings.split(",");
        for (String part : parts) {
            part = part.trim();
            if (part.startsWith("platform:")) {
                return part.substring("platform:".length()).trim().replaceAll(",$", "");
            }
        }
        return "Windows"; // Default per SDL spec
    }

    static boolean platformMatches(String mappingPlatform, String targetPlatform) {
        if (targetPlatform == null) {
            return false;
        }

        String normalizedTarget = normalizePlatform(targetPlatform);
        String normalizedMapping = normalizePlatform(mappingPlatform);

        return normalizedTarget.equals(normalizedMapping);
    }

    static String normalizePlatform(String platform) {
        if (platform == null) {
            return "";
        }

        String normalized = platform.toLowerCase().trim();
        switch (normalized) {
            case "windows":
            case "win32":
                return "windows";
            case "mac os x":
            case "osx":
            case "macos":
                return "macos";
            case "linux":
                return "linux";
            case "ios":
                return "ios";
            case "android":
                return "android";
            case "web":
                return "web";
            case "switch":
                return "switch";
            case "playstation":
                return "playstation";
            default:
                if (normalized.endsWith("-win32")) {
                    return "windows";
                }
                if (normalized.endsWith("-macos")) {
                    return "macos";
                }
                if (normalized.endsWith("-linux")) {
                    return "linux";
                }
                if (normalized.endsWith("-ios")) {
                    return "ios";
                }
                if (normalized.endsWith("-android")) {
                    return "android";
                }
                if (normalized.endsWith("-web")) {
                    return "web";
                }
                if (normalized.endsWith("-nx64")) {
                    return "switch";
                }
                if (normalized.endsWith("-ps4") || normalized.endsWith("-ps5")) {
                    return "playstation";
                }
                return normalized;
        }
    }

    private static boolean isValidGuid(String guid) {
        return guid != null && guid.matches("[0-9a-fA-F]{32}");
    }

    private static GamepadGuidFields parseGuidFields(String guid) {
        if (!isValidGuid(guid)) {
            return null;
        }

        byte[] bytes = new byte[16];
        for (int i = 0; i < bytes.length; ++i) {
            int hi = Character.digit(guid.charAt(i * 2), 16);
            int lo = Character.digit(guid.charAt(i * 2 + 1), 16);
            if (hi < 0 || lo < 0) {
                return null;
            }
            bytes[i] = (byte)((hi << 4) | lo);
        }

        long bus = readUint16LE(bytes, 0);
        long crc = readUint16LE(bytes, 2);
        long vendor = readUint16LE(bytes, 4);
        long product = readUint16LE(bytes, 8);
        long version = readUint16LE(bytes, 12);
        long driverSignature = bytes[14] & 0xffL;
        long driverData = bytes[15] & 0xffL;

        return new GamepadGuidFields(
                bus | (crc << 16),
                vendor | (product << 16),
                version | (driverSignature << 16) | (driverData << 24));
    }

    private static long readUint16LE(byte[] bytes, int offset) {
        return (bytes[offset] & 0xffL) | ((bytes[offset + 1] & 0xffL) << 8);
    }

    private static List<String> convertSdlMappingToDefoldMap(String sdlMappings) {
        List<String> result = new ArrayList<>();

        String[] parts = sdlMappings.split(",");
        for (String part : parts) {
            part = part.trim();
            if (part.isEmpty()) {
                continue;
            }

            if (part.startsWith("platform:")) {
                continue;
            }

            result.addAll(convertSdlMappingEntries(part));
        }

        return result;
    }

    private static List<String> convertSdlMappingEntries(String sdlEntry) {
        List<String> result = new ArrayList<>();

        int colonIdx = sdlEntry.indexOf(':');
        if (colonIdx == -1) {
            return result;
        }

        String logical = sdlEntry.substring(0, colonIdx).trim().toLowerCase();
        String binding = sdlEntry.substring(colonIdx + 1).trim();
        if (logical.isEmpty() || binding.isEmpty()) {
            return result;
        }

        Binding parsedBinding = parseBinding(binding);
        if (parsedBinding == null) {
            return result;
        }

        if (isBidirectionalAxis(logical)) {
            if (hasExplicitAxisDirection(binding, parsedBinding)) {
                result.add(formatMapEntry(SDL_AXIS_TO_INPUT.get((parsedBinding.negative ? "-" : "+") + logical), parsedBinding, parsedBinding.negative, false));
                return result;
            }

            result.add(formatMapEntry(SDL_AXIS_TO_INPUT.get("-" + logical), parsedBinding, !parsedBinding.negative, false));
            result.add(formatMapEntry(SDL_AXIS_TO_INPUT.get("+" + logical), parsedBinding, parsedBinding.negative, false));
            return result;
        }

        String input = getDefoldInput(logical);
        if (input == null) {
            return result;
        }

        result.add(formatMapEntry(input, parsedBinding, parsedBinding.negative, isTrigger(logical)));
        return result;
    }

    /**
     * Get Defold input name from an SDL logical control name.
     */
    public static String getDefoldInput(String sdlEntry) {
        String lookupKey = sdlEntry.trim().toLowerCase();
        if (lookupKey.startsWith("+") || lookupKey.startsWith("-")) {
            String axisInput = SDL_AXIS_TO_INPUT.get(lookupKey);
            if (axisInput != null) {
                return axisInput;
            }
            lookupKey = lookupKey.substring(1);
        }

        String buttonInput = SDL_BUTTON_TO_INPUT.get(lookupKey);
        if (buttonInput != null) {
            return buttonInput;
        }

        String axisInput = SDL_AXIS_TO_INPUT.get(lookupKey);
        if (axisInput != null) {
            return axisInput;
        }

        String hatInput = SDL_HAT_TO_INPUT.get(lookupKey);
        if (hatInput != null) {
            return hatInput;
        }

        return null;
    }

    private static boolean isBidirectionalAxis(String logical) {
        return logical.equals("leftx") || logical.equals("lefty") || logical.equals("rightx") || logical.equals("righty");
    }

    private static boolean isTrigger(String logical) {
        return logical.equals("lefttrigger") || logical.equals("righttrigger");
    }

    private static boolean hasExplicitAxisDirection(String binding, Binding parsedBinding) {
        if (!parsedBinding.type.equals(SDL_TYPE_AXIS)) {
            return false;
        }

        String trimmed = binding.trim();
        if (trimmed.endsWith("~")) {
            trimmed = trimmed.substring(0, trimmed.length() - 1);
        }
        return trimmed.startsWith("+") || trimmed.startsWith("-");
    }

    private static Binding parseBinding(String binding) {
        boolean negative = false;
        boolean invert = false;

        if (binding.endsWith("~")) {
            invert = true;
            binding = binding.substring(0, binding.length() - 1);
        }

        if (binding.startsWith("+") || binding.startsWith("-")) {
            negative = binding.startsWith("-");
            binding = binding.substring(1);
        }

        if (binding.startsWith(SDL_TYPE_AXIS)) {
            return new Binding(SDL_TYPE_AXIS, binding.substring(SDL_TYPE_AXIS.length()), negative ^ invert, false);
        }
        if (binding.startsWith(SDL_TYPE_BUTTON)) {
            return new Binding(SDL_TYPE_BUTTON, binding.substring(SDL_TYPE_BUTTON.length()), false, false);
        }
        if (binding.startsWith(SDL_TYPE_HAT)) {
            String[] hatParts = binding.substring(SDL_TYPE_HAT.length()).split("\\.");
            if (hatParts.length != 2) {
                return null;
            }
            return new Binding(SDL_TYPE_HAT, hatParts[0], false, true, hatParts[1]);
        }

        return null;
    }

    private static String formatMapEntry(String input, Binding binding, boolean negate, boolean trigger) {
        StringBuilder sb = new StringBuilder();
        sb.append("map { input: ").append(input);
        sb.append(" type: ").append(binding.getDefoldType());
        sb.append(" index: ").append(binding.index);

        if (negate) {
            sb.append(" mod { mod: GAMEPAD_MODIFIER_NEGATE }");
        }

        if (binding.type.equals(SDL_TYPE_AXIS)) {
            if (binding.signedDirection || !trigger) {
                sb.append(" mod { mod: GAMEPAD_MODIFIER_CLAMP }");
            } else {
                sb.append(" mod { mod: GAMEPAD_MODIFIER_SCALE }");
            }
        }

        if (binding.type.equals(SDL_TYPE_HAT)) {
            sb.append(" hat_mask: ").append(binding.hatMask);
        }

        sb.append(" }");
        return sb.toString();
    }

    private static class Binding {
        public final String type;
        public final String index;
        public final boolean negative;
        public final boolean signedDirection;
        public final String hatMask;

        public Binding(String type, String index, boolean negative, boolean signedDirection) {
            this(type, index, negative, signedDirection, null);
        }

        public Binding(String type, String index, boolean negative, boolean signedDirection, String hatMask) {
            this.type = type;
            this.index = index;
            this.negative = negative;
            this.signedDirection = signedDirection;
            this.hatMask = hatMask;
        }

        public String getDefoldType() {
            if (type.equals(SDL_TYPE_AXIS)) {
                return TYPE_AXIS_DDF;
            }
            if (type.equals(SDL_TYPE_BUTTON)) {
                return TYPE_BUTTON_DDF;
            }
            return TYPE_HAT_DDF;
        }
    }

    private static String escapeDefoldString(String s) {
        return s.replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\n", "\\n")
                .replace("\r", "\\r")
                .replace("\t", "\\t");
    }
}
