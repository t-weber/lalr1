/**
 * lalr(1) collection -- generate an lalr(1) parser
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

#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>


/**
 * export lalr(1) tables to C++ code
 */
bool Collection::SaveParseTables(const std::string& file, bool stopOnConflicts) const
{
	// create lalr(1) tables
	bool ok = true;
	std::vector<std::size_t> numRhsSymsPerRule{}; // number of symbols on rhs of a production rule
	std::vector<std::size_t> vecRuleLhsIdx{};     // nonterminal index of the rule's result type

	const std::size_t numStates = m_collection.size();

	// lalr(1) tables
	std::vector<std::vector<std::size_t>> _action_shift, _action_reduce, _jump;
	_action_shift.resize(numStates);
	_action_reduce.resize(numStates);
	_jump.resize(numStates);

	// map terminal table index to terminal symbol
	std::unordered_map<std::size_t, TerminalPtr> seen_terminals{};


	for(const t_transition& tup : m_transitions)
	{
		const ClosurePtr& stateFrom = std::get<0>(tup);
		const ClosurePtr& stateTo = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);

		bool symIsTerm = symTrans->IsTerminal();
		bool symIsEps = symTrans->IsEps();

		if(symIsEps)
			continue;

		// terminal transitions -> shift table
		// nonterminal transition -> jump table
		std::vector<std::vector<std::size_t>>* tab =
			symIsTerm ? &_action_shift : &_jump;

		std::size_t symIdx = GetTableIndex(symTrans->GetId(), symIsTerm);
		if(symIsTerm)
		{
			seen_terminals.emplace(std::make_pair(
				symIdx, std::dynamic_pointer_cast<Terminal>(symTrans)));
		}

		auto& _tab_row = (*tab)[stateFrom->GetId()];
		if(_tab_row.size() <= symIdx)
			_tab_row.resize(symIdx+1, ERROR_VAL);
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
			if(vecRuleLhsIdx.size() <= rule)
				vecRuleLhsIdx.resize(rule+1);
			numRhsSymsPerRule[rule] = elem->GetRhs()->NumSymbols(false);
			vecRuleLhsIdx[rule] = GetTableIndex(elem->GetLhs()->GetId(), false);

			auto& _action_row = _action_reduce[closure->GetId()];

			for(const auto& la : elem->GetLookaheads())
			{
				std::size_t laIdx = GetTableIndex(la->GetId(), true);

				// in extended grammar, first production (rule 0) is of the form start -> ...
				if(rule == 0)
					rule = ACCEPT_VAL;

				// semantic rule number -> reduce table
				if(_action_row.size() <= laIdx)
					_action_row.resize(laIdx+1, ERROR_VAL);
				_action_row[laIdx] = rule;
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
		std::optional<std::vector<TerminalPtr>> lookbacks;

		for(std::size_t termidx=0; termidx<numTerminals; ++termidx)
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
				conflictelem = closure->
					GetElementWithCursorAtSymbol(termiter->second);
				if(conflictelem)
					sym_at_cursor = conflictelem->GetSymbolAtCursor();
			}

			// both have an entry?
			if(shiftEntry!=ERROR_VAL && reduceEntry!=ERROR_VAL)
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
	for(const auto& [id, idx] : m_mapTermIdx)
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
	for(const auto& [id, idx] : m_mapNonTermIdx)
		ofstr << "\t{" << id << ", " << idx << "},\n";
	ofstr << "}};\n\n";

	// number of symbols on right-hand side of rule
	ofstr << "const t_vecIdx vec_num_rhs_syms{{ ";
	for(const auto& val : numRhsSymsPerRule)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	// index of lhs nonterminal in rule
	ofstr << "const t_vecIdx vec_lhs_idx{{ ";
	for(const auto& val : vecRuleLhsIdx)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	ofstr << "}\n\n\n";


	ofstr << "static std::tuple<const t_table*, const t_table*, const t_table*,\n"
		<< "\tconst t_mapIdIdx*, const t_mapIdIdx*, const t_vecIdx*, const t_vecIdx*>\n";
	ofstr << "get_lalr1_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n"
		<< "\t\t&_lr1_tables::tab_action_shift, &_lr1_tables::tab_action_reduce, &_lr1_tables::tab_jump,\n"
		<< "\t\t&_lr1_tables::map_term_idx, &_lr1_tables::map_nonterm_idx, &_lr1_tables::vec_num_rhs_syms,\n"
		<< "\t\t&_lr1_tables::vec_lhs_idx);\n";
	ofstr << "}\n\n";


	ofstr << "\n#endif" << std::endl;
	return true;
}


/**
 * export an explicit recursive ascent parser
 * @see https://doi.org/10.1016/0020-0190(88)90061-0
 * @see https://en.wikipedia.org/wiki/Recursive_ascent_parser
 */
bool Collection::SaveParser(const std::string& filename_cpp) const
{
	// output header file stub
	std::string outfile_h = R"raw(
#ifndef __LR1_PARSER_REC_ASC_H__
#define __LR1_PARSER_REC_ASC_H__

#include "lalr1/ast.h"
#include "lalr1/common.h"

#include <stack>

class ParserRecAsc
{
public:
	ParserRecAsc(const std::vector<t_semanticrule>* rules);
	ParserRecAsc() = delete;
	ParserRecAsc(const ParserRecAsc&) = delete;
	ParserRecAsc& operator=(const ParserRecAsc&) = delete;

	t_lalrastbaseptr Parse(const std::vector<t_toknode>* input);

protected:
	void PrintSymbols() const;
	void GetNextLookahead();

%%DECLARE_CLOSURES%%

private:
	// semantic rules
	const std::vector<t_semanticrule>* m_semantics{nullptr};

	// input tokens
	const std::vector<t_toknode>* m_input{nullptr};

	// lookahead token
	t_toknode m_lookahead{nullptr};

	// lookahead identifier
	std::size_t m_lookahead_id{0};

	// index into input token array
	int m_lookahead_idx{-1};

	// currently active symbols
	std::stack<t_lalrastbaseptr> m_symbols{};

	// number of function returns between reduction and performing jump / non-terminal transition
	std::size_t m_dist_to_jump{0};

	// input was accepted
	bool m_accepted{false};
};

#endif
)raw";


	// output cpp file stub
	std::string outfile_cpp = R"raw(
%%INCLUDE_HEADER%%

ParserRecAsc::ParserRecAsc(const std::vector<t_semanticrule>* rules)
	: m_semantics{rules}
{}

void ParserRecAsc::PrintSymbols() const
{
	std::stack<t_lalrastbaseptr> symbols = m_symbols;

	std::cout << symbols.size() << " symbols: ";

	while(symbols.size())
	{
		t_lalrastbaseptr sym = symbols.top();
		symbols.pop();

		std::cout << sym->GetId() << ", ";
	}
	std::cout << std::endl;
}

void ParserRecAsc::GetNextLookahead()
{
	++m_lookahead_idx;
	if(m_lookahead_idx >= int(m_input->size()) || m_lookahead_idx < 0)
	{
		m_lookahead = nullptr;
		m_lookahead_id = 0;
	}
	else
	{
		m_lookahead = (*m_input)[m_lookahead_idx];
		m_lookahead_id = m_lookahead->GetId();
	}
}

t_lalrastbaseptr ParserRecAsc::Parse(const std::vector<t_toknode>* input)
{
	m_input = input;
	m_lookahead_idx = -1;
	m_lookahead_id = 0;
	m_lookahead = nullptr;
	m_dist_to_jump = 0;
	m_accepted = false;
	while(!m_symbols.empty())
		m_symbols.pop();

	GetNextLookahead();
	closure_0();

	if(m_symbols.size() && m_accepted)
		return m_symbols.top();
	return nullptr;
}

%%DEFINE_CLOSURES%%
)raw";


	std::string filename_h = boost::replace_last_copy(filename_cpp, ".cpp", ".h");

	// open output files
	std::ofstream file_cpp(filename_cpp);
	std::ofstream file_h(filename_h);

	if(!file_cpp || !file_h)
	{
		std::cerr << "Cannot open output files \"" << filename_cpp
		<< "\" and \"" << filename_h << "\"." << std::endl;
		return false;
	}


	// generate closures
	std::ostringstream ostr_h, ostr_cpp;


	for(const ClosurePtr& closure : m_collection)
	{
		// write comment
		ostr_cpp << "/*\n" << *closure;

		if(std::vector<TerminalPtr> lookbacks = GetLookbackTerminals(closure);
			lookbacks.size())
		{
			ostr_cpp << "Lookback terminals:";
			for(const TerminalPtr& lookback : lookbacks)
				ostr_cpp << " " << lookback->GetStrId();
			ostr_cpp << "\n";
		}

		if(t_transitions terminal_transitions = GetTransitions(closure, true);
			terminal_transitions.size())
		{
			ostr_cpp << "Terminal transitions:\n";

			for(const t_transition& transition : terminal_transitions)
			{
				const ClosurePtr& closure_to = std::get<1>(transition);
				const SymbolPtr& symTrans = std::get<2>(transition);

				ostr_cpp << "\t- to closure " << closure_to->GetId()
					<< " via symbol " << symTrans->GetStrId()
					<< " (id = " << symTrans->GetId() << ")"
					<< "\n";
			}
		}

		if(t_transitions nonterminal_transitions = GetTransitions(closure, false);
			nonterminal_transitions.size())
		{
			ostr_cpp << "Non-Terminal transitions:\n";

			for(const t_transition& transition : nonterminal_transitions)
			{
				const ClosurePtr& closure_to = std::get<1>(transition);
				const SymbolPtr& symTrans = std::get<2>(transition);

				ostr_cpp << "\t- to closure " << closure_to->GetId()
					<< " via symbol " << symTrans->GetStrId()
					<< " (id = " << symTrans->GetId() << ")"
					<< "\n";
			}
		}

		ostr_cpp << "*/\n";  // end comment


		// write closure function
		ostr_cpp << "void ParserRecAsc::closure_" << closure->GetId() << "()\n";
		ostr_cpp << "{\n";

		// shift actions
		ostr_cpp << "\tswitch(m_lookahead_id)\n";
		ostr_cpp << "\t{\n";

		for(const t_transition& transition : GetTransitions(closure, true))
		{
			const ClosurePtr& closure_to = std::get<1>(transition);
			const SymbolPtr& symTrans = std::get<2>(transition);
			if(symTrans->IsEps() || !symTrans->IsTerminal())
				continue;

			ostr_cpp << "\t\tcase " << symTrans->GetId() << ":\n";
			ostr_cpp << "\t\t{\n";
			ostr_cpp << "\t\t\tm_symbols.push(m_lookahead);\n";
			ostr_cpp << "\t\t\tGetNextLookahead();\n";
			ostr_cpp << "\t\t\tclosure_" << closure_to->GetId() << "();\n";
			ostr_cpp << "\t\t\tbreak;\n";
			ostr_cpp << "\t\t}\n";  // end case
		}

		// TODO: default: error
		ostr_cpp << "\t}\n";        // end switch


		// TODO: reduce actions


		// jump to new closurel
		std::ostringstream ostr_cpp_while;
		ostr_cpp_while << "\twhile(!m_dist_to_jump && m_symbols.size() && !m_accepted)\n";
		ostr_cpp_while << "\t{\n";

		ostr_cpp_while << "\t\tt_lalrastbaseptr topsym = m_symbols.top();\n";
		ostr_cpp_while << "\t\tif(topsym->IsTerminal())\n\t\t\tbreak;\n";
		ostr_cpp_while << "\t\tstd::size_t topsym_id = topsym->GetId();\n";

		ostr_cpp_while << "\t\tswitch(topsym_id)\n";
		ostr_cpp_while << "\t\t{\n";

		bool while_has_entries = false;
		for(const t_transition& transition : GetTransitions(closure, false))
		{
			const ClosurePtr& closure_to = std::get<1>(transition);
			const SymbolPtr& symTrans = std::get<2>(transition);
			if(symTrans->IsEps() || symTrans->IsTerminal())
				continue;

			ostr_cpp_while << "\t\t\tcase " << symTrans->GetId() << ":\n";
			ostr_cpp_while << "\t\t\t\tclosure_" << closure_to->GetId() << "();\n";
			ostr_cpp_while << "\t\t\t\tbreak;\n";

			while_has_entries = true;
		}

		// TODO: default: error
		ostr_cpp_while << "\t\t}\n";  // end switch

		ostr_cpp_while << "\t}\n";    // end while
		if(while_has_entries)
			ostr_cpp << ostr_cpp_while.str();

		// return from closure -> decrement distance counter
		ostr_cpp << "\tif(m_dist_to_jump > 0)\n\t\t--m_dist_to_jump;\n";


		// end closure function
		ostr_cpp << "}\n\n\n";

		ostr_h << "\tvoid closure_" << closure->GetId() << "();\n";
	}


	// write output files
	std::string incl = "#include \"" + filename_h + "\"";
	boost::replace_all(outfile_cpp, "%%INCLUDE_HEADER%%", incl);
	boost::replace_all(outfile_cpp, "%%DEFINE_CLOSURES%%", ostr_cpp.str());
	boost::replace_all(outfile_h, "%%DECLARE_CLOSURES%%", ostr_h.str());

	file_cpp << outfile_cpp << std::endl;
	file_h << outfile_h << std::endl;

	return true;
}
