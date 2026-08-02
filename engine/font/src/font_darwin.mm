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

#include <math.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include "font.h"

static bool GetWeightValue(FontWeight weight, CGFloat* value)
{
    switch (weight)
    {
        // CoreText represents weight as a normalized number in [-1, 1].
        case FONT_WEIGHT_NORMAL:
        case FONT_WEIGHT_REGULAR:     *value = 0.0; return true;
        case FONT_WEIGHT_THIN:        *value = -0.8; return true;
        case FONT_WEIGHT_EXTRALIGHT:
        case FONT_WEIGHT_ULTRALIGHT:  *value = -0.6; return true;
        case FONT_WEIGHT_LIGHT:       *value = -0.4; return true;
        case FONT_WEIGHT_MEDIUM:      *value = 0.23; return true;
        case FONT_WEIGHT_DEMIBOLD:
        case FONT_WEIGHT_SEMIBOLD:    *value = 0.3; return true;
        case FONT_WEIGHT_BOLD:        *value = 0.4; return true;
        case FONT_WEIGHT_EXTRABOLD:
        case FONT_WEIGHT_ULTRABOLD:   *value = 0.5; return true;
        case FONT_WEIGHT_BLACK:       *value = 0.62; return true;
        case FONT_WEIGHT_HEAVY:       *value = 0.56; return true;
        case FONT_WEIGHT_EXTRABLACK:
        case FONT_WEIGHT_ULTRABLACK:  *value = 0.7; return true;
        default:                      return false;
    }
}

static bool GetFontWeight(CGFloat value, FontWeight* weight)
{
    struct WeightMapping
    {
        FontWeight m_Weight;
        CGFloat    m_Value;
    };
    static const WeightMapping mappings[] = {
        { FONT_WEIGHT_NORMAL,     0.0 },
        { FONT_WEIGHT_THIN,      -0.8 },
        { FONT_WEIGHT_EXTRALIGHT,-0.6 },
        { FONT_WEIGHT_LIGHT,     -0.4 },
        { FONT_WEIGHT_MEDIUM,     0.23 },
        { FONT_WEIGHT_DEMIBOLD,   0.3 },
        { FONT_WEIGHT_BOLD,       0.4 },
        { FONT_WEIGHT_EXTRABOLD,  0.5 },
        { FONT_WEIGHT_HEAVY,      0.56 },
        { FONT_WEIGHT_BLACK,      0.62 },
        { FONT_WEIGHT_EXTRABLACK, 0.7 },
    };

    CGFloat best_difference = 2.0;
    FontWeight best_weight = FONT_WEIGHT_NORMAL;
    for (uint32_t i = 0; i < sizeof(mappings) / sizeof(mappings[0]); ++i)
    {
        CGFloat difference = fabs(value - mappings[i].m_Value);
        if (difference < best_difference)
        {
            best_difference = difference;
            best_weight = mappings[i].m_Weight;
        }
    }
    if (best_difference >= 0.05)
        return false;
    *weight = best_weight;
    return true;
}

static bool GetFilePath(CFURLRef url, char* path, uint32_t path_size)
{
    if (!url || !path || path_size == 0)
        return false;
    CFStringRef file_path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (!file_path)
        return false;
    bool result = CFStringGetCString(file_path, path, path_size, kCFStringEncodingUTF8);
    CFRelease(file_path);
    return result;
}

static bool IsReadableTrueTypeFile(const char* path)
{
    const char* extension = strrchr(path, '.');
    return extension && strcasecmp(extension, ".ttf") == 0 && access(path, R_OK) == 0;
}

FontResult FontFindSystemFont(const char* family, FontWeight weight, FontStyle style, char* path, uint32_t path_size)
{
    CGFloat weight_value;
    if (!family || !family[0] || !GetWeightValue(weight, &weight_value) ||
        (style != FONT_STYLE_NORMAL && style != FONT_STYLE_ITALIC) || !path || path_size == 0)
        return FONT_RESULT_ERROR;

    CFStringRef family_name = CFStringCreateWithCString(kCFAllocatorDefault, family, kCFStringEncodingUTF8);
    CFNumberRef weight_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &weight_value);
    CTFontSymbolicTraits symbolic_traits = style == FONT_STYLE_ITALIC ? kCTFontItalicTrait : 0;
    CFNumberRef symbolic_traits_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &symbolic_traits);
    if (!family_name || !weight_number || !symbolic_traits_number)
    {
        if (symbolic_traits_number)
            CFRelease(symbolic_traits_number);
        if (weight_number)
            CFRelease(weight_number);
        if (family_name)
            CFRelease(family_name);
        return FONT_RESULT_ERROR;
    }

    const void* trait_keys[] = { kCTFontWeightTrait, kCTFontSymbolicTrait };
    const void* trait_values[] = { weight_number, symbolic_traits_number };
    CFDictionaryRef traits = CFDictionaryCreate(kCFAllocatorDefault, trait_keys, trait_values, 2,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void* attribute_keys[] = { kCTFontFamilyNameAttribute, kCTFontTraitsAttribute };
    const void* attribute_values[] = { family_name, traits };
    CFDictionaryRef attributes = CFDictionaryCreate(kCFAllocatorDefault, attribute_keys, attribute_values, 2,
                                                     &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef query = CTFontDescriptorCreateWithAttributes(attributes);
    const void* mandatory_keys[] = { kCTFontFamilyNameAttribute };
    CFSetRef mandatory = CFSetCreate(kCFAllocatorDefault, mandatory_keys, 1, &kCFTypeSetCallBacks);
    CTFontDescriptorRef descriptor = CTFontDescriptorCreateMatchingFontDescriptor(query, mandatory);
    FontResult result = FONT_RESULT_ERROR;

    if (descriptor)
    {
        CFStringRef candidate_family = (CFStringRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute);
        CFDictionaryRef candidate_traits = (CFDictionaryRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontTraitsAttribute);
        CFNumberRef candidate_weight_number = candidate_traits ? (CFNumberRef)CFDictionaryGetValue(candidate_traits, kCTFontWeightTrait) : 0;
        CFNumberRef candidate_symbolic_number = candidate_traits ? (CFNumberRef)CFDictionaryGetValue(candidate_traits, kCTFontSymbolicTrait) : 0;
        CGFloat candidate_weight = 0.0;
        CTFontSymbolicTraits candidate_symbolic_traits = 0;
        bool has_candidate_weight = candidate_weight_number && CFNumberGetValue(candidate_weight_number, kCFNumberCGFloatType, &candidate_weight);
        if (candidate_symbolic_number)
            CFNumberGetValue(candidate_symbolic_number, kCFNumberSInt32Type, &candidate_symbolic_traits);
        bool family_matches = candidate_family && CFStringCompare(candidate_family, family_name, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
        bool weight_matches = has_candidate_weight && fabs(candidate_weight - weight_value) < 0.05;
        bool candidate_italic = (candidate_symbolic_traits & kCTFontItalicTrait) != 0;
        bool style_matches = candidate_italic == (style == FONT_STYLE_ITALIC);
        if (candidate_traits)
            CFRelease(candidate_traits);
        if (candidate_family)
            CFRelease(candidate_family);

        if (family_matches && weight_matches && style_matches)
        {
            CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute);
            char candidate_path[2048];
            bool converted = GetFilePath(url, candidate_path, sizeof(candidate_path));
            if (url)
                CFRelease(url);
            if (converted)
            {
                const char* ext = strrchr(candidate_path, '.');
                if (!ext || strcasecmp(ext, ".ttf") != 0)
                    result = FONT_RESULT_NOT_SUPPORTED;
                else if (access(candidate_path, R_OK) == 0 && strlen(candidate_path) + 1 <= path_size)
                {
                    strcpy(path, candidate_path);
                    result = FONT_RESULT_OK;
                }
            }
        }
    }

    if (descriptor)
        CFRelease(descriptor);
    CFRelease(mandatory);
    CFRelease(query);
    CFRelease(attributes);
    CFRelease(traits);
    CFRelease(symbolic_traits_number);
    CFRelease(weight_number);
    CFRelease(family_name);
    return result;
}

FontResult FontIterateSystemFonts(FontSystemFontCallback callback, void* context)
{
    if (!callback)
        return FONT_RESULT_ERROR;

    CFArrayRef urls = CTFontManagerCopyAvailableFontURLs();
    if (!urls)
        return FONT_RESULT_ERROR;

    bool keep_iterating = true;
    CFIndex url_count = CFArrayGetCount(urls);
    for (CFIndex url_index = 0; url_index < url_count && keep_iterating; ++url_index)
    {
        CFURLRef url = (CFURLRef)CFArrayGetValueAtIndex(urls, url_index);
        char path[2048];
        if (!GetFilePath(url, path, sizeof(path)) || !IsReadableTrueTypeFile(path))
            continue;

        CFArrayRef descriptors = CTFontManagerCreateFontDescriptorsFromURL(url);
        if (!descriptors)
            continue;

        CFIndex descriptor_count = CFArrayGetCount(descriptors);
        for (CFIndex descriptor_index = 0; descriptor_index < descriptor_count && keep_iterating; ++descriptor_index)
        {
            CTFontDescriptorRef descriptor = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descriptors, descriptor_index);
            CFStringRef family_name = (CFStringRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute);
            CFDictionaryRef traits = (CFDictionaryRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontTraitsAttribute);
            CFNumberRef weight_number = traits ? (CFNumberRef)CFDictionaryGetValue(traits, kCTFontWeightTrait) : 0;
            CFNumberRef symbolic_number = traits ? (CFNumberRef)CFDictionaryGetValue(traits, kCTFontSymbolicTrait) : 0;
            CGFloat weight_value = 0.0;
            CTFontSymbolicTraits symbolic_traits = 0;
            FontWeight weight;
            char family[512];

            bool has_weight = weight_number && CFNumberGetValue(weight_number, kCFNumberCGFloatType, &weight_value);
            if (symbolic_number)
                CFNumberGetValue(symbolic_number, kCFNumberSInt32Type, &symbolic_traits);
            bool has_family = family_name && CFStringGetCString(family_name, family, sizeof(family), kCFStringEncodingUTF8);
            FontStyle style = (symbolic_traits & kCTFontItalicTrait) != 0 ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL;

            char resolved_path[2048];
            bool usable = has_family && has_weight && GetFontWeight(weight_value, &weight) &&
                          FontFindSystemFont(family, weight, style, resolved_path, sizeof(resolved_path)) == FONT_RESULT_OK &&
                          strcmp(path, resolved_path) == 0;
            if (usable)
            {
                FontSystemFont font = { family, style, weight, path };
                keep_iterating = callback(&font, context);
            }

            if (traits)
                CFRelease(traits);
            if (family_name)
                CFRelease(family_name);
        }
        CFRelease(descriptors);
    }
    CFRelease(urls);
    return FONT_RESULT_OK;
}
