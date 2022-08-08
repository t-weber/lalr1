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
#include <vector>
#include <list>
#include <variant>
#include <memory>
#include <functional>
#include <iostream>



/**
 * LALR(1) collection of closures
 */
class Collection : public std::enable_shared_from_this<Collection>
{
public:
	// transition [ from closure, to closure, symbol ]
	using t_transition = std::tuple<ClosurePtr, ClosurePtr, SymbolPtr>;

	// hash transitions
	struct HashTransition
	{
		std::size_t operator()(const t_transition& tr) const;
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

	using t_transitions = std::unordered_set<t_transition, HashTransition, CompareTransitionsEqual>;
	using t_closurecache = std::shared_ptr<std::unordered_map<std::size_t, ClosurePtr>>;
	using t_seen_closures = std::shared_ptr<std::unordered_set<std::size_t>>;


public:
	Collection(const ClosurePtr& closure);

	void DoTransitions();

	std::vector<TerminalPtr> GetLookbackTerminals(const ClosurePtr& closure) const;

	bool SaveParseTables(const std::string& file, bool stopOnConflicts = true) const;
	bool SaveParser(const std::string& file) const;
	bool SaveGraph(const std::string& file, bool write_full_coll=1) const;

	void SetProgressObserver(std::function<void(const std::string&, bool)> func);
	void ReportProgress(const std::string& msg, bool finished = false);


protected:
	Collection();

	std::vector<TerminalPtr> _GetLookbackTerminals(const ClosurePtr& closure) const;

	void DoTransitions(const ClosurePtr& closure);
	void Simplify();

	static std::size_t hash_transition(const t_transition& trans);


private:
	std::list<ClosurePtr> m_collection{};      // collection of LR(1) closures
	t_transitions m_transitions{};             // transitions between collection, [from, to, transition symbol]
	t_closurecache m_closure_cache{};          // seen closures
	mutable t_seen_closures m_seen_closures{}; // set of seen closures

	std::function<void(const std::string& msg, bool finished)> m_progress_observer{};

	friend std::ostream& operator<<(std::ostream& ostr, const Collection& colls);
};


#endif
