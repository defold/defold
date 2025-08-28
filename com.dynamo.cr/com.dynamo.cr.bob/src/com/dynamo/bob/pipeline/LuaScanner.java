// Copyright 2020-2025 The Defold Foundation
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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.Map;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Objects;
import java.util.logging.Logger;
import java.util.stream.Collectors;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import com.dynamo.bob.pipeline.LuaScanner.Property.Status;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import com.dynamo.bob.pipeline.antlr.lua.LuaParser;
import com.dynamo.bob.pipeline.antlr.lua.LuaLexer;
import com.dynamo.bob.pipeline.antlr.lua.LuaParserBaseListener;

import org.antlr.v4.runtime.CommonTokenStream;
import org.antlr.v4.runtime.tree.ParseTreeWalker;
import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.ParserRuleContext;
import org.antlr.v4.runtime.Token;
import org.antlr.v4.runtime.tree.TerminalNode;
import org.antlr.v4.runtime.TokenStreamRewriter;

public class LuaScanner extends LuaParserBaseListener {

    public static class LuaScannerException extends Exception {
        private final String errorMessage;
        private final int lineNumber;

        public LuaScannerException(String errorMessage, int lineNumber) {
            super(String.format("Error: %s at line: %d", errorMessage, lineNumber));
            this.errorMessage = errorMessage;
            this.lineNumber = lineNumber;
        }

        public String getErrorMessage() {
            return errorMessage;
        }

        public int getLineNumber() {
            return lineNumber;
        }
    }

    private static final Logger LOGGER = Logger.getLogger(LuaScanner.class.getName());

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
            "coroutine",
            "package",
            "string",
            "table",
            "math",
            "io",
            "os",
            "debug",
            "socket",           // LuaSocket
            "bit",              // LuaJIT+Lua addon
            "ffi",              // LuaJIT
            "jit"               // LuaJIT
    ));

    private static final Set<String> LIFECYCLE_FUNCTIONS = new HashSet<String>(Arrays.asList(
            "init",
            "final",
            "update",
            "fixed_update",
            "on_message",
            "on_input",
            "on_reload"));

    private static final Map<Integer, String> QUOTES = new HashMap<>() {{
        put(LuaParser.NORMALSTRING, "\"");
        put(LuaParser.CHARSTRING, "'");
    }};

    private CommonTokenStream tokenStream = null;
    private TokenStreamRewriter rewriter;

    // Using LinkedHashSet keeps insertion order while giving O(1) look‑ups
    private final Set<String> modules = new LinkedHashSet<>();
    private final List<Property> properties = new ArrayList<>();
    public final List<LuaScannerException> exceptions = new ArrayList<>();

    private boolean isDebug;
    private int statDepth;

    public void setDebug() {
        isDebug = true;
    }

    public static class Property {
        public enum Status {
            OK,
            INVALID_ARGS,
            INVALID_VALUE,
            INVALID_LOCATION
        }

        /// Set if status != INVALID_ARGS
        public String name;
        /// Set if status == OK
        public PropertyType type;
        /// Set if status == OK
        public Object value;
        /// Always set
        public int line;
        /// Always set
        public Status status;
        /// Set if property is a resource, need to distinguish between
        /// regular hashes and properties for validation
        public boolean isResource;

        public Property(int line) {
            this.line = line;
        }
    }

    public static class FunctionDescriptor {

        public String functionName;
        public String objectName;

        public FunctionDescriptor(LuaParser.VariableContext variableCtx) {
            // simple function call, like `require()`
            String varName = null;
            if(variableCtx instanceof LuaParser.NamedvariableContext) {
                varName = ((LuaParser.NamedvariableContext)variableCtx).NAME().getText();
            }

            // indexed function call, like `_G.require()` or `go.property()` where
            // `objectName` is `go` and `indexName` is `property`
            String indexName = null;
            if(variableCtx instanceof LuaParser.IndexContext) {
                LuaParser.IndexContext indexVariableCtx = (LuaParser.IndexContext)variableCtx;
                TerminalNode nameNode = indexVariableCtx.NAME();
                if (nameNode != null) {
                    indexName = nameNode.getText();
                }
                objectName = indexVariableCtx.variable().getText();
            }
            functionName = (indexName == null) ?  varName : indexName;
        }

        public boolean is(String name, String objName) {
            return Objects.equals(name, functionName) && Objects.equals(objName, objectName);
        }

        public boolean isObject(String objName) {
            return Objects.equals(objName, objectName);
        }

        public boolean isName(String fnName) {
            return Objects.equals(fnName, functionName);
        }
    }

    public LuaScanner() {}

    /**
     * Parse a string containing Lua code. This will detect and strip
     * require() and go.property() calls
     * @param str Lua code to parse
     * @return Parsed string
     */
    public String parse(String str)  {
        TimeProfiler.start("Parse");
        modules.clear();
        properties.clear();

        // set up the lexer and parser
        // walk the generated parse tree from the
        // first Lua chunk

        LuaLexer lexer = new LuaLexer(CharStreams.fromString(str));
        tokenStream = new CommonTokenStream(lexer);
        rewriter = new TokenStreamRewriter(tokenStream);

        // Remove comments in rewriter
        tokenStream.fill();
        for (Token token : tokenStream.getTokens()) {
             if (token.getChannel() == LuaLexer.COMMENTS) {
                int type = token.getType();
                if (type == LuaLexer.LINE_COMMENT) {
                    // Single line comment
                    rewriter.replace(token, System.lineSeparator());
                }
                else if (type == LuaLexer.COMMENT) {
                    // Multiline comment
                    rewriter.replace(token, System.lineSeparator().repeat(token.getText().split("\r\n|\r|\n").length - 1));
                }
             }
        }

        // parse code
        LuaParser parser = new LuaParser(tokenStream);
        ParseTreeWalker walker = new ParseTreeWalker();
        walker.walk(this, parser.chunk());
        String resultText = rewriter.getText();
        TimeProfiler.stop();
        // return the parsed string
        return resultText;
    }

    /**
     * Get the parsed Lua code
     * @return The parsed Lua code
     */
    public String getParsedLua() {
        return rewriter.getText();
    }

    /**
     * Get a list of all Lua modules found by a call to parse().
     * @return List of Lua modules
     */
    public List<String> getModules() {
        // Preserve original ordering but avoid O(n) contains() look‑ups elsewhere
        return new ArrayList<>(modules);
    }

    /**
     * Get a list of all script properties found by a call to parse().
     * @return List of script properties
     */
    public List<Property> getProperties() {
        return properties;
    }

    // get all tokens spanning a context and belonging to a specific channel
    private List<Token> getTokens(ParserRuleContext ctx, int channel) {
        List<Token> tokens = getTokens(ctx);
        if (tokens == null) {
            return new ArrayList<Token>();
        }
        return tokens.stream().filter(t -> t.getChannel() == channel).collect(Collectors.toList());
    }

    // get all tokens spanning a context
    private List<Token> getTokens(ParserRuleContext ctx) {
        Token startToken = ctx.getStart();
        Token stopToken = ctx.getStop();
        return tokenStream.getTokens(startToken.getTokenIndex(), stopToken.getTokenIndex());
    }

    // replace the token with an empty string
    private void removeToken(Token token) {
        rewriter.delete(token);
    }

    private void removeTokens(List<Token> tokens) {
        for (Token token : tokens) {
            removeToken(token);
        }
    }

    private void removeTokens(List<Token> tokens, boolean shouldRemoveSemicolonAfter) {
        int lastTokenIndex = tokens.get(tokens.size() - 1).getTokenIndex();
        removeTokens(tokens);
        if (shouldRemoveSemicolonAfter) {
            int nextTokenIndex = lastTokenIndex + 1;
            Token token = rewriter.getTokenStream().get(nextTokenIndex);
             /**
             * We use this to remove semicolon statements in the end of line;
             * The semicolon may cause problems if it is at the end of a go.property call
             * as it will be removed after it has been parsed.
             */
            if (token != null && token.getType() == LuaLexer.SEMICOLON) {
                removeToken(token);
            }
        }
    }

    // returns first function argument only if it's a string, otherwise null
    private String getFirstStringArg(LuaParser.ArgsContext argsCtx) {
        if (argsCtx == null) {
            return null;
        }
        ParserRuleContext firstCtx = argsCtx.getRuleContext(ParserRuleContext.class, 0);
        if (firstCtx == null) {
            return null;
        }

        if (firstCtx.getRuleIndex() == LuaParser.RULE_explist) {
            firstCtx = ((LuaParser.ExplistContext)firstCtx).exp(0).getRuleContext(ParserRuleContext.class, 0);
        }
        if (firstCtx.getRuleIndex() != LuaParser.RULE_lstring) {
            return null;
        }
        Token initialToken = firstCtx.getStart();
        return initialToken.getText().replace(QUOTES.getOrDefault(initialToken.getType(), ""), "");
    }

    private List<String> getAllStringArgs(LuaParser.ArgsContext argsCtx) {
        if (argsCtx == null) {
            return null;
        }

        ParserRuleContext firstCtx = argsCtx.getRuleContext(ParserRuleContext.class, 0);
        if (firstCtx == null) {
            return null;
        }

        List<String> stringArgs = new ArrayList<>();

        // Check if firstCtx is an explist
        if (firstCtx.getRuleIndex() == LuaParser.RULE_explist) {
            LuaParser.ExplistContext explistCtx = (LuaParser.ExplistContext) firstCtx;

            // Iterate through each expression in the explist
            for (LuaParser.ExpContext expCtx : explistCtx.exp()) {
                ParserRuleContext expChildCtx = expCtx.getRuleContext(ParserRuleContext.class, 0);

                if (expChildCtx != null) {
                    if (expChildCtx.getRuleIndex() == LuaParser.RULE_lstring) {
                        Token token = expChildCtx.getStart();
                        String stringArg = token.getText().replace(QUOTES.getOrDefault(token.getType(), ""), "");
                        stringArgs.add(stringArg);
                    } else if (expChildCtx instanceof LuaParser.FunctioncallContext) {
                        LuaParser.FunctioncallContext fnCtx = ((LuaParser.FunctioncallContext) expChildCtx);
                        String firstString = getFirstStringArg(fnCtx.nameAndArgs().args());
                        stringArgs.add(firstString);
                    }
                }
                else {
                    stringArgs.add(null);
                }
            }
        }

        return stringArgs;
    }

    // returns boolean if parsing successfull and fill double[] with parsed values with needed length.
    private boolean getNumArgs(LuaParser.ArgsContext argsCtx, double[] resultArgs) {
        LuaParser.ExplistContext expListCtx = argsCtx.explist();
        // no values for example vmath.vector3()
        if (expListCtx != null) {
            List<LuaParser.ExpContext> args = expListCtx.exp();
            int count = 0;
            for(LuaParser.ExpContext val : args) {
                LuaParser.NumberContext num = val.number();
                LuaParser.ExpContext exp = val.exp(0);
                int firstTokenType = val.getStart().getType();
                if (num != null || (exp != null && exp.number() != null && firstTokenType == LuaParser.MINUS)) {
                    resultArgs[count] = Double.parseDouble(val.getText());
                    count++;
                }
                else {
                    // the value isn't a number
                    return false;
                }

            }
            // one value for example vmath.vector3(1), is valid, and all the values should be filled
            if (count == 1) {
                for (int i = count; i < resultArgs.length; i ++) {
                   resultArgs[i] = resultArgs[0];
                }
            }
            else if (count != resultArgs.length) {
                return false;
            }
        }
        return true;
    }

    /**
     * Callback from ANTLR when a function call is entered. We use this to grab all
     * require() calls and all go.property() calls.
     */
    @Override
    public void enterFunctioncall(LuaParser.FunctioncallContext ctx) {
        LuaParser.VariableContext varCtx = ctx.variable();
        FunctionDescriptor fnDesc = new FunctionDescriptor(varCtx);

        if (fnDesc.functionName == null) {
            return;
        }

        if (fnDesc.is("require", null) || fnDesc.is("require", "_G")) {
            String module = getFirstStringArg(ctx.nameAndArgs().args());
            // ignore Lua+LuaJIT standard libraries + Defold additions such as LuaSocket
            if (module != null && !LUA_LIBRARIES.contains(module)) {
                modules.add(module);      // LinkedHashSet prevents duplicates in O(1)
            }
        }
        else if (fnDesc.is("property", "go")) {
            ParserRuleContext paramsContext;
            LuaParser.ArgsContext argsCtx = ctx.nameAndArgs().args();
            paramsContext = argsCtx;
            String firstArg = getFirstStringArg(argsCtx);
            List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
            Property property = new Property(tokens.get(0).getLine() - 1);
            property.name = firstArg;
            if (statDepth > 1) {
                property.status = Status.INVALID_LOCATION;
            } else if (firstArg == null) {
                property.status = Status.INVALID_ARGS;
            } else {
                try {
                    paramsContext = parsePropertyValue(argsCtx, property);
                } catch (LuaScannerException e) {
                    exceptions.add(e);
                }
                if (paramsContext != null) {
                    property.status = Status.OK;
                } else {
                    property.status = Status.INVALID_VALUE;
                }
            }
            properties.add(property);

            // strip property from code
            // keep tokens for hash() in debug build
            // see https://github.com/defold/defold/issues/7422
            if (isDebug && !property.isResource && property.type == PropertyType.PROPERTY_TYPE_HASH) {
                tokens.removeAll(getTokens(paramsContext));
            }
            removeTokens(tokens, true);
        }
    }

    /**
     * Callback from ANTLR when a function is entered. We use this to grab and remove
     * all empty lifecycle functions. 
     */
    @Override
    public void enterFunctionstat(LuaParser.FunctionstatContext ctx) {
        LuaParser.FuncnameContext funcNameCtx = ctx.funcname();
        TerminalNode funcName = funcNameCtx.NAME(1);
        TerminalNode objName = null;
        if (funcName == null) {
            funcName = funcNameCtx.NAME(0);
        }
        else {
            objName = funcNameCtx.NAME(0);
        }
        if (objName == null || objName.getText().equals("_G")) {
            if (LIFECYCLE_FUNCTIONS.contains(funcName.getText())) {
                LuaParser.BlockContext blockCtx = ctx.funcbody().block();
                if (blockCtx.stat().isEmpty() && blockCtx.retstat() == null) {
                    List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
                    removeTokens(tokens);
                }
            }
        }
    }

    private ParserRuleContext parsePropertyValue(LuaParser.ArgsContext argsCtx, Property property) throws LuaScannerException {
        ParserRuleContext resultContext = null;
        List<LuaParser.ExpContext> expCtxList = ((LuaParser.ExplistContext)argsCtx.getRuleContext(ParserRuleContext.class, 0)).exp();
        // go.property(name, vaule) should have a value and only one value
        if (expCtxList.size() == 2) {
            LuaParser.ExpContext expCtx = expCtxList.get(1);
            Token initialToken = expCtx.getStart();
            int type = initialToken.getType();
            // for negative numbers we should take token #2
            if (type == LuaParser.MINUS) {
                List<Token> tokens = getTokens(expCtx, Token.DEFAULT_CHANNEL);
                if (tokens.size() > 1) {
                    initialToken = tokens.get(1);
                    type = initialToken.getType();
                }
            }
            if (type == LuaParser.INT || type == LuaParser.HEX || type == LuaParser.FLOAT || type == LuaParser.HEX_FLOAT) {
                property.type = PropertyType.PROPERTY_TYPE_NUMBER;
                property.value = Double.parseDouble(expCtx.getText());
                resultContext = expCtx;
            } else if (type == LuaParser.FALSE || type == LuaParser.TRUE) {
                property.type = PropertyType.PROPERTY_TYPE_BOOLEAN;
                property.value = Boolean.parseBoolean(initialToken.getText());
                resultContext = expCtx;
            } else if (type == LuaParser.NAME) {
                LuaParser.VariableContext varCtx = expCtx.variable();
                // function expected
                if (!(varCtx instanceof LuaParser.FunctioncallContext)) {
                    return expCtx;
                }
                LuaParser.FunctioncallContext ctx = (LuaParser.FunctioncallContext)varCtx;
                FunctionDescriptor fnDesc = new FunctionDescriptor(ctx.variable());
                if (fnDesc.functionName == null) {
                    return null;
                }
                if (fnDesc.isObject("vmath")){
                     if (fnDesc.isName("vector3")) {
                        Vector3d v = new Vector3d();
                        double[] resultArgs = new double[3];
                        resultContext = getNumArgs(ctx.nameAndArgs().args(), resultArgs) ? ctx : null;
                        v.set(resultArgs);
                        property.value = v;
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR3;
                    } else if (fnDesc.isName("vector4")) {
                        Vector4d v = new Vector4d();
                        double[] resultArgs = new double[4];
                        resultContext = getNumArgs(ctx.nameAndArgs().args(), resultArgs) ? ctx : null;
                        v.set(resultArgs);
                        property.value = v;
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR4;
                    } else if (fnDesc.isName("quat")) {
                        Quat4d q = new Quat4d();
                        double[] resultArgs = new double[4];
                        resultContext = getNumArgs(ctx.nameAndArgs().args(), resultArgs) ? ctx : null;
                        q.set(resultArgs);
                        property.value = q;
                        property.type = PropertyType.PROPERTY_TYPE_QUAT;
                    }
                }
                else {
                    if (fnDesc.isObject("resource")) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        property.isResource = true;
                        resultContext = ctx;
                        String firstStrArg = getFirstStringArg(ctx.nameAndArgs().args());
                        property.value = firstStrArg == null ? "" : firstStrArg;
                    }
                    else if (fnDesc.is("hash", null) || fnDesc.is("hash", "_G")) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        String firstStrArg = getFirstStringArg(ctx.nameAndArgs().args());
                        // hash(arg) requires an argument
                        if (firstStrArg != null) {
                            resultContext = ctx;
                        }
                        property.value = firstStrArg == null ? "" : firstStrArg;
                    }
                    else if (fnDesc.is("url", "msg")) {
                        property.type = PropertyType.PROPERTY_TYPE_URL;
                        resultContext = ctx;
                        List<String> allArgs = getAllStringArgs(ctx.nameAndArgs().args());
                        try {
                            property.value = concatenateUrl(allArgs);
                        } catch (Exception e) {
                            throw new LuaScannerException(e.getMessage(), ctx.start.getLine());
                        }
                    }
                }
            }
        }
        return resultContext;
    }

    @Override
    public void enterStat(LuaParser.StatContext ctx) {
        statDepth++;
    }

    @Override
    public void exitStat(LuaParser.StatContext ctx) {
        statDepth--;
    }

    public static String concatenateUrl(List<String> allArgs) throws Exception {
        if (allArgs == null || allArgs.isEmpty()) {
            return "";
        }
        if (allArgs.contains(null)) {
            throw new Exception("`nil` can't be used in `go.property(msg.url(_))`");
        }
        if (allArgs.size() == 1) {
            return allArgs.get(0);
        }
        if (allArgs.size() != 3) {
            throw new Exception("The URL may have only one or three strings in it");
        }
        return allArgs.get(0) + ":" + allArgs.get(1) + "#" + allArgs.get(2);
    }
}
