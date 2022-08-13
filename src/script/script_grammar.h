/**
 * script grammar example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-jun-2022
 * @license see 'LICENSE' file
 */

#include "lalr1/symbol.h"
#include "ast.h"


/**
 * non-terminals identifiers
 */
enum : std::size_t
{
	START,      // start
	STMTS,      // list of statements
	STMT,       // statement
	EXPR,       // expression
	EXPRS,      // list of expressions
	BOOL_EXPR,  // boolean expression
	IDENTS,     // list of identifiers
};


class ScriptGrammar
{
public:
	void CreateGrammar(bool add_rules = true, bool add_semantics = true);

	std::vector<NonTerminalPtr> GetAllNonTerminals() const;
	NonTerminalPtr GetStartNonTerminal() const;
	const std::vector<t_semanticrule>& GetSemanticRules() const;


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
	std::vector<t_semanticrule> rules{};
};
