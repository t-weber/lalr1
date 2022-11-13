/**
 * lalr(1) parser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_PARSER_H__
#define __LALR1_PARSER_H__

#include "common.h"
#include "stack.h"

#include <optional>


/**
 * lalr(1) parser
 */
class Parser
{
public:
	Parser() = default;
	~Parser() = default;

	Parser(const Parser& parser);
	Parser& operator=(const Parser& parser);

	void SetShiftTable(const t_table* tab) { m_tabActionShift = tab; }
	void SetReduceTable(const t_table* tab) { m_tabActionReduce = tab; }
	void SetJumpTable(const t_table* tab) { m_tabJump = tab; }

	void SetPartialsRulesTerm(const t_table* tab) { m_tabPartialsRulesTerm = tab; }
	void SetPartialsMatchLenTerm(const t_table* tab) { m_tabPartialsMatchLenTerm = tab; }
	void SetPartialsRulesNonTerm(const t_table* tab) { m_tabPartialsRulesNonTerm = tab; }
	void SetPartialsMatchLenNonTerm(const t_table* tab) { m_tabPartialsMatchLenNonTerm = tab; }

	void SetNumRhsSymsPerRule(const t_vecIdx* vec) { m_numRhsSymsPerRule = vec; }
	void SetLhsIndices(const t_vecIdx* vec) { m_vecLhsIndices = vec; }

	void SetSemanticIdxMap(const t_mapSemanticIdIdx* map);
	void SetSemanticRules(const t_semanticrules* rules) { m_semantics = rules; }

	void SetEndId(t_symbol_id id) { m_end = id; }
	void SetStartingState(t_index state) { m_starting_state = state; }

	void SetDebug(bool b) { m_debug = b; }

	t_lalrastbaseptr Parse(const t_toknodes& input) const;


protected:
	// get the rule index and the matching length of a partial match
	std::tuple<std::optional<t_index>, std::optional<std::size_t>>
		GetPartialRule(t_state_id topstate, const t_toknode& curtok,
			const ParseStack<t_lalrastbaseptr>& symbols, bool term) const;

	// get a semantic rule id from a rule index
	t_semantic_id GetRuleId(t_index idx) const;


private:
	// lalr(1) parse tables
	const t_table *m_tabActionShift{nullptr};
	const t_table *m_tabActionReduce{nullptr};
	const t_table *m_tabJump{nullptr};

	// lalr(1) partial matches tables
	const t_table *m_tabPartialsRulesTerm{nullptr};
	const t_table *m_tabPartialsMatchLenTerm{nullptr};
	const t_table *m_tabPartialsRulesNonTerm{nullptr};
	const t_table *m_tabPartialsMatchLenNonTerm{nullptr};

	// number of symbols on right hand side of a rule
	const t_vecIdx *m_numRhsSymsPerRule{nullptr};

	// index of nonterminal on left-hand side of rule
	const t_vecIdx *m_vecLhsIndices{nullptr};

	// semantic index map (id -> idx) and its inverse (idx -> id)
	const t_mapSemanticIdIdx* m_mapSemanticIdx{nullptr};
	t_mapSemanticIdxId m_mapSemanticIdx_inv{};

	// semantic rules
	const t_semanticrules *m_semantics{nullptr};

	// end token id
	t_symbol_id m_end{END_IDENT};

	// state index to begin parsing with
	t_index m_starting_state{0};

	// debug output
	bool m_debug{false};
};


#endif
