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



/**
 * LALR(1) element
 */
class Element : public std::enable_shared_from_this<Element>
{
public:
	// lookahead dependencies; bool flag: true: calculate first, false: copy lookaheads
	using t_dependency = std::pair<ElementPtr, bool>;
	using t_dependencies = std::list<t_dependency>;


public:
	Element(const NonTerminalPtr& lhs, t_index rhsidx,
		t_index cursor, const Terminal::t_terminalset& la);
	Element(const NonTerminalPtr& lhs, t_index rhsidx, t_index cursor);
	Element(const Element& elem);
	const Element& operator=(const Element& elem);

	const NonTerminalPtr& GetLhs() const;
	const WordPtr& GetRhs() const;
	std::optional<t_semantic_id> GetSemanticRule() const;

	t_index GetCursor() const;
	const Terminal::t_terminalset& GetLookaheads() const;
	SymbolPtr GetSymbolAtCursor() const;

	const t_dependencies& GetLookaheadDependencies() const;
	void AddLookaheadDependencies(const t_dependencies& deps);
	void AddLookaheadDependency(const t_dependency& dep);
	void AddLookaheadDependency(const ElementPtr& elem, bool calc_first);
	void ResolveLookaheads(std::size_t recurse_depth = 0);

	const SymbolPtr& GetPossibleTransitionSymbol() const;

	void AdvanceCursor();
	bool IsCursorAtEnd() const;

	bool IsEqual(const Element& elem, bool only_core = false) const;
	bool operator==(const Element& other) const;
	bool operator!=(const Element& other) const
	{ return !operator==(other); }

	t_hash hash(bool only_core = false) const;

	friend std::ostream& operator<<(std::ostream& ostr, const Element& elem);


private:
	NonTerminalPtr m_lhs{nullptr};          // left-hand side of the production rule
	WordPtr m_rhs{nullptr};                 // right-hand side of the production rule

	// optional semantic rule
	std::optional<t_semantic_id> m_semanticrule{std::nullopt};

	t_index m_rhsidx{0};                    // rule index
	t_index m_cursor{0};                    // pointing before element at this index

	t_dependencies m_lookahead_dependencies{};                     // lookahead dependencies
	mutable std::optional<Terminal::t_terminalset> m_lookaheads{}; // lookahead symbols

	// cached hash values
	mutable std::optional<t_hash> m_hash{ std::nullopt };
	mutable std::optional<t_hash> m_hash_core{ std::nullopt };

	// cached transition symbols
	mutable std::unordered_map<t_hash, SymbolPtr> m_transition_symbol{};
};

} // namespace lalr1

#endif
