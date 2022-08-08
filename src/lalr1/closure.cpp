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


Closure::Closure() : std::enable_shared_from_this<Closure>{},
	m_elems{}, m_id{g_id++}
{}


Closure::Closure(const Closure& closure)
	: std::enable_shared_from_this<Closure>{}, m_elems{}
{
	this->operator=(closure);
}


const Closure& Closure::operator=(const Closure& closure)
{
	this->m_id = closure.m_id;

	for(const ElementPtr& elem : closure.m_elems)
		this->m_elems.emplace_back(std::make_shared<Element>(*elem));

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
	// full element already in closure?
	if(HasElement(elem, false).first)
		return;

	// core element already in closure?
	if(auto [core_in_closure, core_idx] = HasElement(elem, true);
		core_in_closure)
	{
		// add new lookahead
		m_elems[core_idx]->AddLookaheads(elem->GetLookaheads());
	}

	// new element
	else
	{
		m_elems.push_back(elem);
	}


	// if the cursor is before a non-terminal, add the rule as element
	const WordPtr& rhs = elem->GetRhs();
	std::size_t cursor = elem->GetCursor();
	if(cursor < rhs->size() && !(*rhs)[cursor]->IsTerminal())
	{
		// get rest of the rule after the cursor and lookaheads
		WordPtr ruleaftercursor = elem->GetRhsAfterCursor();
		const Terminal::t_terminalset& nonterm_la = elem->GetLookaheads();

		// get non-terminal at cursor
		const NonTerminalPtr& nonterm =
			std::dynamic_pointer_cast<NonTerminal>((*rhs)[cursor]);

		// temporary symbols
		WordPtr _ruleaftercursor = std::make_shared<Word>(*ruleaftercursor);
		NonTerminalPtr _ruleaftercursorNT = std::make_shared<NonTerminal>(0, "tmp_ruleaftercursor");
		_ruleaftercursorNT->AddRule(_ruleaftercursor);

		// iterate all rules of the non-terminal
		for(std::size_t nonterm_rhsidx=0; nonterm_rhsidx<nonterm->NumRules(); ++nonterm_rhsidx)
		{
			ElementPtr elem = std::make_shared<Element>(nonterm, nonterm_rhsidx, 0);

			// iterate lookaheads
			Terminal::t_terminalset first_la;
			for(const TerminalPtr& la : nonterm_la)
			{
				// copy ruleaftercursor and add lookahead
				std::size_t sym_idx = _ruleaftercursor->AddSymbol(la);
				t_map_first tmp_first = _ruleaftercursorNT->CalcFirst();
				_ruleaftercursor->RemoveSymbol(sym_idx);

				for(const auto& set_first_pair : tmp_first)
				{
					const Terminal::t_terminalset& set_first
						= set_first_pair.second;
					for(const TerminalPtr& la : set_first)
					{
						if(!la->IsEps())
							first_la.insert(la);
					}
				}
			}

			elem->SetLookaheads(first_la);
			AddElement(elem);
		}
	}

	m_hash = m_hash_core = std::nullopt;
}


/**
 * checks if an element is already in the closure and returns its index
 */
std::pair<bool, std::size_t> Closure::HasElement(
	const ElementPtr& elem, bool only_core) const
{
	if(auto iter = std::find_if(m_elems.begin(), m_elems.end(),
		[&elem, only_core](const ElementPtr& theelem) -> bool
		{
			return theelem->IsEqual(*elem, only_core);
		}); iter != m_elems.end())
	{
		return std::make_pair(true, iter - m_elems.begin());
	}

	return std::make_pair(false, 0);
}


std::size_t Closure::NumElements() const
{
	return m_elems.size();
}


const ElementPtr& Closure::GetElement(std::size_t i) const
{
	return m_elems[i];
}


/**
 * get the element of the closure whose cursor points to the given symbol
 */
ElementPtr Closure::GetElementWithCursorAtSymbol(const SymbolPtr& sym) const
{
	for(std::size_t idx=0; idx<m_elems.size(); ++idx)
	{
		const ElementPtr& theelem = m_elems[idx];
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
std::vector<SymbolPtr> Closure::GetPossibleTransitionSymbols() const
{
	std::vector<SymbolPtr> syms;
	syms.reserve(m_elems.size());

	for(const ElementPtr& theelem : m_elems)
	{
		const SymbolPtr& sym = theelem->GetPossibleTransitionSymbol();
		if(!sym)
			continue;

		// do we already have this symbol?
		bool sym_already_seen = std::find_if(syms.begin(), syms.end(),
			[sym](const SymbolPtr& sym2) -> bool
			{
				return *sym == *sym2;
			}) != syms.end();

		if(sym && !sym_already_seen)
			syms.emplace_back(sym);
	}

	return syms;
}


/**
 * add the lookaheads from another closure with the same core
 */
bool Closure::AddLookaheads(const ClosurePtr& closure)
{
	bool lookaheads_added = false;

	for(std::size_t elemidx=0; elemidx<m_elems.size(); ++elemidx)
	{
		ElementPtr& elem = m_elems[elemidx];
		std::size_t elem_hash = elem->hash(true);

		// find the element whose core has the same hash
		if(auto iter = std::find_if(closure->m_elems.begin(), closure->m_elems.end(),
			[elem_hash](const ElementPtr& closure_elem) -> bool
			{
				return closure_elem->hash(true) == elem_hash;
			}); iter != closure->m_elems.end())
		{
			const ElementPtr& closure_elem = *iter;
			if(elem->AddLookaheads(closure_elem->GetLookaheads()))
				lookaheads_added = true;
		}
	}

	m_hash = m_hash_core = std::nullopt;
	return lookaheads_added;
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
		newclosure->AddElement(newelem);
	}

	return newclosure;
}


/**
 * perform all possible transitions from this closure
 * and get the corresponding lalr(1) collection
 * @return [transition symbol, destination closure]
 */
Closure::t_transitions Closure::DoTransitions() const
{
	std::vector<SymbolPtr> possible_transitions = GetPossibleTransitionSymbols();
	t_transitions transitions;
	transitions.reserve(possible_transitions.size());

	for(const SymbolPtr& transition : possible_transitions)
	{
		ClosurePtr closure = DoTransition(transition);
		transitions.emplace_back(std::make_tuple(transition, closure));
	}

	return transitions;
}


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
	for(std::size_t i=0; i<closure.NumElements(); ++i)
		ostr << "\t" << *closure.GetElement(i)<< "\n";

	return ostr;
}

