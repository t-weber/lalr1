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


Collection::Collection() : std::enable_shared_from_this<Collection>{}
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


std::vector<TerminalPtr> Collection::GetLookbackTerminals(
	const ClosurePtr& closure) const
{
	m_seen_closures = std::make_shared<std::unordered_set<std::size_t>>();
	return _GetLookbackTerminals(closure);
}


std::vector<TerminalPtr> Collection::_GetLookbackTerminals(
	const ClosurePtr& closure) const
{
	std::vector<TerminalPtr> terms;

	for(const t_transition& transition : m_transitions)
	{
		const ClosurePtr& closure_from = std::get<0>(transition);
		const ClosurePtr& closure_to = std::get<1>(transition);
		const SymbolPtr& sym = std::get<2>(transition);

		// only consider transitions to given closure
		if(closure_to->hash() != closure->hash())
			continue;

		if(sym->IsTerminal())
		{
			const TerminalPtr& term =
				std::dynamic_pointer_cast<Terminal>(sym);
			terms.emplace_back(std::move(term));
		}
		else if(closure_from)
		{
			// closure not yet seen?
			std::size_t hash = closure_from->hash();
			if(m_seen_closures->find(hash) == m_seen_closures->end())
			{
				m_seen_closures->insert(hash);

				// get terminals from previous closure
				std::vector<TerminalPtr> _terms =
					_GetLookbackTerminals(closure_from);
				terms.insert(terms.end(), _terms.begin(), _terms.end());
			}
		}
	}

	// remove duplicates
	std::stable_sort(terms.begin(), terms.end(),
		[](const TerminalPtr& term1, const TerminalPtr& term2) -> bool
		{
			return term1->hash() < term2->hash();
		});

	if(auto end = std::unique(terms.begin(), terms.end(),
		[](const TerminalPtr& term1, const TerminalPtr& term2) -> bool
		{
			return *term1 == *term2;
		}); end != terms.end())
	{
		terms.resize(end - terms.begin());
	}

	return terms;
}


/**
 * perform all possible lalr(1) transitions from all collections
 */
void Collection::DoTransitions(const ClosurePtr& closure_from)
{
	if(!m_closure_cache)
	{
		m_closure_cache = std::make_shared<std::unordered_map<std::size_t, ClosurePtr>>();
		m_closure_cache->emplace(std::make_pair(closure_from->hash(true), closure_from));
	}

	Closure::t_transitions transitions = closure_from->DoTransitions();

	// no more transitions?
	if(transitions.size() == 0)
		return;

	for(const Closure::t_transition& tup : transitions)
	{
		const SymbolPtr& trans_sym = std::get<0>(tup);
		const ClosurePtr& closure_to = std::get<1>(tup);
		std::size_t hash_to = closure_to->hash(true);
		auto cacheIter = m_closure_cache->find(hash_to);
		bool new_closure = (cacheIter == m_closure_cache->end());

		std::ostringstream ostrMsg;
		ostrMsg << "Calculating " << (new_closure ? "new " : "") <<  "transition "
			<< closure_from->GetId() << " -> " << closure_to->GetId() << ".";
		ReportProgress(ostrMsg.str(), false);

		if(new_closure)
		{
			// new unique closure
			m_closure_cache->emplace(std::make_pair(hash_to, closure_to));
			m_collection.push_back(closure_to);
			m_transitions.emplace(std::make_tuple(
				closure_from, closure_to, trans_sym));

			DoTransitions(closure_to);
		}
		else
		{
			// reuse closure that has already been seen
			ClosurePtr closure_to_existing = cacheIter->second;

			// unite lookaheads
			bool lookaheads_added = closure_to_existing->AddLookaheads(closure_to);

			// add the transition from the closure
			m_transitions.emplace(std::make_tuple(
				closure_from, closure_to_existing, trans_sym));

			// if a lookahead of an old closure has changed,
			// the transitions of that closure need to be redone
			if(lookaheads_added)
				DoTransitions(closure_to_existing);
		}
	}
}


void Collection::DoTransitions()
{
	m_closure_cache = nullptr;
	DoTransitions(*m_collection.begin());
	Simplify();

	ReportProgress("All transitions done.", true);
}


void Collection::Simplify()
{
	// sort rules
	m_collection.sort(
		[](const ClosurePtr& closure1, const ClosurePtr& closure2) -> bool
		{
			return closure1->GetId() < closure2->GetId();
		});

	// cleanup ids
	std::unordered_map<std::size_t, std::size_t> idmap;
	std::unordered_set<std::size_t> already_seen;
	std::size_t newid = 0;

	for(const ClosurePtr& closure : m_collection)
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


/**
 * export lalr(1) tables to C++ code
 */
bool Collection::SaveParseTables(const std::string& file, bool stopOnConflicts) const
{
	// create lalr(1) tables
	bool ok = true;
	t_mapIdIdx mapNonTermIdx{}, mapTermIdx{};     // maps the ids to table indices
	std::vector<std::size_t> numRhsSymsPerRule{}; // number of symbols on rhs of a production rule

	const std::size_t numStates = m_collection.size();
	const std::size_t errorVal = ERROR_VAL;
	const std::size_t acceptVal = ACCEPT_VAL;

	// lalr(1) tables
	std::vector<std::vector<std::size_t>> _action_shift, _action_reduce, _jump;
	_action_shift.resize(numStates);
	_action_reduce.resize(numStates);
	_jump.resize(numStates);

	// current counters
	std::size_t curNonTermIdx = 0, curTermIdx = 0;

	// map terminal table index to terminal symbol
	std::unordered_map<std::size_t, TerminalPtr> seen_terminals{};


	// translate symbol id to table index
	auto get_idx = [&mapTermIdx, &mapNonTermIdx, &curNonTermIdx, &curTermIdx]
		(std::size_t id, bool is_term) -> std::size_t
		{
			std::size_t* curIdx = is_term ? &curTermIdx : &curNonTermIdx;
			t_mapIdIdx* map = is_term ? &mapTermIdx : &mapNonTermIdx;

			auto iter = map->find(id);
			if(iter == map->end())
				iter = map->emplace(std::make_pair(id, (*curIdx)++)).first;
			return iter->second;
		};


	for(const t_transition& tup : m_transitions)
	{
		const ClosurePtr& stateFrom = std::get<0>(tup);
		const ClosurePtr& stateTo = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);

		bool symIsTerm = symTrans->IsTerminal();
		bool symIsEps = symTrans->IsEps();

		if(symIsEps)
			continue;

		std::vector<std::vector<std::size_t>>* tab = symIsTerm ? &_action_shift : &_jump;

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

			if(numRhsSymsPerRule.size() <= rule)
				numRhsSymsPerRule.resize(rule+1);
			numRhsSymsPerRule[rule] = elem->GetRhs()->NumSymbols(false);

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


	t_table tabActionShift = t_table{_action_shift, errorVal, acceptVal, numStates, curTermIdx};
	t_table tabActionReduce = t_table{_action_reduce, errorVal, acceptVal, numStates, curTermIdx};
	t_table tabJump = t_table{_jump, errorVal, acceptVal, numStates, curNonTermIdx};

	// check for and try to resolve shift/reduce conflicts
	std::size_t state = 0;
	for(const ClosurePtr& closureState : m_collection)
	{
		std::optional<std::vector<TerminalPtr>> lookbacks;

		for(std::size_t termidx=0; termidx<curTermIdx; ++termidx)
		{
			std::size_t& shiftEntry = tabActionShift(state, termidx);
			std::size_t& reduceEntry = tabActionReduce(state, termidx);

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
				if(!lookbacks)
					lookbacks = GetLookbackTerminals(closureState);
				bool solution_found = false;

				// try to resolve conflict using operator precedences/associativities
				if(sym_at_cursor && sym_at_cursor->IsTerminal())
				{
					const TerminalPtr& term_at_cursor =
						std::dynamic_pointer_cast<Terminal>(sym_at_cursor);

					for(const TerminalPtr& lookback : *lookbacks)
					{
						auto prec_lhs = lookback->GetPrecedence();
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
							auto assoc_lhs = lookback->GetAssociativity();
							auto assoc_rhs = term_at_cursor->GetAssociativity();

							// both terminals have an associativity
							if(assoc_lhs && assoc_rhs &&
								*assoc_lhs == *assoc_rhs)
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
					ok = false;

					std::ostringstream ostrErr;
					ostrErr << "Shift/reduce conflict detected"
						<< " for closure " << state;
					if(conflictelem)
						ostrErr << ":\n\t" << *conflictelem << "\n";
					if(lookbacks->size())
					{
						ostrErr << " with look-back terminal(s): ";
						for(std::size_t i=0; i<lookbacks->size(); ++i)
						{
							ostrErr << (*lookbacks)[i]->GetStrId();
							if(i < lookbacks->size()-1)
								ostrErr << ", ";
						}
					}
					if(termid)
						ostrErr << " and look-ahead terminal " << (*termid);
					else
						ostrErr << "and terminal index " << termidx;
					ostrErr << " (can either shift to closure " << shiftEntry
						<< " or reduce using rule " << reduceEntry
						<< ")." << std::endl;

					if(stopOnConflicts)
						throw std::runtime_error(ostrErr.str());
					else
						std::cerr << ostrErr.str() << std::endl;
				}
			}
		}

		++state;
	}

	if(!ok)
		return false;


	// save lalr(1) tables
	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	ofstr << "#ifndef __LALR1_TABLES__\n";
	ofstr << "#define __LALR1_TABLES__\n\n";

	ofstr <<"namespace _lr1_tables {\n\n";

	// save constants
	ofstr << "\tconst std::size_t err = " << ERROR_VAL << ";\n";
	ofstr << "\tconst std::size_t acc = " << ACCEPT_VAL << ";\n";
	ofstr << "\tconst std::size_t eps = " << EPS_IDENT << ";\n";
	ofstr << "\tconst std::size_t end = " << END_IDENT << ";\n";
	ofstr << "\n";

	tabActionShift.SaveCXXDefinition(ofstr, "tab_action_shift");
	tabActionReduce.SaveCXXDefinition(ofstr, "tab_action_reduce");
	tabJump.SaveCXXDefinition(ofstr, "tab_jump");

	// terminal symbol indices
	ofstr << "const t_mapIdIdx map_term_idx{{\n";
	for(const auto& [id, idx] : mapTermIdx)
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
	for(const auto& [id, idx] : mapNonTermIdx)
		ofstr << "\t{" << id << ", " << idx << "},\n";
	ofstr << "}};\n\n";

	ofstr << "const t_vecIdx vec_num_rhs_syms{{ ";
	for(const auto& val : numRhsSymsPerRule)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	ofstr << "}\n\n\n";


	ofstr << "static std::tuple<const t_table*, const t_table*, const t_table*, const t_mapIdIdx*, const t_mapIdIdx*, const t_vecIdx*>\n";
	ofstr << "get_lalr1_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n"
		<< "\t\t&_lr1_tables::tab_action_shift, &_lr1_tables::tab_action_reduce, &_lr1_tables::tab_jump,\n"
		<< "\t\t&_lr1_tables::map_term_idx, &_lr1_tables::map_nonterm_idx, &_lr1_tables::vec_num_rhs_syms);\n";
	ofstr << "}\n\n";


	ofstr << "\n#endif" << std::endl;
	return true;
}


/**
 * export an explicit recursive ascent parser
 * @see https://en.wikipedia.org/wiki/Recursive_ascent_parser
 */
bool Collection::SaveParser(const std::string& file) const
{
	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	for(const ClosurePtr& closure : m_collection)
	{
		ofstr << "/*\n" << *closure;
		if(std::vector<TerminalPtr> lookbacks = GetLookbackTerminals(closure);
			lookbacks.size())
		{
			ofstr << "Lookback terminals: ";
			for(const TerminalPtr& lookback : lookbacks)
				ofstr << lookback->GetStrId() << " ";
			ofstr << "\n";
		}
		ofstr << "*/\n";
		ofstr << "void closure_" << closure->GetId() << "()\n";
		ofstr << "{\n";
		ofstr << "}\n\n\n";
	}

	return true;
}


/**
 * write out the transitions graph
 */
bool Collection::SaveGraph(const std::string& file, bool write_full_coll) const
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
		const ClosurePtr& closure_from = std::get<0>(tup);
		const ClosurePtr& closure_to = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);

		if(symTrans->IsEps())
			continue;

		ofstr << "\t" << closure_from->GetId() << " -> " << closure_to->GetId()
			<< " [label=\"" << symTrans->GetStrId() << "\"];\n";
	}

	ofstr << "}" << std::endl;
	ofstr.close();

	return std::system(("dot -Tsvg " + outfile_graph + " -o " + outfile_svg).c_str()) == 0;
}


std::ostream& operator<<(std::ostream& ostr, const Collection& coll)
{
	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Collection\n";
	ostr << "--------------------------------------------------------------------------------\n";
	for(const ClosurePtr& closure : coll.m_collection)
	{
		ostr << *closure;

		std::vector<TerminalPtr> lookbacks = coll.GetLookbackTerminals(closure);
		if(lookbacks.size())
			ostr << "Lookback terminals: ";
		for(const TerminalPtr& lookback : lookbacks)
			ostr << lookback->GetStrId() << " ";
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
				<< "action_shift[ closure "
				<< stateFrom->GetId() << ", "
				<< symTrans->GetStrId() << " ] = closure "
				<< stateTo->GetId() << "\n";
		}
		else
		{
			ostrJump
				<< "jump[ closure "
				<< stateFrom->GetId() << ", "
				<< symTrans->GetStrId() << " ] = closure "
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

			ostrActionReduce << "action_reduce[ closure " << closure->GetId() << ", ";
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
