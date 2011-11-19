package com.dynamo.cr.luaeditor;

import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.IToken;
import org.eclipse.jface.text.rules.RuleBasedScanner;
import org.eclipse.jface.text.rules.Token;

public class LuaMultiCommentScanner extends RuleBasedScanner {

    public LuaMultiCommentScanner(ColorManager manager) {
        IToken string = new Token(new TextAttribute(
                manager.getColor(ILuaColorConstants.COMMENT)));

        setDefaultReturnToken(string);
    }
}
