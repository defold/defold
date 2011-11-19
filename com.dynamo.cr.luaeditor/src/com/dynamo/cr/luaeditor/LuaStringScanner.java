package com.dynamo.cr.luaeditor;

import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.IToken;
import org.eclipse.jface.text.rules.RuleBasedScanner;
import org.eclipse.jface.text.rules.Token;

public class LuaStringScanner extends RuleBasedScanner {

    public LuaStringScanner(ColorManager manager) {
        IToken string = new Token(new TextAttribute(
                manager.getColor(ILuaColorConstants.STRING)));

        setDefaultReturnToken(string);
    }
}
