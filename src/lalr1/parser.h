/**
 * lalr(1) parser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_PARSER_H__
#define __LALR1_PARSER_H__

#include "ast.h"
#include "common.h"


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

	void SetSemanticRules(const std::vector<t_semanticrule>* rules) { m_semantics = rules; }

	void SetDebug(bool b) { m_debug = b; }

	t_lalrastbaseptr Parse(const std::vector<t_toknode>& input) const;


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

	// semantic rules
	const std::vector<t_semanticrule> *m_semantics{nullptr};

	// debug output
	bool m_debug{false};
};


#endif
