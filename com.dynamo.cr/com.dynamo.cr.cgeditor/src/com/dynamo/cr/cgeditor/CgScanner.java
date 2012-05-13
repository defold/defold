package com.dynamo.cr.cgeditor;

import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.IRule;
import org.eclipse.jface.text.rules.IToken;
import org.eclipse.jface.text.rules.IWordDetector;
import org.eclipse.jface.text.rules.RuleBasedScanner;
import org.eclipse.jface.text.rules.Token;
import org.eclipse.jface.text.rules.WhitespaceRule;
import org.eclipse.jface.text.rules.WordPatternRule;
import org.eclipse.jface.text.rules.WordRule;
import org.eclipse.swt.SWT;

public class CgScanner extends RuleBasedScanner {

    public CgScanner(ColorManager manager) {
        IToken keyword =
            new Token(
                new TextAttribute(manager.getColor(ICgColorConstants.KEYWORD), null, SWT.BOLD));
        IToken type =
            new Token(
                new TextAttribute(manager.getColor(ICgColorConstants.TYPE), null, SWT.BOLD));
        IToken semantic =
            new Token(
                new TextAttribute(manager.getColor(ICgColorConstants.SEMANTIC), null, SWT.ITALIC));
        IToken defaultToken =
            new Token(
                new TextAttribute(manager.getColor(ICgColorConstants.DEFAULT)));

        IRule[] rules = new IRule[7];

        // Add a rule for single-line comment
        rules[0] = new CommentRule(manager);
        // Add generic whitespace rule.
        rules[1] = new WhitespaceRule(new CgWhitespaceDetector());

         IWordDetector nameDetector = new IWordDetector() {
             public boolean isWordStart(char c) {
                 return Character.isLetter(c) || c == '_';
             }

             public boolean isWordPart(char c) {
                 return isWordStart(c) || Character.isDigit(c);
             }
         };

         WordRule nameRule = new WordRule(nameDetector, Token.UNDEFINED, false);
         for (int i = 0; i < KEYWORDS.length; ++i)
             nameRule.addWord(KEYWORDS[i], keyword);
         for (int i = 0; i < NUMERIC_TYPES.length; ++i)
         {
             nameRule.addWord(NUMERIC_TYPES[i], type);
             for (int j = 1; j < 5; ++j)
             {
                 nameRule.addWord(NUMERIC_TYPES[i] + j, type);
                 for (int k = 1; k < 5; ++k)
                     nameRule.addWord(NUMERIC_TYPES[i] + j + "x" + k, type);
             }
         }
         for (int i = 0; i < TYPES.length; ++i)
             nameRule.addWord(TYPES[i], type);
         for (int i = 0; i < SEMANTICS.length; ++i)
             nameRule.addWord(SEMANTICS[i], semantic);
         for (int i = 0; i < 16; ++i)
         {
             nameRule.addWord("TEXUNIT" + i, semantic);
             nameRule.addWord("ATTR" + i, semantic);
         }
         for (int i = 0; i < 8; ++i)
         {
             nameRule.addWord("TEXCOORD" + i, semantic);
             nameRule.addWord("TEX" + i, semantic);
         }
         for (int i = 0; i < 6; ++i)
             nameRule.addWord("CPL" + i, semantic);
         rules[2] = nameRule;

         WordRule icNameRule = new WordRule(nameDetector, Token.UNDEFINED, true);
         for (int i = 0; i < IC_KEYWORDS.length; ++i)
             icNameRule.addWord(IC_KEYWORDS[i], keyword);
         for (int i = 0; i < 256; ++i)
             icNameRule.addWord("c" + i, semantic);
         rules[3] = icNameRule;

         // any word prefixed by '__' is considered a keyword
         rules[4] = new WordPatternRule(nameDetector, "__", "", keyword);

         // preprocessor directives
         rules[5] = new WordPatternRule(nameDetector, "#", "", keyword);

         rules[6] = new WordRule(nameDetector, defaultToken);

        setRules(rules);
    }

    private static final String KEYWORDS[] = {
        "asm_fragment",
        "auto",
        "bool",
        "break",
        "case",
        "catch",
        "char",
        "class",
        "column_major",
        "compile",
        "const",
        "const_cast",
        "continue",
        "default",
        "delete",
        "discard",
        "do",
        "double",
        "dynamic_cast",
        "else",
        "emit",
        "enum",
        "explicit",
        "extern",
        "false",
        "fixed",
        "for",
        "friend",
        "get",
        "goto",
        "half",
        "if",
        "in",
        "inline",
        "inout",
        "int",
        "interface",
        "long",
        "mutable",
        "namespace",
        "new",
        "operator",
        "out",
        "packed",
        "private",
        "protected",
        "public",
        "register",
        "reinterpret_cast",
        "return",
        "row_major",
        "sampler",
        "sampler_state",
        "sampler1D",
        "sampler2D",
        "sampler3D",
        "samplerCUBE",
        "shared",
        "short",
        "signed",
        "sizeof",
        "static",
        "static_cast",
        "struct",
        "switch",
        "template",
        "texture1D",
        "texture2D",
        "texture3D",
        "textureCUBE",
        "textureRECT",
        "this",
        "throw",
        "true",
        "try",
        "typedef",
        "typeid",
        "typename",
        "uniform",
        "union",
        "unsigned",
        "using",
        "virtual",
        "void",
        "volatile",
        "while"
    };

    private static final String IC_KEYWORDS[] = {
        "asm",
        "decl",
        "dword",
        "float",
        "matrix",
        "pass",
        "pixelfragment",
        "pixelshader",
        "string",
        "technique",
        "texture",
        "vector",
        "vertexfragment",
        "vertexshader"
    };

    private static final String NUMERIC_TYPES[] = {
        "int",
        "unsigned",
        "char",
        "short",
        "long",
        "float",
        "half",
        "fixed",
        "bool"
    };

    private static final String TYPES[] = {
        "void",
        "sampler",
        "sampler1D",
        "sampler2D",
        "sampler3D",
        "samplerCUBE",
        "samplerRECT"
    };

    private static final String SEMANTICS[] = {
        "POSITION",
        "BLENDWEIGHT",
        "NORMAL",
        "COLOR",
        "COLOR0",
        "COLOR1",
        "DIFFUSE",
        "SPECULAR",
        "TESSFACTOR",
        "FOGCOORD",
        "PSIZE",
        "PSIZ",
        "BLENDINDICES",
        "TANGENT",
        "BINORMAL",
        "HPOS",
        "FOG",
        "FOGC",
        "COL0",
        "COL1",
        "BCOL0",
        "BCOL1"
    };
}
