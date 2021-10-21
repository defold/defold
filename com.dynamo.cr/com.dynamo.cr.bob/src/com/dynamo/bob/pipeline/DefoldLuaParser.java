package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Set;
import java.util.HashSet;
import java.util.Arrays;
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

	/**
	 * This list of Lua libraries represent the Lua standard libraries as well
	 * as other very commonly used libraries that are included in the Defold
	 * version of Lua.
	 * Third-party Lua modules from the Lua developer community may include
	 * require() calls for these libraries and unless we exclude these require
	 * calls the build will fail since bob will be looking for a corresponding
	 * .lua module file
	 */
	private static final Set<String> LUA_LIBRARIES = new HashSet<String>(Arrays.asList(
	     new String[] {
	 		"coroutine",
	 		"package",
	 		"string",
	 		"table",
	 		"math",
	 		"io",
	 		"os",
	 		"debug",
	 		"socket",			// LuaSocket
	 		"bit",				// LuaJIT+Lua addon
	 		"ffi",				// LuaJIT
	 		"jit"				// LuaJIT
	 	}
	));

	public class PropertyAndLine {
		public String property;
		public int line;
		public PropertyAndLine(String property, int line) {
			this.property = property;
			this.line = line - 1;
		}
	}

	private StringBuffer parsedBuffer = null;
	private CommonTokenStream tokenStream = null;

	private List<String> modules = new ArrayList<String>();
	private List<PropertyAndLine> properties = new ArrayList<PropertyAndLine>();

	private boolean stripComments = false;
	private boolean stripProperties = false;

	public DefoldLuaParser() {}

	/**
	 * Remove Lua comments from the parsed Lua code
	 * @param strip true if comments should be removed
	 */
	public void setStripComments(boolean strip) {
		stripComments = strip;
	}

	/**
	 * Remove go.property() calls from the parsed Lua code
	 * @param strip true if go.property() calls should be removed
	 */
	public void setStripProperties(boolean strip) {
		stripProperties = strip;
	}

	/**
	 * Parse a string containing Lua code. This will detect things such
	 * as require() and go.property() calls and optionally strip comments and
	 * go.property() calls.
	 * @param str Lua code to parse
	 * @return Parsed string
	 */
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
	 * Get all property declarations found while parsing
	 * @return Game object properties
	 */
	public List<PropertyAndLine> getProperties() {
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

	/**
	 * Callback from ANTLR when a Lua chunk is encountered. We start from the
	 * main chunk when we call parse() above. This means that the ChunkContext
	 * will span the entire file and encompass all ANTLR tokens.
	 * We write all tokens to the string buffer, optionally ignoring
 	 * things such as code comments and the EOF marker
	 */
	@Override
	public void enterChunk(LuaParser.ChunkContext ctx) {
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

	/**
	 * Callback from ANTLR when a function is entered. We use this to grab all
	 * require() calls and all go.property() calls.
	 */
	@Override
	public void enterFunctioncall(LuaParser.FunctioncallContext ctx) {
		String text = ctx.getText();
		// require() call?
		if (text.startsWith("require")) {
			List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
			// token 0 is the require function
			// token 1 is either the first token of the require argument or the
			// parenthesis
			// if it is a parenthesis we make sure to skip over both the start
			// and end parenthesis
			int startIndex = 1;
			int endIndex = tokens.size() - 1;
			if (tokens.get(startIndex).getText().equals("(")) {
				startIndex++;

				// find matching right parenthesis
				int open = 1;
				for (int i=startIndex; i<=endIndex; i++) {
					String token = tokens.get(i).getText();
					if (token.equals(")")) {
						open = open - 1;
						if (open == 0) {
							endIndex = i - 1;
							break;
						}
					}
					else if (token.equals("(")) {
						open = open + 1;
					}
				}
			}

			// get the module name from the individual tokens
			String module = "";
			for (int i=startIndex; i<=endIndex; i++) {
				module += tokens.get(i).getText();
			}

			// check that it is a string and not a variable
			// remove the single or double quotes around the string
			if (module.startsWith("\"") || module.startsWith("'")) {
				module = module.replace("\"", "").replace("'", "");
				// ignore Lua+LuaJIT standard libraries + Defold additions such as LuaSocket
				if (!LUA_LIBRARIES.contains(module)) {
					modules.add(module);
				}
			}
		}
		// go.property() call?
		else if (text.startsWith("go.property")) {
			List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
			properties.add(new PropertyAndLine(text, tokens.get(0).getLine()));
			if (this.stripProperties) {
				for (Token token : tokens) {
					int from = token.getStartIndex();
					int to = from + token.getText().length();
					for(int i = from; i <= to; i++) {
						parsedBuffer.replace(i, i + 1, " ");
					}
				}
			}
		}
	}

	public static void main(String[] args) throws IOException {
		DefoldLuaParser listener = new DefoldLuaParser();
		String out = listener.parse(Files.readString(Path.of(args[0])));
		System.out.println(out);
	}
}
