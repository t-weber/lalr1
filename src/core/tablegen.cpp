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

#include "collection.h"

#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <optional>
#include <type_traits>


/**
 * create lalr(1) parse tables to C++ code
 */
bool Collection::CreateParseTables()
{
	const std::size_t numStates = m_collection.size();
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
	partials_rule_term.resize(numStates);
	partials_rule_nonterm.resize(numStates);
	partials_matchlen_term.resize(numStates);
	partials_matchlen_nonterm.resize(numStates);

	for(t_state_id state=0; state<numStates; ++state)
	{
		action_shift[state].resize(numTerminals, ERROR_VAL);
		action_reduce[state].resize(numTerminals, ERROR_VAL);
		jump[state].resize(numNonTerminals, ERROR_VAL);

		partials_rule_term[state].resize(numTerminals, ERROR_VAL);
		partials_rule_nonterm[state].resize(numNonTerminals, ERROR_VAL);
		partials_matchlen_term[state].resize(numTerminals, 0);
		partials_matchlen_nonterm[state].resize(numTerminals, 0);
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
	for(const t_transition& transition : m_transitions)
	{
		const SymbolPtr& symTrans = std::get<2>(transition);
		if(symTrans->IsEps())
			continue;

		// terminal transitions -> shift table
		// nonterminal transitions -> jump table
		const bool symIsTerm = symTrans->IsTerminal();
		IndexTableKind tablekind = symIsTerm ?
			IndexTableKind::TERMINAL : IndexTableKind::NONTERMINAL;

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

		if(m_genPartialMatches)
		{
			// unique partial match?
			if(auto [uniquematch, rule_id, rule_len] = GetUniquePartialMatch(elemsFrom); uniquematch)
			{
				// set partial match table elements
				std::vector<std::vector<t_index>>* partials_rule_tab =
					symIsTerm ? &partials_rule_term : &partials_rule_nonterm;
				std::vector<std::vector<std::size_t>>* partials_matchlen_tab =
					symIsTerm ? &partials_matchlen_term : &partials_matchlen_nonterm;

				t_index rule_idx = GetTableIndex(rule_id, IndexTableKind::SEMANTIC);
				set_tab_elem((*partials_rule_tab)[stateFrom->GetId()], symIdx, rule_idx);
				set_tab_elem((*partials_matchlen_tab)[stateFrom->GetId()], symIdx, rule_len, 0);
			}
		}
	}


	// calculate reduce table entries
	for(const ClosurePtr& closure : m_collection)
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
			set_tab_elem(m_ruleLhsIdx, rule_idx,
				GetTableIndex(elem->GetLhs()->GetId(),
					IndexTableKind::NONTERMINAL), 0);

			auto& _reduce_row = action_reduce[closure->GetId()];
			for(const TerminalPtr& la : elem->GetLookaheads())
			{
				const t_index laIdx = GetTableIndex(
					la->GetId(), IndexTableKind::TERMINAL);

				// in extended grammar, first production (rule 0) is of the form start -> ...
				if(*rule_id == m_accepting_rule)
					rule_idx = ACCEPT_VAL;

				// semantic rule number -> reduce table
				set_tab_elem(_reduce_row, laIdx, rule_idx);
			}
		}
	}


	// lalr(1) tables
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


	// check for and try to resolve shift/reduce conflicts
	t_state_id state = 0;
	for(const ClosurePtr& closure : m_collection)
	{
		std::optional<Terminal::t_terminalset> lookbacks;

		for(t_index termidx=0; termidx<numTerminals; ++termidx)
		{
			// get table entries
			t_index& shiftEntry = m_tabActionShift(state, termidx);
			t_index& reduceEntry = m_tabActionReduce(state, termidx);

			// partial match tables
			t_index& partialRuleEntry = m_tabPartialRuleTerm(state, termidx);
			std::size_t& partialMatchLenEntry = m_tabPartialMatchLenTerm(state, termidx);

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

			// a conflicting element exists and both tables have an entry?
			if(conflictelem && shiftEntry!=ERROR_VAL && reduceEntry!=ERROR_VAL)
			{
				if(!lookbacks)
					lookbacks = GetLookbackTerminals(closure);

				if(!SolveConflict(sym_at_cursor, *lookbacks, &shiftEntry, &reduceEntry))
				{
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

					if(m_stopOnConflicts)
						throw std::runtime_error(ostrErr.str());
					else
						std::cerr << ostrErr.str() << std::endl;
				}
				else  // solution found
				{
					// also apply conflict solution to partial matches
					if(shiftEntry == ERROR_VAL)
					{
						partialRuleEntry = ERROR_VAL;
						partialMatchLenEntry = 0;
					}
				}
			}
		}

		++state;
	}

	return ok;
}
