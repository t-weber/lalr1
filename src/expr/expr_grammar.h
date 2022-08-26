/**
 * expression grammar example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-jun-2022
 * @license see 'LICENSE' file
 */
#ifndef __LALR1_EXPR_GRAMMAR_H__
#define __LALR1_EXPR_GRAMMAR_H__

#include "lalr1/symbol.h"
#include "script/ast.h"


/**
 * non-terminals identifiers
 */
enum : std::size_t
{
	START,      // start
	EXPR,       // expression
};


class ExprGrammar
{
public:
	void CreateGrammar(bool add_rules = true, bool add_semantics = true);

	template<template<class...> class t_cont = std::vector>
	t_cont<NonTerminalPtr> GetAllNonTerminals() const
	{
		return t_cont<NonTerminalPtr>{{ start, expr }};
	}

	const NonTerminalPtr& GetStartNonTerminal() const { return start; }
	const std::vector<t_semanticrule>& GetSemanticRules() const { return rules; }


private:
	// non-terminals
	NonTerminalPtr start{}, expr{};

	// terminals
	//TerminalPtr op_assign{};
	TerminalPtr op_plus{}, op_minus{},
		op_mult{}, op_div{}, op_mod{}, op_pow{};
	TerminalPtr bracket_open{}, bracket_close{};
	TerminalPtr comma{};
	TerminalPtr sym_real{}, sym_int{}, ident{};

	// semantic rules
	std::vector<t_semanticrule> rules{};
};

#endif
