/**
 * differentiating expression grammar example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 16-jul-2023
 * @license see 'LICENSE' file
 */
#ifndef __LALR1_EXPR_GRAMMAR_H__
#define __LALR1_EXPR_GRAMMAR_H__

#include "core/symbol.h"
#include "core/common.h"
#include "script/ast.h"
#include "script/grammar_common.h"


using lalr1::NonTerminalPtr;
using lalr1::TerminalPtr;
using lalr1::t_index;
using lalr1::t_symbol_id;
using lalr1::t_semantic_id;
using lalr1::t_semanticrules;
using lalr1::t_mapIdIdx;


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


class DiffGrammar : public GrammarCommon
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

	void SetDiffVar(const std::string& var) { m_diffvar = var; }
	const std::string& GetDiffVar() const { return m_diffvar; }

        virtual t_symbol_id GetIntId() const override { return sym_int->GetId(); }
        virtual t_symbol_id GetRealId() const override { return sym_real->GetId(); }
        virtual t_symbol_id GetExprId() const override { return expr->GetId(); }


protected:
	t_astbaseptr MakeDiffFunc0(const std::string& ident) const;
	t_astbaseptr MakeDiffFunc1(const std::string& ident,
		const t_astbaseptr& arg) const;
	t_astbaseptr MakeDiffFunc2(const std::string& ident,
		const t_astbaseptr& arg1, const t_astbaseptr& arg2) const;

	t_astbaseptr MakePowFunc(const t_astbaseptr& arg1, const t_astbaseptr& arg2,
		bool only_diff_ast = false) const;


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

	// variable with respect to which we differentiate
	std::string m_diffvar{"x"};
};

#endif
