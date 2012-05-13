package com.dynamo.cr.cgeditor;

import org.eclipse.jface.text.rules.*;

public class CgPartitionScanner extends RuleBasedPartitionScanner {
	public final static String CG_COMMENT = "__cg_comment";

	public CgPartitionScanner() {
		IToken cgComment = new Token(CG_COMMENT);
		IPredicateRule[] rules = new IPredicateRule[1];
		rules[0] = new MultiLineRule("/*", "*/", cgComment);
		setPredicateRules(rules);
	}
}
