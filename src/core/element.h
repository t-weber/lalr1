/**
 * lalr(1) element
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

#ifndef __LALR1_ELEMENT_H__
#define __LALR1_ELEMENT_H__

#include "symbol.h"
#include "common.h"

#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>
#include <iostream>


namespace lalr1 {

class Element;
using ElementPtr = std::shared_ptr<Element>;

class Closure;
using ClosurePtr = std::shared_ptr<Closure>;



enum class LookaheadValidity
{
	INVALID,
	VALID,
	ALWAYS_VALID,
};


/**
 * LALR(1) element
 */
class Element : public std::enable_shared_from_this<Element>
{
public:
	using t_elements = std::list<ElementPtr>;

	// --------------------------------------------------------------------------------
	// lookahead dependencies to previous elements;
	// bool flag: true: calculate first, false: copy lookaheads
	using t_dependency = std::pair<ElementPtr, bool>;

	// hash function for lookahead dependencies
	struct HashLookaheadDependency
	{
		bool only_core = true;
		t_hash operator()(const t_dependency& sym) const;
	};

	//comparator for lookahead dependencies
	struct CompareLookaheadDependenciesEqual
	{
		bool only_core = true;
		bool operator()(const t_dependency& dep1, const t_dependency& dep2) const;
	};

	// can't use a set because during transition calculation the hashes still change
	//using t_dependencies = std::unordered_set<t_dependency,
	//	Element::HashLookaheadDependency, Element::CompareLookaheadDependenciesEqual>;
	using t_dependencies = std::list<t_dependency>;
	// --------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------
	//hash function for elements
	struct HashElement
	{
		bool only_core = false;
		t_hash operator()(const ElementPtr& sym) const;
	};

	// comparator for elements
	struct CompareElementsEqual
	{
		bool only_core = false;
		bool operator()(const ElementPtr& elem1, const ElementPtr& elem2) const;
	};

	// type to map an element to another object
	template<class t_val>
	using t_elementmap = std::unordered_map<ElementPtr, t_val,
		Element::HashElement, Element::CompareElementsEqual>;
	// --------------------------------------------------------------------------------


public:
	Element(const NonTerminalPtr& lhs, t_index rhsidx,
		t_index cursor, const Terminal::t_terminalset& la);
	Element(const NonTerminalPtr& lhs, t_index rhsidx, t_index cursor);
	Element(const Element& elem);
	const Element& operator=(const Element& elem);

	Element() = delete;
	~Element() = default;

	const NonTerminalPtr& GetLhs() const;
	const WordPtr& GetRhs() const;
	std::optional<t_semantic_id> GetSemanticRule() const;

	bool HasLookaheads() const;
	const Terminal::t_terminalset& GetLookaheads() const;

	t_index GetCursor() const;
	SymbolPtr GetSymbolAtCursor() const;

	bool AddLookahead(const TerminalPtr& la);
	void InvalidateForwardLookaheads();
	void SetLookaheadsValid(bool valid = true);
	bool AreLookaheadsValid() const;

	void AddForwardDependency(const ElementPtr& elem);
	const t_elements& GetForwardDependencies() const;

	const t_dependencies& GetLookaheadDependencies() const;
	void AddLookaheadDependencies(const t_dependencies& deps);
	void AddLookaheadDependency(const t_dependency& dep);
	void AddLookaheadDependency(const ElementPtr& elem, bool calc_first);
	void SimplifyLookaheadDependencies(bool only_referenced_elems = true);
	void ResolveLookaheads(
		std::unordered_map<t_hash, Terminal::t_terminalset>* cached_first_sets = nullptr,
		std::size_t recurse_depth = 0);

	const SymbolPtr& GetPossibleTransitionSymbol() const;

	void AdvanceCursor();
	bool IsCursorAtEnd() const;

	void ClearTransitionCaches();
	void ClearDependencies();

	void SetParentClosure(const ClosurePtr& closure);
	const ClosurePtr& GetParentClosure() const;

	void SetReferenced(bool ref = true);
	bool IsReferenced() const;

	bool IsEqual(const ElementPtr& elem, bool only_core = false) const;
	t_hash hash(bool only_core = false) const;

	friend std::ostream& operator<<(std::ostream& ostr, const Element& elem);


private:
	NonTerminalPtr m_lhs{ nullptr };           // left-hand side of the production rule
	WordPtr m_rhs{ nullptr };                  // right-hand side of the production rule

	// optional semantic rule
	std::optional<t_semantic_id> m_semanticrule{std::nullopt};

	t_index m_rhsidx{ 0 };                     // rule index
	t_index m_cursor{ 0 };                     // pointing before element at this index

	// forward dependencies point to the following elements (not in the same closure),
	t_elements m_forward_dependencies{};       // pointers to following closures' elements
	// lookahead dependencies point to the preceding elements (may be in the same closure)
	t_dependencies m_lookahead_dependencies{}; // lookahead dependencies

	// lookahead symbols
	mutable std::optional<Terminal::t_terminalset> m_lookaheads{};
	LookaheadValidity m_lookaheads_valid{ LookaheadValidity::INVALID };

	ClosurePtr m_parent{ nullptr };            // parent closure this element is part of
	bool m_isreferenced{ false };              // is this element in a closure that is still in use?

	// cached hash values
	mutable std::optional<t_hash> m_hash{ std::nullopt };
	mutable std::optional<t_hash> m_hash_core{ std::nullopt };

	// cached transition symbols
	mutable std::unordered_map<t_hash, SymbolPtr> m_cached_transition_symbol{};
};

} // namespace lalr1

#endif
