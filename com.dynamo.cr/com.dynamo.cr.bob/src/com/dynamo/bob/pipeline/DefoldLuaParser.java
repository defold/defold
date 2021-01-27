package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.ArrayList;
import java.util.logging.Logger;
import java.util.stream.Collectors;

import org.antlr.v4.runtime.CommonTokenStream;
import org.antlr.v4.runtime.tree.ParseTree;
import org.antlr.v4.runtime.tree.ParseTreeWalker;
import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.CharStream;
import org.antlr.v4.runtime.ParserRuleContext;
import org.antlr.v4.runtime.Token;

import com.dynamo.bob.pipeline.luaparser.LuaParser;
import com.dynamo.bob.pipeline.luaparser.LuaLexer;
import com.dynamo.bob.pipeline.luaparser.LuaParserBaseListener;


public class DefoldLuaParser extends LuaParserBaseListener {
	private static final Logger LOGGER = Logger.getLogger(DefoldLuaParser.class.getName());

	private StringBuffer parsedBuffer = null;
	private CommonTokenStream tokenStream = null;

	private List<String> modules = new ArrayList<String>();
	private List<String> properties = new ArrayList<String>();

	private boolean stripComments = false;
	private boolean stripProperties = false;

	public DefoldLuaParser() {}

	public void setStripComments(boolean strip) {
		stripComments = strip;
	}

	public void setStripProperties(boolean strip) {
		stripProperties = strip;
	}

	public String parse(String str) {
		// build empty buffer with only spaces and newlines
		// matching the structure of the string
		parsedBuffer = new StringBuffer();
		for(int i=0; i<str.length(); i++) {
			char c = str.charAt(i);
			if (c == '\n') {
				parsedBuffer.append("\n");
			}
			else {
				parsedBuffer.append(" ");
			}
		}

		// set up the lexer and parser
		// walk the generated parse tree from the
		// first Lua chunk
		LuaLexer lexer = new LuaLexer(CharStreams.fromString(str));
		tokenStream = new CommonTokenStream(lexer);
		LuaParser parser = new LuaParser(tokenStream);
		ParseTreeWalker walker = new ParseTreeWalker();
		walker.walk(this, parser.chunk());

		// return the parsed string
		return parsedBuffer.toString();
	}

	/**
	 * Get all modules found while parsing
	 * @return Lua modules
	 */
	public List<String> getModules() {
		return modules;
	}

	/**
	 * Get all proprty declarations found while parsing
	 * @return Game object properties
	 */
	public List<String> getProperties() {
		return properties;
	}

	// get all tokens spanning a context and belonging to a specific channel
	private List<Token> getTokens(ParserRuleContext ctx, int channel) {
		List<Token> tokens = getTokens(ctx);
		return tokens.stream().filter(t -> t.getChannel() == channel).collect(Collectors.toList());
	}

	// get all tokens spanning a context
	private List<Token> getTokens(ParserRuleContext ctx) {
		Token startToken = ctx.getStart();
		Token stopToken = ctx.getStop();
		return tokenStream.getTokens(startToken.getTokenIndex(), stopToken.getTokenIndex());
	}

	// remove a token from the parsed buffer
	private void removeToken(Token token) {
		int from = token.getStartIndex();
		int to = from + token.getText().length();
		for(int i = from; i <= to; i++) {
			parsedBuffer.replace(i, i + 1, " ");
		}
	}

	// callback when a function call is found while walking the tree
	private void handleFunctionCall(ParserRuleContext ctx) {
		String text = ctx.getText();
		if (text.startsWith("require")) {
			List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
			// token 0 is the require function
			// token 1 is either the string or parenthesis
			// if it is a parenthesis we instead get token 2
			String module = tokens.get(1).getText();
			if (module.equals("(")) {
				module = tokens.get(2).getText();
			}
			// remove the single or double quotes around the string
			module = module.replace("\"", "").replace("'", "");
			modules.add(module);
		}
		else if (text.startsWith("go.property")) {
			properties.add(text);
			if (this.stripProperties) {
				List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
				for (Token token : tokens) {
					removeToken(token);
				}
			}
		}
	}

	// Write tokens spanning the parse rule context to the string buffer, ignoring
	// things such as code comments and the EOF marker
	private void writeTokens(ParserRuleContext ctx) {
		List<Token> tokens = getTokens(ctx);
		for(Token token : tokens) {
			// only include tokens from the default channel (as defined in Lua.g4)
			// ignore EOF token
			if ((token.getChannel() == Token.DEFAULT_CHANNEL || !this.stripComments) && token.getType() != Token.EOF) {
				String text = token.getText();
				int start = token.getStartIndex();
				parsedBuffer.replace(start, start+text.length(), text);
			}
		}
	}

	private void log(String s) {
		LOGGER.info(s);
	}
	private void log(ParserRuleContext ctx) {
		log("ctx: " + ctx.getText() + " start token: " + ctx.getStart().getText() + " stop token: " + ctx.getStop().getText());
	}

	private void enterContext(ParserRuleContext ctx) {
		log(ctx);
	}

	private void exitContext(ParserRuleContext ctx) {
		log(ctx);
	}

	@Override public void enterChunk(LuaParser.ChunkContext ctx) { writeTokens(ctx); }
	// @Override public void exitChunk(LuaParser.ChunkContext ctx) { exitContext(ctx); }
	@Override public void enterFunctioncall(LuaParser.FunctioncallContext ctx) { handleFunctionCall(ctx); }
	// @Override public void exitFunctioncall(LuaParser.FunctioncallContext ctx) { exitContext(ctx); }

	// @Override public void enterBlock(LuaParser.BlockContext ctx) { enterContext(ctx); }
	// @Override public void exitBlock(LuaParser.BlockContext ctx) { exitContext(ctx); }
	// @Override public void enterStat(LuaParser.StatContext ctx) { enterContext(ctx); }
	// @Override public void exitStat(LuaParser.StatContext ctx) { exitContext(ctx); }
	// @Override public void enterBreakstat(LuaParser.BreakstatContext ctx) { enterContext(ctx); }
	// @Override public void exitBreakstat(LuaParser.BreakstatContext ctx) { exitContext(ctx); }
	// @Override public void enterGotostat(LuaParser.GotostatContext ctx) { enterContext(ctx); }
	// @Override public void exitGotostat(LuaParser.GotostatContext ctx) { exitContext(ctx); }
	// @Override public void enterDostat(LuaParser.DostatContext ctx) { enterContext(ctx); }
	// @Override public void exitDostat(LuaParser.DostatContext ctx) { exitContext(ctx); }
	// @Override public void enterWhilestat(LuaParser.WhilestatContext ctx) { enterContext(ctx); }
	// @Override public void exitWhilestat(LuaParser.WhilestatContext ctx) { exitContext(ctx); }
	// @Override public void enterRepeatstat(LuaParser.RepeatstatContext ctx) { enterContext(ctx); }
	// @Override public void exitRepeatstat(LuaParser.RepeatstatContext ctx) { exitContext(ctx); }
	// @Override public void enterIfstat(LuaParser.IfstatContext ctx) { enterContext(ctx); }
	// @Override public void exitIfstat(LuaParser.IfstatContext ctx) { exitContext(ctx); }
	// @Override public void enterGenericforstat(LuaParser.GenericforstatContext ctx) { enterContext(ctx); }
	// @Override public void exitGenericforstat(LuaParser.GenericforstatContext ctx) { exitContext(ctx); }
	// @Override public void enterNumericforstat(LuaParser.NumericforstatContext ctx) { enterContext(ctx); }
	// @Override public void exitNumericforstat(LuaParser.NumericforstatContext ctx) { exitContext(ctx); }
	// @Override public void enterFunctionstat(LuaParser.FunctionstatContext ctx) { enterContext(ctx); }
	// @Override public void exitFunctionstat(LuaParser.FunctionstatContext ctx) { exitContext(ctx); }
	// @Override public void enterLocalfunctionstat(LuaParser.LocalfunctionstatContext ctx) { enterContext(ctx); }
	// @Override public void exitLocalfunctionstat(LuaParser.LocalfunctionstatContext ctx) { exitContext(ctx); }
	// @Override public void enterLocalvariablestat(LuaParser.LocalvariablestatContext ctx) { enterContext(ctx); }
	// @Override public void exitLocalvariablestat(LuaParser.LocalvariablestatContext ctx) { exitContext(ctx); }
	// @Override public void enterVariablestat(LuaParser.VariablestatContext ctx) { enterContext(ctx); }
	// @Override public void exitVariablestat(LuaParser.VariablestatContext ctx) { exitContext(ctx); }
	// @Override public void enterAttnamelist(LuaParser.AttnamelistContext ctx) { enterContext(ctx); }
	// @Override public void exitAttnamelist(LuaParser.AttnamelistContext ctx) { exitContext(ctx); }
	// @Override public void enterAttrib(LuaParser.AttribContext ctx) { enterContext(ctx); }
	// @Override public void exitAttrib(LuaParser.AttribContext ctx) { exitContext(ctx); }
	// @Override public void enterRetstat(LuaParser.RetstatContext ctx) { enterContext(ctx); }
	// @Override public void exitRetstat(LuaParser.RetstatContext ctx) { exitContext(ctx); }
	// @Override public void enterLabel(LuaParser.LabelContext ctx) { enterContext(ctx); }
	// @Override public void exitLabel(LuaParser.LabelContext ctx) { exitContext(ctx); }
	// @Override public void enterFuncname(LuaParser.FuncnameContext ctx) { enterContext(ctx); }
	// @Override public void exitFuncname(LuaParser.FuncnameContext ctx) { exitContext(ctx); }
	// @Override public void enterVariablelist(LuaParser.VariablelistContext ctx) { enterContext(ctx); }
	// @Override public void exitVariablelist(LuaParser.VariablelistContext ctx) { exitContext(ctx); }
	// @Override public void enterNamelist(LuaParser.NamelistContext ctx) { enterContext(ctx); }
	// @Override public void exitNamelist(LuaParser.NamelistContext ctx) { exitContext(ctx); }
	// @Override public void enterExplist(LuaParser.ExplistContext ctx) { enterContext(ctx); }
	// @Override public void exitExplist(LuaParser.ExplistContext ctx) { exitContext(ctx); }
	// @Override public void enterExp(LuaParser.ExpContext ctx) { enterContext(ctx); }
	// @Override public void exitExp(LuaParser.ExpContext ctx) { exitContext(ctx); }
	// @Override public void enterNamedvariable(LuaParser.NamedvariableContext ctx) { enterContext(ctx); }
	// @Override public void exitNamedvariable(LuaParser.NamedvariableContext ctx) { exitContext(ctx); }
	// @Override public void enterParenthesesvariable(LuaParser.ParenthesesvariableContext ctx) { enterContext(ctx); }
	// @Override public void exitParenthesesvariable(LuaParser.ParenthesesvariableContext ctx) { exitContext(ctx); }
	// @Override public void enterIndex(LuaParser.IndexContext ctx) { enterContext(ctx); }
	// @Override public void exitIndex(LuaParser.IndexContext ctx) { exitContext(ctx); }
	// @Override public void enterNameAndArgs(LuaParser.NameAndArgsContext ctx) { enterContext(ctx); }
	// @Override public void exitNameAndArgs(LuaParser.NameAndArgsContext ctx) { exitContext(ctx); }
	// @Override public void enterArgs(LuaParser.ArgsContext ctx) { enterContext(ctx); }
	// @Override public void exitArgs(LuaParser.ArgsContext ctx) { exitContext(ctx); }
	// @Override public void enterFunctiondef(LuaParser.FunctiondefContext ctx) { enterContext(ctx); }
	// @Override public void exitFunctiondef(LuaParser.FunctiondefContext ctx) { exitContext(ctx); }
	// @Override public void enterFuncbody(LuaParser.FuncbodyContext ctx) { enterContext(ctx); }
	// @Override public void exitFuncbody(LuaParser.FuncbodyContext ctx) { exitContext(ctx); }
	// @Override public void enterParlist(LuaParser.ParlistContext ctx) { enterContext(ctx); }
	// @Override public void exitParlist(LuaParser.ParlistContext ctx) { exitContext(ctx); }
	// @Override public void enterTableconstructor(LuaParser.TableconstructorContext ctx) { enterContext(ctx); }
	// @Override public void exitTableconstructor(LuaParser.TableconstructorContext ctx) { exitContext(ctx); }
	// @Override public void enterFieldlist(LuaParser.FieldlistContext ctx) { enterContext(ctx); }
	// @Override public void exitFieldlist(LuaParser.FieldlistContext ctx) { exitContext(ctx); }
	// @Override public void enterField(LuaParser.FieldContext ctx) { enterContext(ctx); }
	// @Override public void exitField(LuaParser.FieldContext ctx) { exitContext(ctx); }
	// @Override public void enterFieldsep(LuaParser.FieldsepContext ctx) { enterContext(ctx); }
	// @Override public void exitFieldsep(LuaParser.FieldsepContext ctx) { exitContext(ctx); }
	// @Override public void enterOperatorOr(LuaParser.OperatorOrContext ctx) { enterContext(ctx); }
	// @Override public void exitOperatorOr(LuaParser.OperatorOrContext ctx) { exitContext(ctx); }
	// @Override public void enterOperatorAnd(LuaParser.OperatorAndContext ctx) { enterContext(ctx); }
	// @Override public void exitOperatorAnd(LuaParser.OperatorAndContext ctx) { exitContext(ctx); }
	// @Override public void enterLessthan(LuaParser.LessthanContext ctx) { enterContext(ctx); }
	// @Override public void exitLessthan(LuaParser.LessthanContext ctx) { exitContext(ctx); }
	// @Override public void enterGreaterthan(LuaParser.GreaterthanContext ctx) { enterContext(ctx); }
	// @Override public void exitGreaterthan(LuaParser.GreaterthanContext ctx) { exitContext(ctx); }
	// @Override public void enterLessthanorequal(LuaParser.LessthanorequalContext ctx) { enterContext(ctx); }
	// @Override public void exitLessthanorequal(LuaParser.LessthanorequalContext ctx) { exitContext(ctx); }
	// @Override public void enterGreaterthanorequal(LuaParser.GreaterthanorequalContext ctx) { enterContext(ctx); }
	// @Override public void exitGreaterthanorequal(LuaParser.GreaterthanorequalContext ctx) { exitContext(ctx); }
	// @Override public void enterNotequal(LuaParser.NotequalContext ctx) { enterContext(ctx); }
	// @Override public void exitNotequal(LuaParser.NotequalContext ctx) { exitContext(ctx); }
	// @Override public void enterEqual(LuaParser.EqualContext ctx) { enterContext(ctx); }
	// @Override public void exitEqual(LuaParser.EqualContext ctx) { exitContext(ctx); }
	// @Override public void enterOperatorStrcat(LuaParser.OperatorStrcatContext ctx) { enterContext(ctx); }
	// @Override public void exitOperatorStrcat(LuaParser.OperatorStrcatContext ctx) { exitContext(ctx); }
	// @Override public void enterOperatorAddSub(LuaParser.OperatorAddSubContext ctx) { enterContext(ctx); }
	// @Override public void exitOperatorAddSub(LuaParser.OperatorAddSubContext ctx) { exitContext(ctx); }
	// @Override public void enterOperatorMulDivMod(LuaParser.OperatorMulDivModContext ctx) { enterContext(ctx); }
	// @Override public void exitOperatorMulDivMod(LuaParser.OperatorMulDivModContext ctx) { exitContext(ctx); }
	// @Override public void enterOperatorBitwise(LuaParser.OperatorBitwiseContext ctx) { enterContext(ctx); }
	// @Override public void exitOperatorBitwise(LuaParser.OperatorBitwiseContext ctx) { exitContext(ctx); }
	// @Override public void enterOperatorUnary(LuaParser.OperatorUnaryContext ctx) { enterContext(ctx); }
	// @Override public void exitOperatorUnary(LuaParser.OperatorUnaryContext ctx) { exitContext(ctx); }
	// @Override public void enterOperatorPower(LuaParser.OperatorPowerContext ctx) { enterContext(ctx); }
	// @Override public void exitOperatorPower(LuaParser.OperatorPowerContext ctx) { exitContext(ctx); }
	// @Override public void enterNumber(LuaParser.NumberContext ctx) { enterContext(ctx); }
	// @Override public void exitNumber(LuaParser.NumberContext ctx) { exitContext(ctx); }
	// @Override public void enterLstring(LuaParser.LstringContext ctx) { enterContext(ctx); }
	// @Override public void exitLstring(LuaParser.LstringContext ctx) { exitContext(ctx); }
	// @Override public void enterEveryRule(ParserRuleContext ctx) { enterContext(ctx); }
	// @Override public void exitEveryRule(ParserRuleContext ctx) { exitContext(ctx); }
	// @Override public void visitTerminal(TerminalNode node) { }
	// @Override public void visitErrorNode(ErrorNode node) { }

	public static void main(String[] args) throws IOException {
		DefoldLuaParser listener = new DefoldLuaParser();
		String out = listener.parse(Files.readString(Path.of(args[0])));
		System.out.println(out);
	}
}
