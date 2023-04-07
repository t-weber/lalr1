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
#include "options.h"

#include <sstream>
#include <algorithm>

#include <boost/functional/hash.hpp>


namespace lalr1 {


Element::Element(const NonTerminalPtr& lhs, t_index rhsidx, t_index cursor,
	const Terminal::t_terminalset& la)
	: Element{lhs, rhsidx, cursor}
{
	m_lookaheads = la;
}


Element::Element(const NonTerminalPtr& lhs, t_index rhsidx, t_index cursor)
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

	this->m_forward_dependencies = elem.m_forward_dependencies;
	this->m_lookahead_dependencies = elem.m_lookahead_dependencies;

	this->m_lookaheads = elem.m_lookaheads;
	this->m_lookaheads_valid = elem.m_lookaheads_valid;

	this->m_hash = elem.m_hash;
	this->m_hash_core = elem.m_hash_core;
	this->m_transition_symbol = elem.m_transition_symbol;

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


std::optional<t_semantic_id> Element::GetSemanticRule() const
{
	return m_semanticrule;
}


t_index Element::GetCursor() const
{
	return m_cursor;
}


/**
 * adds a lookahead terminal
 */
bool Element::AddLookahead(const TerminalPtr& la)
{
	auto [iter, inserted] = m_lookaheads->insert(la);
	if(inserted)
		m_hash = std::nullopt;
	return inserted;
}


/**
 * invalidates the lookaheads of the following closures that depend on this one
 */
void Element::InvalidateForwardLookaheads()
{
	for(const ElementPtr& elem : m_forward_dependencies)
	{
		if(elem->AreLookaheadsValid())
		{
			elem->SetLookaheadsValid(false);
			elem->InvalidateForwardLookaheads();
		}
	}
}


/**
 * are the lookaheads of the current element valid
 */
bool Element::AreLookaheadsValid() const
{
	return m_lookaheads_valid && HasLookaheads();
}


/**
 * validates the lookaheads of the current element
 */
void Element::SetLookaheadsValid(bool valid)
{
	m_lookaheads_valid = valid;
}


bool Element::HasLookaheads() const
{
	return m_lookaheads.operator bool();
}


const Terminal::t_terminalset& Element::GetLookaheads() const
{
	if(!HasLookaheads())
	{
		std::ostringstream ostr;
		ostr << "Lookaheads have not been resolved for element " << *this << ".";
		throw std::runtime_error(ostr.str());
	}
	return *m_lookaheads;
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


t_hash Element::HashElement::operator()(const ElementPtr& elem) const
{
	return elem->hash(false);
}


bool Element::CompareElementsEqual::operator()(
	const ElementPtr& elem1, const ElementPtr& elem2) const
{
        return elem1->hash() == elem2->hash();
}


t_hash Element::HashLookaheadDependency::operator()(const t_dependency& dep) const
{
	t_hash hashval = 0;
	boost::hash_combine(hashval, dep.first->hash(true));
	// TODO: also include element's parent closure in hash
	boost::hash_combine(hashval, std::hash<bool>{}(dep.second));
	return hashval;
}


bool Element::CompareLookaheadDependenciesEqual::operator()(
	const t_dependency& dep1, const t_dependency& dep2) const
{
        Element::HashLookaheadDependency hasher;
		return hasher(dep1) == hasher(dep2);
}


/**
 * calculates a unique hash for the closure element (with or without lookaheads)
 */
t_hash Element::hash(bool only_core) const
{
	if(m_hash && !only_core)
		return *m_hash;
	if(m_hash_core && only_core)
		return *m_hash_core;

	t_hash hashLhs = this->GetLhs()->hash();
	t_hash hashRhs = this->GetRhs()->hash();
	t_hash hashCursor = std::hash<t_index>{}(this->GetCursor());

	t_hash hash = 0;
	boost::hash_combine(hash, hashLhs);
	boost::hash_combine(hash, hashRhs);
	boost::hash_combine(hash, hashCursor);

	if(!only_core /*&& HasLookaheads()*/)
	{
		// also include lookaheads in hash
		for(const TerminalPtr& la : GetLookaheads())
		{
			t_hash hashLA = la->hash();
			boost::hash_combine(hash, hashLA);
		}

		m_hash = hash;
	}
	else
	{
		m_hash_core = hash;
	}

	return hash;
}


SymbolPtr Element::GetSymbolAtCursor() const
{
	const WordPtr& rhs = GetRhs();
	if(!rhs)
		return nullptr;

	t_index cursor = GetCursor();
	if(cursor >= rhs->NumSymbols())
		return nullptr;

	return rhs->GetSymbol(cursor);
}


void Element::AddForwardDependency(const ElementPtr& elem)
{
	m_forward_dependencies.push_back(elem);
}


const Element::t_dependencies& Element::GetLookaheadDependencies() const
{
	return m_lookahead_dependencies;
}


void Element::AddLookaheadDependencies(const Element::t_dependencies& deps)
{
	for(const t_dependency& dep : deps)
		AddLookaheadDependency(dep);
}


void Element::AddLookaheadDependency(const Element::t_dependency& dep)
{
	//m_lookahead_dependencies.insert(dep);
	m_lookahead_dependencies.push_back(dep);
}


void Element::AddLookaheadDependency(const ElementPtr& elem, bool calc_first)
{
	//m_lookahead_dependencies.emplace(std::make_pair(elem, calc_first));
	m_lookahead_dependencies.emplace_back(std::make_pair(elem, calc_first));

	this->m_lookaheads = std::nullopt;
	this->m_hash = std::nullopt;
}


/**
 * moves along the lookahead dependency tree and calculates all lookaheads in the graph
 */
void Element::ResolveLookaheads(
	std::unordered_map<t_hash, Terminal::t_terminalset>* cached_first_sets,
	std::size_t recurse_depth)
{
	if(m_lookahead_dependencies.size() == 0)
	{
		SetLookaheadsValid(true);
		return;
	}

	// already resolved?
	// always recalculate if recursive depth is zero, because the FIRST
	// set might be incomplete in case of loops in the production rules
	if(recurse_depth && HasLookaheads())
		return;

	// copy lookaheads from previous closure elements
	std::unordered_set<ElementPtr> already_seen;

	for(auto& [elem, calc_first] : m_lookahead_dependencies)
	{
		if(calc_first || already_seen.contains(elem))
			continue;
		already_seen.insert(elem);

		if(!elem->AreLookaheadsValid() && elem.get() != this)
			elem->ResolveLookaheads(cached_first_sets, recurse_depth + 1);

		if(!HasLookaheads())
			m_lookaheads = Terminal::t_terminalset{};

		bool invalidate_forwards = false;

		for(const TerminalPtr& la : elem->GetLookaheads())
		{
			// this elemnt's lookaheads have changed, invalidate the dependent ones
			if(AddLookahead(la))
				invalidate_forwards = true;
		}

		if(invalidate_forwards)
			InvalidateForwardLookaheads();
		SetLookaheadsValid(true);
	}  // m_lookahead_dependencies


	// calculate first sets from previous closure elements
	already_seen.clear();

	for(auto& [elem, calc_first] : m_lookahead_dependencies)
	{
		if(!calc_first || already_seen.contains(elem))
			continue;
		already_seen.insert(elem);

		if(!elem->AreLookaheadsValid() && elem.get() != this)
			elem->ResolveLookaheads(cached_first_sets, recurse_depth + 1);

		const Terminal::t_terminalset& nonterm_la = elem->GetLookaheads();
		const WordPtr& rhs = elem->GetRhs();
		const t_index cursor = elem->GetCursor();

		bool invalidate_forwards = false;

		// iterate lookaheads
		for(const TerminalPtr& la : nonterm_la)
		{
			// see if the FIRST set of the rhs has already been cached
			const Terminal::t_terminalset* first = nullptr;
			t_hash hashrhs = 0;
			if(cached_first_sets)
			{
				hashrhs = rhs->hash(cursor+1, la);
				if(auto iter = cached_first_sets->find(hashrhs); iter != cached_first_sets->end())
					first = &iter->second;
			}

			// calculate FIRST set of the rhs
			if(!first)
			{
				first = &rhs->CalcFirst(la, cursor+1);

				// cache the FIRST set
				if(cached_first_sets)
					cached_first_sets->emplace(std::make_pair(hashrhs, *first));
			}

			// iterate all terminals in first set
			for(const TerminalPtr& first_elem : *first)
			{
				if(first_elem->IsEps())
					continue;

				if(!HasLookaheads())
					m_lookaheads = Terminal::t_terminalset{};

				// this elemnt's lookaheads have changed, invalidate the dependent ones
				if(AddLookahead(first_elem))
					invalidate_forwards = true;
			}  // first set
		}  // lookaheads

		if(invalidate_forwards)
			InvalidateForwardLookaheads();
		SetLookaheadsValid(true);
	}  // m_lookahead_dependencies
}


/**
 * get possible transition symbol
 */
const SymbolPtr& Element::GetPossibleTransitionSymbol() const
{
	// transition symbol already cached?
	t_hash hashval = hash(true);
	auto iter = m_transition_symbol.find(hashval);
	if(iter != m_transition_symbol.end())
		return iter->second;

	t_index skip_eps = 0;
	while(true)
	{
		// at the end of the rule?
		if(m_cursor + skip_eps >= m_rhs->size())
		{
			std::tie(iter, std::ignore) =
				m_transition_symbol.emplace(
					std::make_pair(hashval, nullptr));
			break;
		}

		const SymbolPtr& sym = (*m_rhs)[m_cursor + skip_eps];
		if(sym->IsEps())
		{
			++skip_eps;
			continue;
		}

		// cache and return found symbol
		std::tie(iter, std::ignore) =
			m_transition_symbol.emplace(
				std::make_pair(hashval, sym));
		break;
	}

	return iter->second;
}


void Element::AdvanceCursor()
{
	if(m_cursor < m_rhs->size())
		++m_cursor;

	m_hash = m_hash_core = std::nullopt;
}


/**
 * is the cursor at the end, i.e. the full handle has been read and can be reduced?
 */
bool Element::IsCursorAtEnd() const
{
	t_index skip_eps = 0;
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
	const std::string& shift_col = g_options.GetTermShiftColour();
	const std::string& reduce_col = g_options.GetTermReduceColour();
	const std::string& jump_col = g_options.GetTermJumpColour();
	const std::string& no_col = g_options.GetTermNoColour();
	const bool use_colour = g_options.GetUseColour();

	const NonTerminalPtr& lhs = elem.GetLhs();
	const WordPtr& rhs = elem.GetRhs();
	bool at_end = elem.IsCursorAtEnd();

	if(use_colour)
	{
		if(at_end)
		{
			ostr << reduce_col;
		}
		else
		{
			if(elem.GetSymbolAtCursor()->IsTerminal())
				ostr << shift_col;
			else
				ostr << jump_col;
		}
	}

	// core element
	ostr << lhs->GetStrId() << " " << g_options.GetArrowChar() << " [ ";
	for(t_index rhs_idx=0; rhs_idx<rhs->size(); ++rhs_idx)
	{
		if(elem.GetCursor() == rhs_idx)
			ostr << g_options.GetCursorChar();

		const SymbolPtr& sym = (*rhs)[rhs_idx];

		ostr << sym->GetStrId();
		if(rhs_idx < rhs->size() - 1)
			ostr << " ";
	}

	// end cursor
	if(at_end)
		ostr << g_options.GetCursorChar();

	// lookaheads
	if(elem.HasLookaheads())
	{
		ostr << " " << g_options.GetSeparatorChar() << " ";

		for(const TerminalPtr& la : elem.GetLookaheads())
			ostr << la->GetStrId() << " ";

		// semantic rule
		if(std::optional<t_semantic_id> rule = elem.GetSemanticRule(); rule)
			ostr << " " << g_options.GetSeparatorChar() << " rule " << *rule << " ";
	}
	else
	{
		ostr << " ";
	}

	ostr << "]";
	if(use_colour)
		ostr << no_col;

	// print lookahead dependencies
	/*if(const auto& deps = elem.GetLookaheadDependencies(); deps.size())
	{
		ostr << "\n\tlookahead dependencies:\n";
		for(const Element::t_dependency& dep : deps)
		{
			Element elem_cpy = *dep.first;
			elem_cpy.m_lookahead_dependencies.clear();  // to prevent recursive printing of dependencies
			ostr << "\t\t" << elem_cpy << ", calc_first: " << std::boolalpha << dep.second << "\n";
		}
	}*/

	return ostr;
}

} // namespace lalr1
