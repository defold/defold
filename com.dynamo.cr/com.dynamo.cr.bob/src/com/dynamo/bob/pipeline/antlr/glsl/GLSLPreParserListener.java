// Generated from GLSLPreParser.g4 by ANTLR 4.9.1
package com.dynamo.bob.pipeline.antlr.glsl;
import org.antlr.v4.runtime.tree.ParseTreeListener;

/**
 * This interface defines a complete listener for a parse tree produced by
 * {@link GLSLPreParser}.
 */
public interface GLSLPreParserListener extends ParseTreeListener {
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#translation_unit}.
	 * @param ctx the parse tree
	 */
	void enterTranslation_unit(GLSLPreParser.Translation_unitContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#translation_unit}.
	 * @param ctx the parse tree
	 */
	void exitTranslation_unit(GLSLPreParser.Translation_unitContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#compiler_directive}.
	 * @param ctx the parse tree
	 */
	void enterCompiler_directive(GLSLPreParser.Compiler_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#compiler_directive}.
	 * @param ctx the parse tree
	 */
	void exitCompiler_directive(GLSLPreParser.Compiler_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#behavior}.
	 * @param ctx the parse tree
	 */
	void enterBehavior(GLSLPreParser.BehaviorContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#behavior}.
	 * @param ctx the parse tree
	 */
	void exitBehavior(GLSLPreParser.BehaviorContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#constant_expression}.
	 * @param ctx the parse tree
	 */
	void enterConstant_expression(GLSLPreParser.Constant_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#constant_expression}.
	 * @param ctx the parse tree
	 */
	void exitConstant_expression(GLSLPreParser.Constant_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#define_directive}.
	 * @param ctx the parse tree
	 */
	void enterDefine_directive(GLSLPreParser.Define_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#define_directive}.
	 * @param ctx the parse tree
	 */
	void exitDefine_directive(GLSLPreParser.Define_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#elif_directive}.
	 * @param ctx the parse tree
	 */
	void enterElif_directive(GLSLPreParser.Elif_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#elif_directive}.
	 * @param ctx the parse tree
	 */
	void exitElif_directive(GLSLPreParser.Elif_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#else_directive}.
	 * @param ctx the parse tree
	 */
	void enterElse_directive(GLSLPreParser.Else_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#else_directive}.
	 * @param ctx the parse tree
	 */
	void exitElse_directive(GLSLPreParser.Else_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#endif_directive}.
	 * @param ctx the parse tree
	 */
	void enterEndif_directive(GLSLPreParser.Endif_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#endif_directive}.
	 * @param ctx the parse tree
	 */
	void exitEndif_directive(GLSLPreParser.Endif_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#error_directive}.
	 * @param ctx the parse tree
	 */
	void enterError_directive(GLSLPreParser.Error_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#error_directive}.
	 * @param ctx the parse tree
	 */
	void exitError_directive(GLSLPreParser.Error_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#error_message}.
	 * @param ctx the parse tree
	 */
	void enterError_message(GLSLPreParser.Error_messageContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#error_message}.
	 * @param ctx the parse tree
	 */
	void exitError_message(GLSLPreParser.Error_messageContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#extension_directive}.
	 * @param ctx the parse tree
	 */
	void enterExtension_directive(GLSLPreParser.Extension_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#extension_directive}.
	 * @param ctx the parse tree
	 */
	void exitExtension_directive(GLSLPreParser.Extension_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#extension_name}.
	 * @param ctx the parse tree
	 */
	void enterExtension_name(GLSLPreParser.Extension_nameContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#extension_name}.
	 * @param ctx the parse tree
	 */
	void exitExtension_name(GLSLPreParser.Extension_nameContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#group_of_lines}.
	 * @param ctx the parse tree
	 */
	void enterGroup_of_lines(GLSLPreParser.Group_of_linesContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#group_of_lines}.
	 * @param ctx the parse tree
	 */
	void exitGroup_of_lines(GLSLPreParser.Group_of_linesContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#if_directive}.
	 * @param ctx the parse tree
	 */
	void enterIf_directive(GLSLPreParser.If_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#if_directive}.
	 * @param ctx the parse tree
	 */
	void exitIf_directive(GLSLPreParser.If_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#ifdef_directive}.
	 * @param ctx the parse tree
	 */
	void enterIfdef_directive(GLSLPreParser.Ifdef_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#ifdef_directive}.
	 * @param ctx the parse tree
	 */
	void exitIfdef_directive(GLSLPreParser.Ifdef_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#ifndef_directive}.
	 * @param ctx the parse tree
	 */
	void enterIfndef_directive(GLSLPreParser.Ifndef_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#ifndef_directive}.
	 * @param ctx the parse tree
	 */
	void exitIfndef_directive(GLSLPreParser.Ifndef_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#line_directive}.
	 * @param ctx the parse tree
	 */
	void enterLine_directive(GLSLPreParser.Line_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#line_directive}.
	 * @param ctx the parse tree
	 */
	void exitLine_directive(GLSLPreParser.Line_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#line_expression}.
	 * @param ctx the parse tree
	 */
	void enterLine_expression(GLSLPreParser.Line_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#line_expression}.
	 * @param ctx the parse tree
	 */
	void exitLine_expression(GLSLPreParser.Line_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#macro_esc_newline}.
	 * @param ctx the parse tree
	 */
	void enterMacro_esc_newline(GLSLPreParser.Macro_esc_newlineContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#macro_esc_newline}.
	 * @param ctx the parse tree
	 */
	void exitMacro_esc_newline(GLSLPreParser.Macro_esc_newlineContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#macro_identifier}.
	 * @param ctx the parse tree
	 */
	void enterMacro_identifier(GLSLPreParser.Macro_identifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#macro_identifier}.
	 * @param ctx the parse tree
	 */
	void exitMacro_identifier(GLSLPreParser.Macro_identifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#macro_name}.
	 * @param ctx the parse tree
	 */
	void enterMacro_name(GLSLPreParser.Macro_nameContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#macro_name}.
	 * @param ctx the parse tree
	 */
	void exitMacro_name(GLSLPreParser.Macro_nameContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#macro_text}.
	 * @param ctx the parse tree
	 */
	void enterMacro_text(GLSLPreParser.Macro_textContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#macro_text}.
	 * @param ctx the parse tree
	 */
	void exitMacro_text(GLSLPreParser.Macro_textContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#macro_text_}.
	 * @param ctx the parse tree
	 */
	void enterMacro_text_(GLSLPreParser.Macro_text_Context ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#macro_text_}.
	 * @param ctx the parse tree
	 */
	void exitMacro_text_(GLSLPreParser.Macro_text_Context ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#number}.
	 * @param ctx the parse tree
	 */
	void enterNumber(GLSLPreParser.NumberContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#number}.
	 * @param ctx the parse tree
	 */
	void exitNumber(GLSLPreParser.NumberContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#off}.
	 * @param ctx the parse tree
	 */
	void enterOff(GLSLPreParser.OffContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#off}.
	 * @param ctx the parse tree
	 */
	void exitOff(GLSLPreParser.OffContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#on}.
	 * @param ctx the parse tree
	 */
	void enterOn(GLSLPreParser.OnContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#on}.
	 * @param ctx the parse tree
	 */
	void exitOn(GLSLPreParser.OnContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#pragma_debug}.
	 * @param ctx the parse tree
	 */
	void enterPragma_debug(GLSLPreParser.Pragma_debugContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#pragma_debug}.
	 * @param ctx the parse tree
	 */
	void exitPragma_debug(GLSLPreParser.Pragma_debugContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#pragma_directive}.
	 * @param ctx the parse tree
	 */
	void enterPragma_directive(GLSLPreParser.Pragma_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#pragma_directive}.
	 * @param ctx the parse tree
	 */
	void exitPragma_directive(GLSLPreParser.Pragma_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#pragma_optimize}.
	 * @param ctx the parse tree
	 */
	void enterPragma_optimize(GLSLPreParser.Pragma_optimizeContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#pragma_optimize}.
	 * @param ctx the parse tree
	 */
	void exitPragma_optimize(GLSLPreParser.Pragma_optimizeContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#profile}.
	 * @param ctx the parse tree
	 */
	void enterProfile(GLSLPreParser.ProfileContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#profile}.
	 * @param ctx the parse tree
	 */
	void exitProfile(GLSLPreParser.ProfileContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#program_text}.
	 * @param ctx the parse tree
	 */
	void enterProgram_text(GLSLPreParser.Program_textContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#program_text}.
	 * @param ctx the parse tree
	 */
	void exitProgram_text(GLSLPreParser.Program_textContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#stdgl}.
	 * @param ctx the parse tree
	 */
	void enterStdgl(GLSLPreParser.StdglContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#stdgl}.
	 * @param ctx the parse tree
	 */
	void exitStdgl(GLSLPreParser.StdglContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#undef_directive}.
	 * @param ctx the parse tree
	 */
	void enterUndef_directive(GLSLPreParser.Undef_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#undef_directive}.
	 * @param ctx the parse tree
	 */
	void exitUndef_directive(GLSLPreParser.Undef_directiveContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLPreParser#version_directive}.
	 * @param ctx the parse tree
	 */
	void enterVersion_directive(GLSLPreParser.Version_directiveContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLPreParser#version_directive}.
	 * @param ctx the parse tree
	 */
	void exitVersion_directive(GLSLPreParser.Version_directiveContext ctx);
}