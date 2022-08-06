/**
 * lalr(1) closure
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

#ifndef __LALR1_CLOSURE_H__
#define __LALR1_CLOSURE_H__

#include "element.h"

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <variant>
#include <memory>
#include <functional>
#include <iostream>


class Closure;
using ClosurePtr = std::shared_ptr<Closure>;


/**
 * closure of LALR(1) element
 */
class Closure : public std::enable_shared_from_this<Closure>
{
public:
	// [ transition terminal, to closure ]
	using t_transition = std::tuple<SymbolPtr, ClosurePtr>;
	using t_transitions = std::vector<t_transition>;

	// backtracing of transitions that lead to this closure
	// [ transition terminal, from closure ]
	using t_comefrom_transition = t_transition;

	// hash comefrom transitions
	struct HashTransition
	{
		std::size_t operator()(const t_transition& tr) const;
	};

	// compare comefrom transitions for equality
	struct CompareTransitionsEqual
	{
		bool operator()(const t_transition& tr1, const t_transition& tr2) const;
	};

	using t_comefrom_transitions = std::unordered_set<t_comefrom_transition,
		HashTransition, CompareTransitionsEqual>;
	using t_seen_closures = std::shared_ptr<std::unordered_set<std::size_t>>;


public:
	Closure();

	Closure(const Closure& coll);
	const Closure& operator=(const Closure& coll);

	std::size_t GetId() const;
	void SetId(std::size_t id);

	void AddElement(const ElementPtr& elem);
	std::pair<bool, std::size_t> HasElement(
		const ElementPtr& elem, bool only_core = false) const;

	std::size_t NumElements() const;
	const ElementPtr& GetElement(std::size_t i) const;
	ElementPtr GetElementWithCursorAtSymbol(const SymbolPtr& sym) const;

	std::vector<SymbolPtr> GetPossibleTransitionSymbols() const;
	ClosurePtr DoTransition(const SymbolPtr&) const;
	t_transitions DoTransitions() const;

	bool AddLookaheads(const ClosurePtr& closure);

	void AddComefrom(t_comefrom_transition&& comefrom);
	void AddComefroms(const t_comefrom_transitions& comefroms);

	const t_comefrom_transitions& GetComefroms() const;
	std::vector<TerminalPtr> GetComefromTerminals() const;

	std::size_t hash(bool only_core = false) const;

	void PrintComefroms(std::ostream& ostr) const;
	friend std::ostream& operator<<(std::ostream& ostr, const Closure& coll);


protected:
	std::vector<TerminalPtr> _GetComefromTerminals() const;


private:
	std::vector<ElementPtr> m_elems{};
	std::size_t m_id{0};      // closure id

	static std::size_t g_id;  // global closure id counter

	// transition that led to this closure
	t_comefrom_transitions m_comefrom_transitions{};

	// cached closures
	mutable t_seen_closures m_seen_closures{};

	// cached hash values
	mutable std::optional<std::size_t> m_hash{ std::nullopt };
	mutable std::optional<std::size_t> m_hash_core{ std::nullopt };
};


#endif
