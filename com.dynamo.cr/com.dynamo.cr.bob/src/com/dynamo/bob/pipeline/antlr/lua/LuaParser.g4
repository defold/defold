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

parser grammar LuaParser;
options {
	tokenVocab = LuaLexer;
}

chunk: block EOF;

block: stat* retstat?;

stat:
	SEMICOLON
	| variablestat
	| variable
	| label
	| breakstat
	| gotostat
	| dostat
	| whilestat
	| repeatstat
	| ifstat
	| numericforstat
	| genericforstat
	| functionstat
	| localfunctionstat
	| localvariablestat;

breakstat: BREAK;
gotostat: GOTO NAME;
dostat: DO block END;
whilestat: WHILE exp DO block END;
repeatstat: REPEAT block UNTIL exp;
ifstat:
	IF exp THEN block (ELSEIF exp THEN block)* (ELSE block)? END;
genericforstat: FOR namelist IN explist DO block END;
numericforstat:
	FOR NAME EQUALS exp COMMA exp (COMMA exp)? DO block END;
functionstat: FUNCTION funcname funcbody;
localfunctionstat: LOCAL FUNCTION NAME funcbody;
localvariablestat: LOCAL attnamelist (EQUALS explist)?;
variablestat: variablelist EQUALS explist;

attnamelist: NAME attrib (COMMA NAME attrib)*;

attrib: (LT NAME GT)?;

retstat: RETURN explist? SEMICOLON?;

label: DCOLON NAME DCOLON;

funcname: NAME (DOT NAME)* (COLON NAME)?;

variablelist: variable (COMMA variable)*;

namelist: NAME (COMMA NAME)*;

explist: exp (COMMA exp)*;

exp:
	NIL
	| FALSE
	| TRUE
	| number
	| lstring
	| DOTS
	| functiondef
	| variable
	| tableconstructor
	| <assoc = right> exp operatorPower exp
	| operatorUnary exp
	| exp operatorMulDivMod exp
	| exp operatorAddSub exp
	| <assoc = right> exp operatorStrcat exp
	| exp operatorComparison exp
	| exp operatorAnd exp
	| exp operatorOr exp
	| exp operatorBitwise exp;

variable:
	NAME										# namedvariable
	| variable (LBRACK exp RBRACK | DOT NAME)	# index
	| LPAREN exp RPAREN							# parenthesesvariable
	| variable nameAndArgs						# functioncall;

nameAndArgs: (COLON NAME)? args;

args: LPAREN explist? RPAREN | tableconstructor | lstring;

functiondef: FUNCTION funcbody;

funcbody: LPAREN parlist? RPAREN block END;

parlist: namelist (COMMA DOTS)? | DOTS;

tableconstructor: LBRACE fieldlist? RBRACE;

fieldlist: field (fieldsep field)* fieldsep?;

field: LBRACK exp RBRACK EQUALS exp | NAME EQUALS exp | exp;

fieldsep: COMMA | SEMICOLON;

operatorOr: OR;

operatorAnd: AND;

operatorComparison:
	LT		# lessthan
	| GT	# greaterthan
	| LTE	# lessthanorequal
	| GTE	# greaterthanorequal
	| NEQ	# notequal
	| EQ	# equal;

operatorStrcat: STRCAT;

operatorAddSub: PLUS | MINUS;

operatorMulDivMod: MUL | DIV | MOD | DIVFLOOR;

operatorBitwise: BITAND | BITOR | BITNOT | BITSHL | BITSHR;

operatorUnary: NOT | LEN | MINUS | BITNOT;

operatorPower: POWER;

number: INT | HEX | FLOAT | HEX_FLOAT;

lstring: NORMALSTRING | CHARSTRING | LONGSTRING;
