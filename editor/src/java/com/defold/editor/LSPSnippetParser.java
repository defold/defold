// Generated from LSPSnippet.g4 by ANTLR 4.9.1
package com.defold.editor;
import org.antlr.v4.runtime.atn.*;
import org.antlr.v4.runtime.dfa.DFA;
import org.antlr.v4.runtime.*;
import org.antlr.v4.runtime.misc.*;
import org.antlr.v4.runtime.tree.*;
import java.util.List;
import java.util.Iterator;
import java.util.ArrayList;

@SuppressWarnings({"all", "warnings", "unchecked", "unused", "cast"})
public class LSPSnippetParser extends Parser {
	static { RuntimeMetaData.checkVersion("4.9.1", RuntimeMetaData.VERSION); }

	protected static final DFA[] _decisionToDFA;
	protected static final PredictionContextCache _sharedContextCache =
		new PredictionContextCache();
	public static final int
		TEXT=1, ID=2, NL=3, CR=4, INT=5, DOLLAR=6, BACK_SLASH=7, OPEN=8, CLOSE=9, 
		COLON=10, PIPE=11, COMMA=12;
	public static final int
		RULE_snippet = 0, RULE_text = 1, RULE_text_esc = 2, RULE_err = 3, RULE_syntax_rule = 4, 
		RULE_newline = 5, RULE_tab_stop = 6, RULE_naked_tab_stop = 7, RULE_curly_tab_stop = 8, 
		RULE_placeholder = 9, RULE_placeholder_content = 10, RULE_placeholder_text = 11, 
		RULE_placeholder_esc = 12, RULE_choice = 13, RULE_choice_elements = 14, 
		RULE_choice_element = 15, RULE_choice_text = 16, RULE_choice_esc = 17, 
		RULE_variable = 18, RULE_variable_text = 19, RULE_naked_variable = 20, 
		RULE_curly_variable = 21, RULE_placeholder_variable = 22;
	private static String[] makeRuleNames() {
		return new String[] {
			"snippet", "text", "text_esc", "err", "syntax_rule", "newline", "tab_stop", 
			"naked_tab_stop", "curly_tab_stop", "placeholder", "placeholder_content", 
			"placeholder_text", "placeholder_esc", "choice", "choice_elements", "choice_element", 
			"choice_text", "choice_esc", "variable", "variable_text", "naked_variable", 
			"curly_variable", "placeholder_variable"
		};
	}
	public static final String[] ruleNames = makeRuleNames();

	private static String[] makeLiteralNames() {
		return new String[] {
			null, null, null, null, "'\r'", null, "'$'", "'\\'", "'{'", "'}'", "':'", 
			"'|'", "','"
		};
	}
	private static final String[] _LITERAL_NAMES = makeLiteralNames();
	private static String[] makeSymbolicNames() {
		return new String[] {
			null, "TEXT", "ID", "NL", "CR", "INT", "DOLLAR", "BACK_SLASH", "OPEN", 
			"CLOSE", "COLON", "PIPE", "COMMA"
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
	public String getGrammarFileName() { return "LSPSnippet.g4"; }

	@Override
	public String[] getRuleNames() { return ruleNames; }

	@Override
	public String getSerializedATN() { return _serializedATN; }

	@Override
	public ATN getATN() { return _ATN; }

	public LSPSnippetParser(TokenStream input) {
		super(input);
		_interp = new ParserATNSimulator(this,_ATN,_decisionToDFA,_sharedContextCache);
	}

	public static class SnippetContext extends ParserRuleContext {
		public List<TextContext> text() {
			return getRuleContexts(TextContext.class);
		}
		public TextContext text(int i) {
			return getRuleContext(TextContext.class,i);
		}
		public List<Text_escContext> text_esc() {
			return getRuleContexts(Text_escContext.class);
		}
		public Text_escContext text_esc(int i) {
			return getRuleContext(Text_escContext.class,i);
		}
		public List<Syntax_ruleContext> syntax_rule() {
			return getRuleContexts(Syntax_ruleContext.class);
		}
		public Syntax_ruleContext syntax_rule(int i) {
			return getRuleContext(Syntax_ruleContext.class,i);
		}
		public List<ErrContext> err() {
			return getRuleContexts(ErrContext.class);
		}
		public ErrContext err(int i) {
			return getRuleContext(ErrContext.class,i);
		}
		public List<NewlineContext> newline() {
			return getRuleContexts(NewlineContext.class);
		}
		public NewlineContext newline(int i) {
			return getRuleContext(NewlineContext.class,i);
		}
		public SnippetContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_snippet; }
	}

	public final SnippetContext snippet() throws RecognitionException {
		SnippetContext _localctx = new SnippetContext(_ctx, getState());
		enterRule(_localctx, 0, RULE_snippet);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(53);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << TEXT) | (1L << ID) | (1L << NL) | (1L << CR) | (1L << INT) | (1L << DOLLAR) | (1L << BACK_SLASH) | (1L << OPEN) | (1L << CLOSE) | (1L << COLON) | (1L << PIPE) | (1L << COMMA))) != 0)) {
				{
				setState(51);
				_errHandler.sync(this);
				switch ( getInterpreter().adaptivePredict(_input,0,_ctx) ) {
				case 1:
					{
					setState(46);
					text();
					}
					break;
				case 2:
					{
					setState(47);
					text_esc();
					}
					break;
				case 3:
					{
					setState(48);
					syntax_rule();
					}
					break;
				case 4:
					{
					setState(49);
					err();
					}
					break;
				case 5:
					{
					setState(50);
					newline();
					}
					break;
				}
				}
				setState(55);
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

	public static class TextContext extends ParserRuleContext {
		public TerminalNode TEXT() { return getToken(LSPSnippetParser.TEXT, 0); }
		public TerminalNode ID() { return getToken(LSPSnippetParser.ID, 0); }
		public TextContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_text; }
	}

	public final TextContext text() throws RecognitionException {
		TextContext _localctx = new TextContext(_ctx, getState());
		enterRule(_localctx, 2, RULE_text);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(56);
			_la = _input.LA(1);
			if ( !(_la==TEXT || _la==ID) ) {
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

	public static class Text_escContext extends ParserRuleContext {
		public List<TerminalNode> BACK_SLASH() { return getTokens(LSPSnippetParser.BACK_SLASH); }
		public TerminalNode BACK_SLASH(int i) {
			return getToken(LSPSnippetParser.BACK_SLASH, i);
		}
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public Text_escContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_text_esc; }
	}

	public final Text_escContext text_esc() throws RecognitionException {
		Text_escContext _localctx = new Text_escContext(_ctx, getState());
		enterRule(_localctx, 4, RULE_text_esc);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(58);
			match(BACK_SLASH);
			setState(59);
			_la = _input.LA(1);
			if ( !(_la==DOLLAR || _la==BACK_SLASH) ) {
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

	public static class ErrContext extends ParserRuleContext {
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode BACK_SLASH() { return getToken(LSPSnippetParser.BACK_SLASH, 0); }
		public TerminalNode OPEN() { return getToken(LSPSnippetParser.OPEN, 0); }
		public TerminalNode CLOSE() { return getToken(LSPSnippetParser.CLOSE, 0); }
		public TerminalNode INT() { return getToken(LSPSnippetParser.INT, 0); }
		public TerminalNode COLON() { return getToken(LSPSnippetParser.COLON, 0); }
		public TerminalNode PIPE() { return getToken(LSPSnippetParser.PIPE, 0); }
		public TerminalNode COMMA() { return getToken(LSPSnippetParser.COMMA, 0); }
		public ErrContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_err; }
	}

	public final ErrContext err() throws RecognitionException {
		ErrContext _localctx = new ErrContext(_ctx, getState());
		enterRule(_localctx, 6, RULE_err);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(61);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << INT) | (1L << DOLLAR) | (1L << BACK_SLASH) | (1L << OPEN) | (1L << CLOSE) | (1L << COLON) | (1L << PIPE) | (1L << COMMA))) != 0)) ) {
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

	public static class Syntax_ruleContext extends ParserRuleContext {
		public Tab_stopContext tab_stop() {
			return getRuleContext(Tab_stopContext.class,0);
		}
		public PlaceholderContext placeholder() {
			return getRuleContext(PlaceholderContext.class,0);
		}
		public ChoiceContext choice() {
			return getRuleContext(ChoiceContext.class,0);
		}
		public VariableContext variable() {
			return getRuleContext(VariableContext.class,0);
		}
		public Syntax_ruleContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_syntax_rule; }
	}

	public final Syntax_ruleContext syntax_rule() throws RecognitionException {
		Syntax_ruleContext _localctx = new Syntax_ruleContext(_ctx, getState());
		enterRule(_localctx, 8, RULE_syntax_rule);
		try {
			setState(67);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,2,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(63);
				tab_stop();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(64);
				placeholder();
				}
				break;
			case 3:
				enterOuterAlt(_localctx, 3);
				{
				setState(65);
				choice();
				}
				break;
			case 4:
				enterOuterAlt(_localctx, 4);
				{
				setState(66);
				variable();
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

	public static class NewlineContext extends ParserRuleContext {
		public TerminalNode NL() { return getToken(LSPSnippetParser.NL, 0); }
		public TerminalNode CR() { return getToken(LSPSnippetParser.CR, 0); }
		public NewlineContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_newline; }
	}

	public final NewlineContext newline() throws RecognitionException {
		NewlineContext _localctx = new NewlineContext(_ctx, getState());
		enterRule(_localctx, 10, RULE_newline);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(69);
			_la = _input.LA(1);
			if ( !(_la==NL || _la==CR) ) {
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

	public static class Tab_stopContext extends ParserRuleContext {
		public Naked_tab_stopContext naked_tab_stop() {
			return getRuleContext(Naked_tab_stopContext.class,0);
		}
		public Curly_tab_stopContext curly_tab_stop() {
			return getRuleContext(Curly_tab_stopContext.class,0);
		}
		public Tab_stopContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_tab_stop; }
	}

	public final Tab_stopContext tab_stop() throws RecognitionException {
		Tab_stopContext _localctx = new Tab_stopContext(_ctx, getState());
		enterRule(_localctx, 12, RULE_tab_stop);
		try {
			setState(73);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,3,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(71);
				naked_tab_stop();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(72);
				curly_tab_stop();
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

	public static class Naked_tab_stopContext extends ParserRuleContext {
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode INT() { return getToken(LSPSnippetParser.INT, 0); }
		public Naked_tab_stopContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_naked_tab_stop; }
	}

	public final Naked_tab_stopContext naked_tab_stop() throws RecognitionException {
		Naked_tab_stopContext _localctx = new Naked_tab_stopContext(_ctx, getState());
		enterRule(_localctx, 14, RULE_naked_tab_stop);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(75);
			match(DOLLAR);
			setState(76);
			match(INT);
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

	public static class Curly_tab_stopContext extends ParserRuleContext {
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode OPEN() { return getToken(LSPSnippetParser.OPEN, 0); }
		public TerminalNode INT() { return getToken(LSPSnippetParser.INT, 0); }
		public TerminalNode CLOSE() { return getToken(LSPSnippetParser.CLOSE, 0); }
		public Curly_tab_stopContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_curly_tab_stop; }
	}

	public final Curly_tab_stopContext curly_tab_stop() throws RecognitionException {
		Curly_tab_stopContext _localctx = new Curly_tab_stopContext(_ctx, getState());
		enterRule(_localctx, 16, RULE_curly_tab_stop);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(78);
			match(DOLLAR);
			setState(79);
			match(OPEN);
			setState(80);
			match(INT);
			setState(81);
			match(CLOSE);
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

	public static class PlaceholderContext extends ParserRuleContext {
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode OPEN() { return getToken(LSPSnippetParser.OPEN, 0); }
		public TerminalNode INT() { return getToken(LSPSnippetParser.INT, 0); }
		public TerminalNode COLON() { return getToken(LSPSnippetParser.COLON, 0); }
		public Placeholder_contentContext placeholder_content() {
			return getRuleContext(Placeholder_contentContext.class,0);
		}
		public TerminalNode CLOSE() { return getToken(LSPSnippetParser.CLOSE, 0); }
		public PlaceholderContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_placeholder; }
	}

	public final PlaceholderContext placeholder() throws RecognitionException {
		PlaceholderContext _localctx = new PlaceholderContext(_ctx, getState());
		enterRule(_localctx, 18, RULE_placeholder);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(83);
			match(DOLLAR);
			setState(84);
			match(OPEN);
			setState(85);
			match(INT);
			setState(86);
			match(COLON);
			setState(87);
			placeholder_content();
			setState(88);
			match(CLOSE);
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

	public static class Placeholder_contentContext extends ParserRuleContext {
		public List<Syntax_ruleContext> syntax_rule() {
			return getRuleContexts(Syntax_ruleContext.class);
		}
		public Syntax_ruleContext syntax_rule(int i) {
			return getRuleContext(Syntax_ruleContext.class,i);
		}
		public List<Placeholder_textContext> placeholder_text() {
			return getRuleContexts(Placeholder_textContext.class);
		}
		public Placeholder_textContext placeholder_text(int i) {
			return getRuleContext(Placeholder_textContext.class,i);
		}
		public List<Placeholder_escContext> placeholder_esc() {
			return getRuleContexts(Placeholder_escContext.class);
		}
		public Placeholder_escContext placeholder_esc(int i) {
			return getRuleContext(Placeholder_escContext.class,i);
		}
		public List<NewlineContext> newline() {
			return getRuleContexts(NewlineContext.class);
		}
		public NewlineContext newline(int i) {
			return getRuleContext(NewlineContext.class,i);
		}
		public Placeholder_contentContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_placeholder_content; }
	}

	public final Placeholder_contentContext placeholder_content() throws RecognitionException {
		Placeholder_contentContext _localctx = new Placeholder_contentContext(_ctx, getState());
		enterRule(_localctx, 20, RULE_placeholder_content);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(96);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << TEXT) | (1L << ID) | (1L << NL) | (1L << CR) | (1L << INT) | (1L << DOLLAR) | (1L << BACK_SLASH) | (1L << OPEN) | (1L << COLON) | (1L << PIPE) | (1L << COMMA))) != 0)) {
				{
				setState(94);
				_errHandler.sync(this);
				switch (_input.LA(1)) {
				case DOLLAR:
					{
					setState(90);
					syntax_rule();
					}
					break;
				case TEXT:
				case ID:
				case INT:
				case OPEN:
				case COLON:
				case PIPE:
				case COMMA:
					{
					setState(91);
					placeholder_text();
					}
					break;
				case BACK_SLASH:
					{
					setState(92);
					placeholder_esc();
					}
					break;
				case NL:
				case CR:
					{
					setState(93);
					newline();
					}
					break;
				default:
					throw new NoViableAltException(this);
				}
				}
				setState(98);
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

	public static class Placeholder_textContext extends ParserRuleContext {
		public TerminalNode TEXT() { return getToken(LSPSnippetParser.TEXT, 0); }
		public TerminalNode ID() { return getToken(LSPSnippetParser.ID, 0); }
		public TerminalNode INT() { return getToken(LSPSnippetParser.INT, 0); }
		public TerminalNode OPEN() { return getToken(LSPSnippetParser.OPEN, 0); }
		public TerminalNode COLON() { return getToken(LSPSnippetParser.COLON, 0); }
		public TerminalNode PIPE() { return getToken(LSPSnippetParser.PIPE, 0); }
		public TerminalNode COMMA() { return getToken(LSPSnippetParser.COMMA, 0); }
		public Placeholder_textContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_placeholder_text; }
	}

	public final Placeholder_textContext placeholder_text() throws RecognitionException {
		Placeholder_textContext _localctx = new Placeholder_textContext(_ctx, getState());
		enterRule(_localctx, 22, RULE_placeholder_text);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(99);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << TEXT) | (1L << ID) | (1L << INT) | (1L << OPEN) | (1L << COLON) | (1L << PIPE) | (1L << COMMA))) != 0)) ) {
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

	public static class Placeholder_escContext extends ParserRuleContext {
		public List<TerminalNode> BACK_SLASH() { return getTokens(LSPSnippetParser.BACK_SLASH); }
		public TerminalNode BACK_SLASH(int i) {
			return getToken(LSPSnippetParser.BACK_SLASH, i);
		}
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode CLOSE() { return getToken(LSPSnippetParser.CLOSE, 0); }
		public Placeholder_escContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_placeholder_esc; }
	}

	public final Placeholder_escContext placeholder_esc() throws RecognitionException {
		Placeholder_escContext _localctx = new Placeholder_escContext(_ctx, getState());
		enterRule(_localctx, 24, RULE_placeholder_esc);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(101);
			match(BACK_SLASH);
			setState(102);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << DOLLAR) | (1L << BACK_SLASH) | (1L << CLOSE))) != 0)) ) {
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

	public static class ChoiceContext extends ParserRuleContext {
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode OPEN() { return getToken(LSPSnippetParser.OPEN, 0); }
		public TerminalNode INT() { return getToken(LSPSnippetParser.INT, 0); }
		public List<TerminalNode> PIPE() { return getTokens(LSPSnippetParser.PIPE); }
		public TerminalNode PIPE(int i) {
			return getToken(LSPSnippetParser.PIPE, i);
		}
		public Choice_elementsContext choice_elements() {
			return getRuleContext(Choice_elementsContext.class,0);
		}
		public TerminalNode CLOSE() { return getToken(LSPSnippetParser.CLOSE, 0); }
		public ChoiceContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_choice; }
	}

	public final ChoiceContext choice() throws RecognitionException {
		ChoiceContext _localctx = new ChoiceContext(_ctx, getState());
		enterRule(_localctx, 26, RULE_choice);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(104);
			match(DOLLAR);
			setState(105);
			match(OPEN);
			setState(106);
			match(INT);
			setState(107);
			match(PIPE);
			setState(108);
			choice_elements();
			setState(109);
			match(PIPE);
			setState(110);
			match(CLOSE);
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

	public static class Choice_elementsContext extends ParserRuleContext {
		public List<Choice_elementContext> choice_element() {
			return getRuleContexts(Choice_elementContext.class);
		}
		public Choice_elementContext choice_element(int i) {
			return getRuleContext(Choice_elementContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LSPSnippetParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LSPSnippetParser.COMMA, i);
		}
		public Choice_elementsContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_choice_elements; }
	}

	public final Choice_elementsContext choice_elements() throws RecognitionException {
		Choice_elementsContext _localctx = new Choice_elementsContext(_ctx, getState());
		enterRule(_localctx, 28, RULE_choice_elements);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(120);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << TEXT) | (1L << ID) | (1L << NL) | (1L << CR) | (1L << INT) | (1L << BACK_SLASH) | (1L << OPEN) | (1L << COLON))) != 0)) {
				{
				setState(112);
				choice_element();
				setState(117);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(113);
					match(COMMA);
					setState(114);
					choice_element();
					}
					}
					setState(119);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
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

	public static class Choice_elementContext extends ParserRuleContext {
		public List<Choice_textContext> choice_text() {
			return getRuleContexts(Choice_textContext.class);
		}
		public Choice_textContext choice_text(int i) {
			return getRuleContext(Choice_textContext.class,i);
		}
		public List<Choice_escContext> choice_esc() {
			return getRuleContexts(Choice_escContext.class);
		}
		public Choice_escContext choice_esc(int i) {
			return getRuleContext(Choice_escContext.class,i);
		}
		public List<NewlineContext> newline() {
			return getRuleContexts(NewlineContext.class);
		}
		public NewlineContext newline(int i) {
			return getRuleContext(NewlineContext.class,i);
		}
		public Choice_elementContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_choice_element; }
	}

	public final Choice_elementContext choice_element() throws RecognitionException {
		Choice_elementContext _localctx = new Choice_elementContext(_ctx, getState());
		enterRule(_localctx, 30, RULE_choice_element);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(125); 
			_errHandler.sync(this);
			_la = _input.LA(1);
			do {
				{
				setState(125);
				_errHandler.sync(this);
				switch (_input.LA(1)) {
				case TEXT:
				case ID:
				case INT:
				case OPEN:
				case COLON:
					{
					setState(122);
					choice_text();
					}
					break;
				case BACK_SLASH:
					{
					setState(123);
					choice_esc();
					}
					break;
				case NL:
				case CR:
					{
					setState(124);
					newline();
					}
					break;
				default:
					throw new NoViableAltException(this);
				}
				}
				setState(127); 
				_errHandler.sync(this);
				_la = _input.LA(1);
			} while ( (((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << TEXT) | (1L << ID) | (1L << NL) | (1L << CR) | (1L << INT) | (1L << BACK_SLASH) | (1L << OPEN) | (1L << COLON))) != 0) );
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

	public static class Choice_textContext extends ParserRuleContext {
		public TerminalNode TEXT() { return getToken(LSPSnippetParser.TEXT, 0); }
		public TerminalNode ID() { return getToken(LSPSnippetParser.ID, 0); }
		public TerminalNode INT() { return getToken(LSPSnippetParser.INT, 0); }
		public TerminalNode OPEN() { return getToken(LSPSnippetParser.OPEN, 0); }
		public TerminalNode COLON() { return getToken(LSPSnippetParser.COLON, 0); }
		public Choice_textContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_choice_text; }
	}

	public final Choice_textContext choice_text() throws RecognitionException {
		Choice_textContext _localctx = new Choice_textContext(_ctx, getState());
		enterRule(_localctx, 32, RULE_choice_text);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(129);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << TEXT) | (1L << ID) | (1L << INT) | (1L << OPEN) | (1L << COLON))) != 0)) ) {
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

	public static class Choice_escContext extends ParserRuleContext {
		public List<TerminalNode> BACK_SLASH() { return getTokens(LSPSnippetParser.BACK_SLASH); }
		public TerminalNode BACK_SLASH(int i) {
			return getToken(LSPSnippetParser.BACK_SLASH, i);
		}
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode CLOSE() { return getToken(LSPSnippetParser.CLOSE, 0); }
		public TerminalNode PIPE() { return getToken(LSPSnippetParser.PIPE, 0); }
		public TerminalNode COMMA() { return getToken(LSPSnippetParser.COMMA, 0); }
		public Choice_escContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_choice_esc; }
	}

	public final Choice_escContext choice_esc() throws RecognitionException {
		Choice_escContext _localctx = new Choice_escContext(_ctx, getState());
		enterRule(_localctx, 34, RULE_choice_esc);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(131);
			match(BACK_SLASH);
			setState(132);
			_la = _input.LA(1);
			if ( !((((_la) & ~0x3f) == 0 && ((1L << _la) & ((1L << DOLLAR) | (1L << BACK_SLASH) | (1L << CLOSE) | (1L << PIPE) | (1L << COMMA))) != 0)) ) {
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

	public static class VariableContext extends ParserRuleContext {
		public Naked_variableContext naked_variable() {
			return getRuleContext(Naked_variableContext.class,0);
		}
		public Curly_variableContext curly_variable() {
			return getRuleContext(Curly_variableContext.class,0);
		}
		public Placeholder_variableContext placeholder_variable() {
			return getRuleContext(Placeholder_variableContext.class,0);
		}
		public VariableContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_variable; }
	}

	public final VariableContext variable() throws RecognitionException {
		VariableContext _localctx = new VariableContext(_ctx, getState());
		enterRule(_localctx, 36, RULE_variable);
		try {
			setState(137);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,10,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(134);
				naked_variable();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(135);
				curly_variable();
				}
				break;
			case 3:
				enterOuterAlt(_localctx, 3);
				{
				setState(136);
				placeholder_variable();
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

	public static class Variable_textContext extends ParserRuleContext {
		public List<TerminalNode> ID() { return getTokens(LSPSnippetParser.ID); }
		public TerminalNode ID(int i) {
			return getToken(LSPSnippetParser.ID, i);
		}
		public List<TerminalNode> INT() { return getTokens(LSPSnippetParser.INT); }
		public TerminalNode INT(int i) {
			return getToken(LSPSnippetParser.INT, i);
		}
		public Variable_textContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_variable_text; }
	}

	public final Variable_textContext variable_text() throws RecognitionException {
		Variable_textContext _localctx = new Variable_textContext(_ctx, getState());
		enterRule(_localctx, 38, RULE_variable_text);
		int _la;
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(139);
			match(ID);
			setState(143);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,11,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(140);
					_la = _input.LA(1);
					if ( !(_la==ID || _la==INT) ) {
					_errHandler.recoverInline(this);
					}
					else {
						if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
						_errHandler.reportMatch(this);
						consume();
					}
					}
					} 
				}
				setState(145);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,11,_ctx);
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

	public static class Naked_variableContext extends ParserRuleContext {
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public Variable_textContext variable_text() {
			return getRuleContext(Variable_textContext.class,0);
		}
		public Naked_variableContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_naked_variable; }
	}

	public final Naked_variableContext naked_variable() throws RecognitionException {
		Naked_variableContext _localctx = new Naked_variableContext(_ctx, getState());
		enterRule(_localctx, 40, RULE_naked_variable);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(146);
			match(DOLLAR);
			setState(147);
			variable_text();
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

	public static class Curly_variableContext extends ParserRuleContext {
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode OPEN() { return getToken(LSPSnippetParser.OPEN, 0); }
		public Variable_textContext variable_text() {
			return getRuleContext(Variable_textContext.class,0);
		}
		public TerminalNode CLOSE() { return getToken(LSPSnippetParser.CLOSE, 0); }
		public Curly_variableContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_curly_variable; }
	}

	public final Curly_variableContext curly_variable() throws RecognitionException {
		Curly_variableContext _localctx = new Curly_variableContext(_ctx, getState());
		enterRule(_localctx, 42, RULE_curly_variable);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(149);
			match(DOLLAR);
			setState(150);
			match(OPEN);
			setState(151);
			variable_text();
			setState(152);
			match(CLOSE);
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

	public static class Placeholder_variableContext extends ParserRuleContext {
		public TerminalNode DOLLAR() { return getToken(LSPSnippetParser.DOLLAR, 0); }
		public TerminalNode OPEN() { return getToken(LSPSnippetParser.OPEN, 0); }
		public Variable_textContext variable_text() {
			return getRuleContext(Variable_textContext.class,0);
		}
		public TerminalNode COLON() { return getToken(LSPSnippetParser.COLON, 0); }
		public Placeholder_contentContext placeholder_content() {
			return getRuleContext(Placeholder_contentContext.class,0);
		}
		public TerminalNode CLOSE() { return getToken(LSPSnippetParser.CLOSE, 0); }
		public Placeholder_variableContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_placeholder_variable; }
	}

	public final Placeholder_variableContext placeholder_variable() throws RecognitionException {
		Placeholder_variableContext _localctx = new Placeholder_variableContext(_ctx, getState());
		enterRule(_localctx, 44, RULE_placeholder_variable);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(154);
			match(DOLLAR);
			setState(155);
			match(OPEN);
			setState(156);
			variable_text();
			setState(157);
			match(COLON);
			setState(158);
			placeholder_content();
			setState(159);
			match(CLOSE);
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
		"\3\u608b\ua72a\u8133\ub9ed\u417c\u3be7\u7786\u5964\3\16\u00a4\4\2\t\2"+
		"\4\3\t\3\4\4\t\4\4\5\t\5\4\6\t\6\4\7\t\7\4\b\t\b\4\t\t\t\4\n\t\n\4\13"+
		"\t\13\4\f\t\f\4\r\t\r\4\16\t\16\4\17\t\17\4\20\t\20\4\21\t\21\4\22\t\22"+
		"\4\23\t\23\4\24\t\24\4\25\t\25\4\26\t\26\4\27\t\27\4\30\t\30\3\2\3\2\3"+
		"\2\3\2\3\2\7\2\66\n\2\f\2\16\29\13\2\3\3\3\3\3\4\3\4\3\4\3\5\3\5\3\6\3"+
		"\6\3\6\3\6\5\6F\n\6\3\7\3\7\3\b\3\b\5\bL\n\b\3\t\3\t\3\t\3\n\3\n\3\n\3"+
		"\n\3\n\3\13\3\13\3\13\3\13\3\13\3\13\3\13\3\f\3\f\3\f\3\f\7\fa\n\f\f\f"+
		"\16\fd\13\f\3\r\3\r\3\16\3\16\3\16\3\17\3\17\3\17\3\17\3\17\3\17\3\17"+
		"\3\17\3\20\3\20\3\20\7\20v\n\20\f\20\16\20y\13\20\5\20{\n\20\3\21\3\21"+
		"\3\21\6\21\u0080\n\21\r\21\16\21\u0081\3\22\3\22\3\23\3\23\3\23\3\24\3"+
		"\24\3\24\5\24\u008c\n\24\3\25\3\25\7\25\u0090\n\25\f\25\16\25\u0093\13"+
		"\25\3\26\3\26\3\26\3\27\3\27\3\27\3\27\3\27\3\30\3\30\3\30\3\30\3\30\3"+
		"\30\3\30\3\30\2\2\31\2\4\6\b\n\f\16\20\22\24\26\30\32\34\36 \"$&(*,.\2"+
		"\13\3\2\3\4\3\2\b\t\3\2\7\16\3\2\5\6\6\2\3\4\7\7\n\n\f\16\4\2\b\t\13\13"+
		"\6\2\3\4\7\7\n\n\f\f\5\2\b\t\13\13\r\16\4\2\4\4\7\7\2\u00a1\2\67\3\2\2"+
		"\2\4:\3\2\2\2\6<\3\2\2\2\b?\3\2\2\2\nE\3\2\2\2\fG\3\2\2\2\16K\3\2\2\2"+
		"\20M\3\2\2\2\22P\3\2\2\2\24U\3\2\2\2\26b\3\2\2\2\30e\3\2\2\2\32g\3\2\2"+
		"\2\34j\3\2\2\2\36z\3\2\2\2 \177\3\2\2\2\"\u0083\3\2\2\2$\u0085\3\2\2\2"+
		"&\u008b\3\2\2\2(\u008d\3\2\2\2*\u0094\3\2\2\2,\u0097\3\2\2\2.\u009c\3"+
		"\2\2\2\60\66\5\4\3\2\61\66\5\6\4\2\62\66\5\n\6\2\63\66\5\b\5\2\64\66\5"+
		"\f\7\2\65\60\3\2\2\2\65\61\3\2\2\2\65\62\3\2\2\2\65\63\3\2\2\2\65\64\3"+
		"\2\2\2\669\3\2\2\2\67\65\3\2\2\2\678\3\2\2\28\3\3\2\2\29\67\3\2\2\2:;"+
		"\t\2\2\2;\5\3\2\2\2<=\7\t\2\2=>\t\3\2\2>\7\3\2\2\2?@\t\4\2\2@\t\3\2\2"+
		"\2AF\5\16\b\2BF\5\24\13\2CF\5\34\17\2DF\5&\24\2EA\3\2\2\2EB\3\2\2\2EC"+
		"\3\2\2\2ED\3\2\2\2F\13\3\2\2\2GH\t\5\2\2H\r\3\2\2\2IL\5\20\t\2JL\5\22"+
		"\n\2KI\3\2\2\2KJ\3\2\2\2L\17\3\2\2\2MN\7\b\2\2NO\7\7\2\2O\21\3\2\2\2P"+
		"Q\7\b\2\2QR\7\n\2\2RS\7\7\2\2ST\7\13\2\2T\23\3\2\2\2UV\7\b\2\2VW\7\n\2"+
		"\2WX\7\7\2\2XY\7\f\2\2YZ\5\26\f\2Z[\7\13\2\2[\25\3\2\2\2\\a\5\n\6\2]a"+
		"\5\30\r\2^a\5\32\16\2_a\5\f\7\2`\\\3\2\2\2`]\3\2\2\2`^\3\2\2\2`_\3\2\2"+
		"\2ad\3\2\2\2b`\3\2\2\2bc\3\2\2\2c\27\3\2\2\2db\3\2\2\2ef\t\6\2\2f\31\3"+
		"\2\2\2gh\7\t\2\2hi\t\7\2\2i\33\3\2\2\2jk\7\b\2\2kl\7\n\2\2lm\7\7\2\2m"+
		"n\7\r\2\2no\5\36\20\2op\7\r\2\2pq\7\13\2\2q\35\3\2\2\2rw\5 \21\2st\7\16"+
		"\2\2tv\5 \21\2us\3\2\2\2vy\3\2\2\2wu\3\2\2\2wx\3\2\2\2x{\3\2\2\2yw\3\2"+
		"\2\2zr\3\2\2\2z{\3\2\2\2{\37\3\2\2\2|\u0080\5\"\22\2}\u0080\5$\23\2~\u0080"+
		"\5\f\7\2\177|\3\2\2\2\177}\3\2\2\2\177~\3\2\2\2\u0080\u0081\3\2\2\2\u0081"+
		"\177\3\2\2\2\u0081\u0082\3\2\2\2\u0082!\3\2\2\2\u0083\u0084\t\b\2\2\u0084"+
		"#\3\2\2\2\u0085\u0086\7\t\2\2\u0086\u0087\t\t\2\2\u0087%\3\2\2\2\u0088"+
		"\u008c\5*\26\2\u0089\u008c\5,\27\2\u008a\u008c\5.\30\2\u008b\u0088\3\2"+
		"\2\2\u008b\u0089\3\2\2\2\u008b\u008a\3\2\2\2\u008c\'\3\2\2\2\u008d\u0091"+
		"\7\4\2\2\u008e\u0090\t\n\2\2\u008f\u008e\3\2\2\2\u0090\u0093\3\2\2\2\u0091"+
		"\u008f\3\2\2\2\u0091\u0092\3\2\2\2\u0092)\3\2\2\2\u0093\u0091\3\2\2\2"+
		"\u0094\u0095\7\b\2\2\u0095\u0096\5(\25\2\u0096+\3\2\2\2\u0097\u0098\7"+
		"\b\2\2\u0098\u0099\7\n\2\2\u0099\u009a\5(\25\2\u009a\u009b\7\13\2\2\u009b"+
		"-\3\2\2\2\u009c\u009d\7\b\2\2\u009d\u009e\7\n\2\2\u009e\u009f\5(\25\2"+
		"\u009f\u00a0\7\f\2\2\u00a0\u00a1\5\26\f\2\u00a1\u00a2\7\13\2\2\u00a2/"+
		"\3\2\2\2\16\65\67EK`bwz\177\u0081\u008b\u0091";
	public static final ATN _ATN =
		new ATNDeserializer().deserialize(_serializedATN.toCharArray());
	static {
		_decisionToDFA = new DFA[_ATN.getNumberOfDecisions()];
		for (int i = 0; i < _ATN.getNumberOfDecisions(); i++) {
			_decisionToDFA[i] = new DFA(_ATN.getDecisionState(i), i);
		}
	}
}