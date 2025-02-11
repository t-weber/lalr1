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
#include "closure.h"
#include "options.h"

#include <sstream>
#include <algorithm>

#include <boost/functional/hash.hpp>


#define __CHECK_INVALID_LOOKAHEAD_DEPENDENCIES  0


namespace lalr1 {


Element::Element(const NonTerminalPtr& lhs, t_index rhsidx, t_index cursor,
	const Terminal::t_terminalset& la)
	: Element{lhs, rhsidx, cursor}
{
	// directly set lookaheads (for example for start symbol) -> always valid
	m_lookaheads = la;
	m_lookaheads_valid = LookaheadValidity::ALWAYS_VALID;
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

	this->m_parent = elem.m_parent;
	this->m_isreferenced = elem.m_isreferenced;

	this->m_hash = elem.m_hash;
	this->m_hash_core = elem.m_hash_core;
	this->m_cached_transition_symbol = elem.m_cached_transition_symbol;

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


void Element::SetParentClosure(const ClosurePtr& closure)
{
	m_parent = closure;
}


const ClosurePtr& Element::GetParentClosure() const
{
	return m_parent;
}


void Element::SetReferenced(bool ref)
{
	m_isreferenced = ref;
}


bool Element::IsReferenced() const
{
	return m_isreferenced;
}


/**
 * adds a lookahead terminal
 */
bool Element::AddLookahead(const TerminalPtr& la)
{
	if(!m_lookaheads)
		m_lookaheads = Terminal::t_terminalset{};

	auto [iter, inserted] = m_lookaheads->insert(la);
	if(inserted)
		m_hash = std::nullopt;
	return inserted;
}


const Element::t_elements& Element::GetForwardDependencies() const
{
	return m_forward_dependencies;
}


/**
 * invalidates the lookaheads of the following closures that depend on this one
 */
void Element::InvalidateForwardLookaheads()
{
	for(const ElementPtr& elem : GetForwardDependencies())
	{
		// are lookaheads already invalid?
		if(!elem->AreLookaheadsValid())
			continue;
		if(elem == shared_from_this())
			continue;

		// recursively invalidate
		elem->SetLookaheadsValid(false);
		elem->InvalidateForwardLookaheads();
	}
}


/**
 * are the lookaheads of the current element valid
 */
bool Element::AreLookaheadsValid() const
{
	return m_lookaheads_valid != LookaheadValidity::INVALID && HasLookaheads();
}


/**
 * validates the lookaheads of the current element
 */
void Element::SetLookaheadsValid(bool valid)
{
	if(m_lookaheads_valid == LookaheadValidity::ALWAYS_VALID)
		return;

	m_lookaheads_valid = valid ? LookaheadValidity::VALID : LookaheadValidity::INVALID;
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
		ostr << "Lookaheads have not been resolved for element " << *this;
		if(m_parent)
			ostr << ", parent closure " << m_parent->GetId();
		ostr << ".";
		throw std::runtime_error(ostr.str());
	}

	return *m_lookaheads;
}


bool Element::IsEqual(const ElementPtr& elem, bool only_core) const
{
	CompareElementsEqual cmp;
	cmp.only_core = only_core;
	return cmp(std::const_pointer_cast<Element>(shared_from_this()), elem);
}


t_hash Element::HashElement::operator()(const ElementPtr& elem) const
{
	return elem->hash(only_core);
}


bool Element::CompareElementsEqual::operator()(
	const ElementPtr& elem1, const ElementPtr& elem2) const
{
        return elem1->hash(only_core) == elem2->hash(only_core);
}


t_hash Element::HashLookaheadDependency::operator()(const t_dependency& dep) const
{
	const ElementPtr& elem = dep.first;
	const ClosurePtr& parent = elem->GetParentClosure();

	if(!parent)
	{
		std::ostringstream ostrerr;
		ostrerr << "Cannot hash since element " << *elem << " has no parent closure.";
		throw std::runtime_error(ostrerr.str());
	}

	t_hash hashval = 0;
	boost::hash_combine(hashval, elem->hash(only_core));
	if(parent)
		boost::hash_combine(hashval, std::hash<t_state_id>{}(parent->GetId()));
	boost::hash_combine(hashval, std::hash<bool>{}(dep.second));
	return hashval;
}


bool Element::CompareLookaheadDependenciesEqual::operator()(
	const t_dependency& dep1, const t_dependency& dep2) const
{
        Element::HashLookaheadDependency hasher;
	hasher.only_core = only_core;
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


/**
 * removes dependencies to invalid or dangling elements
 */
void Element::SimplifyLookaheadDependencies(bool only_referenced_elems)
{
	// simplify backward dependencies
	std::unordered_set<ElementPtr> already_seen_cpy, already_seen_first;

	for(auto iter = m_lookahead_dependencies.begin(); iter != m_lookahead_dependencies.end(); )
	{
		const auto& [elem, calc_first] = *iter;

		bool remove = !elem || !elem->GetParentClosure()
			// points to the same element
			|| (elem->GetParentClosure() == GetParentClosure() && elem->hash(true) == hash(true));
		if(only_referenced_elems)
			remove = remove || !elem->IsReferenced();  // element not part of the final collection

		if(!remove)
		{
			// check if the same pointer has already been inserted
			std::unordered_set<ElementPtr>* already_seen =
				calc_first ? &already_seen_first : &already_seen_cpy;
			if(already_seen->contains(elem))
				remove = true;
			else
				already_seen->insert(elem);
		}

		if(remove)
			iter = m_lookahead_dependencies.erase(iter);
		else
			std::advance(iter, 1);
	}


	// simplify forward dependencies
	std::unordered_set<ElementPtr> already_seen;

	for(auto iter = m_forward_dependencies.begin(); iter != m_forward_dependencies.end(); )
	{
		const ElementPtr& elem = *iter;

		bool remove = !elem || !elem->GetParentClosure()
			// points to the same element
			|| (elem->GetParentClosure() == GetParentClosure() && elem->hash(true) == hash(true));
		if(only_referenced_elems)
			remove = remove || !elem->IsReferenced();  // element not part of the final collection

		if(!remove)
		{
			// check if the same pointer has already been inserted
			if(already_seen.contains(elem))
				remove = true;
			else
				already_seen.insert(elem);
		}

		if(remove)
			iter = m_forward_dependencies.erase(iter);
		else
			std::advance(iter, 1);
	}
}


void Element::AddLookaheadDependencies(const Element::t_dependencies& deps)
{
	for(const t_dependency& dep : deps)
		AddLookaheadDependency(dep);
}


void Element::AddLookaheadDependency(const ElementPtr& elem, bool calc_first)
{
	AddLookaheadDependency(std::make_pair(elem, calc_first));
}


void Element::AddLookaheadDependency(const Element::t_dependency& dep)
{
	// ignore invalid elements
	if(!dep.first || !dep.first->GetParentClosure())
		return;

#if __CHECK_INVALID_LOOKAHEAD_DEPENDENCIES != 0
	// check if we already have this dependency
	Element::HashLookaheadDependency hasher;
	t_hash dep_hash = hasher(dep);
	for(const auto& olddep : GetLookaheadDependencies())
	{
		if(!olddep.first || !olddep.first->GetParentClosure())
			continue;
		if(hasher(olddep) == dep_hash)
			return;
	}
#endif

	m_lookahead_dependencies.push_back(dep);
	m_hash = std::nullopt;
}


/**
 * moves along the lookahead dependency tree and calculates all lookaheads in the graph
 */
void Element::ResolveLookaheads(
	std::unordered_map<t_hash, Terminal::t_terminalset>* cached_first_sets,
	std::size_t recurse_depth)
{
	if(AreLookaheadsValid())
		return;

	// lookaheads already valid since there are no dependencies
	if(GetLookaheadDependencies().size() == 0)
		return;

	// - don't recurse further if we're already in a recursion,
	//   non-resolved dependencies will be resolved later via multiple passes
	// - always recalculate if recursive depth is zero, because the FIRST
	//   set might be incomplete in case of loops in the production rules
	if(recurse_depth && HasLookaheads())
		return;

	SetLookaheadsValid(true);

	// --------------------------------------------------------------------------------
	// copy lookaheads from previous closure elements
	for(auto& [elem, calc_first] : GetLookaheadDependencies())
	{
		// ignore invalid elements having no parent closure
		if(!elem || !elem->GetParentClosure())
			continue;
		if(calc_first || elem.get() == this)
			continue;

		if(!elem->AreLookaheadsValid())
		{
			elem->ResolveLookaheads(cached_first_sets, recurse_depth + 1);
			elem->SetLookaheadsValid(true);
		}
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
	}  // lookahead dependencies
	// --------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------
	// calculate first sets from previous closure elements
	for(auto& [elem, calc_first] : GetLookaheadDependencies())
	{
		// ignore invalid elements having no parent closure
		if(!elem || !elem->GetParentClosure())
			continue;
		if(!calc_first || elem.get() == this)
			continue;

		if(!elem->AreLookaheadsValid())
		{
			elem->ResolveLookaheads(cached_first_sets, recurse_depth + 1);
			elem->SetLookaheadsValid(true);
		}
		if(!HasLookaheads())
			m_lookaheads = Terminal::t_terminalset{};

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

				// this elemnt's lookaheads have changed, invalidate the dependent ones
				if(AddLookahead(first_elem))
					invalidate_forwards = true;
			}  // first set
		}  // lookaheads

		if(invalidate_forwards)
			InvalidateForwardLookaheads();
		SetLookaheadsValid(true);
	}  // lookahead dependencies
	// --------------------------------------------------------------------------------
}


/**
 * get possible transition symbol
 */
const SymbolPtr& Element::GetPossibleTransitionSymbol() const
{
	// transition symbol already cached?
	t_hash hashval = hash(true);
	auto iter = m_cached_transition_symbol.find(hashval);
	if(iter != m_cached_transition_symbol.end())
		return iter->second;

	t_index skip_eps = 0;
	while(true)
	{
		// at the end of the rule?
		if(m_cursor + skip_eps >= m_rhs->size())
		{
			std::tie(iter, std::ignore) =
				m_cached_transition_symbol.emplace(
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
			m_cached_transition_symbol.emplace(
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


/**
 * clears the caches that were used during the calculation of transitions
 */
void Element::ClearTransitionCaches()
{
	m_cached_transition_symbol.clear();
}


void Element::ClearDependencies()
{
	m_lookahead_dependencies.clear();
	m_forward_dependencies.clear();
}


std::ostream& operator<<(std::ostream& ostr, const Element& elem)
{
	const std::string& shift_col = g_options.GetTermShiftColour();
	const std::string& reduce_col = g_options.GetTermReduceColour();
	const std::string& jump_col = g_options.GetTermJumpColour();
	const std::string& no_col = g_options.GetTermNoColour();
	const bool use_colour = g_options.GetUseColour();
	const bool print_la_deps = g_options.GetPrintLookaheadDepenencies();

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
	for(t_index rhs_idx = 0; rhs_idx < rhs->size(); ++rhs_idx)
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

		if(!elem.AreLookaheadsValid())
			ostr << "<possibly incomplete> ";

		// semantic rule
		if(std::optional<t_semantic_id> rule = elem.GetSemanticRule(); rule)
			ostr << g_options.GetSeparatorChar() << " rule " << *rule << " ";
	}
	else
	{
		ostr << " ";
	}

	ostr << "]";
	if(use_colour)
		ostr << no_col;

	// print backward and forward lookahead dependencies
	if(print_la_deps)
	{
		if(elem.GetLookaheadDependencies().size() || elem.GetForwardDependencies().size())
			ostr << "\n";

		// print backward lookahead dependencies
		if(const auto& deps = elem.GetLookaheadDependencies(); deps.size())
		{
			ostr << "\tlookahead backward dependencies:\n";
			for(const Element::t_dependency& dep : deps)
			{
				Element elem_cpy = *dep.first;
				elem_cpy.ClearDependencies();  // to prevent recursive printing of dependencies
				ostr << "\t\telement: " << elem_cpy;
				if(elem_cpy.GetParentClosure())
					ostr << ", closure " << elem_cpy.GetParentClosure()->GetId();
				ostr << ", calc_first: " << std::boolalpha << dep.second << "\n";
			}
		}

		// print forward lookahead dependencies
		if(const auto& deps = elem.GetForwardDependencies(); deps.size())
		{
			ostr << "\tlookahead forward dependencies:\n";
			for(const ElementPtr& dep : deps)
			{
				Element elem_cpy = *dep;
				elem_cpy.ClearDependencies();  // to prevent recursive printing of dependencies
				ostr << "\t\telement: " << elem_cpy;
				if(elem_cpy.GetParentClosure())
					ostr << ", closure " << elem_cpy.GetParentClosure()->GetId();
				ostr << "\n";
			}
		}
	}

	return ostr;
}

} // namespace lalr1
