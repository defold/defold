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

package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.Map;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Objects;
import java.util.logging.Logger;
import java.util.stream.Collectors;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import com.dynamo.bob.pipeline.LuaScanner.Property.Status;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import com.dynamo.bob.pipeline.antlr.lua.LuaParser;
import com.dynamo.bob.pipeline.antlr.lua.LuaLexer;
import com.dynamo.bob.pipeline.antlr.lua.LuaParserBaseListener;

import org.antlr.v4.runtime.CommonTokenStream;
import org.antlr.v4.runtime.tree.ParseTree;
import org.antlr.v4.runtime.tree.ParseTreeWalker;
import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.CharStream;
import org.antlr.v4.runtime.ParserRuleContext;
import org.antlr.v4.runtime.Token;
import org.antlr.v4.runtime.tree.TerminalNode;

public class LuaScanner extends LuaParserBaseListener {

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
         new String[] {
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
        }
    ));

    private static final Map<Integer, String> QUOTES = new HashMap<Integer, String>() {{
        put(LuaParser.NORMALSTRING, "\"");
        put(LuaParser.CHARSTRING, "'");
    }};

    private StringBuffer parsedBuffer = null;
    private CommonTokenStream tokenStream = null;

    private List<String> modules = new ArrayList<String>();
    private List<Property> properties = new ArrayList<Property>();

    public static class Property {
        public enum Status {
            OK,
            INVALID_ARGS,
            INVALID_VALUE
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
            if(variableCtx.getClass() == LuaParser.NamedvariableContext.class) {
                varName = ((LuaParser.NamedvariableContext)variableCtx).NAME().getText();
            }

            // indexed function call, like `_G.require()` or `go.property()` where 
            // `objectName` is `go` and `indexName` is `property`
            String indexName = null;
            if(variableCtx.getClass() == LuaParser.IndexContext.class) {
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
    public String parse(String str) {
        TimeProfiler.start("Parse");
        modules.clear();
        properties.clear();

        parsedBuffer = new StringBuffer(str);

        // set up the lexer and parser
        // walk the generated parse tree from the
        // first Lua chunk

        LuaLexer lexer = new LuaLexer(CharStreams.fromString(str));
        tokenStream = new CommonTokenStream(lexer);
        LuaParser parser = new LuaParser(tokenStream);
        ParseTreeWalker walker = new ParseTreeWalker();
        walker.walk(this, parser.chunk());
        TimeProfiler.stop();
        // return the parsed string
        return parsedBuffer.toString();
    }

    /**
     * Get the parsed Lua code
     * @return The parsed Lua code
     */
    public String getParsedLua() {
        return parsedBuffer.toString();
    }

    /**
     * Get a list of all Lua modules found by a call to parse().
     * @return List of Lua modules
     */
    public List<String> getModules() {
        return modules;
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
        int from = token.getStartIndex();
        int to = from + token.getText().length() - 1;
        for(int i = from; i <= to; i++) {
            parsedBuffer.replace(i, i + 1, " ");
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
     * Callback from ANTLR when a Lua chunk is encountered. We start from the
     * main chunk when we call parse() above. This means that the ChunkContext
     * will span the entire file and encompass all ANTLR tokens.
     * We use this callback to remove all comments.
     */
    @Override
    public void enterChunk(LuaParser.ChunkContext ctx) {
        List<Token> tokens = getTokens(ctx);
        for(Token token : tokens) {
            if (token.getChannel() == LuaLexer.COMMENT) {
                removeToken(token);
            }
        }
    }

    /**
     * Callback from ANTLR when a function is entered. We use this to grab all
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
            // and also don't add the same module twice
            if (module != null && !LUA_LIBRARIES.contains(module) && !modules.contains(module)) {
                modules.add(module);
            }
        }
        else if (fnDesc.is("property", "go")) {
            LuaParser.ArgsContext argsCtx = ctx.nameAndArgs().args();
            String firstArg = getFirstStringArg(argsCtx);
            List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
            Property property = new Property(tokens.get(0).getLine() - 1);
            property.name = firstArg;
            if (firstArg == null) {
                property.status = Status.INVALID_ARGS;
            } else {
                if (parsePropertyValue(argsCtx, property)) {
                    property.status = Status.OK;
                } else {
                    property.status = Status.INVALID_VALUE;
                }
            }
            properties.add(property);

            // strip property from code
            for (Token token : tokens) {
                removeToken(token);
            }

        }
    }

    private boolean parsePropertyValue(LuaParser.ArgsContext argsCtx, Property property) {
        boolean result = false;
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
                result = true;
            } else if (type == LuaParser.FALSE || type == LuaParser.TRUE) {
                property.type = PropertyType.PROPERTY_TYPE_BOOLEAN;
                property.value = Boolean.parseBoolean(initialToken.getText());
                result = true;
            } else if (type == LuaParser.NAME) {
                LuaParser.VariableContext varCtx = expCtx.variable();
                // function expected
                if (!(varCtx instanceof LuaParser.FunctioncallContext)) {
                    return false;
                }
                LuaParser.FunctioncallContext ctx = (LuaParser.FunctioncallContext)varCtx;
                FunctionDescriptor fnDesc = new FunctionDescriptor(ctx.variable());
                if (fnDesc.functionName == null) {
                    return result;
                }
                if (fnDesc.isObject("vmath")){
                     if (fnDesc.isName("vector3")) {
                        Vector3d v = new Vector3d();
                        double[] resultArgs = new double[3];
                        result = getNumArgs(ctx.nameAndArgs().args(), resultArgs);
                        v.set(resultArgs);
                        property.value = v;
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR3;
                    } else if (fnDesc.isName("vector4")) {
                        Vector4d v = new Vector4d();
                        double[] resultArgs = new double[4];
                        result = getNumArgs(ctx.nameAndArgs().args(), resultArgs);
                        v.set(resultArgs);
                        property.value = v;
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR4;
                    } else if (fnDesc.isName("quat")) {
                        Quat4d q = new Quat4d();
                        double[] resultArgs = new double[4];
                        result = getNumArgs(ctx.nameAndArgs().args(), resultArgs);
                        q.set(resultArgs);
                        property.value = q;
                        property.type = PropertyType.PROPERTY_TYPE_QUAT;
                    }
                }
                else {
                    String firstStrArg = getFirstStringArg(ctx.nameAndArgs().args());
                    if (fnDesc.isObject("resource")) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        property.isResource = true;
                        result = true;
                    }
                    else if (fnDesc.is("hash", null) || fnDesc.is("hash", "_G")) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        // hash(arg) requires an argument
                        if (firstStrArg != null) {
                            result = true;
                        }
                    }
                    else if (fnDesc.is("url", "msg")) {
                        property.type = PropertyType.PROPERTY_TYPE_URL;
                        result = true;
                    }
                    property.value = firstStrArg == null ? "" : firstStrArg;
                }
            }
        }
        return result;
    }

    /**
     * Callback from ANTLR when a statement is entered. We use this to remove
     * any stand-alone semicolon statements. The semicolon may cause problems
     * if it is at the end of a go.property call as it will be removed after it
     * has been parsed.
     * Note that semicolons used as field or return separators are not affected.
     */
    @Override public void enterStat(LuaParser.StatContext ctx) {
        List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
        if (tokens.size() == 1) {
            Token token = tokens.get(0);
            if (token.getText().equals(";")) {
                removeToken(token);
            }
        }
    }

}
