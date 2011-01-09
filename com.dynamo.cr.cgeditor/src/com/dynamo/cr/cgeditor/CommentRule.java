package com.dynamo.cr.cgeditor;

import org.eclipse.jface.text.TextAttribute;
import org.eclipse.jface.text.rules.ICharacterScanner;
import org.eclipse.jface.text.rules.IPredicateRule;
import org.eclipse.jface.text.rules.IToken;
import org.eclipse.jface.text.rules.Token;

public class CommentRule implements IPredicateRule {
    IToken comment = null;

    CommentRule(ColorManager manager) {
        comment = new Token(new TextAttribute(manager.getColor(ICgColorConstants.COMMENT)));
    }

    @Override
    public IToken evaluate(ICharacterScanner scanner) {
        return evaluate(scanner, false);
    }

    @Override
    public IToken getSuccessToken() {
        return comment;
    }

    @Override
    public IToken evaluate(ICharacterScanner scanner, boolean resume) {
        int c = scanner.read();
        int c_count = 1;
        boolean found_first = false;
        boolean found_comment = resume;
        while (c != ICharacterScanner.EOF && c != '\n')
        {
            if (!found_comment)
            {
                if (c == '/')
                {
                    if (found_first)
                    {
                        found_comment = true;
                    }
                    else
                    {
                        found_first = true;
                    }
                }
                else
                {
                    found_first = false;
                }
            }
            c = scanner.read();
            ++c_count;
        }
        if (found_comment)
        {
            return comment;
        }
        else
        {
            for (int i = 0; i < c_count; ++i)
                scanner.unread();
            return Token.UNDEFINED;
        }
    }
}
