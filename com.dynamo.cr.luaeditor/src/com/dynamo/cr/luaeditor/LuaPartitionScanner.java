package com.dynamo.cr.luaeditor;

import org.eclipse.jface.text.rules.*;

public class LuaPartitionScanner extends RuleBasedPartitionScanner {
	public final static String LUA_COMMENT = "__lua_comment";

	public LuaPartitionScanner() {
		IToken luaComment = new Token(LUA_COMMENT);
		IPredicateRule[] rules = new IPredicateRule[1];
		rules[0] = new MultiLineRule("--[[", "--]]", luaComment);
		setPredicateRules(rules);
	}
}
