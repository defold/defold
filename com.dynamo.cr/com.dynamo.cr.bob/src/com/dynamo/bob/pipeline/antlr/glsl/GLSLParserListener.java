// Generated from GLSLParser.g4 by ANTLR 4.9.1
package com.dynamo.bob.pipeline.antlr.glsl;
import org.antlr.v4.runtime.tree.ParseTreeListener;

/**
 * This interface defines a complete listener for a parse tree produced by
 * {@link GLSLParser}.
 */
public interface GLSLParserListener extends ParseTreeListener {
	/**
	 * Enter a parse tree produced by {@link GLSLParser#translation_unit}.
	 * @param ctx the parse tree
	 */
	void enterTranslation_unit(GLSLParser.Translation_unitContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#translation_unit}.
	 * @param ctx the parse tree
	 */
	void exitTranslation_unit(GLSLParser.Translation_unitContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#variable_identifier}.
	 * @param ctx the parse tree
	 */
	void enterVariable_identifier(GLSLParser.Variable_identifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#variable_identifier}.
	 * @param ctx the parse tree
	 */
	void exitVariable_identifier(GLSLParser.Variable_identifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#primary_expression}.
	 * @param ctx the parse tree
	 */
	void enterPrimary_expression(GLSLParser.Primary_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#primary_expression}.
	 * @param ctx the parse tree
	 */
	void exitPrimary_expression(GLSLParser.Primary_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#postfix_expression}.
	 * @param ctx the parse tree
	 */
	void enterPostfix_expression(GLSLParser.Postfix_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#postfix_expression}.
	 * @param ctx the parse tree
	 */
	void exitPostfix_expression(GLSLParser.Postfix_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#field_selection}.
	 * @param ctx the parse tree
	 */
	void enterField_selection(GLSLParser.Field_selectionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#field_selection}.
	 * @param ctx the parse tree
	 */
	void exitField_selection(GLSLParser.Field_selectionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#integer_expression}.
	 * @param ctx the parse tree
	 */
	void enterInteger_expression(GLSLParser.Integer_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#integer_expression}.
	 * @param ctx the parse tree
	 */
	void exitInteger_expression(GLSLParser.Integer_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#function_call}.
	 * @param ctx the parse tree
	 */
	void enterFunction_call(GLSLParser.Function_callContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#function_call}.
	 * @param ctx the parse tree
	 */
	void exitFunction_call(GLSLParser.Function_callContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#function_identifier}.
	 * @param ctx the parse tree
	 */
	void enterFunction_identifier(GLSLParser.Function_identifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#function_identifier}.
	 * @param ctx the parse tree
	 */
	void exitFunction_identifier(GLSLParser.Function_identifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#function_call_parameters}.
	 * @param ctx the parse tree
	 */
	void enterFunction_call_parameters(GLSLParser.Function_call_parametersContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#function_call_parameters}.
	 * @param ctx the parse tree
	 */
	void exitFunction_call_parameters(GLSLParser.Function_call_parametersContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#unary_expression}.
	 * @param ctx the parse tree
	 */
	void enterUnary_expression(GLSLParser.Unary_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#unary_expression}.
	 * @param ctx the parse tree
	 */
	void exitUnary_expression(GLSLParser.Unary_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#unary_operator}.
	 * @param ctx the parse tree
	 */
	void enterUnary_operator(GLSLParser.Unary_operatorContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#unary_operator}.
	 * @param ctx the parse tree
	 */
	void exitUnary_operator(GLSLParser.Unary_operatorContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#assignment_expression}.
	 * @param ctx the parse tree
	 */
	void enterAssignment_expression(GLSLParser.Assignment_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#assignment_expression}.
	 * @param ctx the parse tree
	 */
	void exitAssignment_expression(GLSLParser.Assignment_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#assignment_operator}.
	 * @param ctx the parse tree
	 */
	void enterAssignment_operator(GLSLParser.Assignment_operatorContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#assignment_operator}.
	 * @param ctx the parse tree
	 */
	void exitAssignment_operator(GLSLParser.Assignment_operatorContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#binary_expression}.
	 * @param ctx the parse tree
	 */
	void enterBinary_expression(GLSLParser.Binary_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#binary_expression}.
	 * @param ctx the parse tree
	 */
	void exitBinary_expression(GLSLParser.Binary_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterExpression(GLSLParser.ExpressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitExpression(GLSLParser.ExpressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#constant_expression}.
	 * @param ctx the parse tree
	 */
	void enterConstant_expression(GLSLParser.Constant_expressionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#constant_expression}.
	 * @param ctx the parse tree
	 */
	void exitConstant_expression(GLSLParser.Constant_expressionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#declaration}.
	 * @param ctx the parse tree
	 */
	void enterDeclaration(GLSLParser.DeclarationContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#declaration}.
	 * @param ctx the parse tree
	 */
	void exitDeclaration(GLSLParser.DeclarationContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#identifier_list}.
	 * @param ctx the parse tree
	 */
	void enterIdentifier_list(GLSLParser.Identifier_listContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#identifier_list}.
	 * @param ctx the parse tree
	 */
	void exitIdentifier_list(GLSLParser.Identifier_listContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#function_prototype}.
	 * @param ctx the parse tree
	 */
	void enterFunction_prototype(GLSLParser.Function_prototypeContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#function_prototype}.
	 * @param ctx the parse tree
	 */
	void exitFunction_prototype(GLSLParser.Function_prototypeContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#function_parameters}.
	 * @param ctx the parse tree
	 */
	void enterFunction_parameters(GLSLParser.Function_parametersContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#function_parameters}.
	 * @param ctx the parse tree
	 */
	void exitFunction_parameters(GLSLParser.Function_parametersContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#parameter_declarator}.
	 * @param ctx the parse tree
	 */
	void enterParameter_declarator(GLSLParser.Parameter_declaratorContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#parameter_declarator}.
	 * @param ctx the parse tree
	 */
	void exitParameter_declarator(GLSLParser.Parameter_declaratorContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#parameter_declaration}.
	 * @param ctx the parse tree
	 */
	void enterParameter_declaration(GLSLParser.Parameter_declarationContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#parameter_declaration}.
	 * @param ctx the parse tree
	 */
	void exitParameter_declaration(GLSLParser.Parameter_declarationContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#parameter_type_specifier}.
	 * @param ctx the parse tree
	 */
	void enterParameter_type_specifier(GLSLParser.Parameter_type_specifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#parameter_type_specifier}.
	 * @param ctx the parse tree
	 */
	void exitParameter_type_specifier(GLSLParser.Parameter_type_specifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#init_declarator_list}.
	 * @param ctx the parse tree
	 */
	void enterInit_declarator_list(GLSLParser.Init_declarator_listContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#init_declarator_list}.
	 * @param ctx the parse tree
	 */
	void exitInit_declarator_list(GLSLParser.Init_declarator_listContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#single_declaration}.
	 * @param ctx the parse tree
	 */
	void enterSingle_declaration(GLSLParser.Single_declarationContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#single_declaration}.
	 * @param ctx the parse tree
	 */
	void exitSingle_declaration(GLSLParser.Single_declarationContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#typeless_declaration}.
	 * @param ctx the parse tree
	 */
	void enterTypeless_declaration(GLSLParser.Typeless_declarationContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#typeless_declaration}.
	 * @param ctx the parse tree
	 */
	void exitTypeless_declaration(GLSLParser.Typeless_declarationContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#fully_specified_type}.
	 * @param ctx the parse tree
	 */
	void enterFully_specified_type(GLSLParser.Fully_specified_typeContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#fully_specified_type}.
	 * @param ctx the parse tree
	 */
	void exitFully_specified_type(GLSLParser.Fully_specified_typeContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#invariant_qualifier}.
	 * @param ctx the parse tree
	 */
	void enterInvariant_qualifier(GLSLParser.Invariant_qualifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#invariant_qualifier}.
	 * @param ctx the parse tree
	 */
	void exitInvariant_qualifier(GLSLParser.Invariant_qualifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#interpolation_qualifier}.
	 * @param ctx the parse tree
	 */
	void enterInterpolation_qualifier(GLSLParser.Interpolation_qualifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#interpolation_qualifier}.
	 * @param ctx the parse tree
	 */
	void exitInterpolation_qualifier(GLSLParser.Interpolation_qualifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#layout_qualifier}.
	 * @param ctx the parse tree
	 */
	void enterLayout_qualifier(GLSLParser.Layout_qualifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#layout_qualifier}.
	 * @param ctx the parse tree
	 */
	void exitLayout_qualifier(GLSLParser.Layout_qualifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#layout_qualifier_id_list}.
	 * @param ctx the parse tree
	 */
	void enterLayout_qualifier_id_list(GLSLParser.Layout_qualifier_id_listContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#layout_qualifier_id_list}.
	 * @param ctx the parse tree
	 */
	void exitLayout_qualifier_id_list(GLSLParser.Layout_qualifier_id_listContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#layout_qualifier_id}.
	 * @param ctx the parse tree
	 */
	void enterLayout_qualifier_id(GLSLParser.Layout_qualifier_idContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#layout_qualifier_id}.
	 * @param ctx the parse tree
	 */
	void exitLayout_qualifier_id(GLSLParser.Layout_qualifier_idContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#precise_qualifier}.
	 * @param ctx the parse tree
	 */
	void enterPrecise_qualifier(GLSLParser.Precise_qualifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#precise_qualifier}.
	 * @param ctx the parse tree
	 */
	void exitPrecise_qualifier(GLSLParser.Precise_qualifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#type_qualifier}.
	 * @param ctx the parse tree
	 */
	void enterType_qualifier(GLSLParser.Type_qualifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#type_qualifier}.
	 * @param ctx the parse tree
	 */
	void exitType_qualifier(GLSLParser.Type_qualifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#single_type_qualifier}.
	 * @param ctx the parse tree
	 */
	void enterSingle_type_qualifier(GLSLParser.Single_type_qualifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#single_type_qualifier}.
	 * @param ctx the parse tree
	 */
	void exitSingle_type_qualifier(GLSLParser.Single_type_qualifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#storage_qualifier}.
	 * @param ctx the parse tree
	 */
	void enterStorage_qualifier(GLSLParser.Storage_qualifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#storage_qualifier}.
	 * @param ctx the parse tree
	 */
	void exitStorage_qualifier(GLSLParser.Storage_qualifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#type_name_list}.
	 * @param ctx the parse tree
	 */
	void enterType_name_list(GLSLParser.Type_name_listContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#type_name_list}.
	 * @param ctx the parse tree
	 */
	void exitType_name_list(GLSLParser.Type_name_listContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#type_name}.
	 * @param ctx the parse tree
	 */
	void enterType_name(GLSLParser.Type_nameContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#type_name}.
	 * @param ctx the parse tree
	 */
	void exitType_name(GLSLParser.Type_nameContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#type_specifier}.
	 * @param ctx the parse tree
	 */
	void enterType_specifier(GLSLParser.Type_specifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#type_specifier}.
	 * @param ctx the parse tree
	 */
	void exitType_specifier(GLSLParser.Type_specifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#array_specifier}.
	 * @param ctx the parse tree
	 */
	void enterArray_specifier(GLSLParser.Array_specifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#array_specifier}.
	 * @param ctx the parse tree
	 */
	void exitArray_specifier(GLSLParser.Array_specifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#dimension}.
	 * @param ctx the parse tree
	 */
	void enterDimension(GLSLParser.DimensionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#dimension}.
	 * @param ctx the parse tree
	 */
	void exitDimension(GLSLParser.DimensionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#type_specifier_nonarray}.
	 * @param ctx the parse tree
	 */
	void enterType_specifier_nonarray(GLSLParser.Type_specifier_nonarrayContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#type_specifier_nonarray}.
	 * @param ctx the parse tree
	 */
	void exitType_specifier_nonarray(GLSLParser.Type_specifier_nonarrayContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#precision_qualifier}.
	 * @param ctx the parse tree
	 */
	void enterPrecision_qualifier(GLSLParser.Precision_qualifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#precision_qualifier}.
	 * @param ctx the parse tree
	 */
	void exitPrecision_qualifier(GLSLParser.Precision_qualifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#struct_specifier}.
	 * @param ctx the parse tree
	 */
	void enterStruct_specifier(GLSLParser.Struct_specifierContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#struct_specifier}.
	 * @param ctx the parse tree
	 */
	void exitStruct_specifier(GLSLParser.Struct_specifierContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#struct_declaration_list}.
	 * @param ctx the parse tree
	 */
	void enterStruct_declaration_list(GLSLParser.Struct_declaration_listContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#struct_declaration_list}.
	 * @param ctx the parse tree
	 */
	void exitStruct_declaration_list(GLSLParser.Struct_declaration_listContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#struct_declaration}.
	 * @param ctx the parse tree
	 */
	void enterStruct_declaration(GLSLParser.Struct_declarationContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#struct_declaration}.
	 * @param ctx the parse tree
	 */
	void exitStruct_declaration(GLSLParser.Struct_declarationContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#struct_declarator_list}.
	 * @param ctx the parse tree
	 */
	void enterStruct_declarator_list(GLSLParser.Struct_declarator_listContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#struct_declarator_list}.
	 * @param ctx the parse tree
	 */
	void exitStruct_declarator_list(GLSLParser.Struct_declarator_listContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#struct_declarator}.
	 * @param ctx the parse tree
	 */
	void enterStruct_declarator(GLSLParser.Struct_declaratorContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#struct_declarator}.
	 * @param ctx the parse tree
	 */
	void exitStruct_declarator(GLSLParser.Struct_declaratorContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#initializer}.
	 * @param ctx the parse tree
	 */
	void enterInitializer(GLSLParser.InitializerContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#initializer}.
	 * @param ctx the parse tree
	 */
	void exitInitializer(GLSLParser.InitializerContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#initializer_list}.
	 * @param ctx the parse tree
	 */
	void enterInitializer_list(GLSLParser.Initializer_listContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#initializer_list}.
	 * @param ctx the parse tree
	 */
	void exitInitializer_list(GLSLParser.Initializer_listContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#declaration_statement}.
	 * @param ctx the parse tree
	 */
	void enterDeclaration_statement(GLSLParser.Declaration_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#declaration_statement}.
	 * @param ctx the parse tree
	 */
	void exitDeclaration_statement(GLSLParser.Declaration_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#statement}.
	 * @param ctx the parse tree
	 */
	void enterStatement(GLSLParser.StatementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#statement}.
	 * @param ctx the parse tree
	 */
	void exitStatement(GLSLParser.StatementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#simple_statement}.
	 * @param ctx the parse tree
	 */
	void enterSimple_statement(GLSLParser.Simple_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#simple_statement}.
	 * @param ctx the parse tree
	 */
	void exitSimple_statement(GLSLParser.Simple_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#compound_statement}.
	 * @param ctx the parse tree
	 */
	void enterCompound_statement(GLSLParser.Compound_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#compound_statement}.
	 * @param ctx the parse tree
	 */
	void exitCompound_statement(GLSLParser.Compound_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#statement_no_new_scope}.
	 * @param ctx the parse tree
	 */
	void enterStatement_no_new_scope(GLSLParser.Statement_no_new_scopeContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#statement_no_new_scope}.
	 * @param ctx the parse tree
	 */
	void exitStatement_no_new_scope(GLSLParser.Statement_no_new_scopeContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#compound_statement_no_new_scope}.
	 * @param ctx the parse tree
	 */
	void enterCompound_statement_no_new_scope(GLSLParser.Compound_statement_no_new_scopeContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#compound_statement_no_new_scope}.
	 * @param ctx the parse tree
	 */
	void exitCompound_statement_no_new_scope(GLSLParser.Compound_statement_no_new_scopeContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#statement_list}.
	 * @param ctx the parse tree
	 */
	void enterStatement_list(GLSLParser.Statement_listContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#statement_list}.
	 * @param ctx the parse tree
	 */
	void exitStatement_list(GLSLParser.Statement_listContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#expression_statement}.
	 * @param ctx the parse tree
	 */
	void enterExpression_statement(GLSLParser.Expression_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#expression_statement}.
	 * @param ctx the parse tree
	 */
	void exitExpression_statement(GLSLParser.Expression_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#selection_statement}.
	 * @param ctx the parse tree
	 */
	void enterSelection_statement(GLSLParser.Selection_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#selection_statement}.
	 * @param ctx the parse tree
	 */
	void exitSelection_statement(GLSLParser.Selection_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#selection_rest_statement}.
	 * @param ctx the parse tree
	 */
	void enterSelection_rest_statement(GLSLParser.Selection_rest_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#selection_rest_statement}.
	 * @param ctx the parse tree
	 */
	void exitSelection_rest_statement(GLSLParser.Selection_rest_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#condition}.
	 * @param ctx the parse tree
	 */
	void enterCondition(GLSLParser.ConditionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#condition}.
	 * @param ctx the parse tree
	 */
	void exitCondition(GLSLParser.ConditionContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#switch_statement}.
	 * @param ctx the parse tree
	 */
	void enterSwitch_statement(GLSLParser.Switch_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#switch_statement}.
	 * @param ctx the parse tree
	 */
	void exitSwitch_statement(GLSLParser.Switch_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#case_label}.
	 * @param ctx the parse tree
	 */
	void enterCase_label(GLSLParser.Case_labelContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#case_label}.
	 * @param ctx the parse tree
	 */
	void exitCase_label(GLSLParser.Case_labelContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#iteration_statement}.
	 * @param ctx the parse tree
	 */
	void enterIteration_statement(GLSLParser.Iteration_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#iteration_statement}.
	 * @param ctx the parse tree
	 */
	void exitIteration_statement(GLSLParser.Iteration_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#for_init_statement}.
	 * @param ctx the parse tree
	 */
	void enterFor_init_statement(GLSLParser.For_init_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#for_init_statement}.
	 * @param ctx the parse tree
	 */
	void exitFor_init_statement(GLSLParser.For_init_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#for_rest_statement}.
	 * @param ctx the parse tree
	 */
	void enterFor_rest_statement(GLSLParser.For_rest_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#for_rest_statement}.
	 * @param ctx the parse tree
	 */
	void exitFor_rest_statement(GLSLParser.For_rest_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#jump_statement}.
	 * @param ctx the parse tree
	 */
	void enterJump_statement(GLSLParser.Jump_statementContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#jump_statement}.
	 * @param ctx the parse tree
	 */
	void exitJump_statement(GLSLParser.Jump_statementContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#external_declaration}.
	 * @param ctx the parse tree
	 */
	void enterExternal_declaration(GLSLParser.External_declarationContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#external_declaration}.
	 * @param ctx the parse tree
	 */
	void exitExternal_declaration(GLSLParser.External_declarationContext ctx);
	/**
	 * Enter a parse tree produced by {@link GLSLParser#function_definition}.
	 * @param ctx the parse tree
	 */
	void enterFunction_definition(GLSLParser.Function_definitionContext ctx);
	/**
	 * Exit a parse tree produced by {@link GLSLParser#function_definition}.
	 * @param ctx the parse tree
	 */
	void exitFunction_definition(GLSLParser.Function_definitionContext ctx);
}