package com.dynamo.cr.luaeditor;

import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.IToken;
import org.eclipse.jface.text.rules.RuleBasedScanner;
import org.eclipse.jface.text.rules.Token;

public class LuaSingleCommentScanner extends RuleBasedScanner {

    public LuaSingleCommentScanner(ColorManager manager) {
        IToken string = new Token(new TextAttribute(
                manager.getColor(ILuaColorConstants.COMMENT)));

        setDefaultReturnToken(string);
    }
}
