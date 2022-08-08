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

#include "element.h"

#include <sstream>
#include <algorithm>

#include <boost/functional/hash.hpp>


/*Element::Element(const NonTerminalPtr& lhs, std::size_t rhsidx, std::size_t cursor,
	const Terminal::t_terminalset& la)
	: std::enable_shared_from_this<Element>{},
		m_lhs{lhs}, m_rhs{lhs->GetRule(rhsidx)},
		m_semanticrule{lhs->GetSemanticRule(rhsidx)},
		m_rhsidx{rhsidx}, m_cursor{cursor}, m_lookaheads{la}
{
}*/


Element::Element(const NonTerminalPtr& lhs, std::size_t rhsidx, std::size_t cursor,
	const Terminal::t_terminalset& la)
	: Element{lhs, rhsidx, cursor}
{
	m_lookaheads = la;
}


Element::Element(const NonTerminalPtr& lhs, std::size_t rhsidx, std::size_t cursor)
	: std::enable_shared_from_this<Element>{},
		m_lhs{lhs}, m_rhs{lhs->GetRule(rhsidx)},
		m_semanticrule{lhs->GetSemanticRule(rhsidx)},
		m_rhsidx{rhsidx}, m_cursor{cursor}
{
}


Element::Element(const Element& elem) : std::enable_shared_from_this<Element>{}
{
	this->operator=(elem);
}


const Element& Element::operator=(const Element& elem)
{
	this->m_lhs = elem.m_lhs;
	this->m_rhs = elem.m_rhs;
	this->m_semanticrule = elem.m_semanticrule;
	this->m_rhsidx = elem.m_rhsidx;
	this->m_cursor = elem.m_cursor;
	this->m_lookaheads = elem.m_lookaheads;
	this->m_hash = elem.m_hash;
	this->m_hash_core = elem.m_hash_core;

	return *this;
}


const NonTerminalPtr& Element::GetLhs() const
{
	return m_lhs;
}


const WordPtr& Element::GetRhs() const
{
	return m_rhs;
}


std::optional<std::size_t> Element::GetSemanticRule() const
{
	return m_semanticrule;
}


std::size_t Element::GetCursor() const
{
	return m_cursor;
}


const Terminal::t_terminalset& Element::GetLookaheads() const
{
	return m_lookaheads;
}


bool Element::IsEqual(const Element& elem, bool only_core) const
{
	if(*this->GetLhs() != *elem.GetLhs())
		return false;
	if(*this->GetRhs() != *elem.GetRhs())
		return false;
	if(this->GetCursor() != elem.GetCursor())
		return false;

	// also compare lookaheads
	if(!only_core)
	{
		// see if all lookaheads of elem are already in this lookahead set
		for(const TerminalPtr& la : elem.GetLookaheads())
		{
			if(this->GetLookaheads().find(la) == this->GetLookaheads().end())
				return false;
		}
	}

	return true;
}


bool Element::operator==(const Element& other) const
{
	return IsEqual(other, false);
}


std::size_t Element::hash(bool only_core) const
{
	if(m_hash && !only_core)
		return *m_hash;
	if(m_hash_core && only_core)
		return *m_hash_core;

	std::size_t hashLhs = this->GetLhs()->hash();
	std::size_t hashRhs = this->GetRhs()->hash();
	std::size_t hashCursor = std::hash<std::size_t>{}(this->GetCursor());

	boost::hash_combine(hashLhs, hashRhs);
	boost::hash_combine(hashLhs, hashCursor);

	if(!only_core)
	{
		for(const TerminalPtr& la : GetLookaheads())
		{
			std::size_t hashLA = la->hash();
			boost::hash_combine(hashLhs, hashLA);
		}

		m_hash = hashLhs;
	}
	else
	{
		m_hash_core = hashLhs;
	}

	return hashLhs;
}


WordPtr Element::GetRhsAfterCursor() const
{
	WordPtr rule = std::make_shared<Word>();

	for(std::size_t i=GetCursor()+1; i<GetRhs()->size(); ++i)
		rule->AddSymbol(GetRhs()->GetSymbol(i));

	return rule;
}


SymbolPtr Element::GetSymbolAtCursor() const
{
	const WordPtr& rhs = GetRhs();
	if(!rhs)
		return nullptr;

	std::size_t cursor = GetCursor();
	if(cursor >= rhs->NumSymbols())
		return nullptr;

	return rhs->GetSymbol(cursor);
}


bool Element::AddLookahead(const TerminalPtr& term)
{
	m_hash = m_hash_core = std::nullopt;
	return m_lookaheads.insert(term).second;
}


bool Element::AddLookaheads(const Terminal::t_terminalset& las)
{
	bool lookaheads_added = false;

	for(const TerminalPtr& la : las)
	{
		if(AddLookahead(la))
			lookaheads_added = true;
	}

	m_hash = m_hash_core = std::nullopt;
	return lookaheads_added;
}


void Element::SetLookaheads(const Terminal::t_terminalset& las)
{
	m_hash = m_hash_core = std::nullopt;
	m_lookaheads = las;
}


/**
 * get possible transition symbol
 */
SymbolPtr Element::GetPossibleTransitionSymbol() const
{
	std::size_t skip_eps = 0;

	while(true)
	{
		if(m_cursor + skip_eps >= m_rhs->size())
			return nullptr;

		const SymbolPtr& sym = (*m_rhs)[m_cursor + skip_eps];
		if(sym->IsEps())
		{
			++skip_eps;
			continue;
		}

		return sym;
	}
}


void Element::AdvanceCursor()
{
	if(m_cursor < m_rhs->size())
		++m_cursor;

	m_hash = m_hash_core = std::nullopt;
}


/**
 * cursor is at end, i.e. full handle has been read and can be reduced
 */
bool Element::IsCursorAtEnd() const
{
	std::size_t skip_eps = 0;
	for(skip_eps = 0; skip_eps + m_cursor < m_rhs->size(); ++skip_eps)
	{
		const SymbolPtr& sym = (*m_rhs)[m_cursor + skip_eps];
		if(sym->IsEps())
			continue;
		else
			break;
	}

	return m_cursor + skip_eps >= m_rhs->size();
}


std::ostream& operator<<(std::ostream& ostr, const Element& elem)
{
	const NonTerminalPtr& lhs = elem.GetLhs();
	const WordPtr& rhs = elem.GetRhs();

	ostr << lhs->GetStrId() << " -> [ ";
	for(std::size_t i=0; i<rhs->size(); ++i)
	{
		if(elem.GetCursor() == i)
			ostr << "\xe2\x80\xa2";

		const SymbolPtr& sym = (*rhs)[i];

		ostr << sym->GetStrId();
		ostr << " ";
	}

	if(elem.IsCursorAtEnd())
		ostr << "\xe2\x80\xa2";

	ostr << "\xef\xbd\x9c ";

	for(const auto& la : elem.GetLookaheads())
		ostr << la->GetStrId() << " ";

	ostr << "]";

	return ostr;
}
