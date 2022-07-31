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


// ----------------------------------------------------------------------------
/**
 * hash a transition element
 */
std::size_t Closure::HashComefromTransition::operator()(
	const t_comefrom_transition& trans) const
{
	std::size_t hashSym = std::get<0>(trans)->hash();
	std::size_t hashFrom = std::get<1>(trans)->hash(true);

	boost::hash_combine(hashFrom, hashSym);
	return hashFrom;
}


/**
 * compare two transition elements for equality
 */
bool Closure::CompareComefromTransitionsEqual::operator()(
	const t_comefrom_transition& tr1, const t_comefrom_transition& tr2) const
{
	return HashComefromTransition{}(tr1) == HashComefromTransition{}(tr2);
}
// ----------------------------------------------------------------------------


// global closure id counter
std::size_t Closure::g_id = 0;


Closure::Closure() : std::enable_shared_from_this<Closure>{}, m_elems{}, m_id{g_id++}
{}


Closure::Closure(const Closure& closure)
	: std::enable_shared_from_this<Closure>{}, m_elems{}
{
	this->operator=(closure);
}


const Closure& Closure::operator=(const Closure& closure)
{
	this->m_id = closure.m_id;
	this->m_comefrom_transitions = closure.m_comefrom_transitions;

	for(ElementPtr elem : closure.m_elems)
		this->m_elems.emplace_back(std::make_shared<Element>(*elem));

	return *this;
}


/**
 * adds an element and generates the rest of the closure
 */
void Closure::AddElement(const ElementPtr elem)
{
	//std::cout << "adding " << *elem << std::endl;

	// full element already in closure?
	if(HasElement(elem, false).first)
		return;

	// core element already in closure?
	if(auto [core_in_closure, core_idx] = HasElement(elem, true);
		core_in_closure)
	{
		// add new lookahead
		m_elems[core_idx]->AddLookaheads(elem->GetLookaheads());
		//return;
	}

	// new element
	else
	{
		m_elems.push_back(elem);
	}


	// if the cursor is before a non-terminal, add the rule as element
	const Word* rhs = elem->GetRhs();
	std::size_t cursor = elem->GetCursor();
	if(cursor < rhs->size() && !(*rhs)[cursor]->IsTerminal())
	{
		// get rest of the rule after the cursor and lookaheads
		WordPtr ruleaftercursor = elem->GetRhsAfterCursor();
		const Terminal::t_terminalset& nonterm_la = elem->GetLookaheads();

		// get non-terminal at cursor
		NonTerminalPtr nonterm = std::dynamic_pointer_cast<NonTerminal>((*rhs)[cursor]);

		// iterate all rules of the non-terminal
		for(std::size_t nonterm_rhsidx=0; nonterm_rhsidx<nonterm->NumRules(); ++nonterm_rhsidx)
		{
			// iterate lookaheads
			for(const TerminalPtr& la : nonterm_la)
			{
				// copy ruleaftercursor and add lookahead
				WordPtr _ruleaftercursor = std::make_shared<Word>(*ruleaftercursor);
				_ruleaftercursor->AddSymbol(la);
				//std::cout << nonterm->GetId() << ", " << *_ruleaftercursor << std::endl;

				NonTerminalPtr tmpNT = std::make_shared<NonTerminal>(0, "tmp");
				tmpNT->AddRule(*_ruleaftercursor);

				t_map_first tmp_first;
				calc_first(tmpNT, tmp_first);

				Terminal::t_terminalset first_la;
				for(const auto& set_first_pair : tmp_first)
				{
					const Terminal::t_terminalset& set_first = set_first_pair.second;
					for(const TerminalPtr& la : set_first)
					{
						//std::cout << "lookahead: " << la->GetId() << std::endl;
						if(la->IsEps())
							continue;
						first_la.insert(la);
					}
				}
				/*if(first_la.size() != 1)
				{
					std::cerr << "Info: Multiple look-ahead terminals, epsilon transition?" << std::endl;
				}*/

				AddElement(std::make_shared<Element>(nonterm, nonterm_rhsidx, 0, first_la));
			}
		}
	}
}


/**
 * checks if an element is already in the closure and returns its index
 */
std::pair<bool, std::size_t> Closure::HasElement(
	const ElementPtr elem, bool only_core) const
{
	for(std::size_t idx=0; idx<m_elems.size(); ++idx)
	{
		const ElementPtr theelem = m_elems[idx];

		if(theelem->IsEqual(*elem, only_core, false))
			return std::make_pair(true, idx);
	}

	return std::make_pair(false, 0);
}


/**
 * get the element of the collection whose cursor points to the given symbol
 */
const ElementPtr Closure::GetElementWithCursorAtSymbol(
	const SymbolPtr& sym) const
{
	for(std::size_t idx=0; idx<m_elems.size(); ++idx)
	{
		const ElementPtr theelem = m_elems[idx];

		const Word* rhs = theelem->GetRhs();
		if(!rhs)
			continue;
		std::size_t cursor = theelem->GetCursor();

		if(cursor >= rhs->NumSymbols())
			continue;

		if(rhs->GetSymbol(cursor)->GetId() == sym->GetId())
			return theelem;
	}

	return nullptr;
}


/**
 * get possible transition symbols from all elements
 */
std::vector<SymbolPtr> Closure::GetPossibleTransitions() const
{
	std::vector<SymbolPtr> syms;

	for(const ElementPtr& theelem : m_elems)
	{
		const SymbolPtr sym = theelem->GetPossibleTransition();
		if(!sym)
			continue;

		// do we already have this symbol?
		bool sym_already_seen = std::find_if(syms.begin(), syms.end(),
			[sym](const SymbolPtr sym2) -> bool
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
bool Closure::AddLookaheads(const ClosurePtr closure)
{
	bool lookaheads_added = false;

	for(std::size_t elemidx=0; elemidx<m_elems.size(); ++elemidx)
	{
		ElementPtr elem = m_elems[elemidx];
		std::size_t elem_hash = elem->hash(true);

		//ElementPtr closure_elem = closure->m_elems[elemidx];

		// find the element whose core has the same hash
		if(auto iter = std::find_if(closure->m_elems.begin(), closure->m_elems.end(),
			[elem_hash](const ElementPtr closure_elem) -> bool
			{
				return closure_elem->hash(true) == elem_hash;
			}); iter != closure->m_elems.end())
		{
			ElementPtr closure_elem = *iter;
			if(elem->AddLookaheads(closure_elem->GetLookaheads()))
				lookaheads_added = true;
		}
	}

	return lookaheads_added;
}


/**
 * perform a transition and get the corresponding lr(1) closure
 */
ClosurePtr Closure::DoTransition(const SymbolPtr transsym) const
{
	ClosurePtr newclosure = std::make_shared<Closure>();

	// look for elements with that transition
	for(const ElementPtr& theelem : m_elems)
	{
		const SymbolPtr sym = theelem->GetPossibleTransition();
		if(!sym || *sym != *transsym)
			continue;

		// copy element and perform transition
		ElementPtr newelem = std::make_shared<Element>(*theelem);
		newelem->AdvanceCursor();

		ClosurePtr this_closure = std::const_pointer_cast<Closure>(
			shared_from_this());
		newclosure->AddElement(newelem);
		newclosure->m_comefrom_transitions.emplace(
			std::make_tuple(transsym, this_closure));
	}

	return newclosure;
}


/**
 * perform all possible transitions from this closure
 * and get the corresponding lr(1) collection
 * @return [transition symbol, destination closure]
 */
std::vector<std::tuple<SymbolPtr, ClosurePtr>> Closure::DoTransitions() const
{
	std::vector<std::tuple<SymbolPtr, ClosurePtr>> coll;
	std::vector<SymbolPtr> possible_transitions = GetPossibleTransitions();

	for(const SymbolPtr& transition : possible_transitions)
	{
		ClosurePtr closure = DoTransition(transition);
		coll.emplace_back(std::make_tuple(transition, closure));
	}

	return coll;
}


std::size_t Closure::hash(bool only_core) const
{
	// sort element hashes before combining them
	std::vector<std::size_t> hashes;
	hashes.reserve(m_elems.size());

	for(ElementPtr elem : m_elems)
		hashes.emplace_back(elem->hash(only_core));

	std::sort(hashes.begin(), hashes.end(),
		[](std::size_t hash1, std::size_t hash2) -> bool
		{
			return hash1 < hash2;
		});

	std::size_t fullhash = 0;
	for(std::size_t hash : hashes)
		boost::hash_combine(fullhash, hash);
	return fullhash;
}


/**
 * get all terminal symbols that lead to this closure
 */
std::vector<TerminalPtr> Closure::GetComefromTerminals(
	std::shared_ptr<std::unordered_set<std::size_t>> seen_closures) const
{
	std::vector<TerminalPtr> terms;
	terms.reserve(m_comefrom_transitions.size());

	if(!seen_closures)
		seen_closures = std::make_shared<std::unordered_set<std::size_t>>();

	for(const t_comefrom_transition& comefrom : m_comefrom_transitions)
	{
		SymbolPtr sym = std::get<0>(comefrom);
		const ClosurePtr closure = std::get<1>(comefrom);

		if(sym->IsTerminal())
		{
			TerminalPtr term = std::dynamic_pointer_cast<Terminal>(sym);
			terms.emplace_back(std::move(term));
		}
		else if(closure)
		{
			// closure not yet seen?
			std::size_t hash = closure->hash();
			if(seen_closures->find(hash) == seen_closures->end())
			{
				seen_closures->insert(hash);

				// get terminals from comefrom closure
				std::vector<TerminalPtr> _terms =
					closure->GetComefromTerminals(seen_closures);
				terms.insert(terms.end(), _terms.begin(), _terms.end());
			}
		}
	}

	// remove duplicates
	std::stable_sort(terms.begin(), terms.end(),
		[](const TerminalPtr term1, const TerminalPtr term2) -> bool
		{
			return term1->hash() < term2->hash();
			//return term1->GetId() < term2->GetId();
		});
	auto end = std::unique(terms.begin(), terms.end(),
		[](const TerminalPtr term1, const TerminalPtr term2) -> bool
		{
			return *term1 == *term2;
		});
	if(end != terms.end())
		terms.resize(end - terms.begin());

	return terms;
}


/**
 * write the closures where we can come from
 */
void Closure::PrintComefroms(std::ostream& ostr) const
{
	if(m_comefrom_transitions.size())
	{
		ostr << "Coming from:\n";
		for(const auto& comefrom : m_comefrom_transitions)
		{
			ostr << "\tstate " << std::get<1>(comefrom)->GetId()
				<< " via " << std::get<0>(comefrom)->GetStrId()
				<< ".\n";
		}
	}
}


/**
 * prints a closure
 */
std::ostream& operator<<(std::ostream& ostr, const Closure& closure)
{
	ostr << "Closure/State " << closure.GetId() << ":\n";

	// write elements of the closure
	for(std::size_t i=0; i<closure.NumElements(); ++i)
		ostr << "\t" << *closure.GetElement(i)<< "\n";

	//closure.PrintComefroms(ostr);
	return ostr;
}
