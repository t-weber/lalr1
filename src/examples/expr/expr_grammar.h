/**
 * expression grammar example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-jun-2022
 * @license see 'LICENSE' file
 */
#ifndef __LALR1_EXPR_GRAMMAR_H__
#define __LALR1_EXPR_GRAMMAR_H__

#include "core/symbol.h"
#include "script/ast.h"


/**
 * non-terminals identifiers
 */
enum : t_symbol_id
{
	START    = 10,
	EXPR     = 20,
};


/**
 * semantic rule identifiers
 */
enum class Semantics : t_semantic_id
{
	START    = 100,
	BRACKETS = 101,
	ADD      = 200,
	SUB      = 201,
	MUL      = 202,
	DIV      = 203,
	MOD      = 204,
	POW      = 205,
	UADD     = 210,
	USUB     = 211,
	CALL0    = 300,
	CALL1    = 301,
	CALL2    = 302,
	REAL     = 400,
	INT      = 401,
	IDENT    = 410,
	ASSIGN   = 500,
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
	const t_semanticrules& GetSemanticRules() const { return rules; }


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
	t_semanticrules rules{};
};

#endif
