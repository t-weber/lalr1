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
#include <map>
#include <list>
#include <memory>
#include <functional>
#include <optional>
#include <iostream>


namespace lalr1 {

class Collection;
using CollectionPtr = std::shared_ptr<Collection>;


/**
 * LALR(1) collection of closures
 */
class Collection : public std::enable_shared_from_this<Collection>
{
public:
	using t_elements = Closure::t_elements;

	// have to use raw pointer as key because elements with the same hash can be in several different closures
	//using t_element_to_closure = Element::t_elementmap<ClosurePtr>;
	using t_element_to_closure = std::unordered_map<ElementPtr, ClosurePtr>;

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
	Collection();
	Collection(const ClosurePtr& closure);
	Collection(const Collection& coll);
	const Collection& operator=(const Collection& coll);

	void AddClosure(const ClosurePtr& closure);
	void DoTransitions();

	// tests which closures of the collection have reduce/reduce conflicts
	std::map<t_state_id, std::string> HasReduceConflicts() const;

	// tests which closures of the collection have shift/reduce conflicts
	std::map<t_state_id, std::string> HasShiftReduceConflicts() const;

	// get terminals leading to the given closure
	Terminal::t_terminalset GetLookbackTerminals(const ClosurePtr& closure) const;

	// get terminal or non-terminal transitions originating from the given closure
	t_transitions GetTransitions(const ClosurePtr& closure,
		bool term = true, bool only_core_hash = false) const;
	std::optional<t_transition> GetTransition(const ElementPtr& element,
		bool only_core_hash = false) const;

	bool SaveGraph(std::ostream& ostr, bool write_full_coll = true, bool write_elem_wise = false) const;
	bool SaveGraph(const std::string& file, bool write_full_coll = true, bool write_elem_wise = false) const;

	void SetStopOnConflicts(bool b = true);
	void SetSolveReduceConflicts(bool b = true);
	void SetDontGenerateLookbacks(bool b = true);

	bool GetDontGenerateLookbacks() const;

	void SetProgressObserver(std::function<void(const std::string&, bool)> func);
	void ReportProgress(const std::string& msg, bool finished = false);

	bool SolveReduceConflicts();
	bool SolveShiftReduceConflict(
		const SymbolPtr& sym_at_cursor, const Terminal::t_terminalset& lookbacks,
		t_index* shiftEntry, t_index* reduceEntry) const;

	// getters
	const t_closures& GetClosures() const;
	const t_transitions& GetTransitions() const;
	bool GetStopOnConflicts() const;
	bool GetSolveReduceConflicts() const;


public:
	static std::tuple<bool, t_semantic_id, std::size_t, t_symbol_id> GetUniquePartialMatch(
		const t_elements& elemsFrom, bool termTrans);


protected:
	Terminal::t_terminalset _GetLookbackTerminals(const ClosurePtr& closure) const;

	void DoTransitions(const ClosurePtr& closure);
	void Simplify();

	void MapElementsToClosures();
	void MapElementsToFollowingElements();

	static t_hash hash_transition(const t_transition& trans);


private:
	t_closures m_closures{};                    // collection of LR(1) closures
	t_transitions m_transitions{};              // transitions between collection, [from, to, transition symbol]
	t_element_to_closure m_elem_to_closure{};   // maps elements to their parent closures

	t_closurecache m_closure_cache{};           // seen closures
	mutable t_seen_closures m_seen_closures{};  // set of seen closures

	bool m_stopOnConflicts{true};               // stop table/code generation on conflicts
	bool m_trySolveReduceConflicts{false};      // try to solve reduce/reduce conflicts
	bool m_dontGenerateLookbacks{false};        // skip generation of lookback terminals

	std::function<void(const std::string& msg, bool finished)> m_progress_observer{};

	friend std::ostream& operator<<(std::ostream& ostr, const Collection& colls);
};

} // namespace lalr1

#endif
