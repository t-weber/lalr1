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
 */

#include "collection.h"

#include <sstream>
#include <fstream>
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
	/*std::size_t hash1 = HashTransition{}(tr1);
	std::size_t hash2 = HashTransition{}(tr2);
	return hash1 < hash2;*/

	ClosurePtr from1 = std::get<0>(tr1);
	ClosurePtr from2 = std::get<0>(tr2);
	ClosurePtr to1 = std::get<1>(tr1);
	ClosurePtr to2 = std::get<1>(tr2);
	SymbolPtr sym1 = std::get<2>(tr1);
	SymbolPtr sym2 = std::get<2>(tr2);

	if(from1->GetId() < from2->GetId())
		return true;
	if(from1->GetId() == from2->GetId())
		return to1->GetId() < to2->GetId();
	if(from1->GetId() == from2->GetId() && to1->GetId() == to2->GetId())
		return sym1->GetId() < sym2->GetId();
	return false;
}
// ----------------------------------------------------------------------------


Collection::Collection(const ClosurePtr closure)
	: std::enable_shared_from_this<Collection>{}, m_collection{}, m_transitions{}
{
	m_collection.push_back(closure);
}


Collection::Collection()
	: std::enable_shared_from_this<Collection>{}, m_collection{}, m_transitions{}
{
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
 * perform all possible lalr(1) transitions from all collections
 */
void Collection::DoTransitions(const ClosurePtr closure_from, t_closurecache closure_cache)
{
	if(!closure_cache)
	{
		closure_cache = std::make_shared<std::unordered_map<std::size_t, ClosurePtr>>();
		closure_cache->emplace(std::make_pair(closure_from->hash(true), closure_from));
	}

	std::vector<std::tuple<SymbolPtr, ClosurePtr>> closure =
		closure_from->DoTransitions();

	// no more transitions?
	if(closure.size() == 0)
		return;

	for(const auto& tup : closure)
	{
		const SymbolPtr trans_sym = std::get<0>(tup);
		const ClosurePtr closure_to = std::get<1>(tup);
		std::size_t hash_to = closure_to->hash(true);
		auto cacheIter = closure_cache->find(hash_to);
		bool new_closure = (cacheIter == closure_cache->end());

		std::ostringstream ostrMsg;
		ostrMsg << "Calculating " << (new_closure ? "new " : "") <<  "transition "
			<< closure_from->GetId() << " -> " << closure_to->GetId() << ".";
		//ostrMsg << " Transition symbol: " << trans_sym->GetId() << ".";
		ReportProgress(ostrMsg.str(), false);
		//std::cout << std::hex << closure_to->hash() << ", " << *closure_to << std::endl;

		if(new_closure)
		{
			// new unique closure
			closure_cache->emplace(std::make_pair(hash_to, closure_to));
			m_collection.push_back(closure_to);
			m_transitions.emplace(std::make_tuple(
				closure_from, closure_to, trans_sym));

			DoTransitions(closure_to, closure_cache);
		}
		else
		{
			// reuse closure that has already been seen
			ClosurePtr closure_to_existing = cacheIter->second;

			// unite lookaheads
			bool lookaheads_added = closure_to_existing->AddLookaheads(closure_to);

			m_transitions.emplace(std::make_tuple(
				closure_from, closure_to_existing, trans_sym));

			// unite lookbacks
			for(const Closure::t_comefrom_transition& comefrom
				: closure_to->m_comefrom_transitions)
				closure_to_existing->m_comefrom_transitions.insert(comefrom);

			// also add the comefrom symbol to the closure
			closure_to_existing->m_comefrom_transitions.emplace(
				std::make_tuple(trans_sym, closure_from));

			// if a lookahead of an old closure has changed,
			// the transitions of that closure need to be redone
			if(lookaheads_added)
				DoTransitions(closure_to_existing, closure_cache);
		}
	}
}


void Collection::DoTransitions()
{
	DoTransitions(m_collection[0]);
	Simplify();
	ReportProgress("All transitions done.", true);
}


void Collection::Simplify()
{
	constexpr bool do_sort = 1;
	constexpr bool do_cleanup = 1;

	// sort rules
	if constexpr(do_sort)
	{
		std::sort(m_collection.begin(), m_collection.end(),
			[](const ClosurePtr closure1, const ClosurePtr closure2) -> bool
			{
				return closure1->GetId() < closure2->GetId();
			});
	}

	// cleanup ids
	if constexpr(do_cleanup)
	{
		std::unordered_map<std::size_t, std::size_t> idmap;
		std::unordered_set<std::size_t> already_seen;
		std::size_t newid = 0;

		for(ClosurePtr closure : m_collection)
		{
			std::size_t oldid = closure->GetId();
			std::size_t hash = closure->hash();

			if(already_seen.find(hash) != already_seen.end())
				continue;

			auto iditer = idmap.find(oldid);
			if(iditer == idmap.end())
				iditer = idmap.emplace(
					std::make_pair(oldid, newid++)).first;

			closure->SetId(iditer->second);
			already_seen.insert(hash);
		}
	}
}


/**
 * write out the transitions graph
 */
bool Collection::WriteGraph(const std::string& file, bool write_full_coll) const
{
	std::string outfile_graph = file + ".graph";
	std::string outfile_svg = file + ".svg";

	std::ofstream ofstr{outfile_graph};
	if(!ofstr)
		return false;

	ofstr << "digraph G_lr1\n{\n";

	// write states
	for(const ClosurePtr& closure : m_collection)
	{
		ofstr << "\t" << closure->GetId() << " [label=\"";
		if(write_full_coll)
			ofstr << *closure;
		else
			ofstr << closure->GetId();
		ofstr << "\"];\n";
	}

	// write transitions
	ofstr << "\n";
	for(const t_transition& tup : m_transitions)
	{
		const ClosurePtr closure_from = std::get<0>(tup);
		const ClosurePtr closure_to = std::get<1>(tup);
		const SymbolPtr symTrans = std::get<2>(tup);

		//bool symIsTerm = symTrans->IsTerminal();
		bool symIsEps = symTrans->IsEps();

		if(symIsEps)
			continue;

		ofstr << "\t" << closure_from->GetId() << " -> " << closure_to->GetId()
			<< " [label=\"" << symTrans->GetStrId() << "\"];\n";
	}

	ofstr << "}" << std::endl;
	ofstr.flush();
	ofstr.close();

	return std::system(("dot -Tsvg " + outfile_graph + " -o " + outfile_svg).c_str()) == 0;
}



/**
 * creates the lr(1) parser tables
 */
std::tuple<t_table, t_table, t_table, t_mapIdIdx, t_mapIdIdx, t_vecIdx>
Collection::CreateParseTables(bool stopOnConflicts) const
{
	const std::size_t numStates = m_collection.size();
	const std::size_t errorVal = ERROR_VAL;
	const std::size_t acceptVal = ACCEPT_VAL;

	// number of symbols on rhs of a production rule
	std::vector<std::size_t> numRhsSymsPerRule;

	// lr tables
	std::vector<std::vector<std::size_t>> _action_shift, _action_reduce, _jump;
	_action_shift.resize(numStates);
	_action_reduce.resize(numStates);
	_jump.resize(numStates);

	// maps the ids to table indices for the non-terminals and the terminals
	t_mapIdIdx mapNonTermIdx, mapTermIdx;

	// current counters
	std::size_t curNonTermIdx = 0, curTermIdx = 0;

	// map terminal table index to terminal symbol
	std::unordered_map<std::size_t, TerminalPtr> seen_terminals{};


	// translate symbol id to table index
	auto get_idx = [&mapNonTermIdx, &mapTermIdx, &curNonTermIdx, &curTermIdx]
	(std::size_t id, bool is_term) -> std::size_t
	{
		std::size_t* curIdx = is_term ? &curTermIdx : &curNonTermIdx;
		t_mapIdIdx* map = is_term ? &mapTermIdx : &mapNonTermIdx;

		auto iter = map->find(id);
		if(iter == map->end())
			iter = map->emplace(std::make_pair(id, (*curIdx)++)).first;
		return iter->second;
	};


	for(const Collection::t_transition& tup : m_transitions)
	{
		const ClosurePtr& stateFrom = std::get<0>(tup);
		const ClosurePtr& stateTo = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);

		bool symIsTerm = symTrans->IsTerminal();
		bool symIsEps = symTrans->IsEps();

		if(symIsEps)
			continue;

		std::vector<std::vector<std::size_t>>* tab =
			symIsTerm ? &_action_shift : &_jump;

		std::size_t symIdx = get_idx(symTrans->GetId(), symIsTerm);
		if(symIsTerm)
		{
			seen_terminals.emplace(std::make_pair(
				symIdx, std::dynamic_pointer_cast<Terminal>(symTrans)));
		}

		auto& _tab_row = (*tab)[stateFrom->GetId()];
		if(_tab_row.size() <= symIdx)
			_tab_row.resize(symIdx+1, errorVal);
		_tab_row[symIdx] = stateTo->GetId();
	}

	for(const ClosurePtr& closure : m_collection)
	{
		for(std::size_t elemidx=0; elemidx < closure->NumElements(); ++elemidx)
		{
			const ElementPtr& elem = closure->GetElement(elemidx);
			if(!elem->IsCursorAtEnd())
				continue;

			std::optional<std::size_t> rulenr = *elem->GetSemanticRule();
			if(!rulenr)		// no semantic rule assigned
				continue;
			std::size_t rule = *rulenr;

			const Word* rhs = elem->GetRhs();
			std::size_t numRhsSyms = rhs->NumSymbols(false);
			if(numRhsSymsPerRule.size() <= rule)
				numRhsSymsPerRule.resize(rule+1);
			numRhsSymsPerRule[rule] = numRhsSyms;

			auto& _action_row = _action_reduce[closure->GetId()];

			for(const auto& la : elem->GetLookaheads())
			{
				std::size_t laIdx = get_idx(la->GetId(), true);

				// in extended grammar, first production (rule 0) is of the form start -> ...
				if(rule == 0)
					rule = acceptVal;

				if(_action_row.size() <= laIdx)
					_action_row.resize(laIdx+1, errorVal);
				_action_row[laIdx] = rule;
			}
		}
	}


	auto tables = std::make_tuple(
		t_table{_action_shift, errorVal, acceptVal, numStates, curTermIdx},
		t_table{_action_reduce, errorVal, acceptVal, numStates, curTermIdx},
		t_table{_jump, errorVal, acceptVal, numStates, curNonTermIdx},
		mapTermIdx, mapNonTermIdx,
		numRhsSymsPerRule);

	// check for and try to resolve shift/reduce conflicts
	for(std::size_t state=0; state<numStates; ++state)
	{
		ClosurePtr closureState = m_collection[state];
		std::vector<TerminalPtr> comefromTerms = closureState->GetComefromTerminals();

		for(std::size_t termidx=0; termidx<curTermIdx; ++termidx)
		{
			std::size_t& shiftEntry = std::get<0>(tables)(state, termidx);
			std::size_t& reduceEntry = std::get<1>(tables)(state, termidx);
			//const t_mapIdIdx& mapTermIdx = std::get<3>(tables);

			std::optional<std::string> termid;
			ElementPtr conflictelem = nullptr;
			SymbolPtr sym_at_cursor = nullptr;

			if(auto termiter = seen_terminals.find(termidx);
				termiter != seen_terminals.end())
			{
				termid = termiter->second->GetStrId();
				conflictelem = closureState->
					GetElementWithCursorAtSymbol(termiter->second);
				if(conflictelem)
					sym_at_cursor = conflictelem->GetSymbolAtCursor();
			}

			// both have an entry?
			if(shiftEntry!=errorVal && reduceEntry!=errorVal)
			{
				bool solution_found = false;

				// try to resolve conflict using operator precedences/associativities
				if(sym_at_cursor && sym_at_cursor->IsTerminal())
				{
					const TerminalPtr term_at_cursor =
						std::dynamic_pointer_cast<Terminal>(sym_at_cursor);

					for(const TerminalPtr& comefromTerm : comefromTerms)
					{
						auto prec_lhs = comefromTerm->GetPrecedence();
						auto prec_rhs = term_at_cursor->GetPrecedence();

						// both terminals have a precedence
						if(prec_lhs && prec_rhs)
						{
							if(*prec_lhs < *prec_rhs)       // shift
							{
								reduceEntry = errorVal;
								solution_found = true;
							}
							else if(*prec_lhs > *prec_rhs)  // reduce
							{
								shiftEntry = errorVal;
								solution_found = true;
							}

							// same precedence -> use associativity
						}

						if(!solution_found)
						{
							auto assoc_lhs = comefromTerm->GetAssociativity();
							auto assoc_rhs = term_at_cursor->GetAssociativity();

							// both terminals have an associativity
							if(assoc_lhs && assoc_rhs && *assoc_lhs == *assoc_rhs)
							{
								if(*assoc_lhs == 'r')      // shift
								{
									reduceEntry = errorVal;
									solution_found = true;
								}
								else if(*assoc_lhs == 'l') // reduce
								{
									shiftEntry = errorVal;
									solution_found = true;
								}
							}
						}

						if(solution_found)
							break;
					}
				}


				if(!solution_found)
				{
					std::ostringstream ostrErr;
					ostrErr << "Shift/reduce conflict detected"
						<< " for state " << state;
					if(conflictelem)
						ostrErr << ":\n\t" << *conflictelem << "\n";
					if(comefromTerms.size())
					{
						ostrErr << " with look-back terminal(s): ";
						for(std::size_t i=0; i<comefromTerms.size(); ++i)
						{
							ostrErr << comefromTerms[i]->GetStrId();
							if(i < comefromTerms.size()-1)
								ostrErr << ", ";
						}
					}
					if(termid)
						ostrErr << " and look-ahead terminal " << (*termid);
					else
						ostrErr << "and terminal index " << termidx;
					ostrErr << " (can either shift to state " << shiftEntry
						<< " or reduce using rule " << reduceEntry
						<< ")." << std::endl;

					if(stopOnConflicts)
						throw std::runtime_error(ostrErr.str());
					else
						std::cerr << ostrErr.str() << std::endl;
				}
			}
		}
	}

	return tables;
}


/**
 * export lr(1) tables to C++ code
 */
bool Collection::SaveParseTables(const std::tuple<t_table, t_table, t_table,
	t_mapIdIdx, t_mapIdIdx, t_vecIdx>& tabs, const std::string& file)
{
	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	ofstr << "#ifndef __LR1_TABLES__\n";
	ofstr << "#define __LR1_TABLES__\n\n";

	ofstr <<"namespace _lr1_tables {\n\n";

	// save constants
	ofstr << "\tconst std::size_t err = " << ERROR_VAL << ";\n";
	ofstr << "\tconst std::size_t acc = " << ACCEPT_VAL << ";\n";
	ofstr << "\tconst std::size_t eps = " << EPS_IDENT << ";\n";
	ofstr << "\tconst std::size_t end = " << END_IDENT << ";\n";
	ofstr << "\n";

	std::get<0>(tabs).SaveCXXDefinition(ofstr, "tab_action_shift");
	std::get<1>(tabs).SaveCXXDefinition(ofstr, "tab_action_reduce");
	std::get<2>(tabs).SaveCXXDefinition(ofstr, "tab_jump");

	// terminal symbol indices
	ofstr << "const t_mapIdIdx map_term_idx{{\n";
	for(const auto& [id, idx] : std::get<3>(tabs))
	{
		ofstr << "\t{";
		if(id == EPS_IDENT)
			ofstr << "eps";
		else if(id == END_IDENT)
			ofstr << "end";
		else
			ofstr << id;
		ofstr << ", " << idx << "},\n";
	}
	ofstr << "}};\n\n";

	// non-terminal symbol indices
	ofstr << "const t_mapIdIdx map_nonterm_idx{{\n";
	for(const auto& [id, idx] : std::get<4>(tabs))
		ofstr << "\t{" << id << ", " << idx << "},\n";
	ofstr << "}};\n\n";

	ofstr << "const t_vecIdx vec_num_rhs_syms{{ ";
	for(const auto& val : std::get<5>(tabs))
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	ofstr << "}\n\n\n";


	ofstr << "static std::tuple<const t_table*, const t_table*, const t_table*, const t_mapIdIdx*, const t_mapIdIdx*, const t_vecIdx*>\n";
	ofstr << "get_lr1_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n"
		<< "\t\t&_lr1_tables::tab_action_shift, &_lr1_tables::tab_action_reduce, &_lr1_tables::tab_jump,\n"
		<< "\t\t&_lr1_tables::map_term_idx, &_lr1_tables::map_nonterm_idx, &_lr1_tables::vec_num_rhs_syms);\n";
	ofstr << "}\n\n";


	ofstr << "\n#endif" << std::endl;
	return true;
}


std::ostream& operator<<(std::ostream& ostr, const Collection& coll)
{
	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Collection\n";
	ostr << "--------------------------------------------------------------------------------\n";
	for(const ClosurePtr& closure : coll.m_collection)
	{
		ostr << *closure;
		closure->PrintComefroms(ostr);
		ostr << "\n";
	}
	ostr << "\n";


	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Transitions\n";
	ostr << "--------------------------------------------------------------------------------\n";
	for(const Collection::t_transition& tup : coll.m_transitions)
	{
		ostr << std::get<0>(tup)->GetId()
			<< " -> " << std::get<1>(tup)->GetId()
			<< " via " << std::get<2>(tup)->GetStrId()
			<< "\n";
	}
	ostr << "\n\n";


	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Tables\n";
	ostr << "--------------------------------------------------------------------------------\n";
	// sort transitions
	std::vector<Collection::t_transition> transitions;
	transitions.reserve(coll.m_transitions.size());
	std::copy(coll.m_transitions.begin(), coll.m_transitions.end(), std::back_inserter(transitions));
	std::stable_sort(transitions.begin(), transitions.end(), Collection::CompareTransitionsLess{});

	std::ostringstream ostrActionShift, ostrActionReduce, ostrJump;
	for(const Collection::t_transition& tup : /*coll.m_transitions*/ transitions)
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
				<< "action_shift[ state "
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
		for(std::size_t elemidx=0; elemidx < closure->NumElements(); ++elemidx)
		{
			const ElementPtr& elem = closure->GetElement(elemidx);
			if(!elem->IsCursorAtEnd())
				continue;

			ostrActionReduce << "action_reduce[ state " << closure->GetId() << ", ";
			for(const auto& la : elem->GetLookaheads())
				ostrActionReduce << la->GetStrId() << " ";
			ostrActionReduce << "] = ";
			if(elem->GetSemanticRule())
			{
				ostrActionReduce << "[rule "
					<< *elem->GetSemanticRule() << "] ";
			}
			ostrActionReduce << elem->GetLhs()->GetStrId()
				<< " -> " << *elem->GetRhs();
			ostrActionReduce << "\n";
		}
	}

	ostr << ostrActionShift.str() << "\n"
		<< ostrActionReduce.str() << "\n"
		<< ostrJump.str() << "\n";
	return ostr;
}
