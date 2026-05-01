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

package org.commonmark.internal;


import org.commonmark.internal.util.Parsing;
import org.commonmark.node.Block;
import org.commonmark.node.HtmlBlock;
import org.commonmark.node.Paragraph;
import org.commonmark.parser.SourceLine;
import org.commonmark.parser.block.*;

import java.util.regex.Pattern;

// See editor/markdown.clj
public class CustomHtmlBlockParser extends AbstractBlockParser {

    private static final String TAGNAME = "[A-Za-z][A-Za-z0-9-]*";
    private static final String ATTRIBUTENAME = "[a-zA-Z_:][a-zA-Z0-9:._-]*";
    private static final String UNQUOTEDVALUE = "[^\"'=<>`\\x00-\\x20]+";
    private static final String SINGLEQUOTEDVALUE = "'[^']*'";
    private static final String DOUBLEQUOTEDVALUE = "\"[^\"]*\"";
    private static final String ATTRIBUTEVALUE = "(?:" + UNQUOTEDVALUE + "|" + SINGLEQUOTEDVALUE
            + "|" + DOUBLEQUOTEDVALUE + ")";
    private static final String ATTRIBUTEVALUESPEC = "(?:" + "\\s*=" + "\\s*" + ATTRIBUTEVALUE
            + ")";
    private static final String ATTRIBUTE = "(?:" + "\\s+" + ATTRIBUTENAME + ATTRIBUTEVALUESPEC
            + "?)";

    private static final String OPENTAG = "<" + TAGNAME + ATTRIBUTE + "*" + "\\s*/?>";
    private static final String CLOSETAG = "</" + TAGNAME + "\\s*[>]";

    private static final Pattern[][] BLOCK_PATTERNS = new Pattern[][]{
            {null, null}, // not used (no type 0)
            {
                    // change from the original HtmlBlockParser: add div here
                    Pattern.compile("^<(?:script|pre|style|textarea|div)(?:\\s|>|$)", Pattern.CASE_INSENSITIVE),
                    // change from the original HtmlBlockParser: add div here
                    Pattern.compile("</(?:script|pre|style|textarea|div)>", Pattern.CASE_INSENSITIVE)
            },
            {
                    Pattern.compile("^<!--"),
                    Pattern.compile("-->")
            },
            {
                    Pattern.compile("^<[?]"),
                    Pattern.compile("\\?>")
            },
            {
                    Pattern.compile("^<![A-Z]"),
                    Pattern.compile(">")
            },
            {
                    Pattern.compile("^<!\\[CDATA\\["),
                    Pattern.compile("\\]\\]>")
            },
            {
                    Pattern.compile("^</?(?:" +
                            "address|article|aside|" +
                            "base|basefont|blockquote|body|" +
                            "caption|center|col|colgroup|" +
                            // change from the original HtmlBlockParser: remove div here
                            "dd|details|dialog|dir|dl|dt|" +
                            "fieldset|figcaption|figure|footer|form|frame|frameset|" +
                            "h1|h2|h3|h4|h5|h6|head|header|hr|html|" +
                            "iframe|" +
                            "legend|li|link|" +
                            "main|menu|menuitem|" +
                            "nav|noframes|" +
                            "ol|optgroup|option|" +
                            "p|param|" +
                            "search|section|summary|" +
                            "table|tbody|td|tfoot|th|thead|title|tr|track|" +
                            "ul" +
                            ")(?:\\s|[/]?[>]|$)", Pattern.CASE_INSENSITIVE),
                    null // terminated by blank line
            },
            {
                    Pattern.compile("^(?:" + OPENTAG + '|' + CLOSETAG + ")\\s*$", Pattern.CASE_INSENSITIVE),
                    null // terminated by blank line
            }
    };

    private final HtmlBlock block = new HtmlBlock();
    private final Pattern closingPattern;

    private boolean finished = false;
    private BlockContent content = new BlockContent();

    private CustomHtmlBlockParser(Pattern closingPattern) {
        this.closingPattern = closingPattern;
    }

    @Override
    public Block getBlock() {
        return block;
    }

    @Override
    public BlockContinue tryContinue(ParserState state) {
        if (finished) {
            return BlockContinue.none();
        }

        // Blank line ends type 6 and type 7 blocks
        if (state.isBlank() && closingPattern == null) {
            return BlockContinue.none();
        } else {
            return BlockContinue.atIndex(state.getIndex());
        }
    }

    @Override
    public void addLine(SourceLine line) {
        content.add(line.getContent());

        if (closingPattern != null && closingPattern.matcher(line.getContent()).find()) {
            finished = true;
        }
    }

    @Override
    public void closeBlock() {
        block.setLiteral(content.getString());
        content = null;
    }

    public static class Factory extends AbstractBlockParserFactory {

        @Override
        public BlockStart tryStart(ParserState state, MatchedBlockParser matchedBlockParser) {
            int nextNonSpace = state.getNextNonSpaceIndex();
            CharSequence line = state.getLine().getContent();

            if (state.getIndent() < 4 && line.charAt(nextNonSpace) == '<') {
                for (int blockType = 1; blockType <= 7; blockType++) {
                    // Type 7 can not interrupt a paragraph (not even a lazy one)
                    if (blockType == 7 && (
                            matchedBlockParser.getMatchedBlockParser().getBlock() instanceof Paragraph ||
                                    state.getActiveBlockParser().canHaveLazyContinuationLines())) {
                        continue;
                    }
                    Pattern opener = BLOCK_PATTERNS[blockType][0];
                    Pattern closer = BLOCK_PATTERNS[blockType][1];
                    boolean matches = opener.matcher(line.subSequence(nextNonSpace, line.length())).find();
                    if (matches) {
                        return BlockStart.of(new CustomHtmlBlockParser(closer)).atIndex(state.getIndex());
                    }
                }
            }
            return BlockStart.none();
        }
    }
}
