/**
 * lalr(1) collection
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 07-jun-2020
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/lr1.html
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/slr1table.html
 *	- https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 *	- https://en.wikipedia.org/wiki/LR_parser
 */

#ifndef __LALR1_COLLECTION_H__
#define __LALR1_COLLECTION_H__

#include "closure.h"

#include <unordered_set>
#include <unordered_map>
#include <set>
#include <list>
#include <memory>
#include <functional>
#include <iostream>



/**
 * LALR(1) collection of closures
 */
class Collection : public std::enable_shared_from_this<Collection>
{
public:
	using t_elements = Closure::t_elements;

	// transition [ from closure, to closure, symbol, from elements ]
	using t_transition = std::tuple<ClosurePtr, ClosurePtr, SymbolPtr, t_elements>;

	// hash transitions
	struct HashTransition
	{
		t_hash operator()(const t_transition& tr) const;
	};

	// compare transitions for equality
	struct CompareTransitionsEqual
	{
		bool operator()(const t_transition& tr1, const t_transition& tr2) const;
	};

	// compare transitions by order
	struct CompareTransitionsLess
	{
		bool operator()(const t_transition& tr1, const t_transition& tr2) const;
	};

	using t_closures = std::list<ClosurePtr>;
	using t_transitions = std::unordered_set<t_transition, HashTransition, CompareTransitionsEqual>;
	using t_closurecache = std::shared_ptr<std::unordered_map<t_hash, ClosurePtr>>;
	using t_seen_closures = std::shared_ptr<std::unordered_set<t_hash>>;


public:
	Collection(const ClosurePtr& closure);
	Collection(const Collection& coll);
	const Collection& operator=(const Collection& coll);

	void DoTransitions();

	// tests which closures of the collection have reduce/reduce conflicts
	std::set<t_state_id> HasReduceConflicts() const;

	// tests which closures of the collection have shift/reduce conflicts
	std::set<t_state_id> HasShiftReduceConflicts() const;

	// get terminals leading to the given closure
	Terminal::t_terminalset GetLookbackTerminals(const ClosurePtr& closure) const;

	// get terminal or non-terminal transitions originating from the given closure
	t_transitions GetTransitions(const ClosurePtr& closure, bool term = true) const;

	bool CreateParseTables();
	bool SaveParseTablesCXX(const std::string& file) const;
	bool SaveParseTablesJSON(const std::string& file) const;
	bool SaveParser(const std::string& file, const std::string& class_name = "ParserRecAsc") const;

	bool SaveGraph(std::ostream& ostr, bool write_full_coll = true) const;
	bool SaveGraph(const std::string& file, bool write_full_coll = true) const;

	void SetStopOnConflicts(bool b = true);
	void SetUseOpChar(bool b = true);
	void SetGenDebugCode(bool b = true);
	void SetGenErrorCode(bool b = true);
	void SetGenPartialMatches(bool b = true);
	void SetAcceptingRule(t_semantic_id rule_id);

	void SetProgressObserver(std::function<void(const std::string&, bool)> func);
	void ReportProgress(const std::string& msg, bool finished = false);


protected:
	Collection() = delete;

	Terminal::t_terminalset _GetLookbackTerminals(const ClosurePtr& closure) const;

	void DoTransitions(const ClosurePtr& closure);
	void Simplify();

	bool SolveConflict(
		const SymbolPtr& sym_at_cursor, const Terminal::t_terminalset& lookbacks,
		t_index* shiftEntry, t_index* reduceEntry) const;

	void CreateTableIndices();
	t_index GetTableIndex(t_symbol_id id, IndexTableKind table_kind) const;

	static t_hash hash_transition(const t_transition& trans);
	static std::tuple<bool, t_semantic_id, std::size_t> GetUniquePartialMatch(
		const t_elements& elemsFrom);


private:
	t_closures m_collection{};                  // collection of LR(1) closures
	t_transitions m_transitions{};              // transitions between collection, [from, to, transition symbol]

	t_mapIdIdx m_mapTermIdx{};                  // maps the terminal ids to table indices
	t_mapIdIdx m_mapNonTermIdx{};               // maps the non-terminal ids to table indices
	t_mapIdIdx m_mapSemanticIdx{};              // maps the semantic ids to tables indices
	t_closurecache m_closure_cache{};           // seen closures
	mutable t_seen_closures m_seen_closures{};  // set of seen closures

	bool m_stopOnConflicts{true};               // stop table/code generation on conflicts
	bool m_useOpChar{true};                     // use printable character for operators if possible
	bool m_genDebugCode{true};                  // generate debug code in parser output
	bool m_genErrorCode{true};                  // generate error handling code in parser output
	bool m_genPartialMatches{true};             // generates code for handling partial rule matches
	t_semantic_id m_accepting_rule{0};          // rule which leads to accepting the grammar

	t_table m_tabActionShift{};                 // lalr(1) tables
	t_table m_tabActionReduce{};
	t_table m_tabJump{};

	t_table m_tabPartialRuleTerm{};             // partial match tables
	t_table m_tabPartialMatchLenTerm{};
	t_table m_tabPartialRuleNonterm{};
	t_table m_tabPartialMatchLenNonterm{};

	std::vector<std::size_t> m_numRhsSymsPerRule{}; // number of symbols on rhs of a production rule
	std::vector<t_index> m_ruleLhsIdx{};            // nonterminal index of the rule's result type

	std::function<void(const std::string& msg, bool finished)> m_progress_observer{};

	friend std::ostream& operator<<(std::ostream& ostr, const Collection& colls);
};


#endif
