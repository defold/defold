// Copyright 2020-2022 The Defold Foundation
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
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.List;
import java.util.Set;
import java.util.HashSet;
import java.util.logging.Logger;
import java.util.stream.Collectors;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import com.dynamo.bob.pipeline.LuaScanner.Property.Status;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import com.dynamo.bob.pipeline.antlr.LuaParser;
import com.dynamo.bob.pipeline.antlr.LuaLexer;
import com.dynamo.bob.pipeline.antlr.LuaParserBaseListener;

import org.antlr.v4.runtime.CommonTokenStream;
import org.antlr.v4.runtime.tree.ParseTree;
import org.antlr.v4.runtime.tree.ParseTreeWalker;
import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.CharStream;
import org.antlr.v4.runtime.ParserRuleContext;
import org.antlr.v4.runtime.Token;

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

    private static Pattern propertyDeclPattern = Pattern.compile("go.property\\s*?\\((.*?)\\);?(\\s*?--.*?)?$");
    private static Pattern propertyArgsPattern = Pattern.compile("[\"'](.*?)[\"']\\s*,(.*)");

    // http://docs.python.org/dev/library/re.html#simulating-scanf
    private static Pattern numPattern = Pattern.compile("[-+]?(\\d+(\\.\\d*)?|\\.\\d+)([eE][-+]?\\d+)?");
    private static Pattern hashPattern = Pattern.compile("hash\\s*\\([\"'](.*?)[\"']\\)");
    private static Pattern urlPattern = Pattern.compile("msg\\.url\\s*\\(([\"'](.*?)[\"']|)?\\)");
    private static Pattern vec3Pattern1 = Pattern.compile("vmath\\.vector3\\s*\\(((.+?),(.+?),(.+?))\\)");
    private static Pattern vec3Pattern2 = Pattern.compile("vmath\\.vector3\\s*\\(((.+?))\\)");
    private static Pattern vec3Pattern3 = Pattern.compile("vmath\\.vector3\\s*\\(()\\)");
    private static Pattern vec4Pattern1 = Pattern.compile("vmath\\.vector4\\s*\\(((.+?),(.+?),(.+?),(.+?))\\)");
    private static Pattern vec4Pattern2 = Pattern.compile("vmath\\.vector4\\s*\\(((.+?))\\)");
    private static Pattern vec4Pattern3 = Pattern.compile("vmath\\.vector4\\s*\\(()\\)");
    private static Pattern quatPattern = Pattern.compile("vmath\\.quat\\s*\\(((.*?),(.*?),(.*?),(.*?)|)\\)");
    private static Pattern boolPattern = Pattern.compile("(false|true)");
    private static Pattern resourcePattern = Pattern.compile("resource\\.(.*?)\\s*\\(([\"'](.*?)[\"']|)?\\)");
    private static Pattern[] patterns = new Pattern[] { numPattern, hashPattern, urlPattern,
            vec3Pattern1, vec3Pattern2, vec3Pattern3, vec4Pattern1, vec4Pattern2, vec4Pattern3, quatPattern, boolPattern, resourcePattern};

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

        /// Set iff status != INVALID_ARGS
        public String name;
        /// Set iff status == OK
        public PropertyType type;
        /// Set iff status != INVALID_ARGS
        public String rawValue;
        /// Set iff status == OK
        public Object value;
        /// Always set
        public int line;
        /// Always set
        public Status status;

        public Property(int line) {
            this.line = line;
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
        modules.clear();
        properties.clear();

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
            if (token.getChannel() == Token.DEFAULT_CHANNEL && token.getType() != Token.EOF) {
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
        final boolean startsWithRequire = text.startsWith("require");
        final boolean startsWithGlobalRequire = text.startsWith("_G.require");
        if (startsWithRequire || startsWithGlobalRequire) {
            List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
            // in case of _G.require:
            // - token 0 is _G
            // - token 1 is .
            // - token 3 is the require function
            //
            // in case of require:
            // - token 0 is the require function
            //
            // next token is either the first token of the require argument or the
            // parenthesis
            // if it is a parenthesis we make sure to skip over both the start
            // and end parenthesis
            int startIndex = startsWithRequire ? 1 : 3;
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
                // and also don't add the same module twice
                if (!LUA_LIBRARIES.contains(module) && !modules.contains(module)) {
                    modules.add(module);
                }
            }
        }
        // go.property() call?
        else if (text.startsWith("go.property")) {
            List<Token> tokens = getTokens(ctx, Token.DEFAULT_CHANNEL);
            Property property = parseProperty(text, tokens.get(0).getLine() - 1);
            if (property != null) {
                properties.add(property);
            }
            // strip property from code
            for (Token token : tokens) {
                int from = token.getStartIndex();
                int to = from + token.getText().length();
                for(int i = from; i <= to; i++) {
                    parsedBuffer.replace(i, i + 1, " ");
                }
            }
        }
    }



    private static Property parseProperty(String propString, int line) {
        Matcher propDeclMatcher = propertyDeclPattern.matcher(propString);
        if (!propDeclMatcher.matches()) {
            return null;
        }
        Property property = new Property(line);
        Matcher propArgsMatcher = propertyArgsPattern.matcher(propDeclMatcher.group(1).trim());
        if (!propArgsMatcher.matches()) {
            property.status = Status.INVALID_ARGS;
        } else {
            property.name = propArgsMatcher.group(1).trim();
            property.rawValue = propArgsMatcher.group(2).trim();
            if (parsePropertyValue(property.rawValue, property)) {
                property.status = Status.OK;
            } else {
                property.status = Status.INVALID_VALUE;
            }
        }

        return property;
    }


    private static boolean parsePropertyValue(String rawValue, Property property) {
        boolean result = false;
        for (Pattern pattern : patterns) {
            Matcher matcher = pattern.matcher(property.rawValue);
            if (matcher.matches()) {
                try {
                    final Pattern matchedPattern = matcher.pattern();
                    if (matchedPattern == numPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_NUMBER;
                        property.value = Double.parseDouble(property.rawValue);
                    } else if (matchedPattern == hashPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        property.value = matcher.group(1).trim();
                    } else if (matchedPattern == urlPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_URL;
                        if (matcher.group(2) != null) {
                            property.value = matcher.group(2).trim();
                        } else {
                            property.value = "";
                        }
                    } else if ((matchedPattern == vec3Pattern1) || (matchedPattern == vec3Pattern2) || (matchedPattern == vec3Pattern3)) {
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR3;
                        Vector3d v = new Vector3d();
                        if (matchedPattern == vec3Pattern1) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)));
                        }
                        else if (matchedPattern == vec3Pattern2) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)));
                        }
                        property.value = v;
                    } else if ((matchedPattern == vec4Pattern1) || (matchedPattern == vec4Pattern2) || (matchedPattern == vec4Pattern3)) {
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR4;
                        Vector4d v = new Vector4d();
                        if (matchedPattern == vec4Pattern1) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)),
                                    Double.parseDouble(matcher.group(5)));
                        }
                        else if (matchedPattern == vec4Pattern2) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)));
                        }
                        property.value = v;
                    } else if (matchedPattern == quatPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_QUAT;
                        Quat4d q = new Quat4d();
                        if (matcher.group(2) != null) {
                            q.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)),
                                    Double.parseDouble(matcher.group(5)));
                        }
                        property.value = q;
                    } else if (matchedPattern == boolPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_BOOLEAN;
                        property.value = Boolean.parseBoolean(rawValue);
                    } else if (matchedPattern == resourcePattern) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        property.value = matcher.group(3) == null ? "" :  matcher.group(3).trim();
                    }
                    result = true;
                } catch (NumberFormatException e) {
                    result = false;
                }
                break;
            }
        }
        return result;
    }

}
