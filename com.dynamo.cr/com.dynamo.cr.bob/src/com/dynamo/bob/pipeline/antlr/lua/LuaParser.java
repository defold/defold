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

// Generated from LuaParser.g4 by ANTLR 4.9.1
import org.antlr.v4.runtime.atn.*;
import org.antlr.v4.runtime.dfa.DFA;
import org.antlr.v4.runtime.*;
import org.antlr.v4.runtime.misc.*;
import org.antlr.v4.runtime.tree.*;
import java.util.List;
import java.util.Iterator;
import java.util.ArrayList;

@SuppressWarnings({"all", "warnings", "unchecked", "unused", "cast"})
public class LuaParser extends Parser {
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
		RULE_chunk = 0, RULE_block = 1, RULE_stat = 2, RULE_breakstat = 3, RULE_gotostat = 4,
		RULE_dostat = 5, RULE_whilestat = 6, RULE_repeatstat = 7, RULE_ifstat = 8,
		RULE_genericforstat = 9, RULE_numericforstat = 10, RULE_functionstat = 11,
		RULE_localfunctionstat = 12, RULE_localvariablestat = 13, RULE_variablestat = 14,
		RULE_attnamelist = 15, RULE_attrib = 16, RULE_retstat = 17, RULE_label = 18,
		RULE_funcname = 19, RULE_variablelist = 20, RULE_namelist = 21, RULE_explist = 22,
		RULE_exp = 23, RULE_variable = 24, RULE_nameAndArgs = 25, RULE_args = 26,
		RULE_functiondef = 27, RULE_funcbody = 28, RULE_parlist = 29, RULE_tableconstructor = 30,
		RULE_fieldlist = 31, RULE_field = 32, RULE_fieldsep = 33, RULE_operatorOr = 34,
		RULE_operatorAnd = 35, RULE_operatorComparison = 36, RULE_operatorStrcat = 37,
		RULE_operatorAddSub = 38, RULE_operatorMulDivMod = 39, RULE_operatorBitwise = 40,
		RULE_operatorUnary = 41, RULE_operatorPower = 42, RULE_number = 43, RULE_lstring = 44;
	private static String[] makeRuleNames() {
		return new String[] {
			"chunk", "block", "stat", "breakstat", "gotostat", "dostat", "whilestat",
			"repeatstat", "ifstat", "genericforstat", "numericforstat", "functionstat",
			"localfunctionstat", "localvariablestat", "variablestat", "attnamelist",
			"attrib", "retstat", "label", "funcname", "variablelist", "namelist",
			"explist", "exp", "variable", "nameAndArgs", "args", "functiondef", "funcbody",
			"parlist", "tableconstructor", "fieldlist", "field", "fieldsep", "operatorOr",
			"operatorAnd", "operatorComparison", "operatorStrcat", "operatorAddSub",
			"operatorMulDivMod", "operatorBitwise", "operatorUnary", "operatorPower",
			"number", "lstring"
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

	@Override
	public String getGrammarFileName() { return "LuaParser.g4"; }

	@Override
	public String[] getRuleNames() { return ruleNames; }

	@Override
	public String getSerializedATN() { return _serializedATN; }

	@Override
	public ATN getATN() { return _ATN; }

	public LuaParser(TokenStream input) {
		super(input);
		_interp = new ParserATNSimulator(this,_ATN,_decisionToDFA,_sharedContextCache);
	}

	public static class ChunkContext extends ParserRuleContext {
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode EOF() { return getToken(LuaParser.EOF, 0); }
		public ChunkContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_chunk; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterChunk(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitChunk(this);
		}
	}

	public final ChunkContext chunk() throws RecognitionException {
		ChunkContext _localctx = new ChunkContext(_ctx, getState());
		enterRule(_localctx, 0, RULE_chunk);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(90);
			block();
			setState(91);
			match(EOF);
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

	public static class BlockContext extends ParserRuleContext {
		public List<StatContext> stat() {
			return getRuleContexts(StatContext.class);
		}
		public StatContext stat(int i) {
			return getRuleContext(StatContext.class,i);
		}
		public RetstatContext retstat() {
			return getRuleContext(RetstatContext.class,0);
		}
		public BlockContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_block; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterBlock(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitBlock(this);
		}
	}

	public final BlockContext block() throws RecognitionException {
		BlockContext _localctx = new BlockContext(_ctx, getState());
		enterRule(_localctx, 2, RULE_block);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(96);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << SEMICOLON) | (1L << BREAK) | (1L << GOTO) | (1L << DO) | (1L << WHILE) | (1L << REPEAT) | (1L << FOR) | (1L << FUNCTION) | (1L << LOCAL) | (1L << IF) | (1L << DCOLON) | (1L << LPAREN) | (1L << NAME))) != 0)) {
				{
				{
				setState(93);
				stat();
				}
				}
				setState(98);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(100);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==RETURN) {
				{
				setState(99);
				retstat();
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

	public static class StatContext extends ParserRuleContext {
		public TerminalNode SEMICOLON() { return getToken(LuaParser.SEMICOLON, 0); }
		public VariablestatContext variablestat() {
			return getRuleContext(VariablestatContext.class,0);
		}
		public VariableContext variable() {
			return getRuleContext(VariableContext.class,0);
		}
		public LabelContext label() {
			return getRuleContext(LabelContext.class,0);
		}
		public BreakstatContext breakstat() {
			return getRuleContext(BreakstatContext.class,0);
		}
		public GotostatContext gotostat() {
			return getRuleContext(GotostatContext.class,0);
		}
		public DostatContext dostat() {
			return getRuleContext(DostatContext.class,0);
		}
		public WhilestatContext whilestat() {
			return getRuleContext(WhilestatContext.class,0);
		}
		public RepeatstatContext repeatstat() {
			return getRuleContext(RepeatstatContext.class,0);
		}
		public IfstatContext ifstat() {
			return getRuleContext(IfstatContext.class,0);
		}
		public NumericforstatContext numericforstat() {
			return getRuleContext(NumericforstatContext.class,0);
		}
		public GenericforstatContext genericforstat() {
			return getRuleContext(GenericforstatContext.class,0);
		}
		public FunctionstatContext functionstat() {
			return getRuleContext(FunctionstatContext.class,0);
		}
		public LocalfunctionstatContext localfunctionstat() {
			return getRuleContext(LocalfunctionstatContext.class,0);
		}
		public LocalvariablestatContext localvariablestat() {
			return getRuleContext(LocalvariablestatContext.class,0);
		}
		public StatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_stat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterStat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitStat(this);
		}
	}

	public final StatContext stat() throws RecognitionException {
		StatContext _localctx = new StatContext(_ctx, getState());
		enterRule(_localctx, 4, RULE_stat);
		try {
			setState(117);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,2,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(102);
				match(SEMICOLON);
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(103);
				variablestat();
				}
				break;
			case 3:
				enterOuterAlt(_localctx, 3);
				{
				setState(104);
				variable(0);
				}
				break;
			case 4:
				enterOuterAlt(_localctx, 4);
				{
				setState(105);
				label();
				}
				break;
			case 5:
				enterOuterAlt(_localctx, 5);
				{
				setState(106);
				breakstat();
				}
				break;
			case 6:
				enterOuterAlt(_localctx, 6);
				{
				setState(107);
				gotostat();
				}
				break;
			case 7:
				enterOuterAlt(_localctx, 7);
				{
				setState(108);
				dostat();
				}
				break;
			case 8:
				enterOuterAlt(_localctx, 8);
				{
				setState(109);
				whilestat();
				}
				break;
			case 9:
				enterOuterAlt(_localctx, 9);
				{
				setState(110);
				repeatstat();
				}
				break;
			case 10:
				enterOuterAlt(_localctx, 10);
				{
				setState(111);
				ifstat();
				}
				break;
			case 11:
				enterOuterAlt(_localctx, 11);
				{
				setState(112);
				numericforstat();
				}
				break;
			case 12:
				enterOuterAlt(_localctx, 12);
				{
				setState(113);
				genericforstat();
				}
				break;
			case 13:
				enterOuterAlt(_localctx, 13);
				{
				setState(114);
				functionstat();
				}
				break;
			case 14:
				enterOuterAlt(_localctx, 14);
				{
				setState(115);
				localfunctionstat();
				}
				break;
			case 15:
				enterOuterAlt(_localctx, 15);
				{
				setState(116);
				localvariablestat();
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

	public static class BreakstatContext extends ParserRuleContext {
		public TerminalNode BREAK() { return getToken(LuaParser.BREAK, 0); }
		public BreakstatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_breakstat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterBreakstat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitBreakstat(this);
		}
	}

	public final BreakstatContext breakstat() throws RecognitionException {
		BreakstatContext _localctx = new BreakstatContext(_ctx, getState());
		enterRule(_localctx, 6, RULE_breakstat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(119);
			match(BREAK);
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

	public static class GotostatContext extends ParserRuleContext {
		public TerminalNode GOTO() { return getToken(LuaParser.GOTO, 0); }
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public GotostatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_gotostat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterGotostat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitGotostat(this);
		}
	}

	public final GotostatContext gotostat() throws RecognitionException {
		GotostatContext _localctx = new GotostatContext(_ctx, getState());
		enterRule(_localctx, 8, RULE_gotostat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(121);
			match(GOTO);
			setState(122);
			match(NAME);
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

	public static class DostatContext extends ParserRuleContext {
		public TerminalNode DO() { return getToken(LuaParser.DO, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode END() { return getToken(LuaParser.END, 0); }
		public DostatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_dostat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterDostat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitDostat(this);
		}
	}

	public final DostatContext dostat() throws RecognitionException {
		DostatContext _localctx = new DostatContext(_ctx, getState());
		enterRule(_localctx, 10, RULE_dostat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(124);
			match(DO);
			setState(125);
			block();
			setState(126);
			match(END);
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

	public static class WhilestatContext extends ParserRuleContext {
		public TerminalNode WHILE() { return getToken(LuaParser.WHILE, 0); }
		public ExpContext exp() {
			return getRuleContext(ExpContext.class,0);
		}
		public TerminalNode DO() { return getToken(LuaParser.DO, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode END() { return getToken(LuaParser.END, 0); }
		public WhilestatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_whilestat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterWhilestat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitWhilestat(this);
		}
	}

	public final WhilestatContext whilestat() throws RecognitionException {
		WhilestatContext _localctx = new WhilestatContext(_ctx, getState());
		enterRule(_localctx, 12, RULE_whilestat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(128);
			match(WHILE);
			setState(129);
			exp(0);
			setState(130);
			match(DO);
			setState(131);
			block();
			setState(132);
			match(END);
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

	public static class RepeatstatContext extends ParserRuleContext {
		public TerminalNode REPEAT() { return getToken(LuaParser.REPEAT, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode UNTIL() { return getToken(LuaParser.UNTIL, 0); }
		public ExpContext exp() {
			return getRuleContext(ExpContext.class,0);
		}
		public RepeatstatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_repeatstat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterRepeatstat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitRepeatstat(this);
		}
	}

	public final RepeatstatContext repeatstat() throws RecognitionException {
		RepeatstatContext _localctx = new RepeatstatContext(_ctx, getState());
		enterRule(_localctx, 14, RULE_repeatstat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(134);
			match(REPEAT);
			setState(135);
			block();
			setState(136);
			match(UNTIL);
			setState(137);
			exp(0);
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

	public static class IfstatContext extends ParserRuleContext {
		public TerminalNode IF() { return getToken(LuaParser.IF, 0); }
		public List<ExpContext> exp() {
			return getRuleContexts(ExpContext.class);
		}
		public ExpContext exp(int i) {
			return getRuleContext(ExpContext.class,i);
		}
		public List<TerminalNode> THEN() { return getTokens(LuaParser.THEN); }
		public TerminalNode THEN(int i) {
			return getToken(LuaParser.THEN, i);
		}
		public List<BlockContext> block() {
			return getRuleContexts(BlockContext.class);
		}
		public BlockContext block(int i) {
			return getRuleContext(BlockContext.class,i);
		}
		public TerminalNode END() { return getToken(LuaParser.END, 0); }
		public List<TerminalNode> ELSEIF() { return getTokens(LuaParser.ELSEIF); }
		public TerminalNode ELSEIF(int i) {
			return getToken(LuaParser.ELSEIF, i);
		}
		public TerminalNode ELSE() { return getToken(LuaParser.ELSE, 0); }
		public IfstatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_ifstat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterIfstat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitIfstat(this);
		}
	}

	public final IfstatContext ifstat() throws RecognitionException {
		IfstatContext _localctx = new IfstatContext(_ctx, getState());
		enterRule(_localctx, 16, RULE_ifstat);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(139);
			match(IF);
			setState(140);
			exp(0);
			setState(141);
			match(THEN);
			setState(142);
			block();
			setState(150);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==ELSEIF) {
				{
				{
				setState(143);
				match(ELSEIF);
				setState(144);
				exp(0);
				setState(145);
				match(THEN);
				setState(146);
				block();
				}
				}
				setState(152);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(155);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==ELSE) {
				{
				setState(153);
				match(ELSE);
				setState(154);
				block();
				}
			}

			setState(157);
			match(END);
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

	public static class GenericforstatContext extends ParserRuleContext {
		public TerminalNode FOR() { return getToken(LuaParser.FOR, 0); }
		public NamelistContext namelist() {
			return getRuleContext(NamelistContext.class,0);
		}
		public TerminalNode IN() { return getToken(LuaParser.IN, 0); }
		public ExplistContext explist() {
			return getRuleContext(ExplistContext.class,0);
		}
		public TerminalNode DO() { return getToken(LuaParser.DO, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode END() { return getToken(LuaParser.END, 0); }
		public GenericforstatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_genericforstat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterGenericforstat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitGenericforstat(this);
		}
	}

	public final GenericforstatContext genericforstat() throws RecognitionException {
		GenericforstatContext _localctx = new GenericforstatContext(_ctx, getState());
		enterRule(_localctx, 18, RULE_genericforstat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(159);
			match(FOR);
			setState(160);
			namelist();
			setState(161);
			match(IN);
			setState(162);
			explist();
			setState(163);
			match(DO);
			setState(164);
			block();
			setState(165);
			match(END);
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

	public static class NumericforstatContext extends ParserRuleContext {
		public TerminalNode FOR() { return getToken(LuaParser.FOR, 0); }
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public TerminalNode EQUALS() { return getToken(LuaParser.EQUALS, 0); }
		public List<ExpContext> exp() {
			return getRuleContexts(ExpContext.class);
		}
		public ExpContext exp(int i) {
			return getRuleContext(ExpContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuaParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuaParser.COMMA, i);
		}
		public TerminalNode DO() { return getToken(LuaParser.DO, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode END() { return getToken(LuaParser.END, 0); }
		public NumericforstatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_numericforstat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterNumericforstat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitNumericforstat(this);
		}
	}

	public final NumericforstatContext numericforstat() throws RecognitionException {
		NumericforstatContext _localctx = new NumericforstatContext(_ctx, getState());
		enterRule(_localctx, 20, RULE_numericforstat);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(167);
			match(FOR);
			setState(168);
			match(NAME);
			setState(169);
			match(EQUALS);
			setState(170);
			exp(0);
			setState(171);
			match(COMMA);
			setState(172);
			exp(0);
			setState(175);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==COMMA) {
				{
				setState(173);
				match(COMMA);
				setState(174);
				exp(0);
				}
			}

			setState(177);
			match(DO);
			setState(178);
			block();
			setState(179);
			match(END);
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

	public static class FunctionstatContext extends ParserRuleContext {
		public TerminalNode FUNCTION() { return getToken(LuaParser.FUNCTION, 0); }
		public FuncnameContext funcname() {
			return getRuleContext(FuncnameContext.class,0);
		}
		public FuncbodyContext funcbody() {
			return getRuleContext(FuncbodyContext.class,0);
		}
		public FunctionstatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_functionstat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterFunctionstat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitFunctionstat(this);
		}
	}

	public final FunctionstatContext functionstat() throws RecognitionException {
		FunctionstatContext _localctx = new FunctionstatContext(_ctx, getState());
		enterRule(_localctx, 22, RULE_functionstat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(181);
			match(FUNCTION);
			setState(182);
			funcname();
			setState(183);
			funcbody();
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

	public static class LocalfunctionstatContext extends ParserRuleContext {
		public TerminalNode LOCAL() { return getToken(LuaParser.LOCAL, 0); }
		public TerminalNode FUNCTION() { return getToken(LuaParser.FUNCTION, 0); }
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public FuncbodyContext funcbody() {
			return getRuleContext(FuncbodyContext.class,0);
		}
		public LocalfunctionstatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_localfunctionstat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterLocalfunctionstat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitLocalfunctionstat(this);
		}
	}

	public final LocalfunctionstatContext localfunctionstat() throws RecognitionException {
		LocalfunctionstatContext _localctx = new LocalfunctionstatContext(_ctx, getState());
		enterRule(_localctx, 24, RULE_localfunctionstat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(185);
			match(LOCAL);
			setState(186);
			match(FUNCTION);
			setState(187);
			match(NAME);
			setState(188);
			funcbody();
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

	public static class LocalvariablestatContext extends ParserRuleContext {
		public TerminalNode LOCAL() { return getToken(LuaParser.LOCAL, 0); }
		public AttnamelistContext attnamelist() {
			return getRuleContext(AttnamelistContext.class,0);
		}
		public TerminalNode EQUALS() { return getToken(LuaParser.EQUALS, 0); }
		public ExplistContext explist() {
			return getRuleContext(ExplistContext.class,0);
		}
		public LocalvariablestatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_localvariablestat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterLocalvariablestat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitLocalvariablestat(this);
		}
	}

	public final LocalvariablestatContext localvariablestat() throws RecognitionException {
		LocalvariablestatContext _localctx = new LocalvariablestatContext(_ctx, getState());
		enterRule(_localctx, 26, RULE_localvariablestat);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(190);
			match(LOCAL);
			setState(191);
			attnamelist();
			setState(194);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==EQUALS) {
				{
				setState(192);
				match(EQUALS);
				setState(193);
				explist();
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

	public static class VariablestatContext extends ParserRuleContext {
		public VariablelistContext variablelist() {
			return getRuleContext(VariablelistContext.class,0);
		}
		public TerminalNode EQUALS() { return getToken(LuaParser.EQUALS, 0); }
		public ExplistContext explist() {
			return getRuleContext(ExplistContext.class,0);
		}
		public VariablestatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_variablestat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterVariablestat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitVariablestat(this);
		}
	}

	public final VariablestatContext variablestat() throws RecognitionException {
		VariablestatContext _localctx = new VariablestatContext(_ctx, getState());
		enterRule(_localctx, 28, RULE_variablestat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(196);
			variablelist();
			setState(197);
			match(EQUALS);
			setState(198);
			explist();
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

	public static class AttnamelistContext extends ParserRuleContext {
		public List<TerminalNode> NAME() { return getTokens(LuaParser.NAME); }
		public TerminalNode NAME(int i) {
			return getToken(LuaParser.NAME, i);
		}
		public List<AttribContext> attrib() {
			return getRuleContexts(AttribContext.class);
		}
		public AttribContext attrib(int i) {
			return getRuleContext(AttribContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuaParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuaParser.COMMA, i);
		}
		public AttnamelistContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_attnamelist; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterAttnamelist(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitAttnamelist(this);
		}
	}

	public final AttnamelistContext attnamelist() throws RecognitionException {
		AttnamelistContext _localctx = new AttnamelistContext(_ctx, getState());
		enterRule(_localctx, 30, RULE_attnamelist);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(200);
			match(NAME);
			setState(201);
			attrib();
			setState(207);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==COMMA) {
				{
				{
				setState(202);
				match(COMMA);
				setState(203);
				match(NAME);
				setState(204);
				attrib();
				}
				}
				setState(209);
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

	public static class AttribContext extends ParserRuleContext {
		public TerminalNode LT() { return getToken(LuaParser.LT, 0); }
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public TerminalNode GT() { return getToken(LuaParser.GT, 0); }
		public AttribContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_attrib; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterAttrib(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitAttrib(this);
		}
	}

	public final AttribContext attrib() throws RecognitionException {
		AttribContext _localctx = new AttribContext(_ctx, getState());
		enterRule(_localctx, 32, RULE_attrib);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(213);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==LT) {
				{
				setState(210);
				match(LT);
				setState(211);
				match(NAME);
				setState(212);
				match(GT);
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

	public static class RetstatContext extends ParserRuleContext {
		public TerminalNode RETURN() { return getToken(LuaParser.RETURN, 0); }
		public ExplistContext explist() {
			return getRuleContext(ExplistContext.class,0);
		}
		public TerminalNode SEMICOLON() { return getToken(LuaParser.SEMICOLON, 0); }
		public RetstatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_retstat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterRetstat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitRetstat(this);
		}
	}

	public final RetstatContext retstat() throws RecognitionException {
		RetstatContext _localctx = new RetstatContext(_ctx, getState());
		enterRule(_localctx, 34, RULE_retstat);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(215);
			match(RETURN);
			setState(217);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << FUNCTION) | (1L << LPAREN) | (1L << LBRACE) | (1L << MINUS) | (1L << BITNOT) | (1L << NOT) | (1L << LEN) | (1L << NIL) | (1L << FALSE) | (1L << TRUE) | (1L << DOTS) | (1L << NAME) | (1L << NORMALSTRING) | (1L << CHARSTRING) | (1L << LONGSTRING) | (1L << INT) | (1L << HEX) | (1L << FLOAT) | (1L << HEX_FLOAT))) != 0)) {
				{
				setState(216);
				explist();
				}
			}

			setState(220);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==SEMICOLON) {
				{
				setState(219);
				match(SEMICOLON);
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

	public static class LabelContext extends ParserRuleContext {
		public List<TerminalNode> DCOLON() { return getTokens(LuaParser.DCOLON); }
		public TerminalNode DCOLON(int i) {
			return getToken(LuaParser.DCOLON, i);
		}
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public LabelContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_label; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterLabel(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitLabel(this);
		}
	}

	public final LabelContext label() throws RecognitionException {
		LabelContext _localctx = new LabelContext(_ctx, getState());
		enterRule(_localctx, 36, RULE_label);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(222);
			match(DCOLON);
			setState(223);
			match(NAME);
			setState(224);
			match(DCOLON);
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

	public static class FuncnameContext extends ParserRuleContext {
		public List<TerminalNode> NAME() { return getTokens(LuaParser.NAME); }
		public TerminalNode NAME(int i) {
			return getToken(LuaParser.NAME, i);
		}
		public List<TerminalNode> DOT() { return getTokens(LuaParser.DOT); }
		public TerminalNode DOT(int i) {
			return getToken(LuaParser.DOT, i);
		}
		public TerminalNode COLON() { return getToken(LuaParser.COLON, 0); }
		public FuncnameContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_funcname; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterFuncname(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitFuncname(this);
		}
	}

	public final FuncnameContext funcname() throws RecognitionException {
		FuncnameContext _localctx = new FuncnameContext(_ctx, getState());
		enterRule(_localctx, 38, RULE_funcname);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(226);
			match(NAME);
			setState(231);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==DOT) {
				{
				{
				setState(227);
				match(DOT);
				setState(228);
				match(NAME);
				}
				}
				setState(233);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(236);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==COLON) {
				{
				setState(234);
				match(COLON);
				setState(235);
				match(NAME);
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

	public static class VariablelistContext extends ParserRuleContext {
		public List<VariableContext> variable() {
			return getRuleContexts(VariableContext.class);
		}
		public VariableContext variable(int i) {
			return getRuleContext(VariableContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuaParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuaParser.COMMA, i);
		}
		public VariablelistContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_variablelist; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterVariablelist(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitVariablelist(this);
		}
	}

	public final VariablelistContext variablelist() throws RecognitionException {
		VariablelistContext _localctx = new VariablelistContext(_ctx, getState());
		enterRule(_localctx, 40, RULE_variablelist);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(238);
			variable(0);
			setState(243);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==COMMA) {
				{
				{
				setState(239);
				match(COMMA);
				setState(240);
				variable(0);
				}
				}
				setState(245);
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

	public static class NamelistContext extends ParserRuleContext {
		public List<TerminalNode> NAME() { return getTokens(LuaParser.NAME); }
		public TerminalNode NAME(int i) {
			return getToken(LuaParser.NAME, i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuaParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuaParser.COMMA, i);
		}
		public NamelistContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_namelist; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterNamelist(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitNamelist(this);
		}
	}

	public final NamelistContext namelist() throws RecognitionException {
		NamelistContext _localctx = new NamelistContext(_ctx, getState());
		enterRule(_localctx, 42, RULE_namelist);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(246);
			match(NAME);
			setState(251);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,14,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(247);
					match(COMMA);
					setState(248);
					match(NAME);
					}
					}
				}
				setState(253);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,14,_ctx);
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

	public static class ExplistContext extends ParserRuleContext {
		public List<ExpContext> exp() {
			return getRuleContexts(ExpContext.class);
		}
		public ExpContext exp(int i) {
			return getRuleContext(ExpContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuaParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuaParser.COMMA, i);
		}
		public ExplistContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_explist; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterExplist(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitExplist(this);
		}
	}

	public final ExplistContext explist() throws RecognitionException {
		ExplistContext _localctx = new ExplistContext(_ctx, getState());
		enterRule(_localctx, 44, RULE_explist);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(254);
			exp(0);
			setState(259);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==COMMA) {
				{
				{
				setState(255);
				match(COMMA);
				setState(256);
				exp(0);
				}
				}
				setState(261);
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

	public static class ExpContext extends ParserRuleContext {
		public TerminalNode NIL() { return getToken(LuaParser.NIL, 0); }
		public TerminalNode FALSE() { return getToken(LuaParser.FALSE, 0); }
		public TerminalNode TRUE() { return getToken(LuaParser.TRUE, 0); }
		public NumberContext number() {
			return getRuleContext(NumberContext.class,0);
		}
		public LstringContext lstring() {
			return getRuleContext(LstringContext.class,0);
		}
		public TerminalNode DOTS() { return getToken(LuaParser.DOTS, 0); }
		public FunctiondefContext functiondef() {
			return getRuleContext(FunctiondefContext.class,0);
		}
		public VariableContext variable() {
			return getRuleContext(VariableContext.class,0);
		}
		public TableconstructorContext tableconstructor() {
			return getRuleContext(TableconstructorContext.class,0);
		}
		public OperatorUnaryContext operatorUnary() {
			return getRuleContext(OperatorUnaryContext.class,0);
		}
		public List<ExpContext> exp() {
			return getRuleContexts(ExpContext.class);
		}
		public ExpContext exp(int i) {
			return getRuleContext(ExpContext.class,i);
		}
		public OperatorPowerContext operatorPower() {
			return getRuleContext(OperatorPowerContext.class,0);
		}
		public OperatorMulDivModContext operatorMulDivMod() {
			return getRuleContext(OperatorMulDivModContext.class,0);
		}
		public OperatorAddSubContext operatorAddSub() {
			return getRuleContext(OperatorAddSubContext.class,0);
		}
		public OperatorStrcatContext operatorStrcat() {
			return getRuleContext(OperatorStrcatContext.class,0);
		}
		public OperatorComparisonContext operatorComparison() {
			return getRuleContext(OperatorComparisonContext.class,0);
		}
		public OperatorAndContext operatorAnd() {
			return getRuleContext(OperatorAndContext.class,0);
		}
		public OperatorOrContext operatorOr() {
			return getRuleContext(OperatorOrContext.class,0);
		}
		public OperatorBitwiseContext operatorBitwise() {
			return getRuleContext(OperatorBitwiseContext.class,0);
		}
		public ExpContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_exp; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterExp(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitExp(this);
		}
	}

	public final ExpContext exp() throws RecognitionException {
		return exp(0);
	}

	private ExpContext exp(int _p) throws RecognitionException {
		ParserRuleContext _parentctx = _ctx;
		int _parentState = getState();
		ExpContext _localctx = new ExpContext(_ctx, _parentState);
		ExpContext _prevctx = _localctx;
		int _startState = 46;
		enterRecursionRule(_localctx, 46, RULE_exp, _p);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(275);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case NIL:
				{
				setState(263);
				match(NIL);
				}
				break;
			case FALSE:
				{
				setState(264);
				match(FALSE);
				}
				break;
			case TRUE:
				{
				setState(265);
				match(TRUE);
				}
				break;
			case INT:
			case HEX:
			case FLOAT:
			case HEX_FLOAT:
				{
				setState(266);
				number();
				}
				break;
			case NORMALSTRING:
			case CHARSTRING:
			case LONGSTRING:
				{
				setState(267);
				lstring();
				}
				break;
			case DOTS:
				{
				setState(268);
				match(DOTS);
				}
				break;
			case FUNCTION:
				{
				setState(269);
				functiondef();
				}
				break;
			case LPAREN:
			case NAME:
				{
				setState(270);
				variable(0);
				}
				break;
			case LBRACE:
				{
				setState(271);
				tableconstructor();
				}
				break;
			case MINUS:
			case BITNOT:
			case NOT:
			case LEN:
				{
				setState(272);
				operatorUnary();
				setState(273);
				exp(8);
				}
				break;
			default:
				throw new NoViableAltException(this);
			}
			_ctx.stop = _input.LT(-1);
			setState(311);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,18,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					if ( _parseListeners!=null ) triggerExitRuleEvent();
					_prevctx = _localctx;
					{
					setState(309);
					_errHandler.sync(this);
					switch ( getInterpreter().adaptivePredict(_input,17,_ctx) ) {
					case 1:
						{
						_localctx = new ExpContext(_parentctx, _parentState);
						pushNewRecursionContext(_localctx, _startState, RULE_exp);
						setState(277);
						if (!(precpred(_ctx, 9))) throw new FailedPredicateException(this, "precpred(_ctx, 9)");
						setState(278);
						operatorPower();
						setState(279);
						exp(9);
						}
						break;
					case 2:
						{
						_localctx = new ExpContext(_parentctx, _parentState);
						pushNewRecursionContext(_localctx, _startState, RULE_exp);
						setState(281);
						if (!(precpred(_ctx, 7))) throw new FailedPredicateException(this, "precpred(_ctx, 7)");
						setState(282);
						operatorMulDivMod();
						setState(283);
						exp(8);
						}
						break;
					case 3:
						{
						_localctx = new ExpContext(_parentctx, _parentState);
						pushNewRecursionContext(_localctx, _startState, RULE_exp);
						setState(285);
						if (!(precpred(_ctx, 6))) throw new FailedPredicateException(this, "precpred(_ctx, 6)");
						setState(286);
						operatorAddSub();
						setState(287);
						exp(7);
						}
						break;
					case 4:
						{
						_localctx = new ExpContext(_parentctx, _parentState);
						pushNewRecursionContext(_localctx, _startState, RULE_exp);
						setState(289);
						if (!(precpred(_ctx, 5))) throw new FailedPredicateException(this, "precpred(_ctx, 5)");
						setState(290);
						operatorStrcat();
						setState(291);
						exp(5);
						}
						break;
					case 5:
						{
						_localctx = new ExpContext(_parentctx, _parentState);
						pushNewRecursionContext(_localctx, _startState, RULE_exp);
						setState(293);
						if (!(precpred(_ctx, 4))) throw new FailedPredicateException(this, "precpred(_ctx, 4)");
						setState(294);
						operatorComparison();
						setState(295);
						exp(5);
						}
						break;
					case 6:
						{
						_localctx = new ExpContext(_parentctx, _parentState);
						pushNewRecursionContext(_localctx, _startState, RULE_exp);
						setState(297);
						if (!(precpred(_ctx, 3))) throw new FailedPredicateException(this, "precpred(_ctx, 3)");
						setState(298);
						operatorAnd();
						setState(299);
						exp(4);
						}
						break;
					case 7:
						{
						_localctx = new ExpContext(_parentctx, _parentState);
						pushNewRecursionContext(_localctx, _startState, RULE_exp);
						setState(301);
						if (!(precpred(_ctx, 2))) throw new FailedPredicateException(this, "precpred(_ctx, 2)");
						setState(302);
						operatorOr();
						setState(303);
						exp(3);
						}
						break;
					case 8:
						{
						_localctx = new ExpContext(_parentctx, _parentState);
						pushNewRecursionContext(_localctx, _startState, RULE_exp);
						setState(305);
						if (!(precpred(_ctx, 1))) throw new FailedPredicateException(this, "precpred(_ctx, 1)");
						setState(306);
						operatorBitwise();
						setState(307);
						exp(2);
						}
						break;
					}
					}
				}
				setState(313);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,18,_ctx);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			unrollRecursionContexts(_parentctx);
		}
		return _localctx;
	}

	public static class VariableContext extends ParserRuleContext {
		public VariableContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_variable; }

		public VariableContext() { }
		public void copyFrom(VariableContext ctx) {
			super.copyFrom(ctx);
		}
	}
	public static class NamedvariableContext extends VariableContext {
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public NamedvariableContext(VariableContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterNamedvariable(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitNamedvariable(this);
		}
	}
	public static class ParenthesesvariableContext extends VariableContext {
		public TerminalNode LPAREN() { return getToken(LuaParser.LPAREN, 0); }
		public ExpContext exp() {
			return getRuleContext(ExpContext.class,0);
		}
		public TerminalNode RPAREN() { return getToken(LuaParser.RPAREN, 0); }
		public ParenthesesvariableContext(VariableContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterParenthesesvariable(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitParenthesesvariable(this);
		}
	}
	public static class FunctioncallContext extends VariableContext {
		public VariableContext variable() {
			return getRuleContext(VariableContext.class,0);
		}
		public NameAndArgsContext nameAndArgs() {
			return getRuleContext(NameAndArgsContext.class,0);
		}
		public FunctioncallContext(VariableContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterFunctioncall(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitFunctioncall(this);
		}
	}
	public static class IndexContext extends VariableContext {
		public VariableContext variable() {
			return getRuleContext(VariableContext.class,0);
		}
		public TerminalNode LBRACK() { return getToken(LuaParser.LBRACK, 0); }
		public ExpContext exp() {
			return getRuleContext(ExpContext.class,0);
		}
		public TerminalNode RBRACK() { return getToken(LuaParser.RBRACK, 0); }
		public TerminalNode DOT() { return getToken(LuaParser.DOT, 0); }
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public IndexContext(VariableContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterIndex(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitIndex(this);
		}
	}

	public final VariableContext variable() throws RecognitionException {
		return variable(0);
	}

	private VariableContext variable(int _p) throws RecognitionException {
		ParserRuleContext _parentctx = _ctx;
		int _parentState = getState();
		VariableContext _localctx = new VariableContext(_ctx, _parentState);
		VariableContext _prevctx = _localctx;
		int _startState = 48;
		enterRecursionRule(_localctx, 48, RULE_variable, _p);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(320);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case NAME:
				{
				_localctx = new NamedvariableContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;

				setState(315);
				match(NAME);
				}
				break;
			case LPAREN:
				{
				_localctx = new ParenthesesvariableContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(316);
				match(LPAREN);
				setState(317);
				exp(0);
				setState(318);
				match(RPAREN);
				}
				break;
			default:
				throw new NoViableAltException(this);
			}
			_ctx.stop = _input.LT(-1);
			setState(335);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,22,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					if ( _parseListeners!=null ) triggerExitRuleEvent();
					_prevctx = _localctx;
					{
					setState(333);
					_errHandler.sync(this);
					switch ( getInterpreter().adaptivePredict(_input,21,_ctx) ) {
					case 1:
						{
						_localctx = new IndexContext(new VariableContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_variable);
						setState(322);
						if (!(precpred(_ctx, 3))) throw new FailedPredicateException(this, "precpred(_ctx, 3)");
						setState(329);
						_errHandler.sync(this);
						switch (_input.LA(1)) {
						case LBRACK:
							{
							setState(323);
							match(LBRACK);
							setState(324);
							exp(0);
							setState(325);
							match(RBRACK);
							}
							break;
						case DOT:
							{
							setState(327);
							match(DOT);
							setState(328);
							match(NAME);
							}
							break;
						default:
							throw new NoViableAltException(this);
						}
						}
						break;
					case 2:
						{
						_localctx = new FunctioncallContext(new VariableContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_variable);
						setState(331);
						if (!(precpred(_ctx, 1))) throw new FailedPredicateException(this, "precpred(_ctx, 1)");
						setState(332);
						nameAndArgs();
						}
						break;
					}
					}
				}
				setState(337);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,22,_ctx);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			unrollRecursionContexts(_parentctx);
		}
		return _localctx;
	}

	public static class NameAndArgsContext extends ParserRuleContext {
		public ArgsContext args() {
			return getRuleContext(ArgsContext.class,0);
		}
		public TerminalNode COLON() { return getToken(LuaParser.COLON, 0); }
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public NameAndArgsContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_nameAndArgs; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterNameAndArgs(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitNameAndArgs(this);
		}
	}

	public final NameAndArgsContext nameAndArgs() throws RecognitionException {
		NameAndArgsContext _localctx = new NameAndArgsContext(_ctx, getState());
		enterRule(_localctx, 50, RULE_nameAndArgs);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(340);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==COLON) {
				{
				setState(338);
				match(COLON);
				setState(339);
				match(NAME);
				}
			}

			setState(342);
			args();
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

	public static class ArgsContext extends ParserRuleContext {
		public TerminalNode LPAREN() { return getToken(LuaParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuaParser.RPAREN, 0); }
		public ExplistContext explist() {
			return getRuleContext(ExplistContext.class,0);
		}
		public TableconstructorContext tableconstructor() {
			return getRuleContext(TableconstructorContext.class,0);
		}
		public LstringContext lstring() {
			return getRuleContext(LstringContext.class,0);
		}
		public ArgsContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_args; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterArgs(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitArgs(this);
		}
	}

	public final ArgsContext args() throws RecognitionException {
		ArgsContext _localctx = new ArgsContext(_ctx, getState());
		enterRule(_localctx, 52, RULE_args);
		int _la;
		try {
			setState(351);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case LPAREN:
				enterOuterAlt(_localctx, 1);
				{
				setState(344);
				match(LPAREN);
				setState(346);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if ((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << FUNCTION) | (1L << LPAREN) | (1L << LBRACE) | (1L << MINUS) | (1L << BITNOT) | (1L << NOT) | (1L << LEN) | (1L << NIL) | (1L << FALSE) | (1L << TRUE) | (1L << DOTS) | (1L << NAME) | (1L << NORMALSTRING) | (1L << CHARSTRING) | (1L << LONGSTRING) | (1L << INT) | (1L << HEX) | (1L << FLOAT) | (1L << HEX_FLOAT))) != 0)) {
					{
					setState(345);
					explist();
					}
				}

				setState(348);
				match(RPAREN);
				}
				break;
			case LBRACE:
				enterOuterAlt(_localctx, 2);
				{
				setState(349);
				tableconstructor();
				}
				break;
			case NORMALSTRING:
			case CHARSTRING:
			case LONGSTRING:
				enterOuterAlt(_localctx, 3);
				{
				setState(350);
				lstring();
				}
				break;
			default:
				throw new NoViableAltException(this);
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

	public static class FunctiondefContext extends ParserRuleContext {
		public TerminalNode FUNCTION() { return getToken(LuaParser.FUNCTION, 0); }
		public FuncbodyContext funcbody() {
			return getRuleContext(FuncbodyContext.class,0);
		}
		public FunctiondefContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_functiondef; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterFunctiondef(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitFunctiondef(this);
		}
	}

	public final FunctiondefContext functiondef() throws RecognitionException {
		FunctiondefContext _localctx = new FunctiondefContext(_ctx, getState());
		enterRule(_localctx, 54, RULE_functiondef);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(353);
			match(FUNCTION);
			setState(354);
			funcbody();
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

	public static class FuncbodyContext extends ParserRuleContext {
		public TerminalNode LPAREN() { return getToken(LuaParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuaParser.RPAREN, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode END() { return getToken(LuaParser.END, 0); }
		public ParlistContext parlist() {
			return getRuleContext(ParlistContext.class,0);
		}
		public FuncbodyContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_funcbody; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterFuncbody(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitFuncbody(this);
		}
	}

	public final FuncbodyContext funcbody() throws RecognitionException {
		FuncbodyContext _localctx = new FuncbodyContext(_ctx, getState());
		enterRule(_localctx, 56, RULE_funcbody);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(356);
			match(LPAREN);
			setState(358);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==DOTS || _la==NAME) {
				{
				setState(357);
				parlist();
				}
			}

			setState(360);
			match(RPAREN);
			setState(361);
			block();
			setState(362);
			match(END);
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

	public static class ParlistContext extends ParserRuleContext {
		public NamelistContext namelist() {
			return getRuleContext(NamelistContext.class,0);
		}
		public TerminalNode COMMA() { return getToken(LuaParser.COMMA, 0); }
		public TerminalNode DOTS() { return getToken(LuaParser.DOTS, 0); }
		public ParlistContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_parlist; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterParlist(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitParlist(this);
		}
	}

	public final ParlistContext parlist() throws RecognitionException {
		ParlistContext _localctx = new ParlistContext(_ctx, getState());
		enterRule(_localctx, 58, RULE_parlist);
		int _la;
		try {
			setState(370);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case NAME:
				enterOuterAlt(_localctx, 1);
				{
				setState(364);
				namelist();
				setState(367);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==COMMA) {
					{
					setState(365);
					match(COMMA);
					setState(366);
					match(DOTS);
					}
				}

				}
				break;
			case DOTS:
				enterOuterAlt(_localctx, 2);
				{
				setState(369);
				match(DOTS);
				}
				break;
			default:
				throw new NoViableAltException(this);
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

	public static class TableconstructorContext extends ParserRuleContext {
		public TerminalNode LBRACE() { return getToken(LuaParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuaParser.RBRACE, 0); }
		public FieldlistContext fieldlist() {
			return getRuleContext(FieldlistContext.class,0);
		}
		public TableconstructorContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_tableconstructor; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterTableconstructor(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitTableconstructor(this);
		}
	}

	public final TableconstructorContext tableconstructor() throws RecognitionException {
		TableconstructorContext _localctx = new TableconstructorContext(_ctx, getState());
		enterRule(_localctx, 60, RULE_tableconstructor);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(372);
			match(LBRACE);
			setState(374);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << FUNCTION) | (1L << LPAREN) | (1L << LBRACK) | (1L << LBRACE) | (1L << MINUS) | (1L << BITNOT) | (1L << NOT) | (1L << LEN) | (1L << NIL) | (1L << FALSE) | (1L << TRUE) | (1L << DOTS) | (1L << NAME) | (1L << NORMALSTRING) | (1L << CHARSTRING) | (1L << LONGSTRING) | (1L << INT) | (1L << HEX) | (1L << FLOAT) | (1L << HEX_FLOAT))) != 0)) {
				{
				setState(373);
				fieldlist();
				}
			}

			setState(376);
			match(RBRACE);
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

	public static class FieldlistContext extends ParserRuleContext {
		public List<FieldContext> field() {
			return getRuleContexts(FieldContext.class);
		}
		public FieldContext field(int i) {
			return getRuleContext(FieldContext.class,i);
		}
		public List<FieldsepContext> fieldsep() {
			return getRuleContexts(FieldsepContext.class);
		}
		public FieldsepContext fieldsep(int i) {
			return getRuleContext(FieldsepContext.class,i);
		}
		public FieldlistContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_fieldlist; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterFieldlist(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitFieldlist(this);
		}
	}

	public final FieldlistContext fieldlist() throws RecognitionException {
		FieldlistContext _localctx = new FieldlistContext(_ctx, getState());
		enterRule(_localctx, 62, RULE_fieldlist);
		int _la;
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(378);
			field();
			setState(384);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,30,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(379);
					fieldsep();
					setState(380);
					field();
					}
					}
				}
				setState(386);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,30,_ctx);
			}
			setState(388);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==SEMICOLON || _la==COMMA) {
				{
				setState(387);
				fieldsep();
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

	public static class FieldContext extends ParserRuleContext {
		public TerminalNode LBRACK() { return getToken(LuaParser.LBRACK, 0); }
		public List<ExpContext> exp() {
			return getRuleContexts(ExpContext.class);
		}
		public ExpContext exp(int i) {
			return getRuleContext(ExpContext.class,i);
		}
		public TerminalNode RBRACK() { return getToken(LuaParser.RBRACK, 0); }
		public TerminalNode EQUALS() { return getToken(LuaParser.EQUALS, 0); }
		public TerminalNode NAME() { return getToken(LuaParser.NAME, 0); }
		public FieldContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_field; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterField(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitField(this);
		}
	}

	public final FieldContext field() throws RecognitionException {
		FieldContext _localctx = new FieldContext(_ctx, getState());
		enterRule(_localctx, 64, RULE_field);
		try {
			setState(400);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,32,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(390);
				match(LBRACK);
				setState(391);
				exp(0);
				setState(392);
				match(RBRACK);
				setState(393);
				match(EQUALS);
				setState(394);
				exp(0);
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(396);
				match(NAME);
				setState(397);
				match(EQUALS);
				setState(398);
				exp(0);
				}
				break;
			case 3:
				enterOuterAlt(_localctx, 3);
				{
				setState(399);
				exp(0);
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

	public static class FieldsepContext extends ParserRuleContext {
		public TerminalNode COMMA() { return getToken(LuaParser.COMMA, 0); }
		public TerminalNode SEMICOLON() { return getToken(LuaParser.SEMICOLON, 0); }
		public FieldsepContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_fieldsep; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterFieldsep(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitFieldsep(this);
		}
	}

	public final FieldsepContext fieldsep() throws RecognitionException {
		FieldsepContext _localctx = new FieldsepContext(_ctx, getState());
		enterRule(_localctx, 66, RULE_fieldsep);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(402);
			_la = _input.LA(1);
			if ( !(_la==SEMICOLON || _la==COMMA) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
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

	public static class OperatorOrContext extends ParserRuleContext {
		public TerminalNode OR() { return getToken(LuaParser.OR, 0); }
		public OperatorOrContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorOr; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterOperatorOr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitOperatorOr(this);
		}
	}

	public final OperatorOrContext operatorOr() throws RecognitionException {
		OperatorOrContext _localctx = new OperatorOrContext(_ctx, getState());
		enterRule(_localctx, 68, RULE_operatorOr);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(404);
			match(OR);
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

	public static class OperatorAndContext extends ParserRuleContext {
		public TerminalNode AND() { return getToken(LuaParser.AND, 0); }
		public OperatorAndContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorAnd; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterOperatorAnd(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitOperatorAnd(this);
		}
	}

	public final OperatorAndContext operatorAnd() throws RecognitionException {
		OperatorAndContext _localctx = new OperatorAndContext(_ctx, getState());
		enterRule(_localctx, 70, RULE_operatorAnd);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(406);
			match(AND);
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

	public static class OperatorComparisonContext extends ParserRuleContext {
		public OperatorComparisonContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorComparison; }

		public OperatorComparisonContext() { }
		public void copyFrom(OperatorComparisonContext ctx) {
			super.copyFrom(ctx);
		}
	}
	public static class EqualContext extends OperatorComparisonContext {
		public TerminalNode EQ() { return getToken(LuaParser.EQ, 0); }
		public EqualContext(OperatorComparisonContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterEqual(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitEqual(this);
		}
	}
	public static class LessthanContext extends OperatorComparisonContext {
		public TerminalNode LT() { return getToken(LuaParser.LT, 0); }
		public LessthanContext(OperatorComparisonContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterLessthan(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitLessthan(this);
		}
	}
	public static class LessthanorequalContext extends OperatorComparisonContext {
		public TerminalNode LTE() { return getToken(LuaParser.LTE, 0); }
		public LessthanorequalContext(OperatorComparisonContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterLessthanorequal(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitLessthanorequal(this);
		}
	}
	public static class GreaterthanContext extends OperatorComparisonContext {
		public TerminalNode GT() { return getToken(LuaParser.GT, 0); }
		public GreaterthanContext(OperatorComparisonContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterGreaterthan(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitGreaterthan(this);
		}
	}
	public static class NotequalContext extends OperatorComparisonContext {
		public TerminalNode NEQ() { return getToken(LuaParser.NEQ, 0); }
		public NotequalContext(OperatorComparisonContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterNotequal(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitNotequal(this);
		}
	}
	public static class GreaterthanorequalContext extends OperatorComparisonContext {
		public TerminalNode GTE() { return getToken(LuaParser.GTE, 0); }
		public GreaterthanorequalContext(OperatorComparisonContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterGreaterthanorequal(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitGreaterthanorequal(this);
		}
	}

	public final OperatorComparisonContext operatorComparison() throws RecognitionException {
		OperatorComparisonContext _localctx = new OperatorComparisonContext(_ctx, getState());
		enterRule(_localctx, 72, RULE_operatorComparison);
		try {
			setState(414);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case LT:
				_localctx = new LessthanContext(_localctx);
				enterOuterAlt(_localctx, 1);
				{
				setState(408);
				match(LT);
				}
				break;
			case GT:
				_localctx = new GreaterthanContext(_localctx);
				enterOuterAlt(_localctx, 2);
				{
				setState(409);
				match(GT);
				}
				break;
			case LTE:
				_localctx = new LessthanorequalContext(_localctx);
				enterOuterAlt(_localctx, 3);
				{
				setState(410);
				match(LTE);
				}
				break;
			case GTE:
				_localctx = new GreaterthanorequalContext(_localctx);
				enterOuterAlt(_localctx, 4);
				{
				setState(411);
				match(GTE);
				}
				break;
			case NEQ:
				_localctx = new NotequalContext(_localctx);
				enterOuterAlt(_localctx, 5);
				{
				setState(412);
				match(NEQ);
				}
				break;
			case EQ:
				_localctx = new EqualContext(_localctx);
				enterOuterAlt(_localctx, 6);
				{
				setState(413);
				match(EQ);
				}
				break;
			default:
				throw new NoViableAltException(this);
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

	public static class OperatorStrcatContext extends ParserRuleContext {
		public TerminalNode STRCAT() { return getToken(LuaParser.STRCAT, 0); }
		public OperatorStrcatContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorStrcat; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterOperatorStrcat(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitOperatorStrcat(this);
		}
	}

	public final OperatorStrcatContext operatorStrcat() throws RecognitionException {
		OperatorStrcatContext _localctx = new OperatorStrcatContext(_ctx, getState());
		enterRule(_localctx, 74, RULE_operatorStrcat);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(416);
			match(STRCAT);
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

	public static class OperatorAddSubContext extends ParserRuleContext {
		public TerminalNode PLUS() { return getToken(LuaParser.PLUS, 0); }
		public TerminalNode MINUS() { return getToken(LuaParser.MINUS, 0); }
		public OperatorAddSubContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorAddSub; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterOperatorAddSub(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitOperatorAddSub(this);
		}
	}

	public final OperatorAddSubContext operatorAddSub() throws RecognitionException {
		OperatorAddSubContext _localctx = new OperatorAddSubContext(_ctx, getState());
		enterRule(_localctx, 76, RULE_operatorAddSub);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(418);
			_la = _input.LA(1);
			if ( !(_la==PLUS || _la==MINUS) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
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

	public static class OperatorMulDivModContext extends ParserRuleContext {
		public TerminalNode MUL() { return getToken(LuaParser.MUL, 0); }
		public TerminalNode DIV() { return getToken(LuaParser.DIV, 0); }
		public TerminalNode MOD() { return getToken(LuaParser.MOD, 0); }
		public TerminalNode DIVFLOOR() { return getToken(LuaParser.DIVFLOOR, 0); }
		public OperatorMulDivModContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorMulDivMod; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterOperatorMulDivMod(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitOperatorMulDivMod(this);
		}
	}

	public final OperatorMulDivModContext operatorMulDivMod() throws RecognitionException {
		OperatorMulDivModContext _localctx = new OperatorMulDivModContext(_ctx, getState());
		enterRule(_localctx, 78, RULE_operatorMulDivMod);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(420);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << MUL) | (1L << DIV) | (1L << MOD) | (1L << DIVFLOOR))) != 0)) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
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

	public static class OperatorBitwiseContext extends ParserRuleContext {
		public TerminalNode BITAND() { return getToken(LuaParser.BITAND, 0); }
		public TerminalNode BITOR() { return getToken(LuaParser.BITOR, 0); }
		public TerminalNode BITNOT() { return getToken(LuaParser.BITNOT, 0); }
		public TerminalNode BITSHL() { return getToken(LuaParser.BITSHL, 0); }
		public TerminalNode BITSHR() { return getToken(LuaParser.BITSHR, 0); }
		public OperatorBitwiseContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorBitwise; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterOperatorBitwise(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitOperatorBitwise(this);
		}
	}

	public final OperatorBitwiseContext operatorBitwise() throws RecognitionException {
		OperatorBitwiseContext _localctx = new OperatorBitwiseContext(_ctx, getState());
		enterRule(_localctx, 80, RULE_operatorBitwise);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(422);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << BITAND) | (1L << BITOR) | (1L << BITNOT) | (1L << BITSHL) | (1L << BITSHR))) != 0)) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
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

	public static class OperatorUnaryContext extends ParserRuleContext {
		public TerminalNode NOT() { return getToken(LuaParser.NOT, 0); }
		public TerminalNode LEN() { return getToken(LuaParser.LEN, 0); }
		public TerminalNode MINUS() { return getToken(LuaParser.MINUS, 0); }
		public TerminalNode BITNOT() { return getToken(LuaParser.BITNOT, 0); }
		public OperatorUnaryContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorUnary; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterOperatorUnary(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitOperatorUnary(this);
		}
	}

	public final OperatorUnaryContext operatorUnary() throws RecognitionException {
		OperatorUnaryContext _localctx = new OperatorUnaryContext(_ctx, getState());
		enterRule(_localctx, 82, RULE_operatorUnary);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(424);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << MINUS) | (1L << BITNOT) | (1L << NOT) | (1L << LEN))) != 0)) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
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

	public static class OperatorPowerContext extends ParserRuleContext {
		public TerminalNode POWER() { return getToken(LuaParser.POWER, 0); }
		public OperatorPowerContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_operatorPower; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterOperatorPower(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitOperatorPower(this);
		}
	}

	public final OperatorPowerContext operatorPower() throws RecognitionException {
		OperatorPowerContext _localctx = new OperatorPowerContext(_ctx, getState());
		enterRule(_localctx, 84, RULE_operatorPower);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(426);
			match(POWER);
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
		public TerminalNode INT() { return getToken(LuaParser.INT, 0); }
		public TerminalNode HEX() { return getToken(LuaParser.HEX, 0); }
		public TerminalNode FLOAT() { return getToken(LuaParser.FLOAT, 0); }
		public TerminalNode HEX_FLOAT() { return getToken(LuaParser.HEX_FLOAT, 0); }
		public NumberContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_number; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterNumber(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitNumber(this);
		}
	}

	public final NumberContext number() throws RecognitionException {
		NumberContext _localctx = new NumberContext(_ctx, getState());
		enterRule(_localctx, 86, RULE_number);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(428);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << INT) | (1L << HEX) | (1L << FLOAT) | (1L << HEX_FLOAT))) != 0)) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
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

	public static class LstringContext extends ParserRuleContext {
		public TerminalNode NORMALSTRING() { return getToken(LuaParser.NORMALSTRING, 0); }
		public TerminalNode CHARSTRING() { return getToken(LuaParser.CHARSTRING, 0); }
		public TerminalNode LONGSTRING() { return getToken(LuaParser.LONGSTRING, 0); }
		public LstringContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_lstring; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).enterLstring(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuaParserListener ) ((LuaParserListener)listener).exitLstring(this);
		}
	}

	public final LstringContext lstring() throws RecognitionException {
		LstringContext _localctx = new LstringContext(_ctx, getState());
		enterRule(_localctx, 88, RULE_lstring);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(430);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << NORMALSTRING) | (1L << CHARSTRING) | (1L << LONGSTRING))) != 0)) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
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

	public boolean sempred(RuleContext _localctx, int ruleIndex, int predIndex) {
		switch (ruleIndex) {
		case 23:
			return exp_sempred((ExpContext)_localctx, predIndex);
		case 24:
			return variable_sempred((VariableContext)_localctx, predIndex);
		}
		return true;
	}
	private boolean exp_sempred(ExpContext _localctx, int predIndex) {
		switch (predIndex) {
		case 0:
			return precpred(_ctx, 9);
		case 1:
			return precpred(_ctx, 7);
		case 2:
			return precpred(_ctx, 6);
		case 3:
			return precpred(_ctx, 5);
		case 4:
			return precpred(_ctx, 4);
		case 5:
			return precpred(_ctx, 3);
		case 6:
			return precpred(_ctx, 2);
		case 7:
			return precpred(_ctx, 1);
		}
		return true;
	}
	private boolean variable_sempred(VariableContext _localctx, int predIndex) {
		switch (predIndex) {
		case 8:
			return precpred(_ctx, 3);
		case 9:
			return precpred(_ctx, 1);
		}
		return true;
	}

	public static final String _serializedATN =
		"\3\u608b\ua72a\u8133\ub9ed\u417c\u3be7\u7786\u5964\3E\u01b3\4\2\t\2\4"+
		"\3\t\3\4\4\t\4\4\5\t\5\4\6\t\6\4\7\t\7\4\b\t\b\4\t\t\t\4\n\t\n\4\13\t"+
		"\13\4\f\t\f\4\r\t\r\4\16\t\16\4\17\t\17\4\20\t\20\4\21\t\21\4\22\t\22"+
		"\4\23\t\23\4\24\t\24\4\25\t\25\4\26\t\26\4\27\t\27\4\30\t\30\4\31\t\31"+
		"\4\32\t\32\4\33\t\33\4\34\t\34\4\35\t\35\4\36\t\36\4\37\t\37\4 \t \4!"+
		"\t!\4\"\t\"\4#\t#\4$\t$\4%\t%\4&\t&\4\'\t\'\4(\t(\4)\t)\4*\t*\4+\t+\4"+
		",\t,\4-\t-\4.\t.\3\2\3\2\3\2\3\3\7\3a\n\3\f\3\16\3d\13\3\3\3\5\3g\n\3"+
		"\3\4\3\4\3\4\3\4\3\4\3\4\3\4\3\4\3\4\3\4\3\4\3\4\3\4\3\4\3\4\5\4x\n\4"+
		"\3\5\3\5\3\6\3\6\3\6\3\7\3\7\3\7\3\7\3\b\3\b\3\b\3\b\3\b\3\b\3\t\3\t\3"+
		"\t\3\t\3\t\3\n\3\n\3\n\3\n\3\n\3\n\3\n\3\n\3\n\7\n\u0097\n\n\f\n\16\n"+
		"\u009a\13\n\3\n\3\n\5\n\u009e\n\n\3\n\3\n\3\13\3\13\3\13\3\13\3\13\3\13"+
		"\3\13\3\13\3\f\3\f\3\f\3\f\3\f\3\f\3\f\3\f\5\f\u00b2\n\f\3\f\3\f\3\f\3"+
		"\f\3\r\3\r\3\r\3\r\3\16\3\16\3\16\3\16\3\16\3\17\3\17\3\17\3\17\5\17\u00c5"+
		"\n\17\3\20\3\20\3\20\3\20\3\21\3\21\3\21\3\21\3\21\7\21\u00d0\n\21\f\21"+
		"\16\21\u00d3\13\21\3\22\3\22\3\22\5\22\u00d8\n\22\3\23\3\23\5\23\u00dc"+
		"\n\23\3\23\5\23\u00df\n\23\3\24\3\24\3\24\3\24\3\25\3\25\3\25\7\25\u00e8"+
		"\n\25\f\25\16\25\u00eb\13\25\3\25\3\25\5\25\u00ef\n\25\3\26\3\26\3\26"+
		"\7\26\u00f4\n\26\f\26\16\26\u00f7\13\26\3\27\3\27\3\27\7\27\u00fc\n\27"+
		"\f\27\16\27\u00ff\13\27\3\30\3\30\3\30\7\30\u0104\n\30\f\30\16\30\u0107"+
		"\13\30\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31"+
		"\5\31\u0116\n\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31"+
		"\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31\3\31"+
		"\3\31\3\31\3\31\3\31\3\31\3\31\3\31\7\31\u0138\n\31\f\31\16\31\u013b\13"+
		"\31\3\32\3\32\3\32\3\32\3\32\3\32\5\32\u0143\n\32\3\32\3\32\3\32\3\32"+
		"\3\32\3\32\3\32\5\32\u014c\n\32\3\32\3\32\7\32\u0150\n\32\f\32\16\32\u0153"+
		"\13\32\3\33\3\33\5\33\u0157\n\33\3\33\3\33\3\34\3\34\5\34\u015d\n\34\3"+
		"\34\3\34\3\34\5\34\u0162\n\34\3\35\3\35\3\35\3\36\3\36\5\36\u0169\n\36"+
		"\3\36\3\36\3\36\3\36\3\37\3\37\3\37\5\37\u0172\n\37\3\37\5\37\u0175\n"+
		"\37\3 \3 \5 \u0179\n \3 \3 \3!\3!\3!\3!\7!\u0181\n!\f!\16!\u0184\13!\3"+
		"!\5!\u0187\n!\3\"\3\"\3\"\3\"\3\"\3\"\3\"\3\"\3\"\3\"\5\"\u0193\n\"\3"+
		"#\3#\3$\3$\3%\3%\3&\3&\3&\3&\3&\3&\5&\u01a1\n&\3\'\3\'\3(\3(\3)\3)\3*"+
		"\3*\3+\3+\3,\3,\3-\3-\3.\3.\3.\2\4\60\62/\2\4\6\b\n\f\16\20\22\24\26\30"+
		"\32\34\36 \"$&(*,.\60\62\64\668:<>@BDFHJLNPRTVXZ\2\t\4\2\3\3\26\26\3\2"+
		"()\3\2*-\3\2.\62\5\2))\60\60\63\64\3\2>A\3\2;=\2\u01c8\2\\\3\2\2\2\4b"+
		"\3\2\2\2\6w\3\2\2\2\by\3\2\2\2\n{\3\2\2\2\f~\3\2\2\2\16\u0082\3\2\2\2"+
		"\20\u0088\3\2\2\2\22\u008d\3\2\2\2\24\u00a1\3\2\2\2\26\u00a9\3\2\2\2\30"+
		"\u00b7\3\2\2\2\32\u00bb\3\2\2\2\34\u00c0\3\2\2\2\36\u00c6\3\2\2\2 \u00ca"+
		"\3\2\2\2\"\u00d7\3\2\2\2$\u00d9\3\2\2\2&\u00e0\3\2\2\2(\u00e4\3\2\2\2"+
		"*\u00f0\3\2\2\2,\u00f8\3\2\2\2.\u0100\3\2\2\2\60\u0115\3\2\2\2\62\u0142"+
		"\3\2\2\2\64\u0156\3\2\2\2\66\u0161\3\2\2\28\u0163\3\2\2\2:\u0166\3\2\2"+
		"\2<\u0174\3\2\2\2>\u0176\3\2\2\2@\u017c\3\2\2\2B\u0192\3\2\2\2D\u0194"+
		"\3\2\2\2F\u0196\3\2\2\2H\u0198\3\2\2\2J\u01a0\3\2\2\2L\u01a2\3\2\2\2N"+
		"\u01a4\3\2\2\2P\u01a6\3\2\2\2R\u01a8\3\2\2\2T\u01aa\3\2\2\2V\u01ac\3\2"+
		"\2\2X\u01ae\3\2\2\2Z\u01b0\3\2\2\2\\]\5\4\3\2]^\7\2\2\3^\3\3\2\2\2_a\5"+
		"\6\4\2`_\3\2\2\2ad\3\2\2\2b`\3\2\2\2bc\3\2\2\2cf\3\2\2\2db\3\2\2\2eg\5"+
		"$\23\2fe\3\2\2\2fg\3\2\2\2g\5\3\2\2\2hx\7\3\2\2ix\5\36\20\2jx\5\62\32"+
		"\2kx\5&\24\2lx\5\b\5\2mx\5\n\6\2nx\5\f\7\2ox\5\16\b\2px\5\20\t\2qx\5\22"+
		"\n\2rx\5\26\f\2sx\5\24\13\2tx\5\30\r\2ux\5\32\16\2vx\5\34\17\2wh\3\2\2"+
		"\2wi\3\2\2\2wj\3\2\2\2wk\3\2\2\2wl\3\2\2\2wm\3\2\2\2wn\3\2\2\2wo\3\2\2"+
		"\2wp\3\2\2\2wq\3\2\2\2wr\3\2\2\2ws\3\2\2\2wt\3\2\2\2wu\3\2\2\2wv\3\2\2"+
		"\2x\7\3\2\2\2yz\7\4\2\2z\t\3\2\2\2{|\7\5\2\2|}\7:\2\2}\13\3\2\2\2~\177"+
		"\7\6\2\2\177\u0080\5\4\3\2\u0080\u0081\7\b\2\2\u0081\r\3\2\2\2\u0082\u0083"+
		"\7\7\2\2\u0083\u0084\5\60\31\2\u0084\u0085\7\6\2\2\u0085\u0086\5\4\3\2"+
		"\u0086\u0087\7\b\2\2\u0087\17\3\2\2\2\u0088\u0089\7\t\2\2\u0089\u008a"+
		"\5\4\3\2\u008a\u008b\7\n\2\2\u008b\u008c\5\60\31\2\u008c\21\3\2\2\2\u008d"+
		"\u008e\7\16\2\2\u008e\u008f\5\60\31\2\u008f\u0090\7\17\2\2\u0090\u0098"+
		"\5\4\3\2\u0091\u0092\7\20\2\2\u0092\u0093\5\60\31\2\u0093\u0094\7\17\2"+
		"\2\u0094\u0095\5\4\3\2\u0095\u0097\3\2\2\2\u0096\u0091\3\2\2\2\u0097\u009a"+
		"\3\2\2\2\u0098\u0096\3\2\2\2\u0098\u0099\3\2\2\2\u0099\u009d\3\2\2\2\u009a"+
		"\u0098\3\2\2\2\u009b\u009c\7\21\2\2\u009c\u009e\5\4\3\2\u009d\u009b\3"+
		"\2\2\2\u009d\u009e\3\2\2\2\u009e\u009f\3\2\2\2\u009f\u00a0\7\b\2\2\u00a0"+
		"\23\3\2\2\2\u00a1\u00a2\7\13\2\2\u00a2\u00a3\5,\27\2\u00a3\u00a4\7\27"+
		"\2\2\u00a4\u00a5\5.\30\2\u00a5\u00a6\7\6\2\2\u00a6\u00a7\5\4\3\2\u00a7"+
		"\u00a8\7\b\2\2\u00a8\25\3\2\2\2\u00a9\u00aa\7\13\2\2\u00aa\u00ab\7:\2"+
		"\2\u00ab\u00ac\7&\2\2\u00ac\u00ad\5\60\31\2\u00ad\u00ae\7\26\2\2\u00ae"+
		"\u00b1\5\60\31\2\u00af\u00b0\7\26\2\2\u00b0\u00b2\5\60\31\2\u00b1\u00af"+
		"\3\2\2\2\u00b1\u00b2\3\2\2\2\u00b2\u00b3\3\2\2\2\u00b3\u00b4\7\6\2\2\u00b4"+
		"\u00b5\5\4\3\2\u00b5\u00b6\7\b\2\2\u00b6\27\3\2\2\2\u00b7\u00b8\7\f\2"+
		"\2\u00b8\u00b9\5(\25\2\u00b9\u00ba\5:\36\2\u00ba\31\3\2\2\2\u00bb\u00bc"+
		"\7\r\2\2\u00bc\u00bd\7\f\2\2\u00bd\u00be\7:\2\2\u00be\u00bf\5:\36\2\u00bf"+
		"\33\3\2\2\2\u00c0\u00c1\7\r\2\2\u00c1\u00c4\5 \21\2\u00c2\u00c3\7&\2\2"+
		"\u00c3\u00c5\5.\30\2\u00c4\u00c2\3\2\2\2\u00c4\u00c5\3\2\2\2\u00c5\35"+
		"\3\2\2\2\u00c6\u00c7\5*\26\2\u00c7\u00c8\7&\2\2\u00c8\u00c9\5.\30\2\u00c9"+
		"\37\3\2\2\2\u00ca\u00cb\7:\2\2\u00cb\u00d1\5\"\22\2\u00cc\u00cd\7\26\2"+
		"\2\u00cd\u00ce\7:\2\2\u00ce\u00d0\5\"\22\2\u00cf\u00cc\3\2\2\2\u00d0\u00d3"+
		"\3\2\2\2\u00d1\u00cf\3\2\2\2\u00d1\u00d2\3\2\2\2\u00d2!\3\2\2\2\u00d3"+
		"\u00d1\3\2\2\2\u00d4\u00d5\7 \2\2\u00d5\u00d6\7:\2\2\u00d6\u00d8\7!\2"+
		"\2\u00d7\u00d4\3\2\2\2\u00d7\u00d8\3\2\2\2\u00d8#\3\2\2\2\u00d9\u00db"+
		"\7\22\2\2\u00da\u00dc\5.\30\2\u00db\u00da\3\2\2\2\u00db\u00dc\3\2\2\2"+
		"\u00dc\u00de\3\2\2\2\u00dd\u00df\7\3\2\2\u00de\u00dd\3\2\2\2\u00de\u00df"+
		"\3\2\2\2\u00df%\3\2\2\2\u00e0\u00e1\7\24\2\2\u00e1\u00e2\7:\2\2\u00e2"+
		"\u00e3\7\24\2\2\u00e3\'\3\2\2\2\u00e4\u00e9\7:\2\2\u00e5\u00e6\7\25\2"+
		"\2\u00e6\u00e8\7:\2\2\u00e7\u00e5\3\2\2\2\u00e8\u00eb\3\2\2\2\u00e9\u00e7"+
		"\3\2\2\2\u00e9\u00ea\3\2\2\2\u00ea\u00ee\3\2\2\2\u00eb\u00e9\3\2\2\2\u00ec"+
		"\u00ed\7\23\2\2\u00ed\u00ef\7:\2\2\u00ee\u00ec\3\2\2\2\u00ee\u00ef\3\2"+
		"\2\2\u00ef)\3\2\2\2\u00f0\u00f5\5\62\32\2\u00f1\u00f2\7\26\2\2\u00f2\u00f4"+
		"\5\62\32\2\u00f3\u00f1\3\2\2\2\u00f4\u00f7\3\2\2\2\u00f5\u00f3\3\2\2\2"+
		"\u00f5\u00f6\3\2\2\2\u00f6+\3\2\2\2\u00f7\u00f5\3\2\2\2\u00f8\u00fd\7"+
		":\2\2\u00f9\u00fa\7\26\2\2\u00fa\u00fc\7:\2\2\u00fb\u00f9\3\2\2\2\u00fc"+
		"\u00ff\3\2\2\2\u00fd\u00fb\3\2\2\2\u00fd\u00fe\3\2\2\2\u00fe-\3\2\2\2"+
		"\u00ff\u00fd\3\2\2\2\u0100\u0105\5\60\31\2\u0101\u0102\7\26\2\2\u0102"+
		"\u0104\5\60\31\2\u0103\u0101\3\2\2\2\u0104\u0107\3\2\2\2\u0105\u0103\3"+
		"\2\2\2\u0105\u0106\3\2\2\2\u0106/\3\2\2\2\u0107\u0105\3\2\2\2\u0108\u0109"+
		"\b\31\1\2\u0109\u0116\7\66\2\2\u010a\u0116\7\67\2\2\u010b\u0116\78\2\2"+
		"\u010c\u0116\5X-\2\u010d\u0116\5Z.\2\u010e\u0116\79\2\2\u010f\u0116\5"+
		"8\35\2\u0110\u0116\5\62\32\2\u0111\u0116\5> \2\u0112\u0113\5T+\2\u0113"+
		"\u0114\5\60\31\n\u0114\u0116\3\2\2\2\u0115\u0108\3\2\2\2\u0115\u010a\3"+
		"\2\2\2\u0115\u010b\3\2\2\2\u0115\u010c\3\2\2\2\u0115\u010d\3\2\2\2\u0115"+
		"\u010e\3\2\2\2\u0115\u010f\3\2\2\2\u0115\u0110\3\2\2\2\u0115\u0111\3\2"+
		"\2\2\u0115\u0112\3\2\2\2\u0116\u0139\3\2\2\2\u0117\u0118\f\13\2\2\u0118"+
		"\u0119\5V,\2\u0119\u011a\5\60\31\13\u011a\u0138\3\2\2\2\u011b\u011c\f"+
		"\t\2\2\u011c\u011d\5P)\2\u011d\u011e\5\60\31\n\u011e\u0138\3\2\2\2\u011f"+
		"\u0120\f\b\2\2\u0120\u0121\5N(\2\u0121\u0122\5\60\31\t\u0122\u0138\3\2"+
		"\2\2\u0123\u0124\f\7\2\2\u0124\u0125\5L\'\2\u0125\u0126\5\60\31\7\u0126"+
		"\u0138\3\2\2\2\u0127\u0128\f\6\2\2\u0128\u0129\5J&\2\u0129\u012a\5\60"+
		"\31\7\u012a\u0138\3\2\2\2\u012b\u012c\f\5\2\2\u012c\u012d\5H%\2\u012d"+
		"\u012e\5\60\31\6\u012e\u0138\3\2\2\2\u012f\u0130\f\4\2\2\u0130\u0131\5"+
		"F$\2\u0131\u0132\5\60\31\5\u0132\u0138\3\2\2\2\u0133\u0134\f\3\2\2\u0134"+
		"\u0135\5R*\2\u0135\u0136\5\60\31\4\u0136\u0138\3\2\2\2\u0137\u0117\3\2"+
		"\2\2\u0137\u011b\3\2\2\2\u0137\u011f\3\2\2\2\u0137\u0123\3\2\2\2\u0137"+
		"\u0127\3\2\2\2\u0137\u012b\3\2\2\2\u0137\u012f\3\2\2\2\u0137\u0133\3\2"+
		"\2\2\u0138\u013b\3\2\2\2\u0139\u0137\3\2\2\2\u0139\u013a\3\2\2\2\u013a"+
		"\61\3\2\2\2\u013b\u0139\3\2\2\2\u013c\u013d\b\32\1\2\u013d\u0143\7:\2"+
		"\2\u013e\u013f\7\30\2\2\u013f\u0140\5\60\31\2\u0140\u0141\7\31\2\2\u0141"+
		"\u0143\3\2\2\2\u0142\u013c\3\2\2\2\u0142\u013e\3\2\2\2\u0143\u0151\3\2"+
		"\2\2\u0144\u014b\f\5\2\2\u0145\u0146\7\32\2\2\u0146\u0147\5\60\31\2\u0147"+
		"\u0148\7\33\2\2\u0148\u014c\3\2\2\2\u0149\u014a\7\25\2\2\u014a\u014c\7"+
		":\2\2\u014b\u0145\3\2\2\2\u014b\u0149\3\2\2\2\u014c\u0150\3\2\2\2\u014d"+
		"\u014e\f\3\2\2\u014e\u0150\5\64\33\2\u014f\u0144\3\2\2\2\u014f\u014d\3"+
		"\2\2\2\u0150\u0153\3\2\2\2\u0151\u014f\3\2\2\2\u0151\u0152\3\2\2\2\u0152"+
		"\63\3\2\2\2\u0153\u0151\3\2\2\2\u0154\u0155\7\23\2\2\u0155\u0157\7:\2"+
		"\2\u0156\u0154\3\2\2\2\u0156\u0157\3\2\2\2\u0157\u0158\3\2\2\2\u0158\u0159"+
		"\5\66\34\2\u0159\65\3\2\2\2\u015a\u015c\7\30\2\2\u015b\u015d\5.\30\2\u015c"+
		"\u015b\3\2\2\2\u015c\u015d\3\2\2\2\u015d\u015e\3\2\2\2\u015e\u0162\7\31"+
		"\2\2\u015f\u0162\5> \2\u0160\u0162\5Z.\2\u0161\u015a\3\2\2\2\u0161\u015f"+
		"\3\2\2\2\u0161\u0160\3\2\2\2\u0162\67\3\2\2\2\u0163\u0164\7\f\2\2\u0164"+
		"\u0165\5:\36\2\u01659\3\2\2\2\u0166\u0168\7\30\2\2\u0167\u0169\5<\37\2"+
		"\u0168\u0167\3\2\2\2\u0168\u0169\3\2\2\2\u0169\u016a\3\2\2\2\u016a\u016b"+
		"\7\31\2\2\u016b\u016c\5\4\3\2\u016c\u016d\7\b\2\2\u016d;\3\2\2\2\u016e"+
		"\u0171\5,\27\2\u016f\u0170\7\26\2\2\u0170\u0172\79\2\2\u0171\u016f\3\2"+
		"\2\2\u0171\u0172\3\2\2\2\u0172\u0175\3\2\2\2\u0173\u0175\79\2\2\u0174"+
		"\u016e\3\2\2\2\u0174\u0173\3\2\2\2\u0175=\3\2\2\2\u0176\u0178\7\34\2\2"+
		"\u0177\u0179\5@!\2\u0178\u0177\3\2\2\2\u0178\u0179\3\2\2\2\u0179\u017a"+
		"\3\2\2\2\u017a\u017b\7\35\2\2\u017b?\3\2\2\2\u017c\u0182\5B\"\2\u017d"+
		"\u017e\5D#\2\u017e\u017f\5B\"\2\u017f\u0181\3\2\2\2\u0180\u017d\3\2\2"+
		"\2\u0181\u0184\3\2\2\2\u0182\u0180\3\2\2\2\u0182\u0183\3\2\2\2\u0183\u0186"+
		"\3\2\2\2\u0184\u0182\3\2\2\2\u0185\u0187\5D#\2\u0186\u0185\3\2\2\2\u0186"+
		"\u0187\3\2\2\2\u0187A\3\2\2\2\u0188\u0189\7\32\2\2\u0189\u018a\5\60\31"+
		"\2\u018a\u018b\7\33\2\2\u018b\u018c\7&\2\2\u018c\u018d\5\60\31\2\u018d"+
		"\u0193\3\2\2\2\u018e\u018f\7:\2\2\u018f\u0190\7&\2\2\u0190\u0193\5\60"+
		"\31\2\u0191\u0193\5\60\31\2\u0192\u0188\3\2\2\2\u0192\u018e\3\2\2\2\u0192"+
		"\u0191\3\2\2\2\u0193C\3\2\2\2\u0194\u0195\t\2\2\2\u0195E\3\2\2\2\u0196"+
		"\u0197\7\36\2\2\u0197G\3\2\2\2\u0198\u0199\7\37\2\2\u0199I\3\2\2\2\u019a"+
		"\u01a1\7 \2\2\u019b\u01a1\7!\2\2\u019c\u01a1\7\"\2\2\u019d\u01a1\7#\2"+
		"\2\u019e\u01a1\7$\2\2\u019f\u01a1\7%\2\2\u01a0\u019a\3\2\2\2\u01a0\u019b"+
		"\3\2\2\2\u01a0\u019c\3\2\2\2\u01a0\u019d\3\2\2\2\u01a0\u019e\3\2\2\2\u01a0"+
		"\u019f\3\2\2\2\u01a1K\3\2\2\2\u01a2\u01a3\7\'\2\2\u01a3M\3\2\2\2\u01a4"+
		"\u01a5\t\3\2\2\u01a5O\3\2\2\2\u01a6\u01a7\t\4\2\2\u01a7Q\3\2\2\2\u01a8"+
		"\u01a9\t\5\2\2\u01a9S\3\2\2\2\u01aa\u01ab\t\6\2\2\u01abU\3\2\2\2\u01ac"+
		"\u01ad\7\65\2\2\u01adW\3\2\2\2\u01ae\u01af\t\7\2\2\u01afY\3\2\2\2\u01b0"+
		"\u01b1\t\b\2\2\u01b1[\3\2\2\2$bfw\u0098\u009d\u00b1\u00c4\u00d1\u00d7"+
		"\u00db\u00de\u00e9\u00ee\u00f5\u00fd\u0105\u0115\u0137\u0139\u0142\u014b"+
		"\u014f\u0151\u0156\u015c\u0161\u0168\u0171\u0174\u0178\u0182\u0186\u0192"+
		"\u01a0";
	public static final ATN _ATN =
		new ATNDeserializer().deserialize(_serializedATN.toCharArray());
	static {
		_decisionToDFA = new DFA[_ATN.getNumberOfDecisions()];
		for (int i = 0; i < _ATN.getNumberOfDecisions(); i++) {
			_decisionToDFA[i] = new DFA(_ATN.getDecisionState(i), i);
		}
	}
}
