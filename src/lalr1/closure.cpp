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

#include "closure.h"

#include <sstream>
#include <algorithm>

#include <boost/functional/hash.hpp>


// global closure id counter
std::size_t Closure::g_id = 0;


Closure::Closure() : std::enable_shared_from_this<Closure>{}, m_id{g_id++}
{}


Closure::Closure(const Closure& closure)
	: std::enable_shared_from_this<Closure>{}
{
	this->operator=(closure);
}


const Closure& Closure::operator=(const Closure& closure)
{
	this->m_id = closure.m_id;

	for(const ElementPtr& elem : closure.m_elems)
		this->m_elems.emplace_back(std::make_shared<Element>(*elem));

	this->m_hash = closure.m_hash;
	this->m_hash_core = closure.m_hash_core;
	this->m_cached_transition_symbols = closure.m_cached_transition_symbols;
	this->m_cached_transitions = closure.m_cached_transitions;

	return *this;
}


std::size_t Closure::GetId() const
{
	return m_id;
}


void Closure::SetId(std::size_t id)
{
	m_id = id;
	m_hash = m_hash_core = std::nullopt;
}


/**
 * adds an element and generates the rest of the closure
 */
void Closure::AddElement(const ElementPtr& elem)
{
	// core element already in closure?
	if(auto core_iter = FindElement(elem, true); core_iter != m_elems.end())
	{
		// add new lookahead dependencies
		(*core_iter)->AddLookaheadDependencies(elem->GetLookaheadDependencies());
		return;
	}

	// new element
	else
	{
		m_elems.push_back(elem);
	}


	// if the cursor is before a non-terminal, add the rule as element
	const WordPtr& rhs = elem->GetRhs();
	const std::size_t cursor = elem->GetCursor();
	if(cursor < rhs->size() && !(*rhs)[cursor]->IsTerminal())
	{
		// get non-terminal at cursor
		const NonTerminalPtr& nonterm = std::dynamic_pointer_cast<NonTerminal>((*rhs)[cursor]);

		// iterate all rules of the non-terminal
		for(std::size_t nonterm_ruleidx=0; nonterm_ruleidx<nonterm->NumRules(); ++nonterm_ruleidx)
		{
			ElementPtr newelem = std::make_shared<Element>(nonterm, nonterm_ruleidx, 0);
			newelem->AddLookaheadDependency(elem, true);
			AddElement(newelem);
		}
	}

	// invalidate caches
	m_hash = m_hash_core = std::nullopt;
}


/**
 * checks if an element is already in the closure and returns its iterator
 */
typename Closure::t_elements::const_iterator Closure::FindElement(
	const ElementPtr& elem, bool only_core) const
{
	if(auto iter = std::find_if(m_elems.begin(), m_elems.end(),
		[&elem, only_core](const ElementPtr& theelem) -> bool
		{
			return theelem->IsEqual(*elem, only_core);
		}); iter != m_elems.end())
	{
		return iter;
	}

	// element not found
	return m_elems.end();
}


const Closure::t_elements& Closure::GetElements() const
{
	return m_elems;
}


/**
 * get the element of the closure whose cursor points to the given symbol
 */
ElementPtr Closure::GetElementWithCursorAtSymbol(const SymbolPtr& sym) const
{
	for(const ElementPtr& theelem : m_elems)
	{
		std::size_t cursor = theelem->GetCursor();
		const WordPtr& rhs = theelem->GetRhs();

		if(!rhs || cursor >= rhs->NumSymbols())
			continue;
		if(rhs->GetSymbol(cursor)->GetId() == sym->GetId())
			return theelem;
	}

	return nullptr;
}


/**
 * get possible transition symbols from all elements
 */
const Closure::t_symbolset& Closure::GetPossibleTransitionSymbols() const
{
	// transition symbols of closure core already calculated?
	std::size_t hashval = hash(true);
	if(auto iter = m_cached_transition_symbols.find(hashval);
		iter != m_cached_transition_symbols.end())
		return iter->second;

	t_symbolset syms;
	for(const ElementPtr& theelem : m_elems)
	{
		const SymbolPtr& sym = theelem->GetPossibleTransitionSymbol();
		if(!sym)
			continue;
		syms.emplace(sym);
	}

	auto [iter, inserted] = m_cached_transition_symbols.emplace(
		std::make_pair(hashval, std::move(syms)));
	return iter->second;
}


/**
 * add the lookahead dependencies from another closure with the same core
 */
void Closure::AddLookaheadDependencies(const ClosurePtr& closure)
{
	for(const ElementPtr& elem : m_elems)
	{
		std::size_t elem_hash = elem->hash(true);

		// find the element whose core has the same hash
		if(auto iter = std::find_if(closure->m_elems.begin(), closure->m_elems.end(),
			[elem_hash](const ElementPtr& closure_elem) -> bool
			{
				return closure_elem->hash(true) == elem_hash;
			}); iter != closure->m_elems.end())
		{
			const ElementPtr& closure_elem = *iter;
			elem->AddLookaheadDependencies(closure_elem->GetLookaheadDependencies());
		}
	}

	m_hash = m_hash_core = std::nullopt;
}


void Closure::ResolveLookaheads()
{
	for(ElementPtr& elem : m_elems)
		elem->ResolveLookaheads();
}


/**
 * perform a transition and get the corresponding lalr(1) closure
 */
ClosurePtr Closure::DoTransition(const SymbolPtr& transsym) const
{
	ClosurePtr newclosure = std::make_shared<Closure>();

	// look for elements with that transition
	for(const ElementPtr& theelem : m_elems)
	{
		const SymbolPtr& sym = theelem->GetPossibleTransitionSymbol();
		if(!sym || *sym != *transsym)
			continue;

		// copy element and perform transition
		ElementPtr newelem = std::make_shared<Element>(*theelem);
		newelem->AdvanceCursor();
		newelem->AddLookaheadDependency(theelem, false);
		newclosure->AddElement(newelem);
	}

	return newclosure;
}


/**
 * perform all possible transitions from this closure
 * and get the corresponding lalr(1) collection
 * @return [transition symbol, destination closure]
 */
const Closure::t_transitions& Closure::DoTransitions() const
{
	std::size_t hashval = hash(true);
	auto iter = m_cached_transitions.find(hashval);

	// transitions not yet calculated?
	if(iter == m_cached_transitions.end())
	{
		const t_symbolset& possible_transitions = GetPossibleTransitionSymbols();
		t_transitions transitions;
		transitions.reserve(possible_transitions.size());

		for(const SymbolPtr& transition : possible_transitions)
		{
			ClosurePtr closure = DoTransition(transition);
			transitions.emplace_back(std::make_tuple(transition, closure));
		}

		std::tie(iter, std::ignore) = m_cached_transitions.emplace(
			std::make_pair(hashval, transitions));
	}

	return iter->second;
}


/**
 * tests if the closure has a reduce/reduce conflict
 */
bool Closure::HasReduceConflict() const
{
	Terminal::t_terminalset seen_lookaheads;

	for(const ElementPtr& elem : m_elems)
	{
		// only consider finished rules that are reduced
		if(!elem->IsCursorAtEnd())
			continue;

		// different finished closure elements cannot share lookaheads
		for(const TerminalPtr& lookahead : elem->GetLookaheads())
		{
			if(seen_lookaheads.contains(lookahead))
				return true;
			else
				seen_lookaheads.insert(lookahead);
		}
	}

	return false;
}


/**
 * calculates a unique hash for the closure (with or without lookaheads)
 */
std::size_t Closure::hash(bool only_core) const
{
	if(m_hash && !only_core)
		return *m_hash;
	if(m_hash_core && only_core)
		return *m_hash_core;

	// sort element hashes before combining them
	std::vector<std::size_t> hashes;
	hashes.reserve(m_elems.size());

	for(const ElementPtr& elem : m_elems)
		hashes.emplace_back(elem->hash(only_core));

	std::sort(hashes.begin(), hashes.end(),
		[](std::size_t hash1, std::size_t hash2) -> bool
		{
			return hash1 < hash2;
		});

	std::size_t fullhash = 0;
	for(std::size_t hash : hashes)
		boost::hash_combine(fullhash, hash);

	if(only_core)
		m_hash_core = fullhash;
	else
		m_hash = fullhash;;
	return fullhash;
}


/**
 * prints a closure
 */
std::ostream& operator<<(std::ostream& ostr, const Closure& closure)
{
	ostr << "Closure " << closure.GetId() << ":\n";

	// write elements of the closure
	for(const ElementPtr& elem : closure.GetElements())
		ostr << "\t" << *elem << "\n";

	return ostr;
}

