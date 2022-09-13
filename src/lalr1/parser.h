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

#include <deque>
#include <vector>
#include <stack>
#include <tuple>
#include <optional>



/**
 * stack class for parse states and symbols
 */
template<class t_elem, class t_cont = std::deque<t_elem>>
class ParseStack : public std::stack<t_elem, t_cont>
{
protected:
	// the underlying container
	using std::stack<t_elem, t_cont>::c;


public:
	using value_type = typename std::stack<t_elem, t_cont>::value_type;
	using iterator = typename t_cont::iterator;
	using const_iterator = typename t_cont::const_iterator;
	using reverse_iterator = typename t_cont::reverse_iterator;
	using const_reverse_iterator = typename t_cont::const_reverse_iterator;


public:
	iterator begin() { return c.begin(); }
	iterator end() { return c.end(); }
	const_iterator begin() const { return c.begin(); }
	const_iterator end() const { return c.end(); }

	reverse_iterator rbegin() { return c.rbegin(); }
	reverse_iterator rend() { return c.rend(); }
	const_reverse_iterator rbegin() const { return c.rbegin(); }
	const_reverse_iterator rend() const { return c.rend(); }

	/**
	 * get the top N elements on stack
	 */
	template<template<class...> class t_cont_ret>
	t_cont_ret<t_elem> topN(std::size_t N)
	{
		t_cont_ret<t_elem> cont;

		for(auto iter = c.rbegin(); iter != c.rend(); std::advance(iter, 1))
		{
			cont.push_back(*iter);
			if(cont.size() == N)
				break;
		}

		return cont;
	}
};



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


protected:
	// get the rule number and the matching length of a partial match
	std::tuple<std::optional<std::size_t>, std::optional<std::size_t>>
		GetPartialRules(std::size_t topstate, const t_toknode& curtok,
			const ParseStack<t_lalrastbaseptr>& symbols, bool term) const;

	// get a unique identifier for a partial rule
	std::size_t GetPartialRuleHash(std::size_t rule, std::size_t len,
		const ParseStack<std::size_t>& states, const ParseStack<t_lalrastbaseptr>& symbols) const;


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
