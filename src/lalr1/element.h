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

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <variant>
#include <memory>
#include <functional>
#include <iostream>



/**
 * LALR(1) element
 */
class Element : public std::enable_shared_from_this<Element>
{
public:
	Element(const NonTerminalPtr lhs, std::size_t rhsidx,
		std::size_t cursor, const Terminal::t_terminalset& la);

	Element(const Element& elem);
	const Element& operator=(const Element& elem);

	const NonTerminalPtr GetLhs() const { return m_lhs; }
	const Word* GetRhs() const { return m_rhs; }
	std::optional<std::size_t> GetSemanticRule() const { return m_semanticrule; }

	std::size_t GetCursor() const { return m_cursor; }
	const Terminal::t_terminalset& GetLookaheads() const { return m_lookaheads; }
	WordPtr GetRhsAfterCursor() const;
	const SymbolPtr GetSymbolAtCursor() const;

	bool AddLookahead(TerminalPtr term);
	bool AddLookaheads(const Terminal::t_terminalset& las);
	void SetLookaheads(const Terminal::t_terminalset& las);

	const SymbolPtr GetPossibleTransition() const;

	void AdvanceCursor();
	bool IsCursorAtEnd() const;

	bool IsEqual(const Element& elem, bool only_core=false,
		bool full_equal=true) const;
	bool operator==(const Element& other) const
	{ return IsEqual(other, false); }
	bool operator!=(const Element& other) const
	{ return !operator==(other); }

	std::size_t hash(bool only_core = false) const;

	friend std::ostream& operator<<(std::ostream& ostr, const Element& elem);


private:
	NonTerminalPtr m_lhs{nullptr};
	const Word* m_rhs{nullptr};
	std::optional<std::size_t> m_semanticrule{std::nullopt};

	std::size_t m_rhsidx{0};  // rule index
	std::size_t m_cursor{0};  // pointing before element at this index

	Terminal::t_terminalset m_lookaheads{};
};


using ElementPtr = std::shared_ptr<Element>;


#endif
