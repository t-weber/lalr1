/**
 * script grammar example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-jun-2022
 * @license see 'LICENSE' file
 */
#ifndef __LALR1_SCRIPT_GRAMMAR_H__
#define __LALR1_SCRIPT_GRAMMAR_H__

#include "core/symbol.h"
#include "script/ast.h"
#include "script/grammar_common.h"


using lalr1::NonTerminalPtr;
using lalr1::TerminalPtr;
using lalr1::t_semanticrules;
using lalr1::t_symbol_id;


/**
 * non-terminals identifiers
 */
enum : lalr1::t_symbol_id
{
	START,      // start
	STMTS,      // list of statements
	STMT,       // statement
	EXPR,       // expression
	EXPRS,      // list of expressions
	BOOL_EXPR,  // boolean expression
	IDENTS,     // list of identifiers
};


class ScriptGrammar : public GrammarCommon
{
public:
	void CreateGrammar(bool add_rules = true, bool add_semantics = true);

	template<template<class...> class t_cont = std::vector>
	t_cont<NonTerminalPtr> GetAllNonTerminals() const
	{
		return t_cont<NonTerminalPtr>{{
			start, stmts, stmt, exprs, expr, bool_expr, idents }};
	}

	const NonTerminalPtr& GetStartNonTerminal() const { return start; }
	const t_semanticrules& GetSemanticRules() const { return rules; }

        virtual t_symbol_id GetIntId() const override { return sym_int->GetId(); }
        virtual t_symbol_id GetRealId() const override { return sym_real->GetId(); }
        virtual t_symbol_id GetExprId() const override { return expr->GetId(); }


private:
	// non-terminals
	NonTerminalPtr start{},
		stmts{}, stmt{},
		exprs{}, expr{},
		bool_expr{}, idents{};

	// terminals
	TerminalPtr op_assign{}, op_plus{}, op_minus{},
		op_mult{}, op_div{}, op_mod{}, op_pow{};
	TerminalPtr op_and{}, op_or{}, op_not{},
		op_equ{}, op_nequ{},
		op_lt{}, op_gt{}, op_gequ{}, op_lequ{};
	TerminalPtr op_shift_left{}, op_shift_right{};
	TerminalPtr op_binand{}, op_binor{}, op_binxor{}, op_binnot{};
	TerminalPtr bracket_open{}, bracket_close{};
	TerminalPtr block_begin{}, block_end{};
	TerminalPtr keyword_if{}, keyword_else{}, keyword_loop{},
		keyword_break{}, keyword_continue{};
	TerminalPtr keyword_func{}, keyword_extern{}, keyword_return{};
	TerminalPtr comma{}, stmt_end{};
	TerminalPtr sym_real{}, sym_int{}, sym_str{}, ident{};

	// semantic rules
	t_semanticrules rules{};
};

#endif
