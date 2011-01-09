package com.dynamo.cr.luaeditor;

import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.IRule;
import org.eclipse.jface.text.rules.IToken;
import org.eclipse.jface.text.rules.IWordDetector;
import org.eclipse.jface.text.rules.RuleBasedScanner;
import org.eclipse.jface.text.rules.SingleLineRule;
import org.eclipse.jface.text.rules.Token;
import org.eclipse.jface.text.rules.WhitespaceRule;
import org.eclipse.jface.text.rules.WordRule;
import org.eclipse.swt.SWT;

public class LuaScanner extends RuleBasedScanner {

    public LuaScanner(ColorManager manager) {
        IToken string =
            new Token(
                    new TextAttribute(manager.getColor(ILuaColorConstants.STRING)));

        IToken keyword =
            new Token(
                new TextAttribute(manager.getColor(ILuaColorConstants.KEYWORD), null, SWT.BOLD));

        IToken defaultToken =
            new Token(
                new TextAttribute(manager.getColor(ILuaColorConstants.DEFAULT)));

        IRule[] rules = new IRule[5];

        // Add rule for double quotes
        rules[0] = new SingleLineRule("\"", "\"", string, '\\');
        // Add a rule for single-line comment
        rules[1] = new CommentRule(manager);
        // Add generic whitespace rule.
        rules[2] = new WhitespaceRule(new LuaWhitespaceDetector());

         IWordDetector nameDetector = new IWordDetector() {
             public boolean isWordStart(char c) {
                 return Character.isLetter(c) || c == '_' || c == ':';
             }

             public boolean isWordPart(char c) {
                 return isWordStart(c) || Character.isDigit(c) || c == '-';
             }
         };

        WordRule name_rule = new WordRule(nameDetector);

        name_rule.addWord("and", keyword);
        name_rule.addWord("break", keyword);
        name_rule.addWord("do", keyword);
        name_rule.addWord("else", keyword);
        name_rule.addWord("elseif", keyword);
        name_rule.addWord("end", keyword);
        name_rule.addWord("false", keyword);
        name_rule.addWord("for", keyword);
        name_rule.addWord("function", keyword);
        name_rule.addWord("if", keyword);
        name_rule.addWord("in", keyword);
        name_rule.addWord("local", keyword);
        name_rule.addWord("nil", keyword);
        name_rule.addWord("not", keyword);
        name_rule.addWord("or", keyword);
        name_rule.addWord("repeat", keyword);
        name_rule.addWord("return", keyword);
        name_rule.addWord("then", keyword);
        name_rule.addWord("true", keyword);
        name_rule.addWord("until", keyword);
        name_rule.addWord("while", keyword);
        name_rule.addWord("self", keyword);
        rules[3] = name_rule;

        rules[4] = new WordRule(nameDetector, defaultToken);

        setRules(rules);
    }
}
