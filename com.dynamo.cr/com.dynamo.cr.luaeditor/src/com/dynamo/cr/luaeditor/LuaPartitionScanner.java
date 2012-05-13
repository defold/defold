package com.dynamo.cr.luaeditor;

import org.eclipse.jface.text.rules.EndOfLineRule;
import org.eclipse.jface.text.rules.IPredicateRule;
import org.eclipse.jface.text.rules.IToken;
import org.eclipse.jface.text.rules.MultiLineRule;
import org.eclipse.jface.text.rules.RuleBasedPartitionScanner;
import org.eclipse.jface.text.rules.Token;

public class LuaPartitionScanner extends RuleBasedPartitionScanner {
	public final static String LUA_COMMENT_SINGLE = "__lua_comment_single";
    public final static String LUA_COMMENT_MULTI = "__lua_comment_multi";

	public LuaPartitionScanner() {
		IToken luaCommentSingle = new Token(LUA_COMMENT_SINGLE);
		IToken luaCommentMulti = new Token(LUA_COMMENT_MULTI);
		IPredicateRule[] rules = new IPredicateRule[2];
		rules[0] = new MultiLineRule("--[[", "--]]", luaCommentMulti, (char) 0, true);
        rules[1] = new EndOfLineRule("--", luaCommentSingle);
		setPredicateRules(rules);
	}
}
