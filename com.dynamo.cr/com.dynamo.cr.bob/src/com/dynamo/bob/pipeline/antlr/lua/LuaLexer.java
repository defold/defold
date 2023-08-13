// Copyright 2020-2023 The Defold Foundation
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

package com.dynamo.bob.pipeline.antlr.lua;

// Generated from LuaLexer.g4 by ANTLR 4.9.1
import org.antlr.v4.runtime.Lexer;
import org.antlr.v4.runtime.CharStream;
import org.antlr.v4.runtime.Token;
import org.antlr.v4.runtime.TokenStream;
import org.antlr.v4.runtime.*;
import org.antlr.v4.runtime.atn.*;
import org.antlr.v4.runtime.dfa.DFA;
import org.antlr.v4.runtime.misc.*;

@SuppressWarnings({"all", "warnings", "unchecked", "unused", "cast"})
public class LuaLexer extends Lexer {
	static { RuntimeMetaData.checkVersion("4.9.1", RuntimeMetaData.VERSION); }

	protected static final DFA[] _decisionToDFA;
	protected static final PredictionContextCache _sharedContextCache =
		new PredictionContextCache();
	public static final int
		SEMICOLON=1, BREAK=2, GOTO=3, DO=4, WHILE=5, END=6, REPEAT=7, UNTIL=8,
		FOR=9, FUNCTION=10, LOCAL=11, IF=12, THEN=13, ELSEIF=14, ELSE=15, RETURN=16,
		COLON=17, DCOLON=18, DOT=19, COMMA=20, IN=21, LPAREN=22, RPAREN=23, LBRACK=24,
		RBRACK=25, LBRACE=26, RBRACE=27, OR=28, AND=29, LT=30, GT=31, LTE=32,
		GTE=33, NEQ=34, EQ=35, EQUALS=36, STRCAT=37, PLUS=38, MINUS=39, MUL=40,
		DIV=41, MOD=42, DIVFLOOR=43, BITAND=44, BITOR=45, BITNOT=46, BITSHL=47,
		BITSHR=48, NOT=49, LEN=50, POWER=51, NIL=52, FALSE=53, TRUE=54, DOTS=55,
		NAME=56, NORMALSTRING=57, CHARSTRING=58, LONGSTRING=59, INT=60, HEX=61,
		FLOAT=62, HEX_FLOAT=63, COMMENT=64, LINE_COMMENT=65, WS=66, SHEBANG=67;
	public static final int
		COMMENTS=2;
	public static String[] channelNames = {
		"DEFAULT_TOKEN_CHANNEL", "HIDDEN", "COMMENTS"
	};

	public static String[] modeNames = {
		"DEFAULT_MODE"
	};

	private static String[] makeRuleNames() {
		return new String[] {
			"SEMICOLON", "BREAK", "GOTO", "DO", "WHILE", "END", "REPEAT", "UNTIL",
			"FOR", "FUNCTION", "LOCAL", "IF", "THEN", "ELSEIF", "ELSE", "RETURN",
			"COLON", "DCOLON", "DOT", "COMMA", "IN", "LPAREN", "RPAREN", "LBRACK",
			"RBRACK", "LBRACE", "RBRACE", "OR", "AND", "LT", "GT", "LTE", "GTE",
			"NEQ", "EQ", "EQUALS", "STRCAT", "PLUS", "MINUS", "MUL", "DIV", "MOD",
			"DIVFLOOR", "BITAND", "BITOR", "BITNOT", "BITSHL", "BITSHR", "NOT", "LEN",
			"POWER", "NIL", "FALSE", "TRUE", "DOTS", "NAME", "NORMALSTRING", "CHARSTRING",
			"LONGSTRING", "NESTED_STR", "INT", "HEX", "FLOAT", "HEX_FLOAT", "ExponentPart",
			"HexExponentPart", "EscapeSequence", "DecimalEscape", "HexEscape", "UtfEscape",
			"Digit", "HexDigit", "COMMENT", "LINE_COMMENT", "WS", "SHEBANG"
		};
	}
	public static final String[] ruleNames = makeRuleNames();

	private static String[] makeLiteralNames() {
		return new String[] {
			null, "';'", "'break'", "'goto'", "'do'", "'while'", "'end'", "'repeat'",
			"'until'", "'for'", "'function'", "'local'", "'if'", "'then'", "'elseif'",
			"'else'", "'return'", "':'", "'::'", "'.'", "','", "'in'", "'('", "')'",
			"'['", "']'", "'{'", "'}'", "'or'", "'and'", "'<'", "'>'", "'<='", "'>='",
			"'~='", "'=='", "'='", "'..'", "'+'", "'-'", "'*'", "'/'", "'%'", "'//'",
			"'&'", "'|'", "'~'", "'<<'", "'>>'", "'not'", "'#'", "'^'", "'nil'",
			"'false'", "'true'", "'...'"
		};
	}
	private static final String[] _LITERAL_NAMES = makeLiteralNames();
	private static String[] makeSymbolicNames() {
		return new String[] {
			null, "SEMICOLON", "BREAK", "GOTO", "DO", "WHILE", "END", "REPEAT", "UNTIL",
			"FOR", "FUNCTION", "LOCAL", "IF", "THEN", "ELSEIF", "ELSE", "RETURN",
			"COLON", "DCOLON", "DOT", "COMMA", "IN", "LPAREN", "RPAREN", "LBRACK",
			"RBRACK", "LBRACE", "RBRACE", "OR", "AND", "LT", "GT", "LTE", "GTE",
			"NEQ", "EQ", "EQUALS", "STRCAT", "PLUS", "MINUS", "MUL", "DIV", "MOD",
			"DIVFLOOR", "BITAND", "BITOR", "BITNOT", "BITSHL", "BITSHR", "NOT", "LEN",
			"POWER", "NIL", "FALSE", "TRUE", "DOTS", "NAME", "NORMALSTRING", "CHARSTRING",
			"LONGSTRING", "INT", "HEX", "FLOAT", "HEX_FLOAT", "COMMENT", "LINE_COMMENT",
			"WS", "SHEBANG"
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


	public LuaLexer(CharStream input) {
		super(input);
		_interp = new LexerATNSimulator(this,_ATN,_decisionToDFA,_sharedContextCache);
	}

	@Override
	public String getGrammarFileName() { return "LuaLexer.g4"; }

	@Override
	public String[] getRuleNames() { return ruleNames; }

	@Override
	public String getSerializedATN() { return _serializedATN; }

	@Override
	public String[] getChannelNames() { return channelNames; }

	@Override
	public String[] getModeNames() { return modeNames; }

	@Override
	public ATN getATN() { return _ATN; }

	public static final String _serializedATN =
		"\3\u608b\ua72a\u8133\ub9ed\u417c\u3be7\u7786\u5964\2E\u025b\b\1\4\2\t"+
		"\2\4\3\t\3\4\4\t\4\4\5\t\5\4\6\t\6\4\7\t\7\4\b\t\b\4\t\t\t\4\n\t\n\4\13"+
		"\t\13\4\f\t\f\4\r\t\r\4\16\t\16\4\17\t\17\4\20\t\20\4\21\t\21\4\22\t\22"+
		"\4\23\t\23\4\24\t\24\4\25\t\25\4\26\t\26\4\27\t\27\4\30\t\30\4\31\t\31"+
		"\4\32\t\32\4\33\t\33\4\34\t\34\4\35\t\35\4\36\t\36\4\37\t\37\4 \t \4!"+
		"\t!\4\"\t\"\4#\t#\4$\t$\4%\t%\4&\t&\4\'\t\'\4(\t(\4)\t)\4*\t*\4+\t+\4"+
		",\t,\4-\t-\4.\t.\4/\t/\4\60\t\60\4\61\t\61\4\62\t\62\4\63\t\63\4\64\t"+
		"\64\4\65\t\65\4\66\t\66\4\67\t\67\48\t8\49\t9\4:\t:\4;\t;\4<\t<\4=\t="+
		"\4>\t>\4?\t?\4@\t@\4A\tA\4B\tB\4C\tC\4D\tD\4E\tE\4F\tF\4G\tG\4H\tH\4I"+
		"\tI\4J\tJ\4K\tK\4L\tL\4M\tM\3\2\3\2\3\3\3\3\3\3\3\3\3\3\3\3\3\4\3\4\3"+
		"\4\3\4\3\4\3\5\3\5\3\5\3\6\3\6\3\6\3\6\3\6\3\6\3\7\3\7\3\7\3\7\3\b\3\b"+
		"\3\b\3\b\3\b\3\b\3\b\3\t\3\t\3\t\3\t\3\t\3\t\3\n\3\n\3\n\3\n\3\13\3\13"+
		"\3\13\3\13\3\13\3\13\3\13\3\13\3\13\3\f\3\f\3\f\3\f\3\f\3\f\3\r\3\r\3"+
		"\r\3\16\3\16\3\16\3\16\3\16\3\17\3\17\3\17\3\17\3\17\3\17\3\17\3\20\3"+
		"\20\3\20\3\20\3\20\3\21\3\21\3\21\3\21\3\21\3\21\3\21\3\22\3\22\3\23\3"+
		"\23\3\23\3\24\3\24\3\25\3\25\3\26\3\26\3\26\3\27\3\27\3\30\3\30\3\31\3"+
		"\31\3\32\3\32\3\33\3\33\3\34\3\34\3\35\3\35\3\35\3\36\3\36\3\36\3\36\3"+
		"\37\3\37\3 \3 \3!\3!\3!\3\"\3\"\3\"\3#\3#\3#\3$\3$\3$\3%\3%\3&\3&\3&\3"+
		"\'\3\'\3(\3(\3)\3)\3*\3*\3+\3+\3,\3,\3,\3-\3-\3.\3.\3/\3/\3\60\3\60\3"+
		"\60\3\61\3\61\3\61\3\62\3\62\3\62\3\62\3\63\3\63\3\64\3\64\3\65\3\65\3"+
		"\65\3\65\3\66\3\66\3\66\3\66\3\66\3\66\3\67\3\67\3\67\3\67\3\67\38\38"+
		"\38\38\39\39\79\u015b\n9\f9\169\u015e\139\3:\3:\3:\7:\u0163\n:\f:\16:"+
		"\u0166\13:\3:\3:\3;\3;\3;\7;\u016d\n;\f;\16;\u0170\13;\3;\3;\3<\3<\3<"+
		"\3<\3=\3=\3=\3=\3=\3=\7=\u017e\n=\f=\16=\u0181\13=\3=\5=\u0184\n=\3>\6"+
		">\u0187\n>\r>\16>\u0188\3?\3?\3?\6?\u018e\n?\r?\16?\u018f\3@\6@\u0193"+
		"\n@\r@\16@\u0194\3@\3@\7@\u0199\n@\f@\16@\u019c\13@\3@\5@\u019f\n@\3@"+
		"\3@\6@\u01a3\n@\r@\16@\u01a4\3@\5@\u01a8\n@\3@\6@\u01ab\n@\r@\16@\u01ac"+
		"\3@\3@\5@\u01b1\n@\3A\3A\3A\6A\u01b6\nA\rA\16A\u01b7\3A\3A\7A\u01bc\n"+
		"A\fA\16A\u01bf\13A\3A\5A\u01c2\nA\3A\3A\3A\3A\6A\u01c8\nA\rA\16A\u01c9"+
		"\3A\5A\u01cd\nA\3A\3A\3A\6A\u01d2\nA\rA\16A\u01d3\3A\3A\5A\u01d8\nA\3"+
		"B\3B\5B\u01dc\nB\3B\6B\u01df\nB\rB\16B\u01e0\3C\3C\5C\u01e5\nC\3C\6C\u01e8"+
		"\nC\rC\16C\u01e9\3D\3D\3D\3D\5D\u01f0\nD\3D\3D\3D\3D\5D\u01f6\nD\3E\3"+
		"E\3E\3E\3E\3E\3E\3E\3E\3E\3E\5E\u0203\nE\3F\3F\3F\3F\3F\3G\3G\3G\3G\3"+
		"G\6G\u020f\nG\rG\16G\u0210\3G\3G\3H\3H\3I\3I\3J\3J\3J\3J\3J\3J\3J\3J\3"+
		"J\3K\3K\3K\3K\3K\3K\7K\u0228\nK\fK\16K\u022b\13K\3K\3K\7K\u022f\nK\fK"+
		"\16K\u0232\13K\3K\3K\7K\u0236\nK\fK\16K\u0239\13K\3K\3K\7K\u023d\nK\f"+
		"K\16K\u0240\13K\5K\u0242\nK\3K\3K\3K\5K\u0247\nK\3K\3K\3L\6L\u024c\nL"+
		"\rL\16L\u024d\3L\3L\3M\3M\3M\7M\u0255\nM\fM\16M\u0258\13M\3M\3M\3\u017f"+
		"\2N\3\3\5\4\7\5\t\6\13\7\r\b\17\t\21\n\23\13\25\f\27\r\31\16\33\17\35"+
		"\20\37\21!\22#\23%\24\'\25)\26+\27-\30/\31\61\32\63\33\65\34\67\359\36"+
		";\37= ?!A\"C#E$G%I&K\'M(O)Q*S+U,W-Y.[/]\60_\61a\62c\63e\64g\65i\66k\67"+
		"m8o9q:s;u<w=y\2{>}?\177@\u0081A\u0083\2\u0085\2\u0087\2\u0089\2\u008b"+
		"\2\u008d\2\u008f\2\u0091\2\u0093B\u0095C\u0097D\u0099E\3\2\23\5\2C\\a"+
		"ac|\6\2\62;C\\aac|\4\2$$^^\4\2))^^\4\2ZZzz\4\2GGgg\4\2--//\4\2RRrr\f\2"+
		"$$))^^cdhhppttvvxx||\3\2\62\64\3\2\62;\5\2\62;CHch\6\2\f\f\17\17??]]\4"+
		"\2\f\f\17\17\5\2\f\f\17\17]]\4\3\f\f\17\17\5\2\13\f\16\17\"\"\2\u0280"+
		"\2\3\3\2\2\2\2\5\3\2\2\2\2\7\3\2\2\2\2\t\3\2\2\2\2\13\3\2\2\2\2\r\3\2"+
		"\2\2\2\17\3\2\2\2\2\21\3\2\2\2\2\23\3\2\2\2\2\25\3\2\2\2\2\27\3\2\2\2"+
		"\2\31\3\2\2\2\2\33\3\2\2\2\2\35\3\2\2\2\2\37\3\2\2\2\2!\3\2\2\2\2#\3\2"+
		"\2\2\2%\3\2\2\2\2\'\3\2\2\2\2)\3\2\2\2\2+\3\2\2\2\2-\3\2\2\2\2/\3\2\2"+
		"\2\2\61\3\2\2\2\2\63\3\2\2\2\2\65\3\2\2\2\2\67\3\2\2\2\29\3\2\2\2\2;\3"+
		"\2\2\2\2=\3\2\2\2\2?\3\2\2\2\2A\3\2\2\2\2C\3\2\2\2\2E\3\2\2\2\2G\3\2\2"+
		"\2\2I\3\2\2\2\2K\3\2\2\2\2M\3\2\2\2\2O\3\2\2\2\2Q\3\2\2\2\2S\3\2\2\2\2"+
		"U\3\2\2\2\2W\3\2\2\2\2Y\3\2\2\2\2[\3\2\2\2\2]\3\2\2\2\2_\3\2\2\2\2a\3"+
		"\2\2\2\2c\3\2\2\2\2e\3\2\2\2\2g\3\2\2\2\2i\3\2\2\2\2k\3\2\2\2\2m\3\2\2"+
		"\2\2o\3\2\2\2\2q\3\2\2\2\2s\3\2\2\2\2u\3\2\2\2\2w\3\2\2\2\2{\3\2\2\2\2"+
		"}\3\2\2\2\2\177\3\2\2\2\2\u0081\3\2\2\2\2\u0093\3\2\2\2\2\u0095\3\2\2"+
		"\2\2\u0097\3\2\2\2\2\u0099\3\2\2\2\3\u009b\3\2\2\2\5\u009d\3\2\2\2\7\u00a3"+
		"\3\2\2\2\t\u00a8\3\2\2\2\13\u00ab\3\2\2\2\r\u00b1\3\2\2\2\17\u00b5\3\2"+
		"\2\2\21\u00bc\3\2\2\2\23\u00c2\3\2\2\2\25\u00c6\3\2\2\2\27\u00cf\3\2\2"+
		"\2\31\u00d5\3\2\2\2\33\u00d8\3\2\2\2\35\u00dd\3\2\2\2\37\u00e4\3\2\2\2"+
		"!\u00e9\3\2\2\2#\u00f0\3\2\2\2%\u00f2\3\2\2\2\'\u00f5\3\2\2\2)\u00f7\3"+
		"\2\2\2+\u00f9\3\2\2\2-\u00fc\3\2\2\2/\u00fe\3\2\2\2\61\u0100\3\2\2\2\63"+
		"\u0102\3\2\2\2\65\u0104\3\2\2\2\67\u0106\3\2\2\29\u0108\3\2\2\2;\u010b"+
		"\3\2\2\2=\u010f\3\2\2\2?\u0111\3\2\2\2A\u0113\3\2\2\2C\u0116\3\2\2\2E"+
		"\u0119\3\2\2\2G\u011c\3\2\2\2I\u011f\3\2\2\2K\u0121\3\2\2\2M\u0124\3\2"+
		"\2\2O\u0126\3\2\2\2Q\u0128\3\2\2\2S\u012a\3\2\2\2U\u012c\3\2\2\2W\u012e"+
		"\3\2\2\2Y\u0131\3\2\2\2[\u0133\3\2\2\2]\u0135\3\2\2\2_\u0137\3\2\2\2a"+
		"\u013a\3\2\2\2c\u013d\3\2\2\2e\u0141\3\2\2\2g\u0143\3\2\2\2i\u0145\3\2"+
		"\2\2k\u0149\3\2\2\2m\u014f\3\2\2\2o\u0154\3\2\2\2q\u0158\3\2\2\2s\u015f"+
		"\3\2\2\2u\u0169\3\2\2\2w\u0173\3\2\2\2y\u0183\3\2\2\2{\u0186\3\2\2\2}"+
		"\u018a\3\2\2\2\177\u01b0\3\2\2\2\u0081\u01d7\3\2\2\2\u0083\u01d9\3\2\2"+
		"\2\u0085\u01e2\3\2\2\2\u0087\u01f5\3\2\2\2\u0089\u0202\3\2\2\2\u008b\u0204"+
		"\3\2\2\2\u008d\u0209\3\2\2\2\u008f\u0214\3\2\2\2\u0091\u0216\3\2\2\2\u0093"+
		"\u0218\3\2\2\2\u0095\u0221\3\2\2\2\u0097\u024b\3\2\2\2\u0099\u0251\3\2"+
		"\2\2\u009b\u009c\7=\2\2\u009c\4\3\2\2\2\u009d\u009e\7d\2\2\u009e\u009f"+
		"\7t\2\2\u009f\u00a0\7g\2\2\u00a0\u00a1\7c\2\2\u00a1\u00a2\7m\2\2\u00a2"+
		"\6\3\2\2\2\u00a3\u00a4\7i\2\2\u00a4\u00a5\7q\2\2\u00a5\u00a6\7v\2\2\u00a6"+
		"\u00a7\7q\2\2\u00a7\b\3\2\2\2\u00a8\u00a9\7f\2\2\u00a9\u00aa\7q\2\2\u00aa"+
		"\n\3\2\2\2\u00ab\u00ac\7y\2\2\u00ac\u00ad\7j\2\2\u00ad\u00ae\7k\2\2\u00ae"+
		"\u00af\7n\2\2\u00af\u00b0\7g\2\2\u00b0\f\3\2\2\2\u00b1\u00b2\7g\2\2\u00b2"+
		"\u00b3\7p\2\2\u00b3\u00b4\7f\2\2\u00b4\16\3\2\2\2\u00b5\u00b6\7t\2\2\u00b6"+
		"\u00b7\7g\2\2\u00b7\u00b8\7r\2\2\u00b8\u00b9\7g\2\2\u00b9\u00ba\7c\2\2"+
		"\u00ba\u00bb\7v\2\2\u00bb\20\3\2\2\2\u00bc\u00bd\7w\2\2\u00bd\u00be\7"+
		"p\2\2\u00be\u00bf\7v\2\2\u00bf\u00c0\7k\2\2\u00c0\u00c1\7n\2\2\u00c1\22"+
		"\3\2\2\2\u00c2\u00c3\7h\2\2\u00c3\u00c4\7q\2\2\u00c4\u00c5\7t\2\2\u00c5"+
		"\24\3\2\2\2\u00c6\u00c7\7h\2\2\u00c7\u00c8\7w\2\2\u00c8\u00c9\7p\2\2\u00c9"+
		"\u00ca\7e\2\2\u00ca\u00cb\7v\2\2\u00cb\u00cc\7k\2\2\u00cc\u00cd\7q\2\2"+
		"\u00cd\u00ce\7p\2\2\u00ce\26\3\2\2\2\u00cf\u00d0\7n\2\2\u00d0\u00d1\7"+
		"q\2\2\u00d1\u00d2\7e\2\2\u00d2\u00d3\7c\2\2\u00d3\u00d4\7n\2\2\u00d4\30"+
		"\3\2\2\2\u00d5\u00d6\7k\2\2\u00d6\u00d7\7h\2\2\u00d7\32\3\2\2\2\u00d8"+
		"\u00d9\7v\2\2\u00d9\u00da\7j\2\2\u00da\u00db\7g\2\2\u00db\u00dc\7p\2\2"+
		"\u00dc\34\3\2\2\2\u00dd\u00de\7g\2\2\u00de\u00df\7n\2\2\u00df\u00e0\7"+
		"u\2\2\u00e0\u00e1\7g\2\2\u00e1\u00e2\7k\2\2\u00e2\u00e3\7h\2\2\u00e3\36"+
		"\3\2\2\2\u00e4\u00e5\7g\2\2\u00e5\u00e6\7n\2\2\u00e6\u00e7\7u\2\2\u00e7"+
		"\u00e8\7g\2\2\u00e8 \3\2\2\2\u00e9\u00ea\7t\2\2\u00ea\u00eb\7g\2\2\u00eb"+
		"\u00ec\7v\2\2\u00ec\u00ed\7w\2\2\u00ed\u00ee\7t\2\2\u00ee\u00ef\7p\2\2"+
		"\u00ef\"\3\2\2\2\u00f0\u00f1\7<\2\2\u00f1$\3\2\2\2\u00f2\u00f3\7<\2\2"+
		"\u00f3\u00f4\7<\2\2\u00f4&\3\2\2\2\u00f5\u00f6\7\60\2\2\u00f6(\3\2\2\2"+
		"\u00f7\u00f8\7.\2\2\u00f8*\3\2\2\2\u00f9\u00fa\7k\2\2\u00fa\u00fb\7p\2"+
		"\2\u00fb,\3\2\2\2\u00fc\u00fd\7*\2\2\u00fd.\3\2\2\2\u00fe\u00ff\7+\2\2"+
		"\u00ff\60\3\2\2\2\u0100\u0101\7]\2\2\u0101\62\3\2\2\2\u0102\u0103\7_\2"+
		"\2\u0103\64\3\2\2\2\u0104\u0105\7}\2\2\u0105\66\3\2\2\2\u0106\u0107\7"+
		"\177\2\2\u01078\3\2\2\2\u0108\u0109\7q\2\2\u0109\u010a\7t\2\2\u010a:\3"+
		"\2\2\2\u010b\u010c\7c\2\2\u010c\u010d\7p\2\2\u010d\u010e\7f\2\2\u010e"+
		"<\3\2\2\2\u010f\u0110\7>\2\2\u0110>\3\2\2\2\u0111\u0112\7@\2\2\u0112@"+
		"\3\2\2\2\u0113\u0114\7>\2\2\u0114\u0115\7?\2\2\u0115B\3\2\2\2\u0116\u0117"+
		"\7@\2\2\u0117\u0118\7?\2\2\u0118D\3\2\2\2\u0119\u011a\7\u0080\2\2\u011a"+
		"\u011b\7?\2\2\u011bF\3\2\2\2\u011c\u011d\7?\2\2\u011d\u011e\7?\2\2\u011e"+
		"H\3\2\2\2\u011f\u0120\7?\2\2\u0120J\3\2\2\2\u0121\u0122\7\60\2\2\u0122"+
		"\u0123\7\60\2\2\u0123L\3\2\2\2\u0124\u0125\7-\2\2\u0125N\3\2\2\2\u0126"+
		"\u0127\7/\2\2\u0127P\3\2\2\2\u0128\u0129\7,\2\2\u0129R\3\2\2\2\u012a\u012b"+
		"\7\61\2\2\u012bT\3\2\2\2\u012c\u012d\7\'\2\2\u012dV\3\2\2\2\u012e\u012f"+
		"\7\61\2\2\u012f\u0130\7\61\2\2\u0130X\3\2\2\2\u0131\u0132\7(\2\2\u0132"+
		"Z\3\2\2\2\u0133\u0134\7~\2\2\u0134\\\3\2\2\2\u0135\u0136\7\u0080\2\2\u0136"+
		"^\3\2\2\2\u0137\u0138\7>\2\2\u0138\u0139\7>\2\2\u0139`\3\2\2\2\u013a\u013b"+
		"\7@\2\2\u013b\u013c\7@\2\2\u013cb\3\2\2\2\u013d\u013e\7p\2\2\u013e\u013f"+
		"\7q\2\2\u013f\u0140\7v\2\2\u0140d\3\2\2\2\u0141\u0142\7%\2\2\u0142f\3"+
		"\2\2\2\u0143\u0144\7`\2\2\u0144h\3\2\2\2\u0145\u0146\7p\2\2\u0146\u0147"+
		"\7k\2\2\u0147\u0148\7n\2\2\u0148j\3\2\2\2\u0149\u014a\7h\2\2\u014a\u014b"+
		"\7c\2\2\u014b\u014c\7n\2\2\u014c\u014d\7u\2\2\u014d\u014e\7g\2\2\u014e"+
		"l\3\2\2\2\u014f\u0150\7v\2\2\u0150\u0151\7t\2\2\u0151\u0152\7w\2\2\u0152"+
		"\u0153\7g\2\2\u0153n\3\2\2\2\u0154\u0155\7\60\2\2\u0155\u0156\7\60\2\2"+
		"\u0156\u0157\7\60\2\2\u0157p\3\2\2\2\u0158\u015c\t\2\2\2\u0159\u015b\t"+
		"\3\2\2\u015a\u0159\3\2\2\2\u015b\u015e\3\2\2\2\u015c\u015a\3\2\2\2\u015c"+
		"\u015d\3\2\2\2\u015dr\3\2\2\2\u015e\u015c\3\2\2\2\u015f\u0164\7$\2\2\u0160"+
		"\u0163\5\u0087D\2\u0161\u0163\n\4\2\2\u0162\u0160\3\2\2\2\u0162\u0161"+
		"\3\2\2\2\u0163\u0166\3\2\2\2\u0164\u0162\3\2\2\2\u0164\u0165\3\2\2\2\u0165"+
		"\u0167\3\2\2\2\u0166\u0164\3\2\2\2\u0167\u0168\7$\2\2\u0168t\3\2\2\2\u0169"+
		"\u016e\7)\2\2\u016a\u016d\5\u0087D\2\u016b\u016d\n\5\2\2\u016c\u016a\3"+
		"\2\2\2\u016c\u016b\3\2\2\2\u016d\u0170\3\2\2\2\u016e\u016c\3\2\2\2\u016e"+
		"\u016f\3\2\2\2\u016f\u0171\3\2\2\2\u0170\u016e\3\2\2\2\u0171\u0172\7)"+
		"\2\2\u0172v\3\2\2\2\u0173\u0174\7]\2\2\u0174\u0175\5y=\2\u0175\u0176\7"+
		"_\2\2\u0176x\3\2\2\2\u0177\u0178\7?\2\2\u0178\u0179\5y=\2\u0179\u017a"+
		"\7?\2\2\u017a\u0184\3\2\2\2\u017b\u017f\7]\2\2\u017c\u017e\13\2\2\2\u017d"+
		"\u017c\3\2\2\2\u017e\u0181\3\2\2\2\u017f\u0180\3\2\2\2\u017f\u017d\3\2"+
		"\2\2\u0180\u0182\3\2\2\2\u0181\u017f\3\2\2\2\u0182\u0184\7_\2\2\u0183"+
		"\u0177\3\2\2\2\u0183\u017b\3\2\2\2\u0184z\3\2\2\2\u0185\u0187\5\u008f"+
		"H\2\u0186\u0185\3\2\2\2\u0187\u0188\3\2\2\2\u0188\u0186\3\2\2\2\u0188"+
		"\u0189\3\2\2\2\u0189|\3\2\2\2\u018a\u018b\7\62\2\2\u018b\u018d\t\6\2\2"+
		"\u018c\u018e\5\u0091I\2\u018d\u018c\3\2\2\2\u018e\u018f\3\2\2\2\u018f"+
		"\u018d\3\2\2\2\u018f\u0190\3\2\2\2\u0190~\3\2\2\2\u0191\u0193\5\u008f"+
		"H\2\u0192\u0191\3\2\2\2\u0193\u0194\3\2\2\2\u0194\u0192\3\2\2\2\u0194"+
		"\u0195\3\2\2\2\u0195\u0196\3\2\2\2\u0196\u019a\7\60\2\2\u0197\u0199\5"+
		"\u008fH\2\u0198\u0197\3\2\2\2\u0199\u019c\3\2\2\2\u019a\u0198\3\2\2\2"+
		"\u019a\u019b\3\2\2\2\u019b\u019e\3\2\2\2\u019c\u019a\3\2\2\2\u019d\u019f"+
		"\5\u0083B\2\u019e\u019d\3\2\2\2\u019e\u019f\3\2\2\2\u019f\u01b1\3\2\2"+
		"\2\u01a0\u01a2\7\60\2\2\u01a1\u01a3\5\u008fH\2\u01a2\u01a1\3\2\2\2\u01a3"+
		"\u01a4\3\2\2\2\u01a4\u01a2\3\2\2\2\u01a4\u01a5\3\2\2\2\u01a5\u01a7\3\2"+
		"\2\2\u01a6\u01a8\5\u0083B\2\u01a7\u01a6\3\2\2\2\u01a7\u01a8\3\2\2\2\u01a8"+
		"\u01b1\3\2\2\2\u01a9\u01ab\5\u008fH\2\u01aa\u01a9\3\2\2\2\u01ab\u01ac"+
		"\3\2\2\2\u01ac\u01aa\3\2\2\2\u01ac\u01ad\3\2\2\2\u01ad\u01ae\3\2\2\2\u01ae"+
		"\u01af\5\u0083B\2\u01af\u01b1\3\2\2\2\u01b0\u0192\3\2\2\2\u01b0\u01a0"+
		"\3\2\2\2\u01b0\u01aa\3\2\2\2\u01b1\u0080\3\2\2\2\u01b2\u01b3\7\62\2\2"+
		"\u01b3\u01b5\t\6\2\2\u01b4\u01b6\5\u0091I\2\u01b5\u01b4\3\2\2\2\u01b6"+
		"\u01b7\3\2\2\2\u01b7\u01b5\3\2\2\2\u01b7\u01b8\3\2\2\2\u01b8\u01b9\3\2"+
		"\2\2\u01b9\u01bd\7\60\2\2\u01ba\u01bc\5\u0091I\2\u01bb\u01ba\3\2\2\2\u01bc"+
		"\u01bf\3\2\2\2\u01bd\u01bb\3\2\2\2\u01bd\u01be\3\2\2\2\u01be\u01c1\3\2"+
		"\2\2\u01bf\u01bd\3\2\2\2\u01c0\u01c2\5\u0085C\2\u01c1\u01c0\3\2\2\2\u01c1"+
		"\u01c2\3\2\2\2\u01c2\u01d8\3\2\2\2\u01c3\u01c4\7\62\2\2\u01c4\u01c5\t"+
		"\6\2\2\u01c5\u01c7\7\60\2\2\u01c6\u01c8\5\u0091I\2\u01c7\u01c6\3\2\2\2"+
		"\u01c8\u01c9\3\2\2\2\u01c9\u01c7\3\2\2\2\u01c9\u01ca\3\2\2\2\u01ca\u01cc"+
		"\3\2\2\2\u01cb\u01cd\5\u0085C\2\u01cc\u01cb\3\2\2\2\u01cc\u01cd\3\2\2"+
		"\2\u01cd\u01d8\3\2\2\2\u01ce\u01cf\7\62\2\2\u01cf\u01d1\t\6\2\2\u01d0"+
		"\u01d2\5\u0091I\2\u01d1\u01d0\3\2\2\2\u01d2\u01d3\3\2\2\2\u01d3\u01d1"+
		"\3\2\2\2\u01d3\u01d4\3\2\2\2\u01d4\u01d5\3\2\2\2\u01d5\u01d6\5\u0085C"+
		"\2\u01d6\u01d8\3\2\2\2\u01d7\u01b2\3\2\2\2\u01d7\u01c3\3\2\2\2\u01d7\u01ce"+
		"\3\2\2\2\u01d8\u0082\3\2\2\2\u01d9\u01db\t\7\2\2\u01da\u01dc\t\b\2\2\u01db"+
		"\u01da\3\2\2\2\u01db\u01dc\3\2\2\2\u01dc\u01de\3\2\2\2\u01dd\u01df\5\u008f"+
		"H\2\u01de\u01dd\3\2\2\2\u01df\u01e0\3\2\2\2\u01e0\u01de\3\2\2\2\u01e0"+
		"\u01e1\3\2\2\2\u01e1\u0084\3\2\2\2\u01e2\u01e4\t\t\2\2\u01e3\u01e5\t\b"+
		"\2\2\u01e4\u01e3\3\2\2\2\u01e4\u01e5\3\2\2\2\u01e5\u01e7\3\2\2\2\u01e6"+
		"\u01e8\5\u008fH\2\u01e7\u01e6\3\2\2\2\u01e8\u01e9\3\2\2\2\u01e9\u01e7"+
		"\3\2\2\2\u01e9\u01ea\3\2\2\2\u01ea\u0086\3\2\2\2\u01eb\u01ec\7^\2\2\u01ec"+
		"\u01f6\t\n\2\2\u01ed\u01ef\7^\2\2\u01ee\u01f0\7\17\2\2\u01ef\u01ee\3\2"+
		"\2\2\u01ef\u01f0\3\2\2\2\u01f0\u01f1\3\2\2\2\u01f1\u01f6\7\f\2\2\u01f2"+
		"\u01f6\5\u0089E\2\u01f3\u01f6\5\u008bF\2\u01f4\u01f6\5\u008dG\2\u01f5"+
		"\u01eb\3\2\2\2\u01f5\u01ed\3\2\2\2\u01f5\u01f2\3\2\2\2\u01f5\u01f3\3\2"+
		"\2\2\u01f5\u01f4\3\2\2\2\u01f6\u0088\3\2\2\2\u01f7\u01f8\7^\2\2\u01f8"+
		"\u0203\5\u008fH\2\u01f9\u01fa\7^\2\2\u01fa\u01fb\5\u008fH\2\u01fb\u01fc"+
		"\5\u008fH\2\u01fc\u0203\3\2\2\2\u01fd\u01fe\7^\2\2\u01fe\u01ff\t\13\2"+
		"\2\u01ff\u0200\5\u008fH\2\u0200\u0201\5\u008fH\2\u0201\u0203\3\2\2\2\u0202"+
		"\u01f7\3\2\2\2\u0202\u01f9\3\2\2\2\u0202\u01fd\3\2\2\2\u0203\u008a\3\2"+
		"\2\2\u0204\u0205\7^\2\2\u0205\u0206\7z\2\2\u0206\u0207\5\u0091I\2\u0207"+
		"\u0208\5\u0091I\2\u0208\u008c\3\2\2\2\u0209\u020a\7^\2\2\u020a\u020b\7"+
		"w\2\2\u020b\u020c\7}\2\2\u020c\u020e\3\2\2\2\u020d\u020f\5\u0091I\2\u020e"+
		"\u020d\3\2\2\2\u020f\u0210\3\2\2\2\u0210\u020e\3\2\2\2\u0210\u0211\3\2"+
		"\2\2\u0211\u0212\3\2\2\2\u0212\u0213\7\177\2\2\u0213\u008e\3\2\2\2\u0214"+
		"\u0215\t\f\2\2\u0215\u0090\3\2\2\2\u0216\u0217\t\r\2\2\u0217\u0092\3\2"+
		"\2\2\u0218\u0219\7/\2\2\u0219\u021a\7/\2\2\u021a\u021b\7]\2\2\u021b\u021c"+
		"\3\2\2\2\u021c\u021d\5y=\2\u021d\u021e\7_\2\2\u021e\u021f\3\2\2\2\u021f"+
		"\u0220\bJ\2\2\u0220\u0094\3\2\2\2\u0221\u0222\7/\2\2\u0222\u0223\7/\2"+
		"\2\u0223\u0241\3\2\2\2\u0224\u0242\3\2\2\2\u0225\u0229\7]\2\2\u0226\u0228"+
		"\7?\2\2\u0227\u0226\3\2\2\2\u0228\u022b\3\2\2\2\u0229\u0227\3\2\2\2\u0229"+
		"\u022a\3\2\2\2\u022a\u0242\3\2\2\2\u022b\u0229\3\2\2\2\u022c\u0230\7]"+
		"\2\2\u022d\u022f\7?\2\2\u022e\u022d\3\2\2\2\u022f\u0232\3\2\2\2\u0230"+
		"\u022e\3\2\2\2\u0230\u0231\3\2\2\2\u0231\u0233\3\2\2\2\u0232\u0230\3\2"+
		"\2\2\u0233\u0237\n\16\2\2\u0234\u0236\n\17\2\2\u0235\u0234\3\2\2\2\u0236"+
		"\u0239\3\2\2\2\u0237\u0235\3\2\2\2\u0237\u0238\3\2\2\2\u0238\u0242\3\2"+
		"\2\2\u0239\u0237\3\2\2\2\u023a\u023e\n\20\2\2\u023b\u023d\n\17\2\2\u023c"+
		"\u023b\3\2\2\2\u023d\u0240\3\2\2\2\u023e\u023c\3\2\2\2\u023e\u023f\3\2"+
		"\2\2\u023f\u0242\3\2\2\2\u0240\u023e\3\2\2\2\u0241\u0224\3\2\2\2\u0241"+
		"\u0225\3\2\2\2\u0241\u022c\3\2\2\2\u0241\u023a\3\2\2\2\u0242\u0246\3\2"+
		"\2\2\u0243\u0244\7\17\2\2\u0244\u0247\7\f\2\2\u0245\u0247\t\21\2\2\u0246"+
		"\u0243\3\2\2\2\u0246\u0245\3\2\2\2\u0247\u0248\3\2\2\2\u0248\u0249\bK"+
		"\2\2\u0249\u0096\3\2\2\2\u024a\u024c\t\22\2\2\u024b\u024a\3\2\2\2\u024c"+
		"\u024d\3\2\2\2\u024d\u024b\3\2\2\2\u024d\u024e\3\2\2\2\u024e\u024f\3\2"+
		"\2\2\u024f\u0250\bL\3\2\u0250\u0098\3\2\2\2\u0251\u0252\7%\2\2\u0252\u0256"+
		"\7#\2\2\u0253\u0255\n\17\2\2\u0254\u0253\3\2\2\2\u0255\u0258\3\2\2\2\u0256"+
		"\u0254\3\2\2\2\u0256\u0257\3\2\2\2\u0257\u0259\3\2\2\2\u0258\u0256\3\2"+
		"\2\2\u0259\u025a\bM\3\2\u025a\u009a\3\2\2\2*\2\u015c\u0162\u0164\u016c"+
		"\u016e\u017f\u0183\u0188\u018f\u0194\u019a\u019e\u01a4\u01a7\u01ac\u01b0"+
		"\u01b7\u01bd\u01c1\u01c9\u01cc\u01d3\u01d7\u01db\u01e0\u01e4\u01e9\u01ef"+
		"\u01f5\u0202\u0210\u0229\u0230\u0237\u023e\u0241\u0246\u024d\u0256\4\2"+
		"\4\2\2\3\2";
	public static final ATN _ATN =
		new ATNDeserializer().deserialize(_serializedATN.toCharArray());
	static {
		_decisionToDFA = new DFA[_ATN.getNumberOfDecisions()];
		for (int i = 0; i < _ATN.getNumberOfDecisions(); i++) {
			_decisionToDFA[i] = new DFA(_ATN.getDecisionState(i), i);
		}
	}
}
