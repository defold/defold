/*
 BSD License
 
 Copyright (c) 2013, Kazunori Sakamoto Copyright (c) 2016, Alexander Alexeev
 (c) 2020, Steven Johnstone All rights reserved.
 

 
 Redistribution and use in source and binary forms, with or without modification, are permitted
 provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 and the following disclaimer. 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the documentation and/or other
 materials provided with the distribution. 3. Neither the NAME of Rainer Schuster nor the NAMEs of
 its contributors may be used to endorse or promote products derived from this software without
 specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 This grammar file derived from:
 
 Lua 5.3 Reference Manual http://www.lua.org/manual/5.3/manual.html
 
 Lua 5.2 Reference Manual http://www.lua.org/manual/5.2/manual.html
 
 Lua 5.1 grammar written by Nicolai Mainiero http://www.antlr3.org/grammar/1178608849736/Lua.g
 

 

 Tested by Kazunori Sakamoto with Test suite for Lua 5.2 (http://www.lua.org/tests/5.2/)
 
 Tested by Alexander Alexeev with Test suite for Lua 5.3
 http://www.lua.org/tests/lua-5.3.2-tests.tar.gz
 
 Split into separate lexer and parser grammars + minor additions, Steven Johnstone.
 */

lexer grammar LuaLexer;
channels {
	COMMENTS
}

SEMICOLON: ';';
BREAK: 'break';
GOTO: 'goto';

DO: 'do';

WHILE: 'while';

END: 'end';

REPEAT: 'repeat';

UNTIL: 'until';

FOR: 'for';

FUNCTION: 'function';

LOCAL: 'local';

IF: 'if';
THEN: 'then';
ELSEIF: 'elseif';

ELSE: 'else';
RETURN: 'return';

COLON: ':';

DCOLON: '::';

DOT: '.';

COMMA: ',';
IN: 'in';

LPAREN: '(';
RPAREN: ')';

LBRACK: '[';

RBRACK: ']';

LBRACE: '{';

RBRACE: '}';

OR: 'or';

AND: 'and';

LT: '<';
GT: '>';

LTE: '<=';
GTE: '>=';
NEQ: '~=';
EQ: '==';

EQUALS: '=';

STRCAT: '..';
PLUS: '+';
MINUS: '-';
MUL: '*';
DIV: '/';
MOD: '%';

DIVFLOOR: '//';

BITAND: '&';

BITOR: '|';

BITNOT: '~';
BITSHL: '<<';
BITSHR: '>>';

NOT: 'not';

LEN: '#';

POWER: '^';

NIL: 'nil';

FALSE: 'false';

TRUE: 'true';

DOTS: '...';

NAME: [a-zA-Z_][a-zA-Z_0-9]*;

NORMALSTRING: '"' ( EscapeSequence | ~('\\' | '"'))* '"';

CHARSTRING: '\'' ( EscapeSequence | ~('\'' | '\\'))* '\'';

LONGSTRING: '[' NESTED_STR ']';

fragment NESTED_STR: '=' NESTED_STR '=' | '[' .*? ']';

INT: Digit+;

HEX: '0' [xX] HexDigit+;

FLOAT:
	Digit+ '.' Digit* ExponentPart?
	| '.' Digit+ ExponentPart?
	| Digit+ ExponentPart;

HEX_FLOAT:
	'0' [xX] HexDigit+ '.' HexDigit* HexExponentPart?
	| '0' [xX] '.' HexDigit+ HexExponentPart?
	| '0' [xX] HexDigit+ HexExponentPart;

fragment ExponentPart: [eE] [+-]? Digit+;

fragment HexExponentPart: [pP] [+-]? Digit+;

fragment EscapeSequence:
	'\\' [abfnrtvz"'\\]
	| '\\' '\r'? '\n'
	| DecimalEscape
	| HexEscape
	| UtfEscape;

fragment DecimalEscape:
	'\\' Digit
	| '\\' Digit Digit
	| '\\' [0-2] Digit Digit;

fragment HexEscape: '\\' 'x' HexDigit HexDigit;

fragment UtfEscape: '\\' 'u{' HexDigit+ '}';

fragment Digit: [0-9];

fragment HexDigit: [0-9a-fA-F];

COMMENT: '--[' NESTED_STR ']' -> channel(COMMENTS);

LINE_COMMENT:
	'--' (
		// --
		| '[' '='* // --[==
		| '[' '='* ~('=' | '[' | '\r' | '\n') ~('\r' | '\n')* // --[==AA
		| ~('[' | '\r' | '\n') ~('\r' | '\n')* // --AAA
	) ('\r\n' | '\r' | '\n' | EOF) -> channel(COMMENTS);

WS: [ \t\u000C\r\n]+ -> channel(HIDDEN);

SHEBANG: '#' '!' ~('\n' | '\r')* -> channel(HIDDEN);