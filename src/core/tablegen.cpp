/**
 * generates lalr(1) tables from a collection
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

#include "tablegen.h"

#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <optional>
#include <type_traits>


namespace lalr1 {


TableGen::TableGen(const CollectionPtr& coll) : m_collection{coll}
{ }


/**
 * creates indices for the parse tables from the symbol ids
 */
void TableGen::CreateTableIndices()
{
	// generate table indices for terminals
	m_mapTermIdx.clear();
	m_mapTermStrIds.clear();
	m_seen_terminals.clear();
	t_index curTermIdx = 0;

	const Collection::t_closures& closures = m_collection->GetClosures();
	const Collection::t_transitions& transitions = m_collection->GetTransitions();

	for(const Collection::t_transition& tup : transitions)
	{
		const SymbolPtr& symTrans = std::get<2>(tup);

		if(symTrans->IsEps() || !symTrans->IsTerminal())
			continue;

		// terminal id map
		if(auto [iter, inserted] = m_mapTermIdx.try_emplace(
			symTrans->GetId(), curTermIdx); inserted)
		{
			++curTermIdx;

			const TerminalPtr termTrans = std::dynamic_pointer_cast<Terminal>(symTrans);
			m_seen_terminals.push_back(termTrans);
		}

		// terminal string id map
		const std::string& sym_strid = symTrans->GetStrId();
		m_mapTermStrIds.try_emplace(symTrans->GetId(), sym_strid);
	}

	// add end symbol
	if(auto [iter, inserted] = m_mapTermIdx.try_emplace(g_end->GetId(), curTermIdx++);
		inserted)
	{
		const TerminalPtr termTrans = std::dynamic_pointer_cast<Terminal>(g_end);
		m_seen_terminals.push_back(g_end);
	}

	// end string id
	m_mapTermStrIds.try_emplace(g_end->GetId(), g_end->GetStrId());


	// generate table indices for non-terminals and semantic rules
	m_mapNonTermIdx.clear();
	m_mapSemanticIdx.clear();
	m_mapNonTermStrIds.clear();
	t_index curNonTermIdx = 0;
	t_index curSemanticIdx = 0;

	for(const ClosurePtr& closure : closures)
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			if(!elem->IsCursorAtEnd())
				continue;

			// nonterminal id map
			t_symbol_id sym_id = elem->GetLhs()->GetId();
			if(auto [iter, inserted] = m_mapNonTermIdx.try_emplace(
				sym_id, curNonTermIdx); inserted)
				++curNonTermIdx;

			// nonterminal string id map
			const std::string& sym_strid = elem->GetLhs()->GetStrId();
			m_mapNonTermStrIds.try_emplace(sym_id, sym_strid);

			if(std::optional<t_semantic_id> semantic_id =
				elem->GetSemanticRule(); semantic_id)
			{
				if(auto [iter, inserted] = m_mapSemanticIdx.try_emplace(
					*semantic_id, curSemanticIdx); inserted)
					++curSemanticIdx;
			}
		}
	}
}


/**
 * creates operator precedence and associativity tables
 */
void TableGen::CreateTerminalPrecedences()
{
	m_mapTermPrec.clear();
	m_mapTermAssoc.clear();

	for(const TerminalPtr& term : m_seen_terminals)
	{
		if(!term)
			continue;

		if(auto prec = term->GetPrecedence(); prec)
			m_mapTermPrec.emplace(std::make_pair(term->GetId(), *prec));
		if(auto assoc = term->GetAssociativity(); assoc)
			m_mapTermAssoc.emplace(std::make_pair(term->GetId(), *assoc));
	}
}


/**
 * creates lalr(1) parse tables for exporting
 */
bool TableGen::CreateParseTables()
{
	m_collection->ReportProgress("Creating parse tables...", false);

	CreateTableIndices();
	CreateTerminalPrecedences();

	const Collection::t_closures& closures = m_collection->GetClosures();
	const Collection::t_transitions& transitions = m_collection->GetTransitions();

	const std::size_t numStates = closures.size();
	const std::size_t numTerminals = m_mapTermIdx.size();
	const std::size_t numNonTerminals = m_mapNonTermIdx.size();

	bool ok = true;

	// lalr(1) tables
	std::vector<std::vector<t_index>> action_shift, action_reduce, jump;
	action_shift.resize(numStates);
	action_reduce.resize(numStates);
	jump.resize(numStates);

	// tables for partial matches for terminal and non-terminal transition symbols
	std::vector<std::vector<t_index>> partials_rule_term, partials_rule_nonterm;
	std::vector<std::vector<std::size_t>> partials_matchlen_term, partials_matchlen_nonterm;
	std::vector<std::vector<t_symbol_id>> partials_lhsid_nonterm;
	partials_rule_term.resize(numStates);
	partials_rule_nonterm.resize(numStates);
	partials_matchlen_term.resize(numStates);
	partials_matchlen_nonterm.resize(numStates);
	partials_lhsid_nonterm.resize(numStates);

	for(t_state_id state=0; state<numStates; ++state)
	{
		action_shift[state].resize(numTerminals, ERROR_VAL);
		action_reduce[state].resize(numTerminals, ERROR_VAL);
		jump[state].resize(numNonTerminals, ERROR_VAL);

		partials_rule_term[state].resize(numTerminals, ERROR_VAL);
		partials_rule_nonterm[state].resize(numNonTerminals, ERROR_VAL);
		partials_matchlen_term[state].resize(numTerminals, 0);
		partials_matchlen_nonterm[state].resize(numNonTerminals, 0);
		partials_lhsid_nonterm[state].resize(numNonTerminals, ERROR_VAL);
	}


	// set a table item
	auto set_tab_elem = [](std::vector<std::size_t>& vec, t_index idx,
		std::size_t val, t_index filler = ERROR_VAL) -> void
	{
		if(vec.size() <= idx)
			vec.resize(idx+1, filler);
		vec[idx] = val;
	};


	// map terminal table index to terminal symbol
	std::unordered_map<t_index, TerminalPtr> seen_terminals{};


	// calculate shift and jump table entries
	m_collection->ReportProgress("Calculating shift and jump entries...", false);

	for(const Collection::t_transition& transition : transitions)
	{
		const SymbolPtr& symTrans = std::get<2>(transition);
		if(symTrans->IsEps())
			continue;

		// terminal transitions -> shift table
		// nonterminal transitions -> jump table
		const bool symIsTerm = symTrans->IsTerminal();
		IndexTableKind tablekind = symIsTerm ?
			IndexTableKind::TERMINAL : IndexTableKind::NONTERMINAL;

		//std::cerr << "Getting table index for " << symTrans->GetStrId() << std::endl;
		t_index symIdx = GetTableIndex(symTrans->GetId(), tablekind);
		if(symIsTerm)
		{
			seen_terminals.emplace(std::make_pair(
				symIdx, std::dynamic_pointer_cast<Terminal>(symTrans)));
		}

		// set table elements
		std::vector<std::vector<t_index>>* tab = symIsTerm ? &action_shift : &jump;

		const ClosurePtr& stateFrom = std::get<0>(transition);
		const ClosurePtr& stateTo = std::get<1>(transition);
		const Collection::t_elements& elemsFrom = std::get<3>(transition);

		set_tab_elem((*tab)[stateFrom->GetId()], symIdx, stateTo->GetId());

		if(GetGenPartialMatches())
		{
			// unique partial match for terminal transition?
			if(auto [uniquematch, rule_id, rule_len, lhs_id] =
				m_collection->GetUniquePartialMatch(elemsFrom, true); uniquematch)
			{
				// set partial match table elements
				t_index rule_idx = GetTableIndex(rule_id, IndexTableKind::SEMANTIC);
				set_tab_elem(partials_rule_term[stateFrom->GetId()], symIdx, rule_idx);
				set_tab_elem(partials_matchlen_term[stateFrom->GetId()], symIdx, rule_len, 0);
				// TODO: also save lhs_id to table to check against semantic rule's return type
			}

			// unique partial match for non-terminal transition?
			if(auto [uniquematch, rule_id, rule_len, lhs_id] =
				m_collection->GetUniquePartialMatch(elemsFrom, false); uniquematch)
			{
				// set partial match table elements
				t_index rule_idx = GetTableIndex(rule_id, IndexTableKind::SEMANTIC);
				set_tab_elem(partials_rule_nonterm[stateFrom->GetId()], symIdx, rule_idx);
				set_tab_elem(partials_matchlen_nonterm[stateFrom->GetId()], symIdx, rule_len, 0);
				set_tab_elem(partials_lhsid_nonterm[stateFrom->GetId()], symIdx, lhs_id);
			}
		}
	}


	// calculate reduce table entries
	m_collection->ReportProgress("Calculating reduce entries...", false);

	for(const ClosurePtr& closure : closures)
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			// cursor at end -> reduce a completely parsed rule
			if(!elem->IsCursorAtEnd())
				continue;

			std::optional<t_semantic_id> rule_id = elem->GetSemanticRule();
			if(!rule_id)  // no semantic rule assigned
			{
				std::cerr << "Error: No semantic rule assigned to element "
					<< (*elem) << "." << std::endl;
				continue;
			}

			t_index rule_idx = GetTableIndex(*rule_id, IndexTableKind::SEMANTIC);
			set_tab_elem(m_numRhsSymsPerRule, rule_idx,
				elem->GetRhs()->NumSymbols(false), 0);

			//std::cerr << "Getting table index for lhs symbol " << elem->GetLhs()->GetStrId() << std::endl;
			const t_index lhs_idx = GetTableIndex(
				elem->GetLhs()->GetId(),
				IndexTableKind::NONTERMINAL);
			set_tab_elem(m_ruleLhsIdx, rule_idx, lhs_idx, 0);
			// TODO: also save lhs id to table to check against semantic rule's return type

			auto& _reduce_row = action_reduce[closure->GetId()];
			for(const TerminalPtr& la : elem->GetLookaheads())
			{
				const t_index laIdx = GetTableIndex(la->GetId(), IndexTableKind::TERMINAL);

				// in extended grammar, first production (rule 0) is of the form start -> ...
				if(*rule_id == GetAcceptingRule())
					rule_idx = ACCEPT_VAL;

				// semantic rule number -> reduce table
				set_tab_elem(_reduce_row, laIdx, rule_idx);
			}
		}
	}


	// lalr(1) tables
	m_collection->ReportProgress("Creating LALR(1) tables...", false);

	m_tabActionShift = t_table{action_shift,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numTerminals};
	m_tabActionReduce = t_table{action_reduce,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numTerminals};
	m_tabJump = t_table{jump,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numNonTerminals};

	// partial match tables
	m_tabPartialRuleTerm = t_table{partials_rule_term,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numTerminals};
	m_tabPartialMatchLenTerm = t_table{partials_matchlen_term,
		ERROR_VAL, ACCEPT_VAL, 0, numStates, numTerminals};
	m_tabPartialRuleNonterm = t_table{partials_rule_nonterm,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numNonTerminals};
	m_tabPartialMatchLenNonterm = t_table{partials_matchlen_nonterm,
		ERROR_VAL, ACCEPT_VAL, 0, numStates, numNonTerminals};
	m_tabPartialNontermLhsId = t_table{partials_lhsid_nonterm,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numNonTerminals};


	// check for and try to resolve shift/reduce conflicts
	m_collection->ReportProgress("Solving shift/reduce conflicts...", false);

	t_state_id state = 0;
	const bool no_lookbacks_avail = m_collection->GetDontGenerateLookbacks();
	for(const ClosurePtr& closure : closures)
	{
		std::optional<Terminal::t_terminalset> lookbacks;

		for(t_index termidx=0; termidx<numTerminals; ++termidx)
		{
			// get table entries
			t_index& shiftEntry = m_tabActionShift(state, termidx);
			t_index& reduceEntry = m_tabActionReduce(state, termidx);

			// partial match tables
			//t_index& partialRuleEntry = m_tabPartialRuleTerm(state, termidx);
			//std::size_t& partialMatchLenEntry = m_tabPartialMatchLenTerm(state, termidx);

			// get potentially conflicting element
			std::optional<std::string> termid;
			ElementPtr conflictelem = nullptr;
			SymbolPtr sym_at_cursor = nullptr;

			if(auto termiter = seen_terminals.find(termidx); termiter != seen_terminals.end())
			{
				termid = termiter->second->GetStrId();
				sym_at_cursor = termiter->second;
				conflictelem = closure->GetElementWithCursorAtSymbol(sym_at_cursor);
			}

			// no conflicts? -> continue
			if(!conflictelem || shiftEntry==ERROR_VAL || reduceEntry==ERROR_VAL)
				continue;

			// a conflicting element exists and both tables have an entry?
			if(!lookbacks)
				lookbacks = m_collection->GetLookbackTerminals(closure);

			if(m_collection->SolveShiftReduceConflict(sym_at_cursor, *lookbacks, &shiftEntry, &reduceEntry))
			{
				// solution found:
				// also apply conflict solution to partial matches
				/*if(shiftEntry == ERROR_VAL)
				{
					partialRuleEntry = ERROR_VAL;
					partialMatchLenEntry = 0;
				}*/
				continue;
			}

			// no solution found:
			// only report fail state when the lookbacks have been calculated,
			// otherwise the user wants to have the conflict solved in the parser
			if(!no_lookbacks_avail)
				ok = false;

			std::ostringstream ostrErr;
			ostrErr << "Shift/reduce conflict detected"
				<< " for state " << state;
			if(conflictelem)
				ostrErr << ":\n\t" << *conflictelem << "\n";
			if(lookbacks->size())
			{
				ostrErr << " with look-back terminal(s): ";
				std::size_t i = 0;
				for(const TerminalPtr& lookback : *lookbacks)
				{
					ostrErr << lookback->GetStrId();
					if(i < lookbacks->size()-1)
						ostrErr << ", ";
					++i;
				}
			}
			if(termid)
				ostrErr << " and look-ahead terminal " << (*termid);
			else
				ostrErr << "and terminal index " << termidx;
			ostrErr << " (can either shift to state " << shiftEntry
				<< " or reduce using rule " << reduceEntry
				<< ").\n";

			if(GetStopOnConflicts())
				throw std::runtime_error(ostrErr.str());
			else
				std::cerr << ostrErr.str() << std::endl;
		}  // terminal index

		++state;
	}  // closure

	if(ok)
		m_collection->ReportProgress("Created parse tables.", true);
	else
		m_collection->ReportProgress("Failed creating parse tables.", true);
	return ok;
}


/**
 * translates symbol id to table index
 */
t_index TableGen::GetTableIndex(t_symbol_id id, IndexTableKind table_kind) const
{
	const t_mapIdIdx* map = nullptr;
	switch(table_kind)
	{
		case IndexTableKind::TERMINAL:
			map = &m_mapTermIdx;
			break;
		case IndexTableKind::NONTERMINAL:
			map = &m_mapNonTermIdx;
			break;
		case IndexTableKind::SEMANTIC:
			map = &m_mapSemanticIdx;
			break;
	}
	if(!map)
		throw std::runtime_error("Unknown index table selected.");

	auto iter = map->find(id);
	if(iter == map->end())
	{
		std::ostringstream ostrErr;
		ostrErr << "No table index is available for ";
		switch(table_kind)
		{
			case IndexTableKind::TERMINAL:
				ostrErr << "terminal";
				break;
			case IndexTableKind::NONTERMINAL:
				ostrErr << "non-terminal";
				break;
			case IndexTableKind::SEMANTIC:
				ostrErr << "semantic rule";
				break;
			default:
				ostrErr << "<unknown>";
				break;
		}
		ostrErr << " with id " << id << ".";
		throw std::runtime_error(ostrErr.str());
	}

	return iter->second;
}


bool TableGen::GetStopOnConflicts() const
{
	return m_collection->GetStopOnConflicts();
}

} // namespace lalr1
