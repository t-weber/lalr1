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

#include <memory>
#include <functional>
#include <iostream>


class Element;
using ElementPtr = std::shared_ptr<Element>;



/**
 * LALR(1) element
 */
class Element : public std::enable_shared_from_this<Element>
{
public:
	Element(const NonTerminalPtr& lhs, std::size_t rhsidx,
		std::size_t cursor, const Terminal::t_terminalset& la);
	Element(const NonTerminalPtr& lhs, std::size_t rhsidx, std::size_t cursor);
	Element(const Element& elem);
	const Element& operator=(const Element& elem);

	const NonTerminalPtr& GetLhs() const;
	const WordPtr& GetRhs() const;
	std::optional<std::size_t> GetSemanticRule() const;

	std::size_t GetCursor() const;
	const Terminal::t_terminalset& GetLookaheads() const;
	WordPtr GetRhsAfterCursor() const;
	SymbolPtr GetSymbolAtCursor() const;

	bool AddLookahead(const TerminalPtr& term);
	bool AddLookaheads(const Terminal::t_terminalset& las);
	void SetLookaheads(const Terminal::t_terminalset& las);

	SymbolPtr GetPossibleTransitionSymbol() const;

	void AdvanceCursor();
	bool IsCursorAtEnd() const;

	bool IsEqual(const Element& elem, bool only_core = false) const;
	bool operator==(const Element& other) const;
	bool operator!=(const Element& other) const
	{ return !operator==(other); }

	std::size_t hash(bool only_core = false) const;

	friend std::ostream& operator<<(std::ostream& ostr, const Element& elem);


private:
	NonTerminalPtr m_lhs{nullptr};          // left-hand side of the production rule
	WordPtr m_rhs{nullptr};                 // right-hand side of the production rule

	// optional semantic rule
	std::optional<std::size_t> m_semanticrule{std::nullopt};

	std::size_t m_rhsidx{0};                // rule index
	std::size_t m_cursor{0};                // pointing before element at this index

	Terminal::t_terminalset m_lookaheads{}; // lookahead symbols

	// cached hash values
	mutable std::optional<std::size_t> m_hash{ std::nullopt };
	mutable std::optional<std::size_t> m_hash_core{ std::nullopt };
};



#endif
