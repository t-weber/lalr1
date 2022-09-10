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
#include "options.h"

#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>

#include <boost/functional/hash.hpp>


// ----------------------------------------------------------------------------
/**
 * hash a transition element
 */
std::size_t Collection::HashTransition::operator()(
	const t_transition& trans) const
{
	std::size_t hashFrom = std::get<0>(trans)->hash(true);
	std::size_t hashTo = std::get<1>(trans)->hash(true);
	std::size_t hashSym = std::get<2>(trans)->hash();

	boost::hash_combine(hashFrom, hashTo);
	boost::hash_combine(hashFrom, hashSym);

	return hashFrom;
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


Collection::Collection(const ClosurePtr& closure)
	: std::enable_shared_from_this<Collection>{}
{
	m_collection.push_back(closure);
}


Collection::Collection(const Collection& coll)
	: std::enable_shared_from_this<Collection>{}
{
	this->operator=(coll);
}


const Collection& Collection::operator=(const Collection& coll)
{
	this->m_collection = coll.m_collection;
	this->m_transitions = coll.m_transitions;
	this->m_mapTermIdx = coll.m_mapTermIdx;
	this->m_mapNonTermIdx = coll.m_mapNonTermIdx;
	this->m_closure_cache = coll.m_closure_cache;
	this->m_seen_closures = coll.m_seen_closures;
	this->m_stopOnConflicts = coll.m_stopOnConflicts;
	this->m_useOpChar = coll.m_useOpChar;
	this->m_genDebugCode = coll.m_genDebugCode;
	this->m_genErrorCode = coll.m_genErrorCode;
	this->m_progress_observer = coll.m_progress_observer;

	return *this;
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
	const ClosurePtr& closure, bool term) const
{
	t_transitions transitions;

	for(const t_transition& transition : m_transitions)
	{
		const ClosurePtr& closure_from = std::get<0>(transition);
		const SymbolPtr& sym = std::get<2>(transition);

		if(sym->IsEps())
			continue;

		// only consider transitions from the given closure
		if(closure_from->hash() != closure->hash())
			continue;

		if(sym->IsTerminal() == term)
			transitions.insert(transition);
	}

	return transitions;
}


/**
 * get terminals leading to the given closure
 */
Terminal::t_terminalset Collection::GetLookbackTerminals(
	const ClosurePtr& closure) const
{
	m_seen_closures = std::make_shared<std::unordered_set<std::size_t>>();
	return _GetLookbackTerminals(closure);
}


Terminal::t_terminalset Collection::_GetLookbackTerminals(
	const ClosurePtr& closure) const
{
	Terminal::t_terminalset terms;

	for(const t_transition& transition : m_transitions)
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
			std::size_t hash = closure_from->hash();
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
		m_closure_cache = std::make_shared<std::unordered_map<std::size_t, ClosurePtr>>();
		m_closure_cache->emplace(std::make_pair(closure_from->hash(true), closure_from));
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

		std::size_t hash_to = closure_to->hash(true);
		auto cacheIter = m_closure_cache->find(hash_to);
		bool new_closure = (cacheIter == m_closure_cache->end());

		std::ostringstream ostrMsg;
		ostrMsg << "Calculating " << (new_closure ? "new " : "") <<  "transition "
			<< closure_from->GetId() << " " << g_options.GetArrowChar() << " " << closure_to->GetId()
			<< ". Total closures: " << m_collection.size()
			<< ", total transitions: " << m_transitions.size()
			<< ".";
		ReportProgress(ostrMsg.str(), false);

		if(new_closure)
		{
			// new unique closure
			m_closure_cache->emplace(std::make_pair(hash_to, closure_to));
			m_collection.push_back(closure_to);
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
	DoTransitions(*m_collection.begin());
	ReportProgress("Calculated transitions.", true);

	for(ClosurePtr& closure : m_collection)
	{
		std::ostringstream ostrMsg;
		ostrMsg << "Calculating lookaheads for state " << closure->GetId() << ".";
		ReportProgress(ostrMsg.str(), false);
		closure->ResolveLookaheads();
	}
	ReportProgress("Calculated lookaheads.", true);

	Simplify();
	ReportProgress("Simplified transitions.", true);

	// reports reduce/reduce or shift/reduce conflicts
	auto report_conflicts = [this](const std::set<std::size_t>& conflicts, const char* ty) -> void
	{
		if(!conflicts.size())
			return;

		std::ostringstream ostrConflicts;
		ostrConflicts << "The grammar has " << ty << " conflicts in state";
		if(conflicts.size() > 1)
			ostrConflicts << "s";  // plural
		ostrConflicts << " ";

		std::size_t conflict_idx = 0;
		for(std::size_t conflict : conflicts)
		{
			ostrConflicts << conflict;
			if(conflict_idx < conflicts.size() - 1)
				ostrConflicts << ", ";
			++conflict_idx;
		}
		ostrConflicts << ".";

		if(m_stopOnConflicts)
			throw std::runtime_error(ostrConflicts.str());
		else
			std::cerr << "Error: " << ostrConflicts.str() << std::endl;
	};

	report_conflicts(HasReduceConflicts(), "reduce/reduce");
	report_conflicts(HasShiftReduceConflicts(), "shift/reduce");

	CreateTableIndices();
	ReportProgress("Created table indices.", true);
}


void Collection::Simplify()
{
	// sort rules
	m_collection.sort(
		[](const ClosurePtr& closure1, const ClosurePtr& closure2) -> bool
		{
			return closure1->GetId() < closure2->GetId();
		});

	// cleanup closure ids
	std::unordered_map<std::size_t, std::size_t> idmap;
	std::unordered_set<std::size_t> already_seen;
	std::size_t newid = 0;

	for(const ClosurePtr& closure : m_collection)
	{
		std::size_t oldid = closure->GetId();
		std::size_t hash = closure->hash();

		if(already_seen.contains(hash))
			continue;

		auto iditer = idmap.find(oldid);
		if(iditer == idmap.end())
			iditer = idmap.emplace(
				std::make_pair(oldid, newid++)).first;

		closure->SetId(iditer->second);
		already_seen.insert(hash);
	}
}


/**
 * tests which closures of the collection have reduce/reduce conflicts
 */
std::set<std::size_t> Collection::HasReduceConflicts() const
{
	std::set<std::size_t> conflicting_closures;

	for(const ClosurePtr& closure : m_collection)
	{
		if(closure->HasReduceConflict())
			conflicting_closures.insert(closure->GetId());
	}

	return conflicting_closures;
}


/**
 * tests which closures of the collection have shift/reduce conflicts
 */
std::set<std::size_t> Collection::HasShiftReduceConflicts() const
{
	std::set<std::size_t> conflicting_closures;

	for(const ClosurePtr& closure : m_collection)
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
		for(const Collection::t_transition& tup : m_transitions)
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
				conflicting_closures.insert(closure->GetId());
		}
	}

	return conflicting_closures;
}


/**
 * creates indices for the parse tables from the symbol ids
 */
void Collection::CreateTableIndices()
{
	// generate table indices for terminals
	m_mapTermIdx.clear();
	std::size_t curTermIdx = 0;

	for(const t_transition& tup : m_transitions)
	{
		const SymbolPtr& symTrans = std::get<2>(tup);

		if(symTrans->IsEps() || !symTrans->IsTerminal())
			continue;

		if(auto [iter, inserted] = m_mapTermIdx.try_emplace(
			symTrans->GetId(), curTermIdx); inserted)
			++curTermIdx;
	}

	// add end symbol
	m_mapTermIdx.try_emplace(g_end->GetId(), curTermIdx++);


	// generate able indices for non-terminals
	m_mapNonTermIdx.clear();
	std::size_t curNonTermIdx = 0;

	for(const ClosurePtr& closure : m_collection)
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			if(!elem->IsCursorAtEnd())
				continue;

			std::size_t sym_id = elem->GetLhs()->GetId();

			if(auto [iter, inserted] = m_mapNonTermIdx.try_emplace(
				sym_id, curNonTermIdx); inserted)
				++curNonTermIdx;
		}
	}
}


/**
 * translates symbol id to table index
 */
std::size_t Collection::GetTableIndex(std::size_t id, bool is_term) const
{
	const t_mapIdIdx* map = is_term ? &m_mapTermIdx : &m_mapNonTermIdx;

	auto iter = map->find(id);
	if(iter == map->end())
	{
		std::ostringstream ostrErr;
		ostrErr << "No table index is available for ";
		if(is_term)
			ostrErr << "terminal";
		else
			ostrErr << "non-terminal";
		ostrErr << " with id " << id << ".";
		throw std::runtime_error(ostrErr.str());
	}

	return iter->second;
}


/**
 * stop table/code generation on shift/reduce conflicts
 */
void Collection::SetStopOnConflicts(bool b)
{
	m_stopOnConflicts = b;
}


/**
 * use printable operator character if possible
 */
void Collection::SetUseOpChar(bool b)
{
	m_useOpChar = b;
}


/**
 * generate debug code in parser output
 */
void Collection::SetGenDebugCode(bool b)
{
	m_genDebugCode = b;
}


/**
 * generate error handling code in parser output
 */
void Collection::SetGenErrorCode(bool b)
{
	m_genErrorCode = b;
}


/**
 * try to solve a shift/reduce conflict
 */
bool Collection::SolveConflict(
	const SymbolPtr& sym_at_cursor, const Terminal::t_terminalset& lookbacks,
	std::size_t* shiftEntry, std::size_t* reduceEntry) const
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
			auto prec_lhs = lookback->GetPrecedence();
			auto prec_rhs = term_at_cursor->GetPrecedence();

			// both terminals have a precedence
			if(prec_lhs && prec_rhs)
			{
				if(*prec_lhs < *prec_rhs)       // shift
				{
					*reduceEntry = ERROR_VAL;
					solution_found = true;
				}
				else if(*prec_lhs > *prec_rhs)  // reduce
				{
					*shiftEntry = ERROR_VAL;
					solution_found = true;
				}

				// same precedence -> use associativity
			}

			if(!solution_found)
			{
				auto assoc_lhs = lookback->GetAssociativity();
				auto assoc_rhs = term_at_cursor->GetAssociativity();

				// both terminals have an associativity
				if(assoc_lhs && assoc_rhs &&
					*assoc_lhs == *assoc_rhs)
				{
					if(*assoc_lhs == 'r')      // shift
					{
						*reduceEntry = ERROR_VAL;
						solution_found = true;
					}
					else if(*assoc_lhs == 'l') // reduce
					{
						*shiftEntry = ERROR_VAL;
						solution_found = true;
					}
				}
			}

			if(solution_found)
				break;
		}
	}

	return solution_found;
}


/**
 * write out the transitions graph to an ostream
 * @see https://graphviz.org/doc/info/shapes.html#html
 */
bool Collection::SaveGraph(std::ostream& ofstr, bool write_full_coll) const
{
	const std::string& shift_col = g_options.GetShiftColour();
	const std::string& reduce_col = g_options.GetReduceColour();
	const std::string& jump_col = g_options.GetJumpColour();
	const bool use_colour = g_options.GetUseColour();

	ofstr << "digraph G_lalr1\n{\n";

	// write states
	for(const ClosurePtr& closure : m_collection)
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

				ofstr << "<td align=\"left\" sides=\"r\">";
				if(use_colour)
					set_colour();

				ofstr << elem->GetLhs()->GetStrId();
				ofstr << " &#8594; ";

				for(std::size_t rhs_idx=0; rhs_idx<rhs->size(); ++rhs_idx)
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

				const Terminal::t_terminalset& lookaheads = elem->GetLookaheads();
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

				if(std::optional<std::size_t> rule = elem->GetSemanticRule(); rule)
				{
					ofstr << *rule;
				}

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
	for(const t_transition& tup : m_transitions)
	{
		const ClosurePtr& closure_from = std::get<0>(tup);
		const ClosurePtr& closure_to = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);

		bool symIsTerm = symTrans->IsTerminal();
		bool symIsEps = symTrans->IsEps();

		if(symIsEps)
			continue;

		ofstr << "\t" << closure_from->GetId() << " -> " << closure_to->GetId()
			<< " [label=\"" << symTrans->GetStrId()
			<< "\", ";

		if(use_colour)
		{
			if(symIsTerm)
				ofstr << "color=\"" << shift_col << "\", fontcolor=\"" << shift_col << "\"";
			else
				ofstr << "color=\"" << jump_col << "\", fontcolor=\"" << jump_col << "\"";
		}
		ofstr << "];\n";
	}

	ofstr << "}" << std::endl;
	return true;
}


/**
 * write out the transitions graph to a file
 */
bool Collection::SaveGraph(const std::string& file, bool write_full_coll) const
{
	std::string outfile_graph = file + ".graph";
	std::string outfile_svg = file + ".svg";

	std::ofstream ofstr{outfile_graph};
	if(!ofstr)
		return false;

	if(!SaveGraph(ofstr, write_full_coll))
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

	for(const ClosurePtr& closure : coll.m_collection)
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

	for(const Collection::t_transition& tup : coll.m_transitions)
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

		/*const Collection::t_elements& from_elems = std::get<3>(tup);
		ostr << "Coming from element(s):\n";
		std::size_t elem_idx = 0;
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
	std::vector<Collection::t_transition> transitions;
	transitions.reserve(coll.m_transitions.size());
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

	for(const ClosurePtr& closure : coll.m_collection)
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			if(!elem->IsCursorAtEnd())
				continue;

			ostrActionReduce << "reduce[ state " << closure->GetId() << ", ";
			for(const auto& la : elem->GetLookaheads())
				ostrActionReduce << la->GetStrId() << " ";
			ostrActionReduce << "] = ";
			if(elem->GetSemanticRule())
			{
				ostrActionReduce << "[rule "
					<< *elem->GetSemanticRule() << "] ";
			}
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
