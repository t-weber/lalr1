/**
 * lalr(1) collection
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
 *	- https://doi.org/10.1016/0020-0190(88)90061-0
 */

#include "collection.h"
#include "conflicts.h"
#include "options.h"

#include <sstream>
#include <fstream>
#include <deque>
#include <algorithm>

#include <boost/functional/hash.hpp>


namespace lalr1 {


// ----------------------------------------------------------------------------
/**
 * hash a transition element
 */
t_hash Collection::HashTransition::operator()(const t_transition& trans) const
{
	t_hash hashFrom = std::get<0>(trans)->hash(true);
	t_hash hashTo = std::get<1>(trans)->hash(true);
	t_hash hashSym = std::get<2>(trans)->hash();

	t_hash hash = 0;
	boost::hash_combine(hash, hashFrom);
	boost::hash_combine(hash, hashTo);
	boost::hash_combine(hash, hashSym);
	return hash;
}


/**
 * compare two transition elements for equality
 */
bool Collection::CompareTransitionsEqual::operator()(
	const t_transition& tr1, const t_transition& tr2) const
{
	return HashTransition{}(tr1) == HashTransition{}(tr2);
}


/**
 * compare two transition elements based on their order
 */
bool Collection::CompareTransitionsLess::operator()(
	const t_transition& tr1, const t_transition& tr2) const
{
	const ClosurePtr& from1 = std::get<0>(tr1);
	const ClosurePtr& from2 = std::get<0>(tr2);
	const ClosurePtr& to1 = std::get<1>(tr1);
	const ClosurePtr& to2 = std::get<1>(tr2);
	const SymbolPtr& sym1 = std::get<2>(tr1);
	const SymbolPtr& sym2 = std::get<2>(tr2);

	if(from1->GetId() < from2->GetId())
		return true;
	if(from1->GetId() == from2->GetId())
		return to1->GetId() < to2->GetId();
	if(from1->GetId() == from2->GetId() && to1->GetId() == to2->GetId())
		return sym1->GetId() < sym2->GetId();
	return false;
}
// ----------------------------------------------------------------------------

Collection::Collection() : std::enable_shared_from_this<Collection>{}
{
}


Collection::Collection(const ClosurePtr& closure)
	: std::enable_shared_from_this<Collection>{}
{
	AddClosure(closure);
}


Collection::Collection(const Collection& coll)
	: std::enable_shared_from_this<Collection>{}
{
	this->operator=(coll);
}


const Collection& Collection::operator=(const Collection& coll)
{
	this->m_closures = coll.m_closures;
	this->m_transitions = coll.m_transitions;

	this->m_closure_cache = coll.m_closure_cache;
	this->m_seen_closures = coll.m_seen_closures;

	this->m_stopOnConflicts = coll.m_stopOnConflicts;
	this->m_trySolveReduceConflicts = coll.m_trySolveReduceConflicts;
	this->m_dontGenerateLookbacks = coll.m_dontGenerateLookbacks;

	this->m_progress_observer = coll.m_progress_observer;

	return *this;
}


void Collection::AddClosure(const ClosurePtr& closure)
{
	m_closures.push_back(closure);
}


void Collection::SetProgressObserver(std::function<void(const std::string&, bool)> func)
{
	m_progress_observer = func;
}


void Collection::ReportProgress(const std::string& msg, bool finished)
{
	if(m_progress_observer)
		m_progress_observer(msg, finished);
}


/**
 * get terminal or non-terminal transitions originating from the given closure
 */
Collection::t_transitions Collection::GetTransitions(
	const ClosurePtr& closure, bool term, bool only_core_hash) const
{
	t_transitions transitions;

	for(const t_transition& transition : GetTransitions())
	{
		const ClosurePtr& closure_from = std::get<0>(transition);
		const SymbolPtr& sym = std::get<2>(transition);

		if(sym->IsEps())
			continue;

		// only consider transitions from the given closure
		if(closure_from->hash(only_core_hash) != closure->hash(only_core_hash))
			continue;

		if(sym->IsTerminal() == term)
			transitions.insert(transition);
	}

	return transitions;
}


/**
 * get transition originating from the given element
 */
std::optional<Collection::t_transition> Collection::GetTransition(
	const ElementPtr& element, bool only_core_hash) const
{
	// get closure containing the element
	const ClosurePtr& closure = element->GetParentClosure();
	t_hash element_hash = element->hash(only_core_hash);

	// get terminal and non-terminal transitions from closure
	t_transitions transitions = GetTransitions(closure, true, only_core_hash);
	t_transitions transitions_nonterm = GetTransitions(closure, false, only_core_hash);
	transitions.merge(transitions_nonterm);

	for(const t_transition& transition : transitions)
	{
		const t_elements& from_elems = std::get<3>(transition);
		for(const ElementPtr& from_elem : from_elems)
		{
			if(from_elem->hash(only_core_hash) == element_hash)
				return transition;
		}
	}

	return std::nullopt;
}


/**
 * get terminals leading to the given closure
 */
Terminal::t_terminalset Collection::GetLookbackTerminals(
	const ClosurePtr& closure) const
{
	if(m_dontGenerateLookbacks)
		return Terminal::t_terminalset{};

	m_seen_closures = std::make_shared<std::unordered_set<t_hash>>();
	return _GetLookbackTerminals(closure);
}


Terminal::t_terminalset Collection::_GetLookbackTerminals(
	const ClosurePtr& closure) const
{
	if(m_dontGenerateLookbacks)
		return Terminal::t_terminalset{};

	Terminal::t_terminalset terms;

	for(const t_transition& transition : GetTransitions())
	{
		const ClosurePtr& closure_from = std::get<0>(transition);
		const ClosurePtr& closure_to = std::get<1>(transition);
		const SymbolPtr& sym = std::get<2>(transition);

		// only consider transitions to the given closure
		if(closure_to->hash() != closure->hash())
			continue;

		if(sym->IsTerminal())
		{
			const TerminalPtr& term =
				std::dynamic_pointer_cast<Terminal>(sym);
			terms.emplace(std::move(term));
		}
		else if(closure_from)
		{
			// closure not yet seen?
			t_hash hash = closure_from->hash();
			if(!m_seen_closures->contains(hash))
			{
				m_seen_closures->insert(hash);

				// get terminals from previous closure
				Terminal::t_terminalset _terms =
					_GetLookbackTerminals(closure_from);
				terms.merge(_terms);
			}
		}
	}

	return terms;
}


/**
 * perform all possible lalr(1) transitions from all closures
 */
void Collection::DoTransitions(const ClosurePtr& closure_from)
{
	if(!m_closure_cache)
	{
		m_closure_cache = std::make_shared<
			std::unordered_map<t_hash, ClosurePtr>>();
		m_closure_cache->emplace(std::make_pair(
			closure_from->hash(true), closure_from));
	}

	const Closure::t_transitions& transitions = closure_from->DoTransitions();

	// no more transitions?
	if(transitions.size() == 0)
		return;

	for(const Closure::t_transition& tup : transitions)
	{
		const SymbolPtr& trans_sym = std::get<0>(tup);
		const ClosurePtr& closure_to = std::get<1>(tup);
		const Closure::t_elements& elems_from = std::get<2>(tup);

		t_hash hash_to = closure_to->hash(true);
		auto cacheIter = m_closure_cache->find(hash_to);
		bool new_closure = (cacheIter == m_closure_cache->end());

		std::ostringstream ostrMsg;
		ostrMsg << "Calculating " << (new_closure ? "new " : "") <<  "transition "
			<< closure_from->GetId() << " " << g_options.GetArrowChar() << " " << closure_to->GetId()
			<< ". Total closures: " << m_closures.size()
			<< ", total transitions: " << m_transitions.size()
			<< ".";
		ReportProgress(ostrMsg.str(), false);

		if(new_closure)
		{
			// new unique closure
			m_closure_cache->emplace(std::make_pair(hash_to, closure_to));
			m_closures.push_back(closure_to);
			m_transitions.emplace(std::make_tuple(
				closure_from, closure_to, trans_sym, elems_from));

			DoTransitions(closure_to);
		}
		else
		{
			// reuse closure with the same core that has already been seen
			const ClosurePtr& closure_to_existing = cacheIter->second;

			// unite lookaheads
			closure_to_existing->AddLookaheadDependencies(closure_to);

			// add the transition from the closure
			m_transitions.emplace(std::make_tuple(
				closure_from, closure_to_existing, trans_sym, elems_from));
		}
	}
}


void Collection::DoTransitions()
{
	m_closure_cache = nullptr;
	DoTransitions(*m_closures.begin());
	ReportProgress("Calculated transitions.", true);

	MapElementsToClosures();
	MapElementsToFollowingElements();
	ReportProgress("Calculated element graphs.", true);

	for(const ClosurePtr& closure : GetClosures())
	{
		for(const ElementPtr& element : closure->GetElements())
			element->SimplifyLookaheadDependencies();
	}

	for(const ClosurePtr& closure : GetClosures())
	{
		std::ostringstream ostrMsg;
		ostrMsg << "Calculating lookaheads for state " << closure->GetId() << ".";
		ReportProgress(ostrMsg.str(), false);
		closure->ResolveLookaheads();
	}
	ReportProgress("Calculated lookaheads.", true);

	Simplify();
	//MapElementsToClosures();
	ReportProgress("Simplified transitions.", true);

	// reports reduce/reduce or shift/reduce conflicts
	auto report_conflicts = [this](
		const std::map<t_state_id, std::string>& conflicts, const char* ty) -> void
	{
		if(!conflicts.size())
			return;

		std::ostringstream ostrConflicts;
		ostrConflicts << "The grammar has " << ty << " conflicts in state";
		if(conflicts.size() > 1)
			ostrConflicts << "s";  // plural
		ostrConflicts << " ";

		bool comma_list = true;
		t_index conflict_idx = 0;
		for(auto [conflictstate, conflictelems] : conflicts)
		{
			ostrConflicts << conflictstate;
			if(conflictelems == "")
			{
				if(conflict_idx < conflicts.size() - 1)
					ostrConflicts << ", ";
			}
			else
			{
				comma_list = false;
				ostrConflicts << ":\n" << conflictelems;
			}
			++conflict_idx;
		}

		if(comma_list)
			ostrConflicts << ".";

		if(m_stopOnConflicts)
			throw std::runtime_error(ostrConflicts.str());
		else
			std::cerr << "Error: " << ostrConflicts.str() << std::endl;
	};

	if(m_trySolveReduceConflicts)
		SolveReduceConflicts();

	report_conflicts(HasReduceConflicts(), "reduce/reduce");
	report_conflicts(HasShiftReduceConflicts(), "shift/reduce");

	ReportProgress("Calculated all transitions.", true);
}


/**
 * maps the elements to their parent closures containing them
 */
void Collection::MapElementsToClosures()
{
	for(const ClosurePtr& closure : GetClosures())
	{
		closure->SetReferenced(true);

		for(const ElementPtr& elem : closure->GetElements())
		{
			elem->SetParentClosure(closure);
			elem->SetReferenced(true);
		}
	}
}


/**
 * maps the elements to the elements of following closures
 */
void Collection::MapElementsToFollowingElements()
{
	// can't do a full hash, because the lookaheads are not yet resolved
	bool only_core_hash = true;

	for(const ClosurePtr& closure : GetClosures())
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			// transition originating from elem
			std::optional<t_transition> trans = GetTransition(elem, only_core_hash);
			if(!trans)
				continue;

			const ClosurePtr& closure_to = std::get<1>(*trans);
			for(const ElementPtr& elem_to : closure_to->GetElements())
				elem->AddForwardDependency(elem_to);
		}
	}
}


/**
 * simplifies the closures in the collection
 */
void Collection::Simplify()
{
	// sort rules
	m_closures.sort(
		[](const ClosurePtr& closure1, const ClosurePtr& closure2) -> bool
		{
			return closure1->GetId() < closure2->GetId();
		});

	// cleanup closure ids
	std::unordered_map<t_state_id, t_state_id> idmap{};
	std::unordered_set<t_hash> already_seen{};
	t_state_id newid{};

	for(const ClosurePtr& closure : GetClosures())
	{
		t_state_id oldid = closure->GetId();
		t_hash hash = closure->hash();

		// skip already-handled closures
		if(already_seen.contains(hash))
			continue;

		auto iditer = idmap.find(oldid);
		if(iditer == idmap.end())
		{
			// generate a new id
			iditer = idmap.emplace(
				std::make_pair(oldid, newid++)).first;
		}

		// set the new id
		closure->SetId(iditer->second);
		already_seen.insert(hash);
	}
}


/**
 * tests which closures of the collection have reduce/reduce conflicts
 */
std::map<t_state_id, std::string> Collection::HasReduceConflicts() const
{
	std::map<t_state_id, std::string> conflicting_closures;

	for(const ClosurePtr& closure : GetClosures())
	{
		Closure::t_conflictingelements conflicting_elems = closure->GetReduceConflicts();

		for(const auto& [lookahead, elems] : conflicting_elems)
		{
			if(elems.size() > 1)
			{
				std::ostringstream ostrelem;
				for(const ElementPtr& elem : elems)
				{
					ostrelem << "\t" << "lookahead: " << *lookahead
						<< ", element: " << *elem << "\n";
				}

				conflicting_closures.emplace(std::make_pair(closure->GetId(), ostrelem.str()));
			}
		} // iterate lookaheads
	} // iterate closures

	return conflicting_closures;
}


/**
 * tests which closures of the collection have shift/reduce conflicts
 */
std::map<t_state_id, std::string> Collection::HasShiftReduceConflicts() const
{
	std::map<t_state_id, std::string> conflicting_closures;

	for(const ClosurePtr& closure : GetClosures())
	{
		// get all terminals leading to a reduction
		Terminal::t_terminalset reduce_lookaheads;

		for(const ElementPtr& elem : closure->GetElements())
		{
			// reductions take place for finished elements
			if(!elem->IsCursorAtEnd())
				continue;

			const Terminal::t_terminalset& lookaheads = elem->GetLookaheads();
			reduce_lookaheads.insert(lookaheads.begin(), lookaheads.end());
		}

		// get all terminals leading to a shift
		for(const t_transition& tup : GetTransitions())
		{
			const ClosurePtr& stateFrom = std::get<0>(tup);
			const SymbolPtr& symTrans = std::get<2>(tup);

			if(stateFrom->hash() != closure->hash())
				continue;
			if(symTrans->IsEps() || !symTrans->IsTerminal())
				continue;

			const TerminalPtr termTrans = std::dynamic_pointer_cast<Terminal>(symTrans);
			bool has_solution = termTrans->GetPrecedence() || termTrans->GetAssociativity();
			if(reduce_lookaheads.contains(termTrans) && !has_solution)
			{
				std::ostringstream ostrtrans;
				ostrtrans << "\ttransition: " << *termTrans << " from state " << stateFrom->GetId() << "\n";

				conflicting_closures.emplace(std::make_pair(closure->GetId(), ostrtrans.str()));
			}
		}
	}

	return conflicting_closures;
}


/**
 * get the rule number and length of a partial match
 */
std::tuple<bool, t_semantic_id /*rule #*/, std::size_t /*match length*/, t_symbol_id /*lhs id*/>
Collection::GetUniquePartialMatch(
	const Collection::t_elements& elemsFrom, bool termTrans)
{
	std::unordered_map<t_semantic_id, ElementPtr> matching_rules{};

	for(const ElementPtr& elemFrom : elemsFrom)
	{
		// only consider either terminal or non-terminal transitions
		if(elemFrom->GetSymbolAtCursor()->IsTerminal() != termTrans)
			continue;

		// - match terminal transitions with a minimum length of 0
		//   (because the terminal lookahead is also known)
		// - match non-terminal transitions with a minimum length of 1
		//   (because length 0 is rarely unique as the cursor might be before
		//    a nonterminal and the same nonterminal will be at position 0
		//    of multiple generated elements)
		std::size_t match_len = elemFrom->GetCursor();
		if(!termTrans && match_len == 0)
			continue;

		// skip if no rule is assigned to this element
		std::optional<t_semantic_id> rule_id = elemFrom->GetSemanticRule();
		if(!rule_id)
			continue;

		if(auto iter_match = matching_rules.find(*rule_id);
			iter_match != matching_rules.end())
		{
			// longer match with the same rule?
			if(match_len > iter_match->second->GetCursor())
				iter_match->second = elemFrom;
		}
		else
		{
			// insert new match
			matching_rules.emplace(std::make_pair(*rule_id, elemFrom));
		}
	}

	// unique partial match?
	if(matching_rules.size() == 1)
	{
		return std::make_tuple(true,                                // partial match found
			matching_rules.begin()->first,                      // rule number
			matching_rules.begin()->second->GetCursor(),        // length of partial rule match
			matching_rules.begin()->second->GetLhs()->GetId()); // lhs symbol id
	}

	return std::make_tuple(false, 0, 0, 0);
}


/**
 * stop table/code generation on shift/reduce conflicts
 */
void Collection::SetStopOnConflicts(bool b)
{
	m_stopOnConflicts = b;
}


/**
 * try to solve reduce/reduce conflicts
 */
void Collection::SetSolveReduceConflicts(bool b)
{
	m_trySolveReduceConflicts = b;
}


/**
 * skip generation of lookback terminals
 */
void Collection::SetDontGenerateLookbacks(bool b)
{
	m_dontGenerateLookbacks = b;
}


bool Collection::GetDontGenerateLookbacks() const
{
	return m_dontGenerateLookbacks;
}


/**
 * try to solve reduce/reduce conflicts
 */
bool Collection::SolveReduceConflicts()
{
	bool ok = true;

	for(const ClosurePtr& closure : GetClosures())
	{
		if(!closure->SolveReduceConflicts())
			ok = false;
	}

	return ok;
}


/**
 * try to solve a shift/reduce conflict
 */
bool Collection::SolveShiftReduceConflict(
	const SymbolPtr& sym_at_cursor, const Terminal::t_terminalset& lookbacks,
	t_index* shiftEntry, t_index* reduceEntry) const
{
	// no conflict?
	if(*shiftEntry==ERROR_VAL || *reduceEntry==ERROR_VAL)
		return true;

	bool solution_found = false;

	// try to resolve conflict using operator precedences/associativities
	if(sym_at_cursor && sym_at_cursor->IsTerminal())
	{
		const TerminalPtr& term_at_cursor =
			std::dynamic_pointer_cast<Terminal>(sym_at_cursor);

		for(const TerminalPtr& lookback : lookbacks)
		{
			if(ConflictSolution sol = solve_shift_reduce_conflict(lookback, term_at_cursor);
				sol != ConflictSolution::NOT_FOUND)
			{
				if(sol == ConflictSolution::DO_SHIFT)
					*reduceEntry = ERROR_VAL;
				else if(sol == ConflictSolution::DO_REDUCE)
					*shiftEntry = ERROR_VAL;

				solution_found = true;
				break;
			}
		}
	}

	return solution_found;
}


/**
 * write out the transitions graph to an ostream
 * @see https://graphviz.org/doc/info/shapes.html#html
 */
bool Collection::SaveGraph(std::ostream& ofstr, bool write_full_coll, bool write_elem_wise) const
{
	const std::string& shift_col = g_options.GetShiftColour();
	const std::string& reduce_col = g_options.GetReduceColour();
	const std::string& jump_col = g_options.GetJumpColour();
	const bool use_colour = g_options.GetUseColour();

	ofstr << "digraph G_lalr1\n{\n";

	// write states
	for(const ClosurePtr& closure : GetClosures())
	{
		ofstr << "\t" << closure->GetId() << " [label=";
		if(write_full_coll)
		{
			ofstr << "<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\">";
			ofstr << "<tr><td colspan=\"3\" sides=\"b\"><b>" << "State "
				<< closure->GetId() << "</b></td></tr>";

			for(const ElementPtr& elem : closure->GetElements())
			{
				ofstr << "<tr>";
				// closure core
				bool at_end = elem->IsCursorAtEnd();
				const WordPtr& rhs = elem->GetRhs();

				auto set_colour = [&ofstr, &elem, &at_end, &shift_col, &reduce_col, &jump_col]()
				{
					if(at_end)
					{
						ofstr << "<font color=\"" << reduce_col << "\">";
					}
					else
					{
						if(elem->GetSymbolAtCursor()->IsTerminal())
							ofstr << "<font color=\"" << shift_col << "\">";
						else
							ofstr << "<font color=\"" << jump_col << "\">";
					}
				};

				ofstr << "<td align=\"left\" sides=\"r\" port=\"elem_"
					<< std::hex << elem->hash() << std::dec << "\">";
				if(use_colour)
					set_colour();

				ofstr << elem->GetLhs()->GetStrId();
				ofstr << " &#8594; ";

				for(t_index rhs_idx=0; rhs_idx<rhs->size(); ++rhs_idx)
				{
					// write cursor symbol
					if(elem->GetCursor() == rhs_idx)
						ofstr << "&#8226;";

					ofstr << (*rhs)[rhs_idx]->GetStrId();
					if(rhs_idx < rhs->size()-1)
						ofstr << " ";
				}
				if(at_end)
					ofstr << "&#8226;";
				if(use_colour)
					ofstr << "</font>";
				ofstr << " </td>";

				// lookaheads
				ofstr << "<td align=\"left\" sides=\"l\"> ";
				if(use_colour)
					set_colour();

				const Terminal::t_terminalset& lookaheads =
					elem->GetLookaheads();
				std::size_t lookahead_num = 0;
				for(const auto& la : lookaheads)
				{
					ofstr << la->GetStrId();
					if(lookahead_num < lookaheads.size()-1)
						ofstr << " ";
					++lookahead_num;
				}
				if(use_colour)
					ofstr << "</font>";
				ofstr << " </td>";

				// semantic rule
				ofstr << "<td align=\"left\" sides=\"l\"> ";
				if(use_colour)
					set_colour();

				if(std::optional<t_semantic_id> rule = elem->GetSemanticRule(); rule)
					ofstr << *rule;

				if(use_colour)
					ofstr << "</font>";
				ofstr << "</td></tr>";
			}
			ofstr << "</table>>";
		}
		else
		{
			ofstr << "\"" << closure->GetId() << "\"";
		}
		ofstr << "];\n";
	}

	// write transitions
	ofstr << "\n";
	for(const t_transition& tup : GetTransitions())
	{
		const ClosurePtr& closure_from = std::get<0>(tup);
		const ClosurePtr& closure_to = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);
		const t_elements& fromElems = std::get<3>(tup);

		bool symIsTerm = symTrans->IsTerminal();
		bool symIsEps = symTrans->IsEps();

		if(symIsEps)
			continue;

		auto write_colour = [use_colour, symIsTerm, &ofstr, &shift_col, &jump_col]()
		{
			if(use_colour)
			{
				if(symIsTerm)
					ofstr << "color=\"" << shift_col << "\", fontcolor=\"" << shift_col << "\"";
				else
					ofstr << "color=\"" << jump_col << "\", fontcolor=\"" << jump_col << "\"";
			}
		};

		if(write_elem_wise)
		{
			for(const ElementPtr& elem : fromElems)
			{
				ofstr << "\t" << closure_from->GetId()
					<< ":" << "elem_" << std::hex << elem->hash() << std::dec
					<< " -> " << closure_to->GetId()
					<< " [label=\"" << symTrans->GetStrId()
					<< "\", ";
				write_colour();
				ofstr << "];\n";

				// quick consistency check of the element map
				//if(elem->GetParentClosure()->GetId() != closure_from->GetId())
				//	std::cerr << "Error: Invalid closure mapping!" << std::endl;

				//std::cout << "closure " << closure_from->GetId() << ": " << *elem << " " << std::hex << elem->hash() << std::dec << std::endl;
			}
		}
		else
		{
			ofstr << "\t" << closure_from->GetId() << " -> " << closure_to->GetId()
				<< " [label=\"" << symTrans->GetStrId()
				<< "\", ";
			write_colour();
			ofstr << "];\n";
		}
	}

	ofstr << "}" << std::endl;
	return true;
}


/**
 * write out the transitions graph to a file
 */
bool Collection::SaveGraph(const std::string& file, bool write_full_coll, bool write_elem_wise) const
{
	std::string outfile_graph = file + ".graph";
	std::string outfile_svg = file + ".svg";

	std::ofstream ofstr{outfile_graph};
	if(!ofstr)
		return false;

	if(!SaveGraph(ofstr, write_full_coll, write_elem_wise))
		return false;

	ofstr.flush();
	ofstr.close();

	return std::system(("dot -Tsvg " + outfile_graph + " -o " + outfile_svg).c_str()) == 0;
}


std::ostream& operator<<(std::ostream& ostr, const Collection& coll)
{
	const std::string& shift_col = g_options.GetTermShiftColour();
	const std::string& reduce_col = g_options.GetTermReduceColour();
	const std::string& jump_col = g_options.GetTermJumpColour();
	const std::string& no_col = g_options.GetTermNoColour();
	const std::string& bold_col = g_options.GetTermBoldColour();
	const bool use_colour = g_options.GetUseColour();

	if(use_colour)
		ostr << bold_col;
	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Collection\n";
	ostr << "--------------------------------------------------------------------------------\n";
	if(use_colour)
		ostr << no_col;

	for(const ClosurePtr& closure : coll.GetClosures())
	{
		ostr << *closure;

		Terminal::t_terminalset lookbacks = coll.GetLookbackTerminals(closure);
		if(lookbacks.size())
			ostr << "Lookback terminals: ";
		for(const TerminalPtr& lookback : lookbacks)
			ostr << lookback->GetStrId() << " ";
		if(lookbacks.size())
			ostr << "\n";
		ostr << "\n";
	}
	ostr << "\n";


	if(use_colour)
		ostr << bold_col;
	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Transitions\n";
	ostr << "--------------------------------------------------------------------------------\n";
	if(use_colour)
		ostr << no_col;

	for(const Collection::t_transition& tup : coll.GetTransitions())
	{
		const SymbolPtr& symTrans = std::get<2>(tup);

		if(use_colour)
		{
			if(symTrans->IsTerminal())
				ostr << shift_col;
			else
				ostr << jump_col;
		}

		ostr << "state " << std::get<0>(tup)->GetId()
			<< " " << g_options.GetArrowChar() << " " << std::get<1>(tup)->GetId()
			<< " via " << symTrans->GetStrId()
			<< "\n";
		if(use_colour)
			ostr << no_col;

		/*const t_elements& from_elems = std::get<3>(tup);
		ostr << "Coming from element(s):\n";
		t_index elem_idx = 0;
		for(const ElementPtr& from_elem : from_elems)
		{
			ostr << "\t(" << elem_idx << ") " << *from_elem << "\n";
			++elem_idx;
		}*/
	}
	ostr << "\n\n";


	if(use_colour)
		ostr << bold_col;
	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Tables\n";
	ostr << "--------------------------------------------------------------------------------\n";
	if(use_colour)
		ostr << no_col;

	// sort transitions
	std::deque<Collection::t_transition> transitions;
	std::copy(coll.m_transitions.begin(), coll.m_transitions.end(), std::back_inserter(transitions));
	std::stable_sort(transitions.begin(), transitions.end(), Collection::CompareTransitionsLess{});

	std::ostringstream ostrActionShift, ostrActionReduce, ostrJump;

	if(use_colour)
	{
		ostrActionShift << shift_col;
		ostrActionReduce << reduce_col;
		ostrJump << jump_col;
	}

	for(const Collection::t_transition& tup : transitions)
	{
		const ClosurePtr& stateFrom = std::get<0>(tup);
		const ClosurePtr& stateTo = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);

		bool symIsTerm = symTrans->IsTerminal();
		bool symIsEps = symTrans->IsEps();

		if(symIsEps)
			continue;

		if(symIsTerm)
		{
			ostrActionShift
				<< "shift[ state "
				<< stateFrom->GetId() << ", "
				<< symTrans->GetStrId() << " ] = state "
				<< stateTo->GetId() << "\n";
		}
		else
		{
			ostrJump
				<< "jump[ state "
				<< stateFrom->GetId() << ", "
				<< symTrans->GetStrId() << " ] = state "
				<< stateTo->GetId() << "\n";
		}
	}

	for(const ClosurePtr& closure : coll.GetClosures())
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			if(!elem->IsCursorAtEnd())
				continue;

			ostrActionReduce << "reduce[ state " << closure->GetId() << ", ";
			for(const auto& la : elem->GetLookaheads())
				ostrActionReduce << la->GetStrId() << " ";
			ostrActionReduce << "] = ";
			if(auto rule = elem->GetSemanticRule(); rule)
				ostrActionReduce << "[rule " << *rule << "] ";
			ostrActionReduce << elem->GetLhs()->GetStrId()
				<< " " << g_options.GetArrowChar() << " " << *elem->GetRhs();
			ostrActionReduce << "\n";
		}
	}

	if(use_colour)
	{
		ostrActionShift << no_col;
		ostrActionReduce << no_col;
		ostrJump << no_col;
	}

	ostr << ostrActionShift.str() << "\n"
		<< ostrActionReduce.str() << "\n"
		<< ostrJump.str() << "\n";
	return ostr;
}


const Collection::t_closures& Collection::GetClosures() const
{
	return m_closures;
}


const Collection::t_transitions& Collection::GetTransitions() const
{
	return m_transitions;
}


bool Collection::GetStopOnConflicts() const
{
	return m_stopOnConflicts;
}


bool Collection::GetSolveReduceConflicts() const
{
	return m_trySolveReduceConflicts;
}


} // namespace lalr1
