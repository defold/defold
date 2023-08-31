// Generated from GLSLPreParser.g4 by ANTLR 4.9.1
package com.dynamo.bob.pipeline.antlr.glsl;
import org.antlr.v4.runtime.atn.*;
import org.antlr.v4.runtime.dfa.DFA;
import org.antlr.v4.runtime.*;
import org.antlr.v4.runtime.misc.*;
import org.antlr.v4.runtime.tree.*;
import java.util.List;
import java.util.Iterator;
import java.util.ArrayList;

@SuppressWarnings({"all", "warnings", "unchecked", "unused", "cast"})
public class GLSLPreParser extends Parser {
	static { RuntimeMetaData.checkVersion("4.9.1", RuntimeMetaData.VERSION); }

	protected static final DFA[] _decisionToDFA;
	protected static final PredictionContextCache _sharedContextCache =
		new PredictionContextCache();
	public static final int
		ATOMIC_UINT=1, ATTRIBUTE=2, BOOL=3, BREAK=4, BUFFER=5, BVEC2=6, BVEC3=7, 
		BVEC4=8, CASE=9, CENTROID=10, COHERENT=11, CONST=12, CONTINUE=13, DEFAULT=14, 
		DISCARD=15, DMAT2=16, DMAT2X2=17, DMAT2X3=18, DMAT2X4=19, DMAT3=20, DMAT3X2=21, 
		DMAT3X3=22, DMAT3X4=23, DMAT4=24, DMAT4X2=25, DMAT4X3=26, DMAT4X4=27, 
		DO=28, DOUBLE=29, DVEC2=30, DVEC3=31, DVEC4=32, ELSE=33, FALSE=34, FLAT=35, 
		FLOAT=36, FOR=37, HIGHP=38, IF=39, IIMAGE1D=40, IIMAGE1DARRAY=41, IIMAGE2D=42, 
		IIMAGE2DARRAY=43, IIMAGE2DMS=44, IIMAGE2DMSARRAY=45, IIMAGE2DRECT=46, 
		IIMAGE3D=47, IIMAGEBUFFER=48, IIMAGECUBE=49, IIMAGECUBEARRAY=50, IMAGE1D=51, 
		IMAGE1DARRAY=52, IMAGE2D=53, IMAGE2DARRAY=54, IMAGE2DMS=55, IMAGE2DMSARRAY=56, 
		IMAGE2DRECT=57, IMAGE3D=58, IMAGEBUFFER=59, IMAGECUBE=60, IMAGECUBEARRAY=61, 
		IN=62, INOUT=63, INT=64, INVARIANT=65, ISAMPLER1D=66, ISAMPLER1DARRAY=67, 
		ISAMPLER2D=68, ISAMPLER2DARRAY=69, ISAMPLER2DMS=70, ISAMPLER2DMSARRAY=71, 
		ISAMPLER2DRECT=72, ISAMPLER3D=73, ISAMPLERBUFFER=74, ISAMPLERCUBE=75, 
		ISAMPLERCUBEARRAY=76, ISUBPASSINPUT=77, ISUBPASSINPUTMS=78, ITEXTURE1D=79, 
		ITEXTURE1DARRAY=80, ITEXTURE2D=81, ITEXTURE2DARRAY=82, ITEXTURE2DMS=83, 
		ITEXTURE2DMSARRAY=84, ITEXTURE2DRECT=85, ITEXTURE3D=86, ITEXTUREBUFFER=87, 
		ITEXTURECUBE=88, ITEXTURECUBEARRAY=89, IVEC2=90, IVEC3=91, IVEC4=92, LAYOUT=93, 
		LOWP=94, MAT2=95, MAT2X2=96, MAT2X3=97, MAT2X4=98, MAT3=99, MAT3X2=100, 
		MAT3X3=101, MAT3X4=102, MAT4=103, MAT4X2=104, MAT4X3=105, MAT4X4=106, 
		MEDIUMP=107, NOPERSPECTIVE=108, OUT=109, PATCH=110, PRECISE=111, PRECISION=112, 
		READONLY=113, RESTRICT=114, RETURN=115, SAMPLE=116, SAMPLER=117, SAMPLER1D=118, 
		SAMPLER1DARRAY=119, SAMPLER1DARRAYSHADOW=120, SAMPLER1DSHADOW=121, SAMPLER2D=122, 
		SAMPLER2DARRAY=123, SAMPLER2DARRAYSHADOW=124, SAMPLER2DMS=125, SAMPLER2DMSARRAY=126, 
		SAMPLER2DRECT=127, SAMPLER2DRECTSHADOW=128, SAMPLER2DSHADOW=129, SAMPLER3D=130, 
		SAMPLERBUFFER=131, SAMPLERCUBE=132, SAMPLERCUBEARRAY=133, SAMPLERCUBEARRAYSHADOW=134, 
		SAMPLERCUBESHADOW=135, SAMPLERSHADOW=136, SHARED=137, SMOOTH=138, STRUCT=139, 
		SUBPASSINPUT=140, SUBPASSINPUTMS=141, SUBROUTINE=142, SWITCH=143, TEXTURE1D=144, 
		TEXTURE1DARRAY=145, TEXTURE2D=146, TEXTURE2DARRAY=147, TEXTURE2DMS=148, 
		TEXTURE2DMSARRAY=149, TEXTURE2DRECT=150, TEXTURE3D=151, TEXTUREBUFFER=152, 
		TEXTURECUBE=153, TEXTURECUBEARRAY=154, TRUE=155, UIMAGE1D=156, UIMAGE1DARRAY=157, 
		UIMAGE2D=158, UIMAGE2DARRAY=159, UIMAGE2DMS=160, UIMAGE2DMSARRAY=161, 
		UIMAGE2DRECT=162, UIMAGE3D=163, UIMAGEBUFFER=164, UIMAGECUBE=165, UIMAGECUBEARRAY=166, 
		UINT=167, UNIFORM=168, USAMPLER1D=169, USAMPLER1DARRAY=170, USAMPLER2D=171, 
		USAMPLER2DARRAY=172, USAMPLER2DMS=173, USAMPLER2DMSARRAY=174, USAMPLER2DRECT=175, 
		USAMPLER3D=176, USAMPLERBUFFER=177, USAMPLERCUBE=178, USAMPLERCUBEARRAY=179, 
		USUBPASSINPUT=180, USUBPASSINPUTMS=181, UTEXTURE1D=182, UTEXTURE1DARRAY=183, 
		UTEXTURE2D=184, UTEXTURE2DARRAY=185, UTEXTURE2DMS=186, UTEXTURE2DMSARRAY=187, 
		UTEXTURE2DRECT=188, UTEXTURE3D=189, UTEXTUREBUFFER=190, UTEXTURECUBE=191, 
		UTEXTURECUBEARRAY=192, UVEC2=193, UVEC3=194, UVEC4=195, VARYING=196, VEC2=197, 
		VEC3=198, VEC4=199, VOID=200, VOLATILE=201, WHILE=202, WRITEONLY=203, 
		ADD_ASSIGN=204, AMPERSAND=205, AND_ASSIGN=206, AND_OP=207, BANG=208, CARET=209, 
		COLON=210, COMMA=211, DASH=212, DEC_OP=213, DIV_ASSIGN=214, DOT=215, EQ_OP=216, 
		EQUAL=217, GE_OP=218, INC_OP=219, LE_OP=220, LEFT_ANGLE=221, LEFT_ASSIGN=222, 
		LEFT_BRACE=223, LEFT_BRACKET=224, LEFT_OP=225, LEFT_PAREN=226, MOD_ASSIGN=227, 
		MUL_ASSIGN=228, NE_OP=229, NUMBER_SIGN=230, OR_ASSIGN=231, OR_OP=232, 
		PERCENT=233, PLUS=234, QUESTION=235, RIGHT_ANGLE=236, RIGHT_ASSIGN=237, 
		RIGHT_BRACE=238, RIGHT_BRACKET=239, RIGHT_OP=240, RIGHT_PAREN=241, SEMICOLON=242, 
		SLASH=243, STAR=244, SUB_ASSIGN=245, TILDE=246, VERTICAL_BAR=247, XOR_ASSIGN=248, 
		XOR_OP=249, DOUBLECONSTANT=250, FLOATCONSTANT=251, INTCONSTANT=252, UINTCONSTANT=253, 
		BLOCK_COMMENT=254, LINE_COMMENT=255, LINE_CONTINUATION=256, IDENTIFIER=257, 
		WHITE_SPACE=258, DEFINE_DIRECTIVE=259, ELIF_DIRECTIVE=260, ELSE_DIRECTIVE=261, 
		ENDIF_DIRECTIVE=262, ERROR_DIRECTIVE=263, EXTENSION_DIRECTIVE=264, IF_DIRECTIVE=265, 
		IFDEF_DIRECTIVE=266, IFNDEF_DIRECTIVE=267, LINE_DIRECTIVE=268, PRAGMA_DIRECTIVE=269, 
		UNDEF_DIRECTIVE=270, VERSION_DIRECTIVE=271, INCLUDE_DIRECTIVE=272, SPACE_TAB_0=273, 
		NEWLINE_0=274, MACRO_NAME=275, NEWLINE_1=276, SPACE_TAB_1=277, CONSTANT_EXPRESSION=278, 
		NEWLINE_2=279, ERROR_MESSAGE=280, NEWLINE_3=281, BEHAVIOR=282, EXTENSION_NAME=283, 
		NEWLINE_4=284, SPACE_TAB_2=285, NEWLINE_5=286, MACRO_IDENTIFIER=287, NEWLINE_6=288, 
		SPACE_TAB_3=289, LINE_EXPRESSION=290, NEWLINE_7=291, MACRO_ESC_NEWLINE=292, 
		MACRO_TEXT=293, NEWLINE_8=294, DEBUG=295, NEWLINE_9=296, OFF=297, ON=298, 
		OPTIMIZE=299, SPACE_TAB_5=300, STDGL=301, PROGRAM_TEXT=302, NEWLINE_10=303, 
		SPACE_TAB_6=304, NEWLINE_11=305, NUMBER=306, PROFILE=307, SPACE_TAB_7=308, 
		INCLUDE_PATH=309, NEWLINE_12=310, SPACE_TAB_8=311;
	public static final int
		RULE_translation_unit = 0, RULE_compiler_directive = 1, RULE_behavior = 2, 
		RULE_constant_expression = 3, RULE_define_directive = 4, RULE_elif_directive = 5, 
		RULE_else_directive = 6, RULE_endif_directive = 7, RULE_error_directive = 8, 
		RULE_error_message = 9, RULE_extension_directive = 10, RULE_extension_name = 11, 
		RULE_group_of_lines = 12, RULE_if_directive = 13, RULE_ifdef_directive = 14, 
		RULE_ifndef_directive = 15, RULE_line_directive = 16, RULE_line_expression = 17, 
		RULE_macro_esc_newline = 18, RULE_macro_identifier = 19, RULE_macro_name = 20, 
		RULE_macro_text = 21, RULE_macro_text_ = 22, RULE_number = 23, RULE_off = 24, 
		RULE_on = 25, RULE_pragma_debug = 26, RULE_pragma_directive = 27, RULE_pragma_optimize = 28, 
		RULE_profile = 29, RULE_program_text = 30, RULE_stdgl = 31, RULE_undef_directive = 32, 
		RULE_version_directive = 33;
	private static String[] makeRuleNames() {
		return new String[] {
			"translation_unit", "compiler_directive", "behavior", "constant_expression", 
			"define_directive", "elif_directive", "else_directive", "endif_directive", 
			"error_directive", "error_message", "extension_directive", "extension_name", 
			"group_of_lines", "if_directive", "ifdef_directive", "ifndef_directive", 
			"line_directive", "line_expression", "macro_esc_newline", "macro_identifier", 
			"macro_name", "macro_text", "macro_text_", "number", "off", "on", "pragma_debug", 
			"pragma_directive", "pragma_optimize", "profile", "program_text", "stdgl", 
			"undef_directive", "version_directive"
		};
	}
	public static final String[] ruleNames = makeRuleNames();

	private static String[] makeLiteralNames() {
		return new String[] {
			null, "'atomic_uint'", "'attribute'", "'bool'", "'break'", "'buffer'", 
			"'bvec2'", "'bvec3'", "'bvec4'", "'case'", "'centroid'", "'coherent'", 
			"'const'", "'continue'", "'default'", "'discard'", "'dmat2'", "'dmat2x2'", 
			"'dmat2x3'", "'dmat2x4'", "'dmat3'", "'dmat3x2'", "'dmat3x3'", "'dmat3x4'", 
			"'dmat4'", "'dmat4x2'", "'dmat4x3'", "'dmat4x4'", "'do'", "'double'", 
			"'dvec2'", "'dvec3'", "'dvec4'", "'else'", "'false'", "'flat'", "'float'", 
			"'for'", "'highp'", "'if'", "'iimage1D'", "'iimage1DArray'", "'iimage2D'", 
			"'iimage2DArray'", "'iimage2DMS'", "'iimage2DMSArray'", "'iimage2DRect'", 
			"'iimage3D'", "'iimageBuffer'", "'iimageCube'", "'iimageCubeArray'", 
			"'image1D'", "'image1DArray'", "'image2D'", "'image2DArray'", "'image2DMS'", 
			"'image2DMSArray'", "'image2DRect'", "'image3D'", "'imageBuffer'", "'imageCube'", 
			"'imageCubeArray'", "'in'", "'inout'", "'int'", "'invariant'", "'isampler1D'", 
			"'isampler1DArray'", "'isampler2D'", "'isampler2DArray'", "'isampler2DMS'", 
			"'isampler2DMSArray'", "'isampler2DRect'", "'isampler3D'", "'isamplerBuffer'", 
			"'isamplerCube'", "'isamplerCubeArray'", "'isubpassInput'", "'isubpassInputMS'", 
			"'itexture1D'", "'itexture1DArray'", "'itexture2D'", "'itexture2DArray'", 
			"'itexture2DMS'", "'itexture2DMSArray'", "'itexture2DRect'", "'itexture3D'", 
			"'itextureBuffer'", "'itextureCube'", "'itextureCubeArray'", "'ivec2'", 
			"'ivec3'", "'ivec4'", "'layout'", "'lowp'", "'mat2'", "'mat2x2'", "'mat2x3'", 
			"'mat2x4'", "'mat3'", "'mat3x2'", "'mat3x3'", "'mat3x4'", "'mat4'", "'mat4x2'", 
			"'mat4x3'", "'mat4x4'", "'mediump'", "'noperspective'", "'out'", "'patch'", 
			"'precise'", "'precision'", "'readonly'", "'restrict'", "'return'", "'sample'", 
			"'sampler'", "'sampler1D'", "'sampler1DArray'", "'sampler1DArrayShadow'", 
			"'sampler1DShadow'", "'sampler2D'", "'sampler2DArray'", "'sampler2DArrayShadow'", 
			"'sampler2DMS'", "'sampler2DMSArray'", "'sampler2DRect'", "'sampler2DRectShadow'", 
			"'sampler2DShadow'", "'sampler3D'", "'samplerBuffer'", "'samplerCube'", 
			"'samplerCubeArray'", "'samplerCubeArrayShadow'", "'samplerCubeShadow'", 
			"'samplerShadow'", "'shared'", "'smooth'", "'struct'", "'subpassInput'", 
			"'subpassInputMS'", "'subroutine'", "'switch'", "'texture1D'", "'texture1DArray'", 
			"'texture2D'", "'texture2DArray'", "'texture2DMS'", "'texture2DMSArray'", 
			"'texture2DRect'", "'texture3D'", "'textureBuffer'", "'textureCube'", 
			"'textureCubeArray'", "'true'", "'uimage1D'", "'uimage1DArray'", "'uimage2D'", 
			"'uimage2DArray'", "'uimage2DMS'", "'uimage2DMSArray'", "'uimage2DRect'", 
			"'uimage3D'", "'uimageBuffer'", "'uimageCube'", "'uimageCubeArray'", 
			"'uint'", "'uniform'", "'usampler1D'", "'usampler1DArray'", "'usampler2D'", 
			"'usampler2DArray'", "'usampler2DMS'", "'usampler2DMSArray'", "'usampler2DRect'", 
			"'usampler3D'", "'usamplerBuffer'", "'usamplerCube'", "'usamplerCubeArray'", 
			"'usubpassInput'", "'usubpassInputMS'", "'utexture1D'", "'utexture1DArray'", 
			"'utexture2D'", "'utexture2DArray'", "'utexture2DMS'", "'utexture2DMSArray'", 
			"'utexture2DRect'", "'utexture3D'", "'utextureBuffer'", "'utextureCube'", 
			"'utextureCubeArray'", "'uvec2'", "'uvec3'", "'uvec4'", "'varying'", 
			"'vec2'", "'vec3'", "'vec4'", "'void'", "'volatile'", "'while'", "'writeonly'", 
			"'+='", "'&'", "'&='", "'&&'", "'!'", "'^'", "':'", "','", "'-'", "'--'", 
			"'/='", "'.'", "'=='", "'='", "'>='", "'++'", "'<='", "'<'", "'<<='", 
			"'{'", "'['", "'<<'", "'('", "'%='", "'*='", "'!='", null, "'|='", "'||'", 
			"'%'", "'+'", "'?'", "'>'", "'>>='", "'}'", "']'", "'>>'", "')'", "';'", 
			"'/'", "'*'", "'-='", "'~'", "'|'", "'^='", "'^^'", null, null, null, 
			null, null, null, null, null, null, null, null, null, null, null, null, 
			null, null, null, null, null, null, null, null, null, null, null, null, 
			null, null, null, null, null, null, null, null, null, null, null, null, 
			null, null, null, null, null, null, "'debug'", null, "'off'", "'on'", 
			"'optimize'", null, "'STDGL'"
		};
	}
	private static final String[] _LITERAL_NAMES = makeLiteralNames();
	private static String[] makeSymbolicNames() {
		return new String[] {
			null, "ATOMIC_UINT", "ATTRIBUTE", "BOOL", "BREAK", "BUFFER", "BVEC2", 
			"BVEC3", "BVEC4", "CASE", "CENTROID", "COHERENT", "CONST", "CONTINUE", 
			"DEFAULT", "DISCARD", "DMAT2", "DMAT2X2", "DMAT2X3", "DMAT2X4", "DMAT3", 
			"DMAT3X2", "DMAT3X3", "DMAT3X4", "DMAT4", "DMAT4X2", "DMAT4X3", "DMAT4X4", 
			"DO", "DOUBLE", "DVEC2", "DVEC3", "DVEC4", "ELSE", "FALSE", "FLAT", "FLOAT", 
			"FOR", "HIGHP", "IF", "IIMAGE1D", "IIMAGE1DARRAY", "IIMAGE2D", "IIMAGE2DARRAY", 
			"IIMAGE2DMS", "IIMAGE2DMSARRAY", "IIMAGE2DRECT", "IIMAGE3D", "IIMAGEBUFFER", 
			"IIMAGECUBE", "IIMAGECUBEARRAY", "IMAGE1D", "IMAGE1DARRAY", "IMAGE2D", 
			"IMAGE2DARRAY", "IMAGE2DMS", "IMAGE2DMSARRAY", "IMAGE2DRECT", "IMAGE3D", 
			"IMAGEBUFFER", "IMAGECUBE", "IMAGECUBEARRAY", "IN", "INOUT", "INT", "INVARIANT", 
			"ISAMPLER1D", "ISAMPLER1DARRAY", "ISAMPLER2D", "ISAMPLER2DARRAY", "ISAMPLER2DMS", 
			"ISAMPLER2DMSARRAY", "ISAMPLER2DRECT", "ISAMPLER3D", "ISAMPLERBUFFER", 
			"ISAMPLERCUBE", "ISAMPLERCUBEARRAY", "ISUBPASSINPUT", "ISUBPASSINPUTMS", 
			"ITEXTURE1D", "ITEXTURE1DARRAY", "ITEXTURE2D", "ITEXTURE2DARRAY", "ITEXTURE2DMS", 
			"ITEXTURE2DMSARRAY", "ITEXTURE2DRECT", "ITEXTURE3D", "ITEXTUREBUFFER", 
			"ITEXTURECUBE", "ITEXTURECUBEARRAY", "IVEC2", "IVEC3", "IVEC4", "LAYOUT", 
			"LOWP", "MAT2", "MAT2X2", "MAT2X3", "MAT2X4", "MAT3", "MAT3X2", "MAT3X3", 
			"MAT3X4", "MAT4", "MAT4X2", "MAT4X3", "MAT4X4", "MEDIUMP", "NOPERSPECTIVE", 
			"OUT", "PATCH", "PRECISE", "PRECISION", "READONLY", "RESTRICT", "RETURN", 
			"SAMPLE", "SAMPLER", "SAMPLER1D", "SAMPLER1DARRAY", "SAMPLER1DARRAYSHADOW", 
			"SAMPLER1DSHADOW", "SAMPLER2D", "SAMPLER2DARRAY", "SAMPLER2DARRAYSHADOW", 
			"SAMPLER2DMS", "SAMPLER2DMSARRAY", "SAMPLER2DRECT", "SAMPLER2DRECTSHADOW", 
			"SAMPLER2DSHADOW", "SAMPLER3D", "SAMPLERBUFFER", "SAMPLERCUBE", "SAMPLERCUBEARRAY", 
			"SAMPLERCUBEARRAYSHADOW", "SAMPLERCUBESHADOW", "SAMPLERSHADOW", "SHARED", 
			"SMOOTH", "STRUCT", "SUBPASSINPUT", "SUBPASSINPUTMS", "SUBROUTINE", "SWITCH", 
			"TEXTURE1D", "TEXTURE1DARRAY", "TEXTURE2D", "TEXTURE2DARRAY", "TEXTURE2DMS", 
			"TEXTURE2DMSARRAY", "TEXTURE2DRECT", "TEXTURE3D", "TEXTUREBUFFER", "TEXTURECUBE", 
			"TEXTURECUBEARRAY", "TRUE", "UIMAGE1D", "UIMAGE1DARRAY", "UIMAGE2D", 
			"UIMAGE2DARRAY", "UIMAGE2DMS", "UIMAGE2DMSARRAY", "UIMAGE2DRECT", "UIMAGE3D", 
			"UIMAGEBUFFER", "UIMAGECUBE", "UIMAGECUBEARRAY", "UINT", "UNIFORM", "USAMPLER1D", 
			"USAMPLER1DARRAY", "USAMPLER2D", "USAMPLER2DARRAY", "USAMPLER2DMS", "USAMPLER2DMSARRAY", 
			"USAMPLER2DRECT", "USAMPLER3D", "USAMPLERBUFFER", "USAMPLERCUBE", "USAMPLERCUBEARRAY", 
			"USUBPASSINPUT", "USUBPASSINPUTMS", "UTEXTURE1D", "UTEXTURE1DARRAY", 
			"UTEXTURE2D", "UTEXTURE2DARRAY", "UTEXTURE2DMS", "UTEXTURE2DMSARRAY", 
			"UTEXTURE2DRECT", "UTEXTURE3D", "UTEXTUREBUFFER", "UTEXTURECUBE", "UTEXTURECUBEARRAY", 
			"UVEC2", "UVEC3", "UVEC4", "VARYING", "VEC2", "VEC3", "VEC4", "VOID", 
			"VOLATILE", "WHILE", "WRITEONLY", "ADD_ASSIGN", "AMPERSAND", "AND_ASSIGN", 
			"AND_OP", "BANG", "CARET", "COLON", "COMMA", "DASH", "DEC_OP", "DIV_ASSIGN", 
			"DOT", "EQ_OP", "EQUAL", "GE_OP", "INC_OP", "LE_OP", "LEFT_ANGLE", "LEFT_ASSIGN", 
			"LEFT_BRACE", "LEFT_BRACKET", "LEFT_OP", "LEFT_PAREN", "MOD_ASSIGN", 
			"MUL_ASSIGN", "NE_OP", "NUMBER_SIGN", "OR_ASSIGN", "OR_OP", "PERCENT", 
			"PLUS", "QUESTION", "RIGHT_ANGLE", "RIGHT_ASSIGN", "RIGHT_BRACE", "RIGHT_BRACKET", 
			"RIGHT_OP", "RIGHT_PAREN", "SEMICOLON", "SLASH", "STAR", "SUB_ASSIGN", 
			"TILDE", "VERTICAL_BAR", "XOR_ASSIGN", "XOR_OP", "DOUBLECONSTANT", "FLOATCONSTANT", 
			"INTCONSTANT", "UINTCONSTANT", "BLOCK_COMMENT", "LINE_COMMENT", "LINE_CONTINUATION", 
			"IDENTIFIER", "WHITE_SPACE", "DEFINE_DIRECTIVE", "ELIF_DIRECTIVE", "ELSE_DIRECTIVE", 
			"ENDIF_DIRECTIVE", "ERROR_DIRECTIVE", "EXTENSION_DIRECTIVE", "IF_DIRECTIVE", 
			"IFDEF_DIRECTIVE", "IFNDEF_DIRECTIVE", "LINE_DIRECTIVE", "PRAGMA_DIRECTIVE", 
			"UNDEF_DIRECTIVE", "VERSION_DIRECTIVE", "INCLUDE_DIRECTIVE", "SPACE_TAB_0", 
			"NEWLINE_0", "MACRO_NAME", "NEWLINE_1", "SPACE_TAB_1", "CONSTANT_EXPRESSION", 
			"NEWLINE_2", "ERROR_MESSAGE", "NEWLINE_3", "BEHAVIOR", "EXTENSION_NAME", 
			"NEWLINE_4", "SPACE_TAB_2", "NEWLINE_5", "MACRO_IDENTIFIER", "NEWLINE_6", 
			"SPACE_TAB_3", "LINE_EXPRESSION", "NEWLINE_7", "MACRO_ESC_NEWLINE", "MACRO_TEXT", 
			"NEWLINE_8", "DEBUG", "NEWLINE_9", "OFF", "ON", "OPTIMIZE", "SPACE_TAB_5", 
			"STDGL", "PROGRAM_TEXT", "NEWLINE_10", "SPACE_TAB_6", "NEWLINE_11", "NUMBER", 
			"PROFILE", "SPACE_TAB_7", "INCLUDE_PATH", "NEWLINE_12", "SPACE_TAB_8"
		};
	}
	private static final String[] _SYMBOLIC_NAMES = makeSymbolicNames();
	public static final Vocabulary VOCABULARY = new VocabularyImpl(_LITERAL_NAMES, _SYMBOLIC_NAMES);

	/**
	 * @deprecated Use {@link #VOCABULARY} instead.
	 */
	@Deprecated
	public static final String[] tokenNames;
	static {
		tokenNames = new String[_SYMBOLIC_NAMES.length];
		for (int i = 0; i < tokenNames.length; i++) {
			tokenNames[i] = VOCABULARY.getLiteralName(i);
			if (tokenNames[i] == null) {
				tokenNames[i] = VOCABULARY.getSymbolicName(i);
			}

			if (tokenNames[i] == null) {
				tokenNames[i] = "<INVALID>";
			}
		}
	}

	@Override
	@Deprecated
	public String[] getTokenNames() {
		return tokenNames;
	}

	@Override

	public Vocabulary getVocabulary() {
		return VOCABULARY;
	}

	@Override
	public String getGrammarFileName() { return "GLSLPreParser.g4"; }

	@Override
	public String[] getRuleNames() { return ruleNames; }

	@Override
	public String getSerializedATN() { return _serializedATN; }

	@Override
	public ATN getATN() { return _ATN; }

	public GLSLPreParser(TokenStream input) {
		super(input);
		_interp = new ParserATNSimulator(this,_ATN,_decisionToDFA,_sharedContextCache);
	}

	public static class Translation_unitContext extends ParserRuleContext {
		public List<Compiler_directiveContext> compiler_directive() {
			return getRuleContexts(Compiler_directiveContext.class);
		}
		public Compiler_directiveContext compiler_directive(int i) {
			return getRuleContext(Compiler_directiveContext.class,i);
		}
		public Translation_unitContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_translation_unit; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterTranslation_unit(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitTranslation_unit(this);
		}
	}

	public final Translation_unitContext translation_unit() throws RecognitionException {
		Translation_unitContext _localctx = new Translation_unitContext(_ctx, getState());
		enterRule(_localctx, 0, RULE_translation_unit);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(71);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==NUMBER_SIGN) {
				{
				{
				setState(68);
				compiler_directive();
				}
				}
				setState(73);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Compiler_directiveContext extends ParserRuleContext {
		public Define_directiveContext define_directive() {
			return getRuleContext(Define_directiveContext.class,0);
		}
		public Elif_directiveContext elif_directive() {
			return getRuleContext(Elif_directiveContext.class,0);
		}
		public Else_directiveContext else_directive() {
			return getRuleContext(Else_directiveContext.class,0);
		}
		public Endif_directiveContext endif_directive() {
			return getRuleContext(Endif_directiveContext.class,0);
		}
		public Error_directiveContext error_directive() {
			return getRuleContext(Error_directiveContext.class,0);
		}
		public Extension_directiveContext extension_directive() {
			return getRuleContext(Extension_directiveContext.class,0);
		}
		public If_directiveContext if_directive() {
			return getRuleContext(If_directiveContext.class,0);
		}
		public Ifdef_directiveContext ifdef_directive() {
			return getRuleContext(Ifdef_directiveContext.class,0);
		}
		public Ifndef_directiveContext ifndef_directive() {
			return getRuleContext(Ifndef_directiveContext.class,0);
		}
		public Line_directiveContext line_directive() {
			return getRuleContext(Line_directiveContext.class,0);
		}
		public Pragma_directiveContext pragma_directive() {
			return getRuleContext(Pragma_directiveContext.class,0);
		}
		public Undef_directiveContext undef_directive() {
			return getRuleContext(Undef_directiveContext.class,0);
		}
		public Version_directiveContext version_directive() {
			return getRuleContext(Version_directiveContext.class,0);
		}
		public Compiler_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_compiler_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterCompiler_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitCompiler_directive(this);
		}
	}

	public final Compiler_directiveContext compiler_directive() throws RecognitionException {
		Compiler_directiveContext _localctx = new Compiler_directiveContext(_ctx, getState());
		enterRule(_localctx, 2, RULE_compiler_directive);
		try {
			setState(87);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,1,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(74);
				define_directive();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(75);
				elif_directive();
				}
				break;
			case 3:
				enterOuterAlt(_localctx, 3);
				{
				setState(76);
				else_directive();
				}
				break;
			case 4:
				enterOuterAlt(_localctx, 4);
				{
				setState(77);
				endif_directive();
				}
				break;
			case 5:
				enterOuterAlt(_localctx, 5);
				{
				setState(78);
				error_directive();
				}
				break;
			case 6:
				enterOuterAlt(_localctx, 6);
				{
				setState(79);
				extension_directive();
				}
				break;
			case 7:
				enterOuterAlt(_localctx, 7);
				{
				setState(80);
				if_directive();
				}
				break;
			case 8:
				enterOuterAlt(_localctx, 8);
				{
				setState(81);
				ifdef_directive();
				}
				break;
			case 9:
				enterOuterAlt(_localctx, 9);
				{
				setState(82);
				ifndef_directive();
				}
				break;
			case 10:
				enterOuterAlt(_localctx, 10);
				{
				setState(83);
				line_directive();
				}
				break;
			case 11:
				enterOuterAlt(_localctx, 11);
				{
				setState(84);
				pragma_directive();
				}
				break;
			case 12:
				enterOuterAlt(_localctx, 12);
				{
				setState(85);
				undef_directive();
				}
				break;
			case 13:
				enterOuterAlt(_localctx, 13);
				{
				setState(86);
				version_directive();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class BehaviorContext extends ParserRuleContext {
		public TerminalNode BEHAVIOR() { return getToken(GLSLPreParser.BEHAVIOR, 0); }
		public BehaviorContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_behavior; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterBehavior(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitBehavior(this);
		}
	}

	public final BehaviorContext behavior() throws RecognitionException {
		BehaviorContext _localctx = new BehaviorContext(_ctx, getState());
		enterRule(_localctx, 4, RULE_behavior);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(89);
			match(BEHAVIOR);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Constant_expressionContext extends ParserRuleContext {
		public TerminalNode CONSTANT_EXPRESSION() { return getToken(GLSLPreParser.CONSTANT_EXPRESSION, 0); }
		public Constant_expressionContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_constant_expression; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterConstant_expression(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitConstant_expression(this);
		}
	}

	public final Constant_expressionContext constant_expression() throws RecognitionException {
		Constant_expressionContext _localctx = new Constant_expressionContext(_ctx, getState());
		enterRule(_localctx, 6, RULE_constant_expression);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(91);
			match(CONSTANT_EXPRESSION);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Define_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode DEFINE_DIRECTIVE() { return getToken(GLSLPreParser.DEFINE_DIRECTIVE, 0); }
		public Macro_nameContext macro_name() {
			return getRuleContext(Macro_nameContext.class,0);
		}
		public Macro_textContext macro_text() {
			return getRuleContext(Macro_textContext.class,0);
		}
		public Define_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_define_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterDefine_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitDefine_directive(this);
		}
	}

	public final Define_directiveContext define_directive() throws RecognitionException {
		Define_directiveContext _localctx = new Define_directiveContext(_ctx, getState());
		enterRule(_localctx, 8, RULE_define_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(93);
			match(NUMBER_SIGN);
			setState(94);
			match(DEFINE_DIRECTIVE);
			setState(95);
			macro_name();
			setState(96);
			macro_text();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Elif_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode ELIF_DIRECTIVE() { return getToken(GLSLPreParser.ELIF_DIRECTIVE, 0); }
		public Constant_expressionContext constant_expression() {
			return getRuleContext(Constant_expressionContext.class,0);
		}
		public Group_of_linesContext group_of_lines() {
			return getRuleContext(Group_of_linesContext.class,0);
		}
		public Elif_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_elif_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterElif_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitElif_directive(this);
		}
	}

	public final Elif_directiveContext elif_directive() throws RecognitionException {
		Elif_directiveContext _localctx = new Elif_directiveContext(_ctx, getState());
		enterRule(_localctx, 10, RULE_elif_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(98);
			match(NUMBER_SIGN);
			setState(99);
			match(ELIF_DIRECTIVE);
			setState(100);
			constant_expression();
			setState(101);
			group_of_lines();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Else_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode ELSE_DIRECTIVE() { return getToken(GLSLPreParser.ELSE_DIRECTIVE, 0); }
		public Group_of_linesContext group_of_lines() {
			return getRuleContext(Group_of_linesContext.class,0);
		}
		public Else_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_else_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterElse_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitElse_directive(this);
		}
	}

	public final Else_directiveContext else_directive() throws RecognitionException {
		Else_directiveContext _localctx = new Else_directiveContext(_ctx, getState());
		enterRule(_localctx, 12, RULE_else_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(103);
			match(NUMBER_SIGN);
			setState(104);
			match(ELSE_DIRECTIVE);
			setState(105);
			group_of_lines();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Endif_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode ENDIF_DIRECTIVE() { return getToken(GLSLPreParser.ENDIF_DIRECTIVE, 0); }
		public Endif_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_endif_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterEndif_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitEndif_directive(this);
		}
	}

	public final Endif_directiveContext endif_directive() throws RecognitionException {
		Endif_directiveContext _localctx = new Endif_directiveContext(_ctx, getState());
		enterRule(_localctx, 14, RULE_endif_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(107);
			match(NUMBER_SIGN);
			setState(108);
			match(ENDIF_DIRECTIVE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Error_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode ERROR_DIRECTIVE() { return getToken(GLSLPreParser.ERROR_DIRECTIVE, 0); }
		public Error_messageContext error_message() {
			return getRuleContext(Error_messageContext.class,0);
		}
		public Error_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_error_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterError_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitError_directive(this);
		}
	}

	public final Error_directiveContext error_directive() throws RecognitionException {
		Error_directiveContext _localctx = new Error_directiveContext(_ctx, getState());
		enterRule(_localctx, 16, RULE_error_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(110);
			match(NUMBER_SIGN);
			setState(111);
			match(ERROR_DIRECTIVE);
			setState(112);
			error_message();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Error_messageContext extends ParserRuleContext {
		public TerminalNode ERROR_MESSAGE() { return getToken(GLSLPreParser.ERROR_MESSAGE, 0); }
		public Error_messageContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_error_message; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterError_message(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitError_message(this);
		}
	}

	public final Error_messageContext error_message() throws RecognitionException {
		Error_messageContext _localctx = new Error_messageContext(_ctx, getState());
		enterRule(_localctx, 18, RULE_error_message);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(114);
			match(ERROR_MESSAGE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Extension_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode EXTENSION_DIRECTIVE() { return getToken(GLSLPreParser.EXTENSION_DIRECTIVE, 0); }
		public Extension_nameContext extension_name() {
			return getRuleContext(Extension_nameContext.class,0);
		}
		public TerminalNode COLON() { return getToken(GLSLPreParser.COLON, 0); }
		public BehaviorContext behavior() {
			return getRuleContext(BehaviorContext.class,0);
		}
		public Extension_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_extension_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterExtension_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitExtension_directive(this);
		}
	}

	public final Extension_directiveContext extension_directive() throws RecognitionException {
		Extension_directiveContext _localctx = new Extension_directiveContext(_ctx, getState());
		enterRule(_localctx, 20, RULE_extension_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(116);
			match(NUMBER_SIGN);
			setState(117);
			match(EXTENSION_DIRECTIVE);
			setState(118);
			extension_name();
			setState(119);
			match(COLON);
			setState(120);
			behavior();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Extension_nameContext extends ParserRuleContext {
		public TerminalNode EXTENSION_NAME() { return getToken(GLSLPreParser.EXTENSION_NAME, 0); }
		public Extension_nameContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_extension_name; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterExtension_name(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitExtension_name(this);
		}
	}

	public final Extension_nameContext extension_name() throws RecognitionException {
		Extension_nameContext _localctx = new Extension_nameContext(_ctx, getState());
		enterRule(_localctx, 22, RULE_extension_name);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(122);
			match(EXTENSION_NAME);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Group_of_linesContext extends ParserRuleContext {
		public List<Program_textContext> program_text() {
			return getRuleContexts(Program_textContext.class);
		}
		public Program_textContext program_text(int i) {
			return getRuleContext(Program_textContext.class,i);
		}
		public List<Compiler_directiveContext> compiler_directive() {
			return getRuleContexts(Compiler_directiveContext.class);
		}
		public Compiler_directiveContext compiler_directive(int i) {
			return getRuleContext(Compiler_directiveContext.class,i);
		}
		public Group_of_linesContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_group_of_lines; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterGroup_of_lines(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitGroup_of_lines(this);
		}
	}

	public final Group_of_linesContext group_of_lines() throws RecognitionException {
		Group_of_linesContext _localctx = new Group_of_linesContext(_ctx, getState());
		enterRule(_localctx, 24, RULE_group_of_lines);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(128);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,3,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					setState(126);
					_errHandler.sync(this);
					switch (_input.LA(1)) {
					case PROGRAM_TEXT:
						{
						setState(124);
						program_text();
						}
						break;
					case NUMBER_SIGN:
						{
						setState(125);
						compiler_directive();
						}
						break;
					default:
						throw new NoViableAltException(this);
					}
					} 
				}
				setState(130);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,3,_ctx);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class If_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode IF_DIRECTIVE() { return getToken(GLSLPreParser.IF_DIRECTIVE, 0); }
		public Constant_expressionContext constant_expression() {
			return getRuleContext(Constant_expressionContext.class,0);
		}
		public Group_of_linesContext group_of_lines() {
			return getRuleContext(Group_of_linesContext.class,0);
		}
		public Endif_directiveContext endif_directive() {
			return getRuleContext(Endif_directiveContext.class,0);
		}
		public List<Elif_directiveContext> elif_directive() {
			return getRuleContexts(Elif_directiveContext.class);
		}
		public Elif_directiveContext elif_directive(int i) {
			return getRuleContext(Elif_directiveContext.class,i);
		}
		public Else_directiveContext else_directive() {
			return getRuleContext(Else_directiveContext.class,0);
		}
		public If_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_if_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterIf_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitIf_directive(this);
		}
	}

	public final If_directiveContext if_directive() throws RecognitionException {
		If_directiveContext _localctx = new If_directiveContext(_ctx, getState());
		enterRule(_localctx, 26, RULE_if_directive);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(131);
			match(NUMBER_SIGN);
			setState(132);
			match(IF_DIRECTIVE);
			setState(133);
			constant_expression();
			setState(134);
			group_of_lines();
			setState(138);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,4,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(135);
					elif_directive();
					}
					} 
				}
				setState(140);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,4,_ctx);
			}
			setState(142);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,5,_ctx) ) {
			case 1:
				{
				setState(141);
				else_directive();
				}
				break;
			}
			setState(144);
			endif_directive();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Ifdef_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode IFDEF_DIRECTIVE() { return getToken(GLSLPreParser.IFDEF_DIRECTIVE, 0); }
		public Macro_identifierContext macro_identifier() {
			return getRuleContext(Macro_identifierContext.class,0);
		}
		public Group_of_linesContext group_of_lines() {
			return getRuleContext(Group_of_linesContext.class,0);
		}
		public Endif_directiveContext endif_directive() {
			return getRuleContext(Endif_directiveContext.class,0);
		}
		public List<Elif_directiveContext> elif_directive() {
			return getRuleContexts(Elif_directiveContext.class);
		}
		public Elif_directiveContext elif_directive(int i) {
			return getRuleContext(Elif_directiveContext.class,i);
		}
		public Else_directiveContext else_directive() {
			return getRuleContext(Else_directiveContext.class,0);
		}
		public Ifdef_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_ifdef_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterIfdef_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitIfdef_directive(this);
		}
	}

	public final Ifdef_directiveContext ifdef_directive() throws RecognitionException {
		Ifdef_directiveContext _localctx = new Ifdef_directiveContext(_ctx, getState());
		enterRule(_localctx, 28, RULE_ifdef_directive);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(146);
			match(NUMBER_SIGN);
			setState(147);
			match(IFDEF_DIRECTIVE);
			setState(148);
			macro_identifier();
			setState(149);
			group_of_lines();
			setState(153);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,6,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(150);
					elif_directive();
					}
					} 
				}
				setState(155);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,6,_ctx);
			}
			setState(157);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,7,_ctx) ) {
			case 1:
				{
				setState(156);
				else_directive();
				}
				break;
			}
			setState(159);
			endif_directive();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Ifndef_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode IFNDEF_DIRECTIVE() { return getToken(GLSLPreParser.IFNDEF_DIRECTIVE, 0); }
		public Macro_identifierContext macro_identifier() {
			return getRuleContext(Macro_identifierContext.class,0);
		}
		public Group_of_linesContext group_of_lines() {
			return getRuleContext(Group_of_linesContext.class,0);
		}
		public Endif_directiveContext endif_directive() {
			return getRuleContext(Endif_directiveContext.class,0);
		}
		public List<Elif_directiveContext> elif_directive() {
			return getRuleContexts(Elif_directiveContext.class);
		}
		public Elif_directiveContext elif_directive(int i) {
			return getRuleContext(Elif_directiveContext.class,i);
		}
		public Else_directiveContext else_directive() {
			return getRuleContext(Else_directiveContext.class,0);
		}
		public Ifndef_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_ifndef_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterIfndef_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitIfndef_directive(this);
		}
	}

	public final Ifndef_directiveContext ifndef_directive() throws RecognitionException {
		Ifndef_directiveContext _localctx = new Ifndef_directiveContext(_ctx, getState());
		enterRule(_localctx, 30, RULE_ifndef_directive);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(161);
			match(NUMBER_SIGN);
			setState(162);
			match(IFNDEF_DIRECTIVE);
			setState(163);
			macro_identifier();
			setState(164);
			group_of_lines();
			setState(168);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,8,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(165);
					elif_directive();
					}
					} 
				}
				setState(170);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,8,_ctx);
			}
			setState(172);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,9,_ctx) ) {
			case 1:
				{
				setState(171);
				else_directive();
				}
				break;
			}
			setState(174);
			endif_directive();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Line_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode LINE_DIRECTIVE() { return getToken(GLSLPreParser.LINE_DIRECTIVE, 0); }
		public Line_expressionContext line_expression() {
			return getRuleContext(Line_expressionContext.class,0);
		}
		public Line_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_line_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterLine_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitLine_directive(this);
		}
	}

	public final Line_directiveContext line_directive() throws RecognitionException {
		Line_directiveContext _localctx = new Line_directiveContext(_ctx, getState());
		enterRule(_localctx, 32, RULE_line_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(176);
			match(NUMBER_SIGN);
			setState(177);
			match(LINE_DIRECTIVE);
			setState(178);
			line_expression();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Line_expressionContext extends ParserRuleContext {
		public TerminalNode LINE_EXPRESSION() { return getToken(GLSLPreParser.LINE_EXPRESSION, 0); }
		public Line_expressionContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_line_expression; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterLine_expression(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitLine_expression(this);
		}
	}

	public final Line_expressionContext line_expression() throws RecognitionException {
		Line_expressionContext _localctx = new Line_expressionContext(_ctx, getState());
		enterRule(_localctx, 34, RULE_line_expression);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(180);
			match(LINE_EXPRESSION);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Macro_esc_newlineContext extends ParserRuleContext {
		public TerminalNode MACRO_ESC_NEWLINE() { return getToken(GLSLPreParser.MACRO_ESC_NEWLINE, 0); }
		public Macro_esc_newlineContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_macro_esc_newline; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterMacro_esc_newline(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitMacro_esc_newline(this);
		}
	}

	public final Macro_esc_newlineContext macro_esc_newline() throws RecognitionException {
		Macro_esc_newlineContext _localctx = new Macro_esc_newlineContext(_ctx, getState());
		enterRule(_localctx, 36, RULE_macro_esc_newline);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(182);
			match(MACRO_ESC_NEWLINE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Macro_identifierContext extends ParserRuleContext {
		public TerminalNode MACRO_IDENTIFIER() { return getToken(GLSLPreParser.MACRO_IDENTIFIER, 0); }
		public Macro_identifierContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_macro_identifier; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterMacro_identifier(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitMacro_identifier(this);
		}
	}

	public final Macro_identifierContext macro_identifier() throws RecognitionException {
		Macro_identifierContext _localctx = new Macro_identifierContext(_ctx, getState());
		enterRule(_localctx, 38, RULE_macro_identifier);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(184);
			match(MACRO_IDENTIFIER);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Macro_nameContext extends ParserRuleContext {
		public TerminalNode MACRO_NAME() { return getToken(GLSLPreParser.MACRO_NAME, 0); }
		public Macro_nameContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_macro_name; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterMacro_name(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitMacro_name(this);
		}
	}

	public final Macro_nameContext macro_name() throws RecognitionException {
		Macro_nameContext _localctx = new Macro_nameContext(_ctx, getState());
		enterRule(_localctx, 40, RULE_macro_name);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(186);
			match(MACRO_NAME);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Macro_textContext extends ParserRuleContext {
		public List<Macro_text_Context> macro_text_() {
			return getRuleContexts(Macro_text_Context.class);
		}
		public Macro_text_Context macro_text_(int i) {
			return getRuleContext(Macro_text_Context.class,i);
		}
		public List<Macro_esc_newlineContext> macro_esc_newline() {
			return getRuleContexts(Macro_esc_newlineContext.class);
		}
		public Macro_esc_newlineContext macro_esc_newline(int i) {
			return getRuleContext(Macro_esc_newlineContext.class,i);
		}
		public Macro_textContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_macro_text; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterMacro_text(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitMacro_text(this);
		}
	}

	public final Macro_textContext macro_text() throws RecognitionException {
		Macro_textContext _localctx = new Macro_textContext(_ctx, getState());
		enterRule(_localctx, 42, RULE_macro_text);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(192);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==MACRO_ESC_NEWLINE || _la==MACRO_TEXT) {
				{
				setState(190);
				_errHandler.sync(this);
				switch (_input.LA(1)) {
				case MACRO_TEXT:
					{
					setState(188);
					macro_text_();
					}
					break;
				case MACRO_ESC_NEWLINE:
					{
					setState(189);
					macro_esc_newline();
					}
					break;
				default:
					throw new NoViableAltException(this);
				}
				}
				setState(194);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Macro_text_Context extends ParserRuleContext {
		public TerminalNode MACRO_TEXT() { return getToken(GLSLPreParser.MACRO_TEXT, 0); }
		public Macro_text_Context(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_macro_text_; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterMacro_text_(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitMacro_text_(this);
		}
	}

	public final Macro_text_Context macro_text_() throws RecognitionException {
		Macro_text_Context _localctx = new Macro_text_Context(_ctx, getState());
		enterRule(_localctx, 44, RULE_macro_text_);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(195);
			match(MACRO_TEXT);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class NumberContext extends ParserRuleContext {
		public TerminalNode NUMBER() { return getToken(GLSLPreParser.NUMBER, 0); }
		public NumberContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_number; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterNumber(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitNumber(this);
		}
	}

	public final NumberContext number() throws RecognitionException {
		NumberContext _localctx = new NumberContext(_ctx, getState());
		enterRule(_localctx, 46, RULE_number);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(197);
			match(NUMBER);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class OffContext extends ParserRuleContext {
		public TerminalNode OFF() { return getToken(GLSLPreParser.OFF, 0); }
		public OffContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_off; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterOff(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitOff(this);
		}
	}

	public final OffContext off() throws RecognitionException {
		OffContext _localctx = new OffContext(_ctx, getState());
		enterRule(_localctx, 48, RULE_off);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(199);
			match(OFF);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class OnContext extends ParserRuleContext {
		public TerminalNode ON() { return getToken(GLSLPreParser.ON, 0); }
		public OnContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_on; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterOn(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitOn(this);
		}
	}

	public final OnContext on() throws RecognitionException {
		OnContext _localctx = new OnContext(_ctx, getState());
		enterRule(_localctx, 50, RULE_on);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(201);
			match(ON);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Pragma_debugContext extends ParserRuleContext {
		public TerminalNode DEBUG() { return getToken(GLSLPreParser.DEBUG, 0); }
		public TerminalNode LEFT_PAREN() { return getToken(GLSLPreParser.LEFT_PAREN, 0); }
		public TerminalNode RIGHT_PAREN() { return getToken(GLSLPreParser.RIGHT_PAREN, 0); }
		public OnContext on() {
			return getRuleContext(OnContext.class,0);
		}
		public OffContext off() {
			return getRuleContext(OffContext.class,0);
		}
		public Pragma_debugContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_pragma_debug; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterPragma_debug(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitPragma_debug(this);
		}
	}

	public final Pragma_debugContext pragma_debug() throws RecognitionException {
		Pragma_debugContext _localctx = new Pragma_debugContext(_ctx, getState());
		enterRule(_localctx, 52, RULE_pragma_debug);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(203);
			match(DEBUG);
			setState(204);
			match(LEFT_PAREN);
			setState(207);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case ON:
				{
				setState(205);
				on();
				}
				break;
			case OFF:
				{
				setState(206);
				off();
				}
				break;
			default:
				throw new NoViableAltException(this);
			}
			setState(209);
			match(RIGHT_PAREN);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Pragma_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode PRAGMA_DIRECTIVE() { return getToken(GLSLPreParser.PRAGMA_DIRECTIVE, 0); }
		public StdglContext stdgl() {
			return getRuleContext(StdglContext.class,0);
		}
		public Pragma_debugContext pragma_debug() {
			return getRuleContext(Pragma_debugContext.class,0);
		}
		public Pragma_optimizeContext pragma_optimize() {
			return getRuleContext(Pragma_optimizeContext.class,0);
		}
		public Pragma_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_pragma_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterPragma_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitPragma_directive(this);
		}
	}

	public final Pragma_directiveContext pragma_directive() throws RecognitionException {
		Pragma_directiveContext _localctx = new Pragma_directiveContext(_ctx, getState());
		enterRule(_localctx, 54, RULE_pragma_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(211);
			match(NUMBER_SIGN);
			setState(212);
			match(PRAGMA_DIRECTIVE);
			setState(216);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case STDGL:
				{
				setState(213);
				stdgl();
				}
				break;
			case DEBUG:
				{
				setState(214);
				pragma_debug();
				}
				break;
			case OPTIMIZE:
				{
				setState(215);
				pragma_optimize();
				}
				break;
			default:
				throw new NoViableAltException(this);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Pragma_optimizeContext extends ParserRuleContext {
		public TerminalNode OPTIMIZE() { return getToken(GLSLPreParser.OPTIMIZE, 0); }
		public TerminalNode LEFT_PAREN() { return getToken(GLSLPreParser.LEFT_PAREN, 0); }
		public TerminalNode RIGHT_PAREN() { return getToken(GLSLPreParser.RIGHT_PAREN, 0); }
		public OnContext on() {
			return getRuleContext(OnContext.class,0);
		}
		public OffContext off() {
			return getRuleContext(OffContext.class,0);
		}
		public Pragma_optimizeContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_pragma_optimize; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterPragma_optimize(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitPragma_optimize(this);
		}
	}

	public final Pragma_optimizeContext pragma_optimize() throws RecognitionException {
		Pragma_optimizeContext _localctx = new Pragma_optimizeContext(_ctx, getState());
		enterRule(_localctx, 56, RULE_pragma_optimize);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(218);
			match(OPTIMIZE);
			setState(219);
			match(LEFT_PAREN);
			setState(222);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case ON:
				{
				setState(220);
				on();
				}
				break;
			case OFF:
				{
				setState(221);
				off();
				}
				break;
			default:
				throw new NoViableAltException(this);
			}
			setState(224);
			match(RIGHT_PAREN);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class ProfileContext extends ParserRuleContext {
		public TerminalNode PROFILE() { return getToken(GLSLPreParser.PROFILE, 0); }
		public ProfileContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_profile; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterProfile(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitProfile(this);
		}
	}

	public final ProfileContext profile() throws RecognitionException {
		ProfileContext _localctx = new ProfileContext(_ctx, getState());
		enterRule(_localctx, 58, RULE_profile);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(226);
			match(PROFILE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Program_textContext extends ParserRuleContext {
		public TerminalNode PROGRAM_TEXT() { return getToken(GLSLPreParser.PROGRAM_TEXT, 0); }
		public Program_textContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_program_text; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterProgram_text(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitProgram_text(this);
		}
	}

	public final Program_textContext program_text() throws RecognitionException {
		Program_textContext _localctx = new Program_textContext(_ctx, getState());
		enterRule(_localctx, 60, RULE_program_text);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(228);
			match(PROGRAM_TEXT);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class StdglContext extends ParserRuleContext {
		public TerminalNode STDGL() { return getToken(GLSLPreParser.STDGL, 0); }
		public StdglContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_stdgl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterStdgl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitStdgl(this);
		}
	}

	public final StdglContext stdgl() throws RecognitionException {
		StdglContext _localctx = new StdglContext(_ctx, getState());
		enterRule(_localctx, 62, RULE_stdgl);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(230);
			match(STDGL);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Undef_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode UNDEF_DIRECTIVE() { return getToken(GLSLPreParser.UNDEF_DIRECTIVE, 0); }
		public Macro_identifierContext macro_identifier() {
			return getRuleContext(Macro_identifierContext.class,0);
		}
		public Undef_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_undef_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterUndef_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitUndef_directive(this);
		}
	}

	public final Undef_directiveContext undef_directive() throws RecognitionException {
		Undef_directiveContext _localctx = new Undef_directiveContext(_ctx, getState());
		enterRule(_localctx, 64, RULE_undef_directive);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(232);
			match(NUMBER_SIGN);
			setState(233);
			match(UNDEF_DIRECTIVE);
			setState(234);
			macro_identifier();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static class Version_directiveContext extends ParserRuleContext {
		public TerminalNode NUMBER_SIGN() { return getToken(GLSLPreParser.NUMBER_SIGN, 0); }
		public TerminalNode VERSION_DIRECTIVE() { return getToken(GLSLPreParser.VERSION_DIRECTIVE, 0); }
		public NumberContext number() {
			return getRuleContext(NumberContext.class,0);
		}
		public ProfileContext profile() {
			return getRuleContext(ProfileContext.class,0);
		}
		public Version_directiveContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_version_directive; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).enterVersion_directive(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof GLSLPreParserListener ) ((GLSLPreParserListener)listener).exitVersion_directive(this);
		}
	}

	public final Version_directiveContext version_directive() throws RecognitionException {
		Version_directiveContext _localctx = new Version_directiveContext(_ctx, getState());
		enterRule(_localctx, 66, RULE_version_directive);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(236);
			match(NUMBER_SIGN);
			setState(237);
			match(VERSION_DIRECTIVE);
			setState(238);
			number();
			setState(240);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==PROFILE) {
				{
				setState(239);
				profile();
				}
			}

			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public static final String _serializedATN =
		"\3\u608b\ua72a\u8133\ub9ed\u417c\u3be7\u7786\u5964\3\u0139\u00f5\4\2\t"+
		"\2\4\3\t\3\4\4\t\4\4\5\t\5\4\6\t\6\4\7\t\7\4\b\t\b\4\t\t\t\4\n\t\n\4\13"+
		"\t\13\4\f\t\f\4\r\t\r\4\16\t\16\4\17\t\17\4\20\t\20\4\21\t\21\4\22\t\22"+
		"\4\23\t\23\4\24\t\24\4\25\t\25\4\26\t\26\4\27\t\27\4\30\t\30\4\31\t\31"+
		"\4\32\t\32\4\33\t\33\4\34\t\34\4\35\t\35\4\36\t\36\4\37\t\37\4 \t \4!"+
		"\t!\4\"\t\"\4#\t#\3\2\7\2H\n\2\f\2\16\2K\13\2\3\3\3\3\3\3\3\3\3\3\3\3"+
		"\3\3\3\3\3\3\3\3\3\3\3\3\3\3\5\3Z\n\3\3\4\3\4\3\5\3\5\3\6\3\6\3\6\3\6"+
		"\3\6\3\7\3\7\3\7\3\7\3\7\3\b\3\b\3\b\3\b\3\t\3\t\3\t\3\n\3\n\3\n\3\n\3"+
		"\13\3\13\3\f\3\f\3\f\3\f\3\f\3\f\3\r\3\r\3\16\3\16\7\16\u0081\n\16\f\16"+
		"\16\16\u0084\13\16\3\17\3\17\3\17\3\17\3\17\7\17\u008b\n\17\f\17\16\17"+
		"\u008e\13\17\3\17\5\17\u0091\n\17\3\17\3\17\3\20\3\20\3\20\3\20\3\20\7"+
		"\20\u009a\n\20\f\20\16\20\u009d\13\20\3\20\5\20\u00a0\n\20\3\20\3\20\3"+
		"\21\3\21\3\21\3\21\3\21\7\21\u00a9\n\21\f\21\16\21\u00ac\13\21\3\21\5"+
		"\21\u00af\n\21\3\21\3\21\3\22\3\22\3\22\3\22\3\23\3\23\3\24\3\24\3\25"+
		"\3\25\3\26\3\26\3\27\3\27\7\27\u00c1\n\27\f\27\16\27\u00c4\13\27\3\30"+
		"\3\30\3\31\3\31\3\32\3\32\3\33\3\33\3\34\3\34\3\34\3\34\5\34\u00d2\n\34"+
		"\3\34\3\34\3\35\3\35\3\35\3\35\3\35\5\35\u00db\n\35\3\36\3\36\3\36\3\36"+
		"\5\36\u00e1\n\36\3\36\3\36\3\37\3\37\3 \3 \3!\3!\3\"\3\"\3\"\3\"\3#\3"+
		"#\3#\3#\5#\u00f3\n#\3#\2\2$\2\4\6\b\n\f\16\20\22\24\26\30\32\34\36 \""+
		"$&(*,.\60\62\64\668:<>@BD\2\2\2\u00ee\2I\3\2\2\2\4Y\3\2\2\2\6[\3\2\2\2"+
		"\b]\3\2\2\2\n_\3\2\2\2\fd\3\2\2\2\16i\3\2\2\2\20m\3\2\2\2\22p\3\2\2\2"+
		"\24t\3\2\2\2\26v\3\2\2\2\30|\3\2\2\2\32\u0082\3\2\2\2\34\u0085\3\2\2\2"+
		"\36\u0094\3\2\2\2 \u00a3\3\2\2\2\"\u00b2\3\2\2\2$\u00b6\3\2\2\2&\u00b8"+
		"\3\2\2\2(\u00ba\3\2\2\2*\u00bc\3\2\2\2,\u00c2\3\2\2\2.\u00c5\3\2\2\2\60"+
		"\u00c7\3\2\2\2\62\u00c9\3\2\2\2\64\u00cb\3\2\2\2\66\u00cd\3\2\2\28\u00d5"+
		"\3\2\2\2:\u00dc\3\2\2\2<\u00e4\3\2\2\2>\u00e6\3\2\2\2@\u00e8\3\2\2\2B"+
		"\u00ea\3\2\2\2D\u00ee\3\2\2\2FH\5\4\3\2GF\3\2\2\2HK\3\2\2\2IG\3\2\2\2"+
		"IJ\3\2\2\2J\3\3\2\2\2KI\3\2\2\2LZ\5\n\6\2MZ\5\f\7\2NZ\5\16\b\2OZ\5\20"+
		"\t\2PZ\5\22\n\2QZ\5\26\f\2RZ\5\34\17\2SZ\5\36\20\2TZ\5 \21\2UZ\5\"\22"+
		"\2VZ\58\35\2WZ\5B\"\2XZ\5D#\2YL\3\2\2\2YM\3\2\2\2YN\3\2\2\2YO\3\2\2\2"+
		"YP\3\2\2\2YQ\3\2\2\2YR\3\2\2\2YS\3\2\2\2YT\3\2\2\2YU\3\2\2\2YV\3\2\2\2"+
		"YW\3\2\2\2YX\3\2\2\2Z\5\3\2\2\2[\\\7\u011c\2\2\\\7\3\2\2\2]^\7\u0118\2"+
		"\2^\t\3\2\2\2_`\7\u00e8\2\2`a\7\u0105\2\2ab\5*\26\2bc\5,\27\2c\13\3\2"+
		"\2\2de\7\u00e8\2\2ef\7\u0106\2\2fg\5\b\5\2gh\5\32\16\2h\r\3\2\2\2ij\7"+
		"\u00e8\2\2jk\7\u0107\2\2kl\5\32\16\2l\17\3\2\2\2mn\7\u00e8\2\2no\7\u0108"+
		"\2\2o\21\3\2\2\2pq\7\u00e8\2\2qr\7\u0109\2\2rs\5\24\13\2s\23\3\2\2\2t"+
		"u\7\u011a\2\2u\25\3\2\2\2vw\7\u00e8\2\2wx\7\u010a\2\2xy\5\30\r\2yz\7\u00d4"+
		"\2\2z{\5\6\4\2{\27\3\2\2\2|}\7\u011d\2\2}\31\3\2\2\2~\u0081\5> \2\177"+
		"\u0081\5\4\3\2\u0080~\3\2\2\2\u0080\177\3\2\2\2\u0081\u0084\3\2\2\2\u0082"+
		"\u0080\3\2\2\2\u0082\u0083\3\2\2\2\u0083\33\3\2\2\2\u0084\u0082\3\2\2"+
		"\2\u0085\u0086\7\u00e8\2\2\u0086\u0087\7\u010b\2\2\u0087\u0088\5\b\5\2"+
		"\u0088\u008c\5\32\16\2\u0089\u008b\5\f\7\2\u008a\u0089\3\2\2\2\u008b\u008e"+
		"\3\2\2\2\u008c\u008a\3\2\2\2\u008c\u008d\3\2\2\2\u008d\u0090\3\2\2\2\u008e"+
		"\u008c\3\2\2\2\u008f\u0091\5\16\b\2\u0090\u008f\3\2\2\2\u0090\u0091\3"+
		"\2\2\2\u0091\u0092\3\2\2\2\u0092\u0093\5\20\t\2\u0093\35\3\2\2\2\u0094"+
		"\u0095\7\u00e8\2\2\u0095\u0096\7\u010c\2\2\u0096\u0097\5(\25\2\u0097\u009b"+
		"\5\32\16\2\u0098\u009a\5\f\7\2\u0099\u0098\3\2\2\2\u009a\u009d\3\2\2\2"+
		"\u009b\u0099\3\2\2\2\u009b\u009c\3\2\2\2\u009c\u009f\3\2\2\2\u009d\u009b"+
		"\3\2\2\2\u009e\u00a0\5\16\b\2\u009f\u009e\3\2\2\2\u009f\u00a0\3\2\2\2"+
		"\u00a0\u00a1\3\2\2\2\u00a1\u00a2\5\20\t\2\u00a2\37\3\2\2\2\u00a3\u00a4"+
		"\7\u00e8\2\2\u00a4\u00a5\7\u010d\2\2\u00a5\u00a6\5(\25\2\u00a6\u00aa\5"+
		"\32\16\2\u00a7\u00a9\5\f\7\2\u00a8\u00a7\3\2\2\2\u00a9\u00ac\3\2\2\2\u00aa"+
		"\u00a8\3\2\2\2\u00aa\u00ab\3\2\2\2\u00ab\u00ae\3\2\2\2\u00ac\u00aa\3\2"+
		"\2\2\u00ad\u00af\5\16\b\2\u00ae\u00ad\3\2\2\2\u00ae\u00af\3\2\2\2\u00af"+
		"\u00b0\3\2\2\2\u00b0\u00b1\5\20\t\2\u00b1!\3\2\2\2\u00b2\u00b3\7\u00e8"+
		"\2\2\u00b3\u00b4\7\u010e\2\2\u00b4\u00b5\5$\23\2\u00b5#\3\2\2\2\u00b6"+
		"\u00b7\7\u0124\2\2\u00b7%\3\2\2\2\u00b8\u00b9\7\u0126\2\2\u00b9\'\3\2"+
		"\2\2\u00ba\u00bb\7\u0121\2\2\u00bb)\3\2\2\2\u00bc\u00bd\7\u0115\2\2\u00bd"+
		"+\3\2\2\2\u00be\u00c1\5.\30\2\u00bf\u00c1\5&\24\2\u00c0\u00be\3\2\2\2"+
		"\u00c0\u00bf\3\2\2\2\u00c1\u00c4\3\2\2\2\u00c2\u00c0\3\2\2\2\u00c2\u00c3"+
		"\3\2\2\2\u00c3-\3\2\2\2\u00c4\u00c2\3\2\2\2\u00c5\u00c6\7\u0127\2\2\u00c6"+
		"/\3\2\2\2\u00c7\u00c8\7\u0134\2\2\u00c8\61\3\2\2\2\u00c9\u00ca\7\u012b"+
		"\2\2\u00ca\63\3\2\2\2\u00cb\u00cc\7\u012c\2\2\u00cc\65\3\2\2\2\u00cd\u00ce"+
		"\7\u0129\2\2\u00ce\u00d1\7\u00e4\2\2\u00cf\u00d2\5\64\33\2\u00d0\u00d2"+
		"\5\62\32\2\u00d1\u00cf\3\2\2\2\u00d1\u00d0\3\2\2\2\u00d2\u00d3\3\2\2\2"+
		"\u00d3\u00d4\7\u00f3\2\2\u00d4\67\3\2\2\2\u00d5\u00d6\7\u00e8\2\2\u00d6"+
		"\u00da\7\u010f\2\2\u00d7\u00db\5@!\2\u00d8\u00db\5\66\34\2\u00d9\u00db"+
		"\5:\36\2\u00da\u00d7\3\2\2\2\u00da\u00d8\3\2\2\2\u00da\u00d9\3\2\2\2\u00db"+
		"9\3\2\2\2\u00dc\u00dd\7\u012d\2\2\u00dd\u00e0\7\u00e4\2\2\u00de\u00e1"+
		"\5\64\33\2\u00df\u00e1\5\62\32\2\u00e0\u00de\3\2\2\2\u00e0\u00df\3\2\2"+
		"\2\u00e1\u00e2\3\2\2\2\u00e2\u00e3\7\u00f3\2\2\u00e3;\3\2\2\2\u00e4\u00e5"+
		"\7\u0135\2\2\u00e5=\3\2\2\2\u00e6\u00e7\7\u0130\2\2\u00e7?\3\2\2\2\u00e8"+
		"\u00e9\7\u012f\2\2\u00e9A\3\2\2\2\u00ea\u00eb\7\u00e8\2\2\u00eb\u00ec"+
		"\7\u0110\2\2\u00ec\u00ed\5(\25\2\u00edC\3\2\2\2\u00ee\u00ef\7\u00e8\2"+
		"\2\u00ef\u00f0\7\u0111\2\2\u00f0\u00f2\5\60\31\2\u00f1\u00f3\5<\37\2\u00f2"+
		"\u00f1\3\2\2\2\u00f2\u00f3\3\2\2\2\u00f3E\3\2\2\2\22IY\u0080\u0082\u008c"+
		"\u0090\u009b\u009f\u00aa\u00ae\u00c0\u00c2\u00d1\u00da\u00e0\u00f2";
	public static final ATN _ATN =
		new ATNDeserializer().deserialize(_serializedATN.toCharArray());
	static {
		_decisionToDFA = new DFA[_ATN.getNumberOfDecisions()];
		for (int i = 0; i < _ATN.getNumberOfDecisions(); i++) {
			_decisionToDFA[i] = new DFA(_ATN.getDecisionState(i), i);
		}
	}
}