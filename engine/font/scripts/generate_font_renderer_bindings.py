#!/usr/bin/env python3
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "dlib" / "scripts"))
from generate_java_bindings import generate_bindings

PACKAGE = "com.dynamo.bob.font.generated"
HEADER_CLASS = "FontRendererFFM"
SYMBOLS_CLASS = "FontRendererSymbols"
FUNCTIONS = (
    "FontcCompileStyle",
    "FontcFreeStyle",
    "FontcSetStyle",
    "FontcCreate",
    "FontcCreateGlyphBank",
    "FontcDestroy",
    "FontcParseMarkup",
    "FontcDestroyMarkup",
    "FontcGetMarkupData",
    "FontcMeasure",
    "FontcMeasureMarkup",
    "FontcMeasureParsedMarkup",
    "FontcFilterMarkup",
    "FontcGenerateGlyph",
    "FontcFreeGlyph",
    "FontcGetGlyphMetrics",
    "FontcGetSupportedGlyphMetrics",
    "FontcDecodeImage",
    "FontcFreeImage",
    "FontcSetProperties",
    "FontcSetText",
    "FontcSetMarkup",
    "FontcHash",
    "FontcBeginBatch",
    "FontcGenerateTexture",
    "FontcGetEffectTexture",
    "FontcGetVectorTextures",
    "FontcFreeTexture",
    "FontcGetVertexBufferSize",
    "FontcGetVertices",
)
STRUCTS = (
    "FontcStyle",
    "FontcStyleEffect",
    "FontcStyleData",
    "FontcParams",
    "FontcGlyphBankGlyph",
    "FontcLayout",
    "FontcGlyph",
    "FontcGlyphMetrics",
    "FontcImage",
    "FontcMarkupString",
    "FontcMarkupAttribute",
    "FontcMarkupNode",
    "FontcMarkupSpan",
    "FontcMarkupError",
    "FontcMarkupData",
    "FontcProperties",
    "FontcTexture",
)
TYPEDEFS = (
    "HFontRenderer",
    "HFontcMarkup",
    "FontRendererResult",
    "FontRendererLayer",
    "FontcMarkupErrorType",
)
CONSTANTS = (
    "FONT_RENDERER_RESULT_OK",
    "FONT_RENDERER_RESULT_INVALID_ARGUMENT",
    "FONT_RENDERER_RESULT_FONT_ERROR",
    "FONT_RENDERER_RESULT_TEXT_ERROR",
    "FONT_RENDERER_RESULT_GLYPH_ERROR",
    "FONT_RENDERER_RESULT_OUT_OF_MEMORY",
    "FONT_RENDERER_LAYER_FACE",
    "FONT_RENDERER_LAYER_OUTLINE",
    "FONT_RENDERER_LAYER_SHADOW",
    "FONTC_MARKUP_ERROR_NONE",
    "FONTC_MARKUP_ERROR_INCOMPLETE_TAG",
    "FONTC_MARKUP_ERROR_INCOMPLETE_ENTITY",
    "FONTC_MARKUP_ERROR_UNCLOSED_TAG",
    "FONTC_MARKUP_ERROR_INVALID_TAG",
    "FONTC_MARKUP_ERROR_INVALID_ATTRIBUTE",
    "FONTC_MARKUP_ERROR_INVALID_ENTITY",
    "FONTC_MARKUP_ERROR_UNEXPECTED_CLOSING_TAG",
    "FONTC_MARKUP_ERROR_MISMATCHED_CLOSING_TAG",
    "FONTC_MARKUP_ERROR_INVALID_UTF8",
    "FONTC_MARKUP_ERROR_LIMIT_EXCEEDED",
    "FONTC_MARKUP_ERROR_UNSUPPORTED",
    "FONTC_MARKUP_ERROR_UNKNOWN_TAG",
    "FONTC_MARKUP_ERROR_UNKNOWN_ATTRIBUTE",
    "FONTC_MARKUP_ERROR_INVALID_ATTRIBUTE_VALUE",
    "FONTC_MARKUP_ERROR_INVALID_GRADIENT_RADIUS",
)


if __name__ == "__main__":
    generate_bindings(PACKAGE, HEADER_CLASS, SYMBOLS_CLASS, FUNCTIONS, STRUCTS, TYPEDEFS, CONSTANTS,
                      "Generate Java 25 FFM bindings for the font renderer")
