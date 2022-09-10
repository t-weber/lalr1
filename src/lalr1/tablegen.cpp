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
#include "timer.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>


/**
 * export lalr(1) tables to C++ code
 */
bool Collection::SaveParseTables(const std::string& file) const
{
	const std::size_t numStates = m_collection.size();
	const std::size_t numTerminals = m_mapTermIdx.size();
	const std::size_t numNonTerminals = m_mapNonTermIdx.size();


	// create lalr(1) tables
	bool ok = true;
	std::vector<std::size_t> numRhsSymsPerRule{}; // number of symbols on rhs of a production rule
	std::vector<std::size_t> ruleLhsIdx{};        // nonterminal index of the rule's result type

	// lalr(1) tables
	std::vector<std::vector<std::size_t>> action_shift, action_reduce, jump;
	action_shift.resize(numStates);
	action_reduce.resize(numStates);
	jump.resize(numStates);

	// tables for partial matches for terminal and non-terminal transition symbols
	std::vector<std::vector<std::size_t>> partials_rule_term, partials_rule_nonterm;
	std::vector<std::vector<std::size_t>> partials_matchlen_term, partials_matchlen_nonterm;
	partials_rule_term.resize(numStates);
	partials_rule_nonterm.resize(numStates);
	partials_matchlen_term.resize(numStates);
	partials_matchlen_nonterm.resize(numStates);

	for(std::size_t state=0; state<numStates; ++state)
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
	auto set_tab_elem = [](std::vector<std::size_t>& vec, std::size_t idx,
		std::size_t val, std::size_t filler = ERROR_VAL) -> void
	{
		if(vec.size() <= idx)
			vec.resize(idx+1, filler);
		vec[idx] = val;
	};


	// map terminal table index to terminal symbol
	std::unordered_map<std::size_t, TerminalPtr> seen_terminals{};


	// calculate shift and jump table entries
	for(const t_transition& transition : m_transitions)
	{
		const SymbolPtr& symTrans = std::get<2>(transition);
		if(symTrans->IsEps())
			continue;

		// terminal transitions -> shift table
		// nonterminal transitions -> jump table
		const bool symIsTerm = symTrans->IsTerminal();

		std::size_t symIdx = GetTableIndex(symTrans->GetId(), symIsTerm);
		if(symIsTerm)
		{
			seen_terminals.emplace(std::make_pair(
				symIdx, std::dynamic_pointer_cast<Terminal>(symTrans)));
		}

		// set table elements
		std::vector<std::vector<std::size_t>>* tab =
			symIsTerm ? &action_shift : &jump;

		const ClosurePtr& stateFrom = std::get<0>(transition);
		const ClosurePtr& stateTo = std::get<1>(transition);
		set_tab_elem((*tab)[stateFrom->GetId()], symIdx, stateTo->GetId());

		// set partial match table elements
		std::vector<std::vector<std::size_t>>* partials_rule_tab =
			symIsTerm ? &partials_rule_term : &partials_rule_nonterm;
		std::vector<std::vector<std::size_t>>* partials_matchlen_tab =
			symIsTerm ? &partials_matchlen_term : &partials_matchlen_nonterm;

		const Collection::t_elements& elemsFrom = std::get<3>(transition);
		std::unordered_map<std::size_t, ElementPtr> matching_rules;

		for(const ElementPtr& elemFrom : elemsFrom)
		{
			std::size_t match_len = elemFrom->GetCursor();
			std::optional<std::size_t> rulenr = elemFrom->GetSemanticRule();

			if(match_len == 0 || !rulenr)
				continue;

			if(auto iter_match = matching_rules.find(*rulenr);
				iter_match != matching_rules.end())
			{
				// longer match with the same rule?
				if(match_len > iter_match->second->GetCursor())
					iter_match->second = elemFrom;
			}
			else
			{
				// insert new match
				matching_rules.emplace(std::make_pair(*rulenr, elemFrom));
			}
		}

		// unique partial match?
		if(matching_rules.size() == 1)
		{
			set_tab_elem((*partials_rule_tab)[stateFrom->GetId()], symIdx,
				matching_rules.begin()->first);
			set_tab_elem((*partials_matchlen_tab)[stateFrom->GetId()], symIdx,
				matching_rules.begin()->second->GetCursor(), 0);
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

			std::optional<std::size_t> rulenr = elem->GetSemanticRule();

			if(!rulenr)  // no semantic rule assigned
			{
				std::cerr << "Error: No semantic rule assigned to element "
					<< (*elem) << "." << std::endl;
				continue;
			}

			set_tab_elem(numRhsSymsPerRule, *rulenr, elem->GetRhs()->NumSymbols(false), 0);
			set_tab_elem(ruleLhsIdx, *rulenr, GetTableIndex(elem->GetLhs()->GetId(), false), 0);

			auto& _reduce_row = action_reduce[closure->GetId()];
			for(const TerminalPtr& la : elem->GetLookaheads())
			{
				const std::size_t laIdx = GetTableIndex(la->GetId(), true);

				// in extended grammar, first production (rule 0) is of the form start -> ...
				if(*rulenr == 0)
					*rulenr = ACCEPT_VAL;

				// semantic rule number -> reduce table
				set_tab_elem(_reduce_row, laIdx, *rulenr);
			}
		}
	}


	// lalr(1) tables
	t_table tabActionShift = t_table{action_shift,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numTerminals};
	t_table tabActionReduce = t_table{action_reduce,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numTerminals};
	t_table tabJump = t_table{jump,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numNonTerminals};

	// partial match tables
	t_table tabPartialRuleTerm = t_table{partials_rule_term,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numTerminals};
	t_table tabPartialMatchLenTerm = t_table{partials_matchlen_term,
		ERROR_VAL, ACCEPT_VAL, 0, numStates, numTerminals};
	t_table tabPartialRuleNonterm = t_table{partials_rule_nonterm,
		ERROR_VAL, ACCEPT_VAL, ERROR_VAL, numStates, numNonTerminals};
	t_table tabPartialMatchLenNonterm = t_table{partials_matchlen_nonterm,
		ERROR_VAL, ACCEPT_VAL, 0, numStates, numNonTerminals};


	// check for and try to resolve shift/reduce conflicts
	std::size_t state = 0;
	for(const ClosurePtr& closure : m_collection)
	{
		std::optional<Terminal::t_terminalset> lookbacks;

		for(std::size_t termidx=0; termidx<numTerminals; ++termidx)
		{
			// get table entries
			std::size_t& shiftEntry = tabActionShift(state, termidx);
			std::size_t& reduceEntry = tabActionReduce(state, termidx);

			// partial match tables
			std::size_t& partialRuleEntry = tabPartialRuleTerm(state, termidx);
			std::size_t& partialMatchLenEntry = tabPartialMatchLenTerm(state, termidx);

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
						<< ")." << std::endl;

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

	if(!ok)
		return false;


	// save lalr(1) tables
	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	ofstr << "/*\n";
	ofstr << " * Parsing tables created on " << get_timestamp();
	ofstr << " using liblalr1 by Tobias Weber, 2020-2022.\n";
	ofstr << " * DOI: https://doi.org/10.5281/zenodo.6987396\n";
	ofstr << " */\n\n";

	ofstr << "#ifndef __LALR1_TABLES__\n";
	ofstr << "#define __LALR1_TABLES__\n\n";

	ofstr <<"namespace _lalr1_tables {\n\n";

	// save constants
	ofstr << "const constexpr std::size_t err = " << ERROR_VAL << ";\n";
	ofstr << "const constexpr std::size_t acc = " << ACCEPT_VAL << ";\n";
	ofstr << "const constexpr std::size_t eps = " << EPS_IDENT << ";\n";
	ofstr << "const constexpr std::size_t end = " << END_IDENT << ";\n";
	ofstr << "\n";

	// save lalr(1) tables
	tabActionShift.SaveCXXDefinition(ofstr, "tab_action_shift", "state", "terminal");
	tabActionReduce.SaveCXXDefinition(ofstr, "tab_action_reduce", "state", "lookahead");
	tabJump.SaveCXXDefinition(ofstr, "tab_jump", "state", "nonterminal");

	// save partial match tables
	tabPartialRuleTerm.SaveCXXDefinition(ofstr, "tab_partials_rule_term", "state", "terminal");
	tabPartialMatchLenTerm.SaveCXXDefinition(ofstr, "tab_partials_matchlen_term", "state", "terminal");
	tabPartialRuleNonterm.SaveCXXDefinition(ofstr, "tab_partials_rule_nonterm", "state", "nonterminal");
	tabPartialMatchLenNonterm.SaveCXXDefinition(ofstr, "tab_partials_matchlen_nonterm", "state", "nonterminal");

	// terminal symbol indices
	ofstr << "const t_mapIdIdx map_term_idx\n{{\n";
	for(const auto& [id, idx] : m_mapTermIdx)
	{
		ofstr << "\t{ ";
		if(id == EPS_IDENT)
			ofstr << "eps";
		else if(id == END_IDENT)
			ofstr << "end";
		else if(m_useOpChar && std::isprint(id))
			ofstr << "'" << char(id) << "'";
		else
			ofstr << id;
		ofstr << ", " << idx << " },\n";
	}
	ofstr << "}};\n\n";

	// non-terminal symbol indices
	ofstr << "const t_mapIdIdx map_nonterm_idx\n{{\n";
	for(const auto& [id, idx] : m_mapNonTermIdx)
		ofstr << "\t{ " << id << ", " << idx << " },\n";
	ofstr << "}};\n\n";

	// number of symbols on right-hand side of rule
	ofstr << "const t_vecIdx vec_num_rhs_syms{{ ";
	for(const auto& val : numRhsSymsPerRule)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	// index of lhs nonterminal in rule
	ofstr << "const t_vecIdx vec_lhs_idx{{ ";
	for(const auto& val : ruleLhsIdx)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	ofstr << "}\n\n\n";


	// lalr(1) tables getter
	ofstr << "static\nstd::tuple<const t_table*, const t_table*, const t_table*,\n";
	ofstr << "\tconst t_vecIdx*, const t_vecIdx*>\n";
	ofstr << "get_lalr1_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t&_lalr1_tables::tab_action_shift, &_lalr1_tables::tab_action_reduce, &_lalr1_tables::tab_jump,\n";
	ofstr << "\t\t&_lalr1_tables::vec_num_rhs_syms, &_lalr1_tables::vec_lhs_idx);\n";
	ofstr << "}\n\n";

	// partial match tables getter
	ofstr << "[[maybe_unused]] static\nstd::tuple<const t_table*, const t_table*,\n";
	ofstr << "\tconst t_table*, const t_table*>\n";
	ofstr << "get_lalr1_partials_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t&_lalr1_tables::tab_partials_rule_term, &_lalr1_tables::tab_partials_matchlen_term,\n";
	ofstr << "\t\t&_lalr1_tables::tab_partials_rule_nonterm, &_lalr1_tables::tab_partials_matchlen_nonterm);\n";
	ofstr << "}\n\n";

	// index maps getter
	ofstr << "static\nstd::tuple<const t_mapIdIdx*, const t_mapIdIdx*>\n";
	ofstr << "get_lalr1_table_indices()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t&_lalr1_tables::map_term_idx, &_lalr1_tables::map_nonterm_idx);\n";
	ofstr << "}\n\n";


	ofstr << "\n#endif" << std::endl;
	return true;
}
