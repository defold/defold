// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline.antlr.lua;

// Generated from LuaParser.g4 by ANTLR 4.9.1
import org.antlr.v4.runtime.tree.ParseTreeListener;

/**
 * This interface defines a complete listener for a parse tree produced by
 * {@link LuaParser}.
 */
public interface LuaParserListener extends ParseTreeListener {
	/**
	 * Enter a parse tree produced by {@link LuaParser#chunk}.
	 * @param ctx the parse tree
	 */
	void enterChunk(LuaParser.ChunkContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#chunk}.
	 * @param ctx the parse tree
	 */
	void exitChunk(LuaParser.ChunkContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#block}.
	 * @param ctx the parse tree
	 */
	void enterBlock(LuaParser.BlockContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#block}.
	 * @param ctx the parse tree
	 */
	void exitBlock(LuaParser.BlockContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#stat}.
	 * @param ctx the parse tree
	 */
	void enterStat(LuaParser.StatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#stat}.
	 * @param ctx the parse tree
	 */
	void exitStat(LuaParser.StatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#breakstat}.
	 * @param ctx the parse tree
	 */
	void enterBreakstat(LuaParser.BreakstatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#breakstat}.
	 * @param ctx the parse tree
	 */
	void exitBreakstat(LuaParser.BreakstatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#gotostat}.
	 * @param ctx the parse tree
	 */
	void enterGotostat(LuaParser.GotostatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#gotostat}.
	 * @param ctx the parse tree
	 */
	void exitGotostat(LuaParser.GotostatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#dostat}.
	 * @param ctx the parse tree
	 */
	void enterDostat(LuaParser.DostatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#dostat}.
	 * @param ctx the parse tree
	 */
	void exitDostat(LuaParser.DostatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#whilestat}.
	 * @param ctx the parse tree
	 */
	void enterWhilestat(LuaParser.WhilestatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#whilestat}.
	 * @param ctx the parse tree
	 */
	void exitWhilestat(LuaParser.WhilestatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#repeatstat}.
	 * @param ctx the parse tree
	 */
	void enterRepeatstat(LuaParser.RepeatstatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#repeatstat}.
	 * @param ctx the parse tree
	 */
	void exitRepeatstat(LuaParser.RepeatstatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#ifstat}.
	 * @param ctx the parse tree
	 */
	void enterIfstat(LuaParser.IfstatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#ifstat}.
	 * @param ctx the parse tree
	 */
	void exitIfstat(LuaParser.IfstatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#genericforstat}.
	 * @param ctx the parse tree
	 */
	void enterGenericforstat(LuaParser.GenericforstatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#genericforstat}.
	 * @param ctx the parse tree
	 */
	void exitGenericforstat(LuaParser.GenericforstatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#numericforstat}.
	 * @param ctx the parse tree
	 */
	void enterNumericforstat(LuaParser.NumericforstatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#numericforstat}.
	 * @param ctx the parse tree
	 */
	void exitNumericforstat(LuaParser.NumericforstatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#functionstat}.
	 * @param ctx the parse tree
	 */
	void enterFunctionstat(LuaParser.FunctionstatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#functionstat}.
	 * @param ctx the parse tree
	 */
	void exitFunctionstat(LuaParser.FunctionstatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#localfunctionstat}.
	 * @param ctx the parse tree
	 */
	void enterLocalfunctionstat(LuaParser.LocalfunctionstatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#localfunctionstat}.
	 * @param ctx the parse tree
	 */
	void exitLocalfunctionstat(LuaParser.LocalfunctionstatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#localvariablestat}.
	 * @param ctx the parse tree
	 */
	void enterLocalvariablestat(LuaParser.LocalvariablestatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#localvariablestat}.
	 * @param ctx the parse tree
	 */
	void exitLocalvariablestat(LuaParser.LocalvariablestatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#variablestat}.
	 * @param ctx the parse tree
	 */
	void enterVariablestat(LuaParser.VariablestatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#variablestat}.
	 * @param ctx the parse tree
	 */
	void exitVariablestat(LuaParser.VariablestatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#attnamelist}.
	 * @param ctx the parse tree
	 */
	void enterAttnamelist(LuaParser.AttnamelistContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#attnamelist}.
	 * @param ctx the parse tree
	 */
	void exitAttnamelist(LuaParser.AttnamelistContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#attrib}.
	 * @param ctx the parse tree
	 */
	void enterAttrib(LuaParser.AttribContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#attrib}.
	 * @param ctx the parse tree
	 */
	void exitAttrib(LuaParser.AttribContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#retstat}.
	 * @param ctx the parse tree
	 */
	void enterRetstat(LuaParser.RetstatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#retstat}.
	 * @param ctx the parse tree
	 */
	void exitRetstat(LuaParser.RetstatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#label}.
	 * @param ctx the parse tree
	 */
	void enterLabel(LuaParser.LabelContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#label}.
	 * @param ctx the parse tree
	 */
	void exitLabel(LuaParser.LabelContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#funcname}.
	 * @param ctx the parse tree
	 */
	void enterFuncname(LuaParser.FuncnameContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#funcname}.
	 * @param ctx the parse tree
	 */
	void exitFuncname(LuaParser.FuncnameContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#variablelist}.
	 * @param ctx the parse tree
	 */
	void enterVariablelist(LuaParser.VariablelistContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#variablelist}.
	 * @param ctx the parse tree
	 */
	void exitVariablelist(LuaParser.VariablelistContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#namelist}.
	 * @param ctx the parse tree
	 */
	void enterNamelist(LuaParser.NamelistContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#namelist}.
	 * @param ctx the parse tree
	 */
	void exitNamelist(LuaParser.NamelistContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#explist}.
	 * @param ctx the parse tree
	 */
	void enterExplist(LuaParser.ExplistContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#explist}.
	 * @param ctx the parse tree
	 */
	void exitExplist(LuaParser.ExplistContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#exp}.
	 * @param ctx the parse tree
	 */
	void enterExp(LuaParser.ExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#exp}.
	 * @param ctx the parse tree
	 */
	void exitExp(LuaParser.ExpContext ctx);
	/**
	 * Enter a parse tree produced by the {@code namedvariable}
	 * labeled alternative in {@link LuaParser#variable}.
	 * @param ctx the parse tree
	 */
	void enterNamedvariable(LuaParser.NamedvariableContext ctx);
	/**
	 * Exit a parse tree produced by the {@code namedvariable}
	 * labeled alternative in {@link LuaParser#variable}.
	 * @param ctx the parse tree
	 */
	void exitNamedvariable(LuaParser.NamedvariableContext ctx);
	/**
	 * Enter a parse tree produced by the {@code parenthesesvariable}
	 * labeled alternative in {@link LuaParser#variable}.
	 * @param ctx the parse tree
	 */
	void enterParenthesesvariable(LuaParser.ParenthesesvariableContext ctx);
	/**
	 * Exit a parse tree produced by the {@code parenthesesvariable}
	 * labeled alternative in {@link LuaParser#variable}.
	 * @param ctx the parse tree
	 */
	void exitParenthesesvariable(LuaParser.ParenthesesvariableContext ctx);
	/**
	 * Enter a parse tree produced by the {@code functioncall}
	 * labeled alternative in {@link LuaParser#variable}.
	 * @param ctx the parse tree
	 */
	void enterFunctioncall(LuaParser.FunctioncallContext ctx);
	/**
	 * Exit a parse tree produced by the {@code functioncall}
	 * labeled alternative in {@link LuaParser#variable}.
	 * @param ctx the parse tree
	 */
	void exitFunctioncall(LuaParser.FunctioncallContext ctx);
	/**
	 * Enter a parse tree produced by the {@code index}
	 * labeled alternative in {@link LuaParser#variable}.
	 * @param ctx the parse tree
	 */
	void enterIndex(LuaParser.IndexContext ctx);
	/**
	 * Exit a parse tree produced by the {@code index}
	 * labeled alternative in {@link LuaParser#variable}.
	 * @param ctx the parse tree
	 */
	void exitIndex(LuaParser.IndexContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#nameAndArgs}.
	 * @param ctx the parse tree
	 */
	void enterNameAndArgs(LuaParser.NameAndArgsContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#nameAndArgs}.
	 * @param ctx the parse tree
	 */
	void exitNameAndArgs(LuaParser.NameAndArgsContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#args}.
	 * @param ctx the parse tree
	 */
	void enterArgs(LuaParser.ArgsContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#args}.
	 * @param ctx the parse tree
	 */
	void exitArgs(LuaParser.ArgsContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#functiondef}.
	 * @param ctx the parse tree
	 */
	void enterFunctiondef(LuaParser.FunctiondefContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#functiondef}.
	 * @param ctx the parse tree
	 */
	void exitFunctiondef(LuaParser.FunctiondefContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#funcbody}.
	 * @param ctx the parse tree
	 */
	void enterFuncbody(LuaParser.FuncbodyContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#funcbody}.
	 * @param ctx the parse tree
	 */
	void exitFuncbody(LuaParser.FuncbodyContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#parlist}.
	 * @param ctx the parse tree
	 */
	void enterParlist(LuaParser.ParlistContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#parlist}.
	 * @param ctx the parse tree
	 */
	void exitParlist(LuaParser.ParlistContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#tableconstructor}.
	 * @param ctx the parse tree
	 */
	void enterTableconstructor(LuaParser.TableconstructorContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#tableconstructor}.
	 * @param ctx the parse tree
	 */
	void exitTableconstructor(LuaParser.TableconstructorContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#fieldlist}.
	 * @param ctx the parse tree
	 */
	void enterFieldlist(LuaParser.FieldlistContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#fieldlist}.
	 * @param ctx the parse tree
	 */
	void exitFieldlist(LuaParser.FieldlistContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#field}.
	 * @param ctx the parse tree
	 */
	void enterField(LuaParser.FieldContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#field}.
	 * @param ctx the parse tree
	 */
	void exitField(LuaParser.FieldContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#fieldsep}.
	 * @param ctx the parse tree
	 */
	void enterFieldsep(LuaParser.FieldsepContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#fieldsep}.
	 * @param ctx the parse tree
	 */
	void exitFieldsep(LuaParser.FieldsepContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#operatorOr}.
	 * @param ctx the parse tree
	 */
	void enterOperatorOr(LuaParser.OperatorOrContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#operatorOr}.
	 * @param ctx the parse tree
	 */
	void exitOperatorOr(LuaParser.OperatorOrContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#operatorAnd}.
	 * @param ctx the parse tree
	 */
	void enterOperatorAnd(LuaParser.OperatorAndContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#operatorAnd}.
	 * @param ctx the parse tree
	 */
	void exitOperatorAnd(LuaParser.OperatorAndContext ctx);
	/**
	 * Enter a parse tree produced by the {@code lessthan}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void enterLessthan(LuaParser.LessthanContext ctx);
	/**
	 * Exit a parse tree produced by the {@code lessthan}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void exitLessthan(LuaParser.LessthanContext ctx);
	/**
	 * Enter a parse tree produced by the {@code greaterthan}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void enterGreaterthan(LuaParser.GreaterthanContext ctx);
	/**
	 * Exit a parse tree produced by the {@code greaterthan}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void exitGreaterthan(LuaParser.GreaterthanContext ctx);
	/**
	 * Enter a parse tree produced by the {@code lessthanorequal}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void enterLessthanorequal(LuaParser.LessthanorequalContext ctx);
	/**
	 * Exit a parse tree produced by the {@code lessthanorequal}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void exitLessthanorequal(LuaParser.LessthanorequalContext ctx);
	/**
	 * Enter a parse tree produced by the {@code greaterthanorequal}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void enterGreaterthanorequal(LuaParser.GreaterthanorequalContext ctx);
	/**
	 * Exit a parse tree produced by the {@code greaterthanorequal}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void exitGreaterthanorequal(LuaParser.GreaterthanorequalContext ctx);
	/**
	 * Enter a parse tree produced by the {@code notequal}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void enterNotequal(LuaParser.NotequalContext ctx);
	/**
	 * Exit a parse tree produced by the {@code notequal}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void exitNotequal(LuaParser.NotequalContext ctx);
	/**
	 * Enter a parse tree produced by the {@code equal}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void enterEqual(LuaParser.EqualContext ctx);
	/**
	 * Exit a parse tree produced by the {@code equal}
	 * labeled alternative in {@link LuaParser#operatorComparison}.
	 * @param ctx the parse tree
	 */
	void exitEqual(LuaParser.EqualContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#operatorStrcat}.
	 * @param ctx the parse tree
	 */
	void enterOperatorStrcat(LuaParser.OperatorStrcatContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#operatorStrcat}.
	 * @param ctx the parse tree
	 */
	void exitOperatorStrcat(LuaParser.OperatorStrcatContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#operatorAddSub}.
	 * @param ctx the parse tree
	 */
	void enterOperatorAddSub(LuaParser.OperatorAddSubContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#operatorAddSub}.
	 * @param ctx the parse tree
	 */
	void exitOperatorAddSub(LuaParser.OperatorAddSubContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#operatorMulDivMod}.
	 * @param ctx the parse tree
	 */
	void enterOperatorMulDivMod(LuaParser.OperatorMulDivModContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#operatorMulDivMod}.
	 * @param ctx the parse tree
	 */
	void exitOperatorMulDivMod(LuaParser.OperatorMulDivModContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#operatorBitwise}.
	 * @param ctx the parse tree
	 */
	void enterOperatorBitwise(LuaParser.OperatorBitwiseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#operatorBitwise}.
	 * @param ctx the parse tree
	 */
	void exitOperatorBitwise(LuaParser.OperatorBitwiseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#operatorUnary}.
	 * @param ctx the parse tree
	 */
	void enterOperatorUnary(LuaParser.OperatorUnaryContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#operatorUnary}.
	 * @param ctx the parse tree
	 */
	void exitOperatorUnary(LuaParser.OperatorUnaryContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#operatorPower}.
	 * @param ctx the parse tree
	 */
	void enterOperatorPower(LuaParser.OperatorPowerContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#operatorPower}.
	 * @param ctx the parse tree
	 */
	void exitOperatorPower(LuaParser.OperatorPowerContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#number}.
	 * @param ctx the parse tree
	 */
	void enterNumber(LuaParser.NumberContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#number}.
	 * @param ctx the parse tree
	 */
	void exitNumber(LuaParser.NumberContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuaParser#lstring}.
	 * @param ctx the parse tree
	 */
	void enterLstring(LuaParser.LstringContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuaParser#lstring}.
	 * @param ctx the parse tree
	 */
	void exitLstring(LuaParser.LstringContext ctx);
}
