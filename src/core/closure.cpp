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

#include <deque>
#include <sstream>
#include <algorithm>

#include <boost/functional/hash.hpp>


#define __CACHE_FIRST_SETS 0


namespace lalr1 {


// global closure id counter
t_state_id Closure::g_id = 0;


Closure::Closure() : std::enable_shared_from_this<Closure>{}, m_id{g_id++}
{}


Closure::Closure(const Closure& closure)
	: std::enable_shared_from_this<Closure>{}
{
	this->operator=(closure);
}


Closure::~Closure()
{
	//std::cerr << "Destroying closure " << GetId() << "." << std::endl;
	clear();
}


const Closure& Closure::operator=(const Closure& closure)
{
	this->m_id = closure.m_id;

	for(const ElementPtr& elem : closure.GetElements())
	{
		ElementPtr newelem = std::make_shared<Element>(*elem);
		newelem->SetParentClosure(shared_from_this());
		AddElement(newelem);
	}

	this->m_isreferenced = closure.m_isreferenced;

	this->m_hash = closure.m_hash;
	this->m_hash_core = closure.m_hash_core;
	this->m_cached_transition_symbols = closure.m_cached_transition_symbols;
	this->m_cached_transitions = closure.m_cached_transitions;

	return *this;
}


t_state_id Closure::GetId() const
{
	return m_id;
}


void Closure::SetId(t_state_id id)
{
	m_id = id;
	m_hash = m_hash_core = std::nullopt;
}


void Closure::SetReferenced(bool ref)
{
	m_isreferenced = ref;
}


bool Closure::IsReferenced() const
{
	return m_isreferenced;
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

	// insert new element
	elem->SetParentClosure(shared_from_this());
	m_elems.push_back(elem);

	// if the cursor is before a non-terminal, add the rule as element
	const WordPtr& rhs = elem->GetRhs();
	const t_index cursor = elem->GetCursor();
	if(cursor < rhs->size() && !(*rhs)[cursor]->IsTerminal())
	{
		// get non-terminal at cursor
		const NonTerminalPtr& nonterm = std::dynamic_pointer_cast<NonTerminal>((*rhs)[cursor]);

		// iterate all rules of the non-terminal
		for(t_index nonterm_ruleidx=0; nonterm_ruleidx<nonterm->NumRules(); ++nonterm_ruleidx)
		{
			ElementPtr newelem = std::make_shared<Element>(nonterm, nonterm_ruleidx, 0);
			newelem->SetParentClosure(shared_from_this());
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
			return theelem->IsEqual(elem, only_core);
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
	for(const ElementPtr& theelem : GetElements())
	{
		t_index cursor = theelem->GetCursor();
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
	t_hash hashval = hash(true);
	if(auto iter = m_cached_transition_symbols.find(hashval);
		iter != m_cached_transition_symbols.end())
		return iter->second;

	t_symbolset syms;
	for(const ElementPtr& theelem : GetElements())
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
	for(const ElementPtr& elem : GetElements())
	{
		t_hash elem_hash = elem->hash(true);

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


/**
 * tests if all elemets' lookaheads are valid
 */
bool Closure::AreLookaheadsValid() const
{
	for(const ElementPtr& elem : GetElements())
		if(!elem->AreLookaheadsValid())
			return false;
	return true;
}


/**
 * resolves the lookaheads for all elements in the closure
 */
void Closure::ResolveLookaheads()
{
	std::unordered_map<t_hash, Terminal::t_terminalset>* pcached_first_sets = nullptr;

#if __CACHE_FIRST_SETS != 0
	std::unordered_map<t_hash, Terminal::t_terminalset> cached_first_sets;
	pcached_first_sets = &cached_first_sets;
#endif

	for(const ElementPtr& elem : GetElements())
	{
		if(elem->AreLookaheadsValid())
			continue;

		// resolve invalid lookaheads
		elem->ResolveLookaheads(pcached_first_sets);
	}
}


/**
 * perform a transition and get the corresponding lalr(1) closure
 */
std::tuple<ClosurePtr, Closure::t_elements>
Closure::DoTransition(const SymbolPtr& transsym) const
{
	ClosurePtr new_closure = std::make_shared<Closure>();
	t_elements from_elems;

	// look for elements with that transition
	for(const ElementPtr& theelem : GetElements())
	{
		const SymbolPtr& sym = theelem->GetPossibleTransitionSymbol();
		if(!sym || *sym != *transsym)
			continue;

		// save the element from which this transition comes from
		from_elems.push_back(theelem);

		// copy element and perform transition
		ElementPtr newelem = std::make_shared<Element>(*theelem);
		newelem->AdvanceCursor();
		newelem->SetParentClosure(new_closure);
		newelem->AddLookaheadDependency(theelem, false);
		new_closure->AddElement(newelem);
	}

	return std::make_tuple(new_closure, from_elems);
}


/**
 * perform all possible transitions from this closure
 * and get the corresponding lalr(1) collection
 * @return [transition symbol, destination closure]
 */
const Closure::t_transitions& Closure::DoTransitions() const
{
	t_hash hashval = hash(true);
	auto iter = m_cached_transitions.find(hashval);

	// transitions not yet calculated?
	if(iter == m_cached_transitions.end())
	{
		const t_symbolset& possible_transitions = GetPossibleTransitionSymbols();
		t_transitions transitions;

		for(const SymbolPtr& transition : possible_transitions)
		{
			auto [new_closure, from_elems] = DoTransition(transition);
			transitions.emplace_back(std::make_tuple(transition,
				new_closure, from_elems));
		}

		std::tie(iter, std::ignore) = m_cached_transitions.emplace(
			std::make_pair(hashval, transitions));
	}

	return iter->second;
}


/**
 * clears the caches that were used during the calculation of transitions
 */
void Closure::ClearTransitionCaches()
{
	m_cached_transition_symbols.clear();
	m_cached_transitions.clear();

	for(const ElementPtr& elem : GetElements())
		elem->ClearTransitionCaches();
}


/**
 * clear all
 */
void Closure::clear()
{
	ClearTransitionCaches();

	// remove elements' parent closure
	for(const ElementPtr& elem : GetElements())
	{
		elem->SetParentClosure(nullptr);
		elem->ClearDependencies();
	}

	m_elems.clear();
}


/**
 * get elements that produce a reduce/reduce conflict
 */
Closure::t_conflictingelements Closure::GetReduceConflicts() const
{
	Closure::t_conflictingelements seen_lookaheads;

	for(const ElementPtr& elem : GetElements())
	{
		// only consider finished rules that are reduced
		if(!elem->IsCursorAtEnd())
			continue;

		// different finished closure elements cannot share lookaheads
		for(const TerminalPtr& lookahead : elem->GetLookaheads())
		{
			auto iter = seen_lookaheads.find(lookahead);
			if(iter != seen_lookaheads.end())
			{
				iter->second.push_back(elem);
			}
			else
			{
				seen_lookaheads.emplace(std::make_pair(
					lookahead, t_elements{elem}));
			}
		}
	}

	return seen_lookaheads;
}


/**
 * tests if the closure has a reduce/reduce conflict
 */
bool Closure::HasReduceConflict() const
{
	return GetReduceConflicts().size() > 0;
}


/**
 * try to solve possible reduce/reduce conflicts
 */
bool Closure::SolveReduceConflicts()
{
	Closure::t_conflictingelements conflicting_elems = GetReduceConflicts();

	// TODO: keep longest matching element and discard the others

	return true;
}


/**
 * calculates a unique hash for the closure (with or without lookaheads)
 */
t_hash Closure::hash(bool only_core) const
{
	if(m_hash && !only_core)
		return *m_hash;
	if(m_hash_core && only_core)
		return *m_hash_core;

	// sort element hashes before combining them
	std::deque<t_hash> hashes;

	for(const ElementPtr& elem : GetElements())
		hashes.emplace_back(elem->hash(only_core));

	std::sort(hashes.begin(), hashes.end(),
		[](t_hash hash1, t_hash hash2) -> bool
		{
			return hash1 < hash2;
		});

	t_hash fullhash = 0;
	for(t_hash hash : hashes)
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
	ostr << "State " << closure.GetId() << ":\n";

	// write elements of the closure
	for(const ElementPtr& elem : closure.GetElements())
		ostr << "\t" << *elem << "\n";

	return ostr;
}

} // namespace lalr1
