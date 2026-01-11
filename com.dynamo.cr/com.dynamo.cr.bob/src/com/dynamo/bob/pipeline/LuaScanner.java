// Copyright 2020-2026 The Defold Foundation
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

import com.dynamo.bob.pipeline.LuaScanner.Property.Status;
import com.dynamo.bob.pipeline.antlr.lua.LuaLexer;
import com.dynamo.bob.pipeline.antlr.lua.LuaParser;
import com.dynamo.bob.pipeline.antlr.lua.LuaParserBaseListener;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import org.antlr.v4.runtime.BaseErrorListener;
import org.antlr.v4.runtime.CharStream;
import org.antlr.v4.runtime.CharStreams;
import org.antlr.v4.runtime.CommonTokenStream;
import org.antlr.v4.runtime.ParserRuleContext;
import org.antlr.v4.runtime.RecognitionException;
import org.antlr.v4.runtime.Recognizer;
import org.antlr.v4.runtime.Token;
import org.antlr.v4.runtime.tree.ErrorNode;
import org.antlr.v4.runtime.tree.ParseTreeWalker;
import org.antlr.v4.runtime.tree.TerminalNode;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;
import java.io.IOException;
import java.io.Reader;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.function.Predicate;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class LuaScanner {

    /**
     * @param message     error message
     * @param startLine   error start line, 0-indexed
     * @param startColumn error start column, 0-indexed
     * @param endLine     error end line, 0-indexed
     * @param endColumn   error end column, 0-indexed
     */
    public record ParseError(String message,
                             int startLine,
                             int startColumn,
                             int endLine,
                             int endColumn) {
    }

    /**
     * @param success    if compilation succeeded and all properties are valid
     * @param errors     syntax errors and go.property declaration errors
     * @param modules    distinct list of dependencies
     * @param properties list of properties
     * @param code       result code
     */
    public record Result(boolean success,
                         List<ParseError> errors,
                         List<String> modules,
                         List<Property> properties,
                         String code) {
    }

    /**
     * @param status       always set
     * @param startLine    always set, 0-indexed
     * @param startColumn  always set, 0-indexed
     * @param endLine      always set, 0-indexed
     * @param endColumn    always set, 0-indexed
     * @param name         set if status != {@code INVALID_ARGS}
     * @param type         set if status == {@code OK}
     * @param value        set if status == {@code OK}
     * @param resourceKind set if property is a resource, resource kind, e.g. material, atlas etc.
     */
    public record Property(Status status,
                           int startLine,
                           int startColumn,
                           int endLine,
                           int endColumn,
                           String name,
                           PropertyType type,
                           Object value,
                           String resourceKind) {

        public enum Status {
            OK,
            INVALID_ARGS,
            INVALID_VALUE,
            INVALID_LOCATION
        }

        public boolean isResource() {
            return resourceKind != null;
        }
    }


    /**
     * This list of Lua libraries represent the Lua standard libraries as well
     * as other very commonly used libraries that are included in the Defold
     * version of Lua.
     * Third-party Lua modules from the Lua developer community may include
     * require() calls for these libraries and unless we exclude these require
     * calls the build will fail since bob will be looking for a corresponding
     * .lua module file
     */
    private static final Set<String> LUA_LIBRARIES = Set.of(
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
    );


    private static final Set<String> LIFECYCLE_FUNCTIONS = Set.of(
            "init",
            "final",
            "update",
            "late_update",
            "fixed_update",
            "on_message",
            "on_input",
            "on_reload");

    public static Result parse(String code) {
        return parse(code, false);
    }

    public static Result parse(String code, boolean isDebug) {
        return parse(CharStreams.fromString(code), isDebug, DEFAULT_RESOURCE_KIND_PREDICATE);
    }

    public static Result parse(String code, boolean isDebug, Predicate<String> resourceKindPredicate) throws IOException {
        return parse(CharStreams.fromString(code), isDebug, resourceKindPredicate);
    }

    public static Result parse(Reader reader, boolean isDebug, Predicate<String> resourceKindPredicate) throws IOException {
        return parse(CharStreams.fromReader(reader), isDebug, resourceKindPredicate);
    }

    public static Result parse(CharStream charStream, boolean isDebug, Predicate<String> resourceKindPredicate) {
        TimeProfiler.start("Parse");

        var errors = new ArrayList<ParseError>();
        var errorListener = new ErrorListener(errors);
        // Using LinkedHashSet keeps insertion order while giving O(1) look‑ups
        var modules = new LinkedHashSet<String>();
        var properties = new ArrayList<Property>();

        // set up the lexer and parser
        // walk the generated parse tree from the
        // first Lua chunk
        var lexer = new LuaLexer(charStream);
        lexer.removeErrorListeners();
        lexer.addErrorListener(errorListener);
        var tokenStream = new CommonTokenStream(lexer);
        var tokenEditor = new TokenEditor();

        // Remove comments
        tokenStream.fill();
        for (Token token : tokenStream.getTokens()) {
            if (token.getChannel() == LuaLexer.COMMENTS) {
                int type = token.getType();
                if (type == LuaLexer.LINE_COMMENT) {
                    // Single line comment (might end with \n)
                    tokenEditor.replaceToken(token, token.getText().endsWith("\n") ? System.lineSeparator() : "");
                } else if (type == LuaLexer.COMMENT) {
                    // Multiline comment
                    if (isDebug) {
                        tokenEditor.replaceToken(token, NON_LINE_BREAKS.matcher(token.getText()).replaceAll(" "));
                    } else {
                        tokenEditor.replaceToken(token, System.lineSeparator().repeat(token.getText().split("\r\n|\r|\n").length - 1));
                    }
                }
            }
        }

        // parse code
        var parser = new LuaParser(tokenStream);
        parser.removeErrorListeners();
        parser.addErrorListener(errorListener);
        new ParseTreeWalker().walk(new ScannerListener(modules, tokenStream, errors, properties, tokenEditor, isDebug, resourceKindPredicate), parser.chunk());
        String resultCode = buildText(charStream, tokenStream.getTokens(), tokenEditor);

        var result = new Result(errors.isEmpty(), errors, List.copyOf(modules), properties, resultCode);
        TimeProfiler.stop();
        return result;
    }

    private static class ErrorListener extends BaseErrorListener {
        private final List<ParseError> errors;

        public ErrorListener(List<ParseError> errors) {
            this.errors = errors;
        }

        @Override
        public void syntaxError(Recognizer<?, ?> recognizer, Object offendingSymbol, int line, int charPositionInLine, String msg, RecognitionException e) {
            var length = offendingSymbol instanceof Token token ? token.getText().length() : 1;
            errors.add(new ParseError(msg, line - 1, charPositionInLine, line - 1, charPositionInLine + length));
        }
    }

    private LuaScanner() {
    }

    private static final Predicate<String> DEFAULT_RESOURCE_KIND_PREDICATE = resourceKind -> true;

    private static final Pattern NON_LINE_BREAKS = Pattern.compile("[^\r\n]");

    private static class ScannerListener extends LuaParserBaseListener {

        private final LinkedHashSet<String> modules;
        private final CommonTokenStream tokenStream;
        private final ArrayList<ParseError> errors;
        private final ArrayList<Property> properties;
        private final TokenEditor tokenEditor;
        private final boolean isDebug;
        private final Predicate<String> resourceKindPredicate;

        // These vars are modified during the walk
        int nestingDepth = 0;
        boolean encounteredError = false;

        public ScannerListener(LinkedHashSet<String> modules, CommonTokenStream tokenStream, ArrayList<ParseError> errors, ArrayList<Property> properties, TokenEditor tokenEditor, boolean isDebug, Predicate<String> resourceKindPredicate) {
            this.modules = modules;
            this.tokenStream = tokenStream;
            this.errors = errors;
            this.properties = properties;
            this.tokenEditor = tokenEditor;
            this.isDebug = isDebug;
            this.resourceKindPredicate = resourceKindPredicate;
        }

        /**
         * Callback from ANTLR when a function call is entered. We use this to grab all
         * require() calls and all go.property() calls.
         */
        @Override
        public void enterFunctioncall(LuaParser.FunctioncallContext ctx) {
            if (encounteredError) {
                return;
            }
            var varCtx = ctx.variable();
            var fnDesc = functionDescriptor(varCtx);

            if (fnDesc.functionName == null) {
                return;
            }

            if (fnDesc.is("require", null) || fnDesc.is("require", "_G")) {
                String module = getFirstStringArg(ctx.nameAndArgs().args());
                // ignore Lua+LuaJIT standard libraries + Defold additions such as LuaSocket
                if (module != null && !LUA_LIBRARIES.contains(module)) {
                    modules.add(module);      // LinkedHashSet prevents duplicates in O(1)
                }
            } else if (fnDesc.is("property", "go")) {
                var argsCtx = ctx.nameAndArgs().args();
                ParserRuleContext paramsContext = argsCtx;
                var propertyName = getFirstStringArg(argsCtx);
                var tokens = getTokens(tokenStream, ctx, Token.DEFAULT_CHANNEL);
                var startLine = tokens.getFirst().getLine() - 1;
                var startColumn = tokens.getFirst().getCharPositionInLine();
                var endLine = tokens.getLast().getLine() - 1;
                var endColumn = tokens.getLast().getCharPositionInLine() + tokens.getLast().getText().length();

                Property property;
                if (propertyName == null || propertyName.isEmpty()) {
                    property = new Property(Status.INVALID_ARGS, startLine, startColumn, endLine, endColumn, null, null, null, null);
                    errors.add(new ParseError("invalid go.property args", startLine, startColumn, endLine, endColumn));
                } else {
                    var parseResult = parsePropertyValue(tokenStream, argsCtx, resourceKindPredicate);
                    switch (parseResult) {
                        case InvalidValue invalidValue -> {
                            property = new Property(Status.INVALID_VALUE, startLine, startColumn, endLine, endColumn, propertyName, null, null, null);
                            errors.add(new ParseError(invalidValue.message, startLine, startColumn, endLine, endColumn));
                        }
                        case Success success -> {
                            paramsContext = success.resultContext;
                            // Only allow go.property as a standalone top‑level statement
                            // Top-level statements have nesting depth 4: chunk -> block -> stat -> variable
                            if (nestingDepth != 4) {
                                property = new Property(Status.INVALID_LOCATION, startLine, startColumn, endLine, endColumn, propertyName, success.type, success.value, success.resourceKind);
                                errors.add(new ParseError("go.property declaration should be a top-level statement", startLine, startColumn, endLine, endColumn));
                            } else {
                                property = new Property(Status.OK, startLine, startColumn, endLine, endColumn, propertyName, success.type, success.value, success.resourceKind);
                            }
                        }
                    }
                }
                properties.add(property);

                // strip property from code
                // keep tokens for hash() in debug build
                // see https://github.com/defold/defold/issues/7422
                if (isDebug && !property.isResource() && property.type == PropertyType.PROPERTY_TYPE_HASH) {
                    tokens.removeAll(getTokens(tokenStream, paramsContext));
                }
                removeTokens(tokenEditor, tokenStream, tokens, isDebug);
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
                if (funcName == null) {
                    return;
                }
            } else {
                objName = funcNameCtx.NAME(0);
            }
            if (objName == null || objName.getText().equals("_G")) {
                if (LIFECYCLE_FUNCTIONS.contains(funcName.getText())) {
                    LuaParser.BlockContext blockCtx = ctx.funcbody().block();
                    if (blockCtx != null && blockCtx.stat().isEmpty() && blockCtx.retstat() == null) {
                        List<Token> tokens = getTokens(tokenStream, ctx, Token.DEFAULT_CHANNEL);
                        removeTokens(tokenEditor, tokenStream, tokens, isDebug);
                    }
                }
            }
        }

        @Override
        public void enterEveryRule(ParserRuleContext ctx) {
            nestingDepth++;
        }

        @Override
        public void exitEveryRule(ParserRuleContext ctx) {
            nestingDepth--;
        }

        @Override
        public void visitErrorNode(ErrorNode node) {
            encounteredError = true;
        }
    }

    // get all tokens spanning a context and belonging to a specific channel
    private static List<Token> getTokens(CommonTokenStream tokenStream, ParserRuleContext ctx, int channel) {
        List<Token> tokens = getTokens(tokenStream, ctx);
        if (tokens == null) {
            return new ArrayList<>();
        }
        return tokens.stream().filter(t -> t.getChannel() == channel).collect(Collectors.toList());
    }

    // get all tokens spanning a context
    private static List<Token> getTokens(CommonTokenStream tokenStream, ParserRuleContext ctx) {
        Token startToken = ctx.getStart();
        Token stopToken = ctx.getStop();
        return tokenStream.getTokens(startToken.getTokenIndex(), stopToken.getTokenIndex());
    }

    // replace the token with an empty string
    private static void removeTokens(TokenEditor tokenEditor, CommonTokenStream tokenStream, List<Token> tokens, boolean isDebug) {
        if (tokens.isEmpty()) {
            return;
        }
        int rangeStartIndex = tokens.getFirst().getTokenIndex();
        int rangeStopIndex = rangeStartIndex;
        int rangeLength = tokens.getFirst().getText().length();
        for (int i = 1; i < tokens.size(); i++) {
            Token token = tokens.get(i);
            int tokenIndex = token.getTokenIndex();
            if (tokenIndex == rangeStopIndex + 1) {
                rangeStopIndex = tokenIndex;
                rangeLength += token.getText().length();
            } else {
                removeTokenRange(tokenEditor, rangeStartIndex, rangeStopIndex, rangeLength, isDebug);
                rangeStartIndex = tokenIndex;
                rangeStopIndex = tokenIndex;
                rangeLength = token.getText().length();
            }
        }
        removeTokenRange(tokenEditor, rangeStartIndex, rangeStopIndex, rangeLength, isDebug);

        int lastTokenIndex = tokens.getLast().getTokenIndex();
        int nextTokenIndex = lastTokenIndex + 1;
        Token token = tokenStream.get(nextTokenIndex);
        while (token != null && token.getType() == LuaLexer.WS) {
            token = tokenStream.get(token.getTokenIndex() + 1);
        }

        // We use this to remove semicolon statements in the end of line;
        // The semicolon may cause problems if it is at the end of a go.property call
        // as it will be removed after it has been parsed.
        if (token != null && token.getType() == LuaLexer.SEMICOLON) {
            removeToken(tokenEditor, token, isDebug);
        }
    }

    private static void removeTokenRange(TokenEditor tokenEditor, int startTokenIndex, int stopTokenIndex, int length, boolean isDebug) {
        tokenEditor.replaceRange(startTokenIndex, stopTokenIndex, isDebug ? " ".repeat(length) : "");
    }

    private static void removeToken(TokenEditor tokenEditor, Token token, boolean isDebug) {
        tokenEditor.replaceRange(token.getTokenIndex(), token.getTokenIndex(), isDebug ? " ".repeat(token.getText().length()) : "");
    }

    private record RangeEdit(int startIndex, int stopIndex, String replacement) {
    }

    private static final class TokenEditor {
        private final ArrayList<RangeEdit> edits = new ArrayList<>();

        void replaceToken(Token token, String replacement) {
            replaceRange(token.getTokenIndex(), token.getTokenIndex(), replacement);
        }

        void replaceRange(int startIndex, int stopIndex, String replacement) {
            edits.add(new RangeEdit(startIndex, stopIndex, replacement));
        }

        List<RangeEdit> all() {
            return edits;
        }
    }

    private static String buildText(CharStream charStream, List<Token> tokens, TokenEditor tokenEditor) {
        List<RangeEdit> ranges = tokenEditor.all();
        ranges.sort((a, b) -> Integer.compare(a.startIndex(), b.startIndex()));

        int initialCapacity = Math.max(0, charStream.size());
        StringBuilder builder = new StringBuilder(initialCapacity);

        int rangeIndex = 0;
        RangeEdit currentRange = rangeIndex < ranges.size() ? ranges.get(rangeIndex) : null;
        for (int i = 0; i < tokens.size(); i++) {
            if (currentRange != null && i == currentRange.startIndex()) {
                builder.append(currentRange.replacement());
                i = currentRange.stopIndex();
                rangeIndex++;
                currentRange = rangeIndex < ranges.size() ? ranges.get(rangeIndex) : null;
                continue;
            }
            Token token = tokens.get(i);
            if (token.getType() != Token.EOF) {
                builder.append(token.getText());
            }
        }
        return builder.toString();
    }

    private static final Map<Integer, String> QUOTES = Map.of(
            LuaParser.NORMALSTRING, "\"",
            LuaParser.CHARSTRING, "'"
    );

    // returns a string literal argument, or fallback if no arguments were provided, or null if the first argument
    // is not a string literal
    private static String getFirstStringLiteralArg(LuaParser.ArgsContext argsCtx, String fallback) {
        if (argsCtx == null) {
            return fallback;
        }
        ParserRuleContext firstCtx = argsCtx.getRuleContext(ParserRuleContext.class, 0);
        if (firstCtx == null) {
            return fallback;
        }

        if (firstCtx.getRuleIndex() == LuaParser.RULE_explist) {
            firstCtx = ((LuaParser.ExplistContext) firstCtx).exp(0).getRuleContext(ParserRuleContext.class, 0);
        }
        if (firstCtx.getRuleIndex() != LuaParser.RULE_lstring) {
            return null;
        }
        Token initialToken = firstCtx.getStart();
        return initialToken.getText().replace(QUOTES.getOrDefault(initialToken.getType(), ""), "");
    }

    // returns first function argument only if it's a string, otherwise null
    private static String getFirstStringArg(LuaParser.ArgsContext argsCtx) {
        return getFirstStringLiteralArg(argsCtx, null);
    }

    private static List<String> getAllStringArgs(LuaParser.ArgsContext argsCtx) {
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
                } else {
                    stringArgs.add(null);
                }
            }
        }

        return stringArgs;
    }

    // returns boolean if parsing successful and fill double[] with parsed values with needed length.
    private static boolean getNumArgs(LuaParser.ArgsContext argsCtx, double[] resultArgs) {
        LuaParser.ExplistContext expListCtx = argsCtx.explist();
        // no values for example vmath.vector3()
        if (expListCtx != null) {
            List<LuaParser.ExpContext> args = expListCtx.exp();
            int count = 0;
            for (LuaParser.ExpContext val : args) {
                LuaParser.NumberContext num = val.number();
                LuaParser.ExpContext exp = val.exp(0);
                int firstTokenType = val.getStart().getType();
                if (num != null || (exp != null && exp.number() != null && firstTokenType == LuaParser.MINUS)) {
                    resultArgs[count] = Double.parseDouble(val.getText());
                    count++;
                } else {
                    // the value isn't a number
                    return false;
                }

            }
            // one value for example vmath.vector3(1), is valid, and all the values should be filled
            if (count == 1) {
                for (int i = count; i < resultArgs.length; i++) {
                    resultArgs[i] = resultArgs[0];
                }
            } else {
                return count == resultArgs.length;
            }
        }
        return true;
    }

    private record FunctionDescriptor(String objectName, String functionName) {
        public boolean is(String functionName, String objectName) {
            return Objects.equals(functionName, this.functionName) && Objects.equals(objectName, this.objectName);
        }

        public boolean isObject(String objectName) {
            return Objects.equals(objectName, this.objectName);
        }

        public boolean isName(String functionName) {
            return Objects.equals(functionName, this.functionName);
        }
    }

    private static FunctionDescriptor functionDescriptor(LuaParser.VariableContext variableCtx) {
        // simple function call, like `require()`
        String varName = null;
        if (variableCtx instanceof LuaParser.NamedvariableContext) {
            varName = ((LuaParser.NamedvariableContext) variableCtx).NAME().getText();
        }

        // indexed function call, like `_G.require()` or `go.property()` where
        // `objectName` is `go` and `indexName` is `property`
        String indexName = null;
        String objectName = null;
        if (variableCtx instanceof LuaParser.IndexContext) {
            LuaParser.IndexContext indexVariableCtx = (LuaParser.IndexContext) variableCtx;
            TerminalNode nameNode = indexVariableCtx.NAME();
            if (nameNode != null) {
                indexName = nameNode.getText();
            }
            objectName = indexVariableCtx.variable().getText();
        }
        var functionName = (indexName == null) ? varName : indexName;
        return new FunctionDescriptor(objectName, functionName);
    }

    private sealed interface ParsePropertyResult permits InvalidValue, Success {
    }

    private record InvalidValue(String message) implements ParsePropertyResult {
    }

    private record Success(ParserRuleContext resultContext,
                           PropertyType type,
                           Object value,
                           String resourceKind) implements ParsePropertyResult {
        public Success(ParserRuleContext resultContext, PropertyType type, Object value) {
            this(resultContext, type, value, null);
        }
    }

    private static ParsePropertyResult parsePropertyValue(CommonTokenStream tokenStream, LuaParser.ArgsContext argsCtx, Predicate<String> resourceKindPredicate) {
        List<LuaParser.ExpContext> expCtxList = ((LuaParser.ExplistContext) argsCtx.getRuleContext(ParserRuleContext.class, 0)).exp();
        // go.property(name, value) should have a value and only one value
        if (expCtxList.size() == 2) {
            LuaParser.ExpContext expCtx = expCtxList.get(1);
            Token initialToken = expCtx.getStart();
            int type = initialToken.getType();
            // for negative numbers we should take token #2
            if (type == LuaParser.MINUS) {
                List<Token> tokens = getTokens(tokenStream, expCtx, Token.DEFAULT_CHANNEL);
                if (tokens.size() > 1) {
                    initialToken = tokens.get(1);
                    type = initialToken.getType();
                }
            }
            if (type == LuaParser.INT || type == LuaParser.HEX || type == LuaParser.FLOAT || type == LuaParser.HEX_FLOAT) {
                return new Success(expCtx, PropertyType.PROPERTY_TYPE_NUMBER, Double.parseDouble(expCtx.getText()));
            } else if (type == LuaParser.FALSE || type == LuaParser.TRUE) {
                return new Success(expCtx, PropertyType.PROPERTY_TYPE_BOOLEAN, Boolean.parseBoolean(initialToken.getText()));
            } else if (type == LuaParser.NAME) {
                LuaParser.VariableContext varCtx = expCtx.variable();
                // function expected
                if (!(varCtx instanceof LuaParser.FunctioncallContext ctx)) {
                    return new InvalidValue("expecting a function");
                }
                FunctionDescriptor fnDesc = functionDescriptor(ctx.variable());
                if (fnDesc.functionName == null) {
                    return new InvalidValue("expecting named function");
                }
                if (fnDesc.isObject("vmath")) {
                    if (fnDesc.isName("vector3")) {
                        Vector3d v = new Vector3d();
                        double[] resultArgs = new double[3];
                        if (getNumArgs(ctx.nameAndArgs().args(), resultArgs)) {
                            v.set(resultArgs);
                            return new Success(ctx, PropertyType.PROPERTY_TYPE_VECTOR3, v);
                        } else {
                            return new InvalidValue("invalid vmath.vector3() arguments");
                        }
                    } else if (fnDesc.isName("vector4")) {
                        Vector4d v = new Vector4d();
                        double[] resultArgs = new double[4];
                        if (getNumArgs(ctx.nameAndArgs().args(), resultArgs)) {
                            v.set(resultArgs);
                            return new Success(ctx, PropertyType.PROPERTY_TYPE_VECTOR4, v);
                        } else {
                            return new InvalidValue("invalid vmath.vector4() arguments");
                        }
                    } else if (fnDesc.isName("quat")) {
                        Quat4d q = new Quat4d();
                        double[] resultArgs = new double[4];
                        if (getNumArgs(ctx.nameAndArgs().args(), resultArgs)) {
                            q.set(resultArgs);
                            return new Success(ctx, PropertyType.PROPERTY_TYPE_QUAT, q);
                        } else {
                            return new InvalidValue("invalid vmath.quat() arguments");
                        }
                    } else {
                        return new InvalidValue("unexpected vmath function");
                    }
                } else if (fnDesc.isObject("resource")) {
                    var firstStrArg = getFirstStringLiteralArg(ctx.nameAndArgs().args(), "");
                    if (firstStrArg == null) {
                        return new InvalidValue("expected a string literal resource argument");
                    }
                    String resourceKind = fnDesc.functionName;
                    if (resourceKindPredicate.test(resourceKind)) {
                        return new Success(ctx, PropertyType.PROPERTY_TYPE_HASH, firstStrArg, resourceKind);
                    } else {
                        return new InvalidValue("invalid resource kind: " + resourceKind);
                    }
                } else if (fnDesc.is("hash", null) || fnDesc.is("hash", "_G")) {
                    var firstStrArg = getFirstStringArg(ctx.nameAndArgs().args());
                    if (firstStrArg != null) {
                        return new Success(ctx, PropertyType.PROPERTY_TYPE_HASH, firstStrArg);
                    } else {
                        return new InvalidValue("hash requires an argument");
                    }
                } else if (fnDesc.is("url", "msg")) {
                    List<String> allArgs = getAllStringArgs(ctx.nameAndArgs().args());
                    String result;
                    if (allArgs == null || allArgs.isEmpty()) {
                        result = "";
                    } else {
                        if (allArgs.contains(null)) {
                            return new InvalidValue("`nil` can't be used in `go.property(msg.url(_))`");
                        }
                        if (allArgs.size() == 1) {
                            result = allArgs.get(0);
                        } else {
                            if (allArgs.size() != 3) {
                                return new InvalidValue("The URL may have only one or three strings in it");
                            }
                            result = allArgs.get(0) + ":" + allArgs.get(1) + "#" + allArgs.get(2);
                        }
                    }
                    return new Success(ctx, PropertyType.PROPERTY_TYPE_URL, result);
                } else {
                    return new InvalidValue("unexpected function");
                }
            } else {
                return new InvalidValue("unexpected argument");
            }
        } else {
            return new InvalidValue("2 arguments expected");
        }
    }
}
