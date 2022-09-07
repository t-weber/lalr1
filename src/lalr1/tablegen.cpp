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

	// create lalr(1) tables
	bool ok = true;
	std::vector<std::size_t> numRhsSymsPerRule{}; // number of symbols on rhs of a production rule
	std::vector<std::size_t> ruleLhsIdx{};        // nonterminal index of the rule's result type

	// partial match per rule number
	t_partialmatches partials{};                  // partial matches per closure
	partials.reserve(numStates);

	// lalr(1) tables
	std::vector<std::vector<std::size_t>> _action_shift, _action_reduce, _jump;
	_action_shift.resize(numStates);
	_action_reduce.resize(numStates);
	_jump.resize(numStates);

	for(std::size_t state=0; state<numStates; ++state)
	{
		_action_shift[state].reserve(m_mapTermIdx.size());
		_action_reduce[state].reserve(m_mapTermIdx.size());
		_jump[state].reserve(m_mapNonTermIdx.size());
	}


	// set a table item
	auto set_item = [](std::vector<std::size_t>& vec, std::size_t idx,
		std::size_t val, std::size_t filler = ERROR_VAL) -> void
	{
		if(vec.size() <= idx)
			vec.resize(idx+1, filler);
		vec[idx] = val;
	};


	// map terminal table index to terminal symbol
	std::unordered_map<std::size_t, TerminalPtr> seen_terminals{};


	for(const t_transition& tup : m_transitions)
	{
		const SymbolPtr& symTrans = std::get<2>(tup);
		if(symTrans->IsEps())
			continue;

		// terminal transitions -> shift table
		// nonterminal transition -> jump table
		const bool symIsTerm = symTrans->IsTerminal();

		std::vector<std::vector<std::size_t>>* tab =
			symIsTerm ? &_action_shift : &_jump;

		std::size_t symIdx = GetTableIndex(symTrans->GetId(), symIsTerm);
		if(symIsTerm)
		{
			seen_terminals.emplace(std::make_pair(
				symIdx, std::dynamic_pointer_cast<Terminal>(symTrans)));
		}

		const ClosurePtr& stateFrom = std::get<0>(tup);
		const ClosurePtr& stateTo = std::get<1>(tup);
		set_item((*tab)[stateFrom->GetId()], symIdx, stateTo->GetId());
	}

	for(const ClosurePtr& closure : m_collection)
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			std::optional<std::size_t> rulenr = elem->GetSemanticRule();

			// cursor at end -> reduce a completely parsed rule
			if(elem->IsCursorAtEnd())
			{
				if(!rulenr)  // no semantic rule assigned
				{
					std::cerr << "Error: No semantic rule assigned to element "
						<< (*elem) << "." << std::endl;
					continue;
				}

				set_item(numRhsSymsPerRule, *rulenr, elem->GetRhs()->NumSymbols(false), 0);
				set_item(ruleLhsIdx, *rulenr, GetTableIndex(elem->GetLhs()->GetId(), false), 0);

				auto& _action_row = _action_reduce[closure->GetId()];
				for(const TerminalPtr& la : elem->GetLookaheads())
				{
					std::size_t laIdx = GetTableIndex(la->GetId(), true);

					// in extended grammar, first production (rule 0) is of the form start -> ...
					if(*rulenr == 0)
						*rulenr = ACCEPT_VAL;

					// semantic rule number -> reduce table
					set_item(_action_row, laIdx, *rulenr);
				}
			}

			// cursor not at end -> partially parsed rule
			else
			{
				if(!rulenr)  // no semantic rule assigned
					continue;

				if(partials.size() <= closure->GetId())
					partials.resize(closure->GetId() + 1);
				t_partialmatch& partial = partials[closure->GetId()];

				auto update_match = [this, &elem](PartialMatch& match) -> void
				{
					const Terminal::t_terminalset& lookaheads = elem->GetLookaheads();

					match.matched_len = elem->GetCursor();
					match.lookahead_indices.clear();
					match.lookahead_indices.reserve(lookaheads.size());

					for(const TerminalPtr& la : lookaheads)
					{
						std::size_t laIdx = GetTableIndex(la->GetId(), true);
						match.lookahead_indices.push_back(laIdx);
					}
				};

				if(auto iter = partial.find(*rulenr); iter != partial.end())
				{
					// update existing partial match for this closure and rule
					PartialMatch& match = iter->second;

					// longer match with the same rule?
					if(elem->GetCursor() > match.matched_len)
						update_match(match);
				}
				else
				{
					// add a new partial match
					PartialMatch match;
					update_match(match);

					if(match.matched_len > 0)
						partial.emplace(std::make_pair(*rulenr, std::move(match)));
				}

				// TODO: save partial matches
			}
		}
	}


	const std::size_t numTerminals = m_mapTermIdx.size();
	const std::size_t numNonTerminals = m_mapNonTermIdx.size();

	t_table tabActionShift = t_table{_action_shift, ERROR_VAL, ACCEPT_VAL, numStates, numTerminals};
	t_table tabActionReduce = t_table{_action_reduce, ERROR_VAL, ACCEPT_VAL, numStates, numTerminals};
	t_table tabJump = t_table{_jump, ERROR_VAL, ACCEPT_VAL, numStates, numNonTerminals};

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
						<< " for closure " << state;
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
					ostrErr << " (can either shift to closure " << shiftEntry
						<< " or reduce using rule " << reduceEntry
						<< ")." << std::endl;

					if(m_stopOnConflicts)
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

	ofstr << "/*\n";
	ofstr << " * Parsing tables created using liblalr1 by Tobias Weber, 2020-2022.\n";
	ofstr << " * DOI: https://doi.org/10.5281/zenodo.6987396\n";
	ofstr << " */\n\n";

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
	ofstr << "const t_mapIdIdx map_nonterm_idx{{\n";
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

	// partial matches
	ofstr << "const t_partialmatches vec_partials{{\n";
	for(const t_partialmatch& partial : partials)
	{
		ofstr << "\tt_partialmatch\n\t{\n";
		for(const auto& [rulenr, partialmatch] : partial)
		{
			ofstr << "\t\tstd::make_pair(" << rulenr << ", ";
			ofstr << "PartialMatch{ .matched_len=" << partialmatch.matched_len << ", ";
			ofstr << ".lookahead_indices={{ ";
			for(std::size_t laidx : partialmatch.lookahead_indices)
				ofstr << laidx << ", ";
			ofstr << "}}";
			ofstr << " }),\n";
		}
		ofstr << "\t},\n";
	}
	ofstr << "}};\n\n";

	ofstr << "}\n\n\n";


	ofstr << "static std::tuple<const t_table*, const t_table*, const t_table*,\n"
		<< "\tconst t_vecIdx*, const t_vecIdx*>\n";
	ofstr << "get_lalr1_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n"
		<< "\t\t&_lr1_tables::tab_action_shift, &_lr1_tables::tab_action_reduce, &_lr1_tables::tab_jump,\n"
		<< "\t\t&_lr1_tables::vec_num_rhs_syms, &_lr1_tables::vec_lhs_idx);\n";
	ofstr << "}\n\n";

	ofstr << "static std::tuple<const t_mapIdIdx*, const t_mapIdIdx*>\n";
	ofstr << "get_lalr1_table_indices()\n{\n";
	ofstr << "\treturn std::make_tuple(\n"
		<< "\t\t&_lr1_tables::map_term_idx, &_lr1_tables::map_nonterm_idx);\n";
	ofstr << "}\n\n";


	ofstr << "\n#endif" << std::endl;
	return true;
}
