/**
 * generates an lalr(1) parser from a collection
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date aug-2022
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
#include "options.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>


/**
 * export an explicit recursive ascent parser
 * @see https://doi.org/10.1016/0020-0190(88)90061-0
 * @see https://en.wikipedia.org/wiki/Recursive_ascent_parser
 */
bool Collection::SaveParser(const std::string& filename_cpp, const std::string& class_name) const
{
	// output header file stub
	std::string outfile_h = R"raw(/*
 * Parser created on %%TIME_STAMP%% using liblalr1 by Tobias Weber, 2020-2022.
 * DOI: https://doi.org/10.5281/zenodo.6987396
 */
#ifndef __LALR1_PARSER_REC_ASC_H__
#define __LALR1_PARSER_REC_ASC_H__

#include "lalr1/ast.h"
#include "lalr1/common.h"

#include <vector>
#include <stack>

class %%PARSER_CLASS%%
{
public:
	using t_token = t_toknode;                 // token data type
	using t_symbol = t_lalrastbaseptr;         // symbol data type

	%%PARSER_CLASS%%() = default;
	~%%PARSER_CLASS%%() = default;
	%%PARSER_CLASS%%(const %%PARSER_CLASS%%&) = delete;
	%%PARSER_CLASS%%& operator=(const %%PARSER_CLASS%%&) = delete;

	void SetDebug(bool b);
	void DebugMessageState(std::size_t state_id, const char* state_func) const;
	void DebugMessageReturn(std::size_t state_id) const;
	void DebugMessageReduce(std::size_t num_rhs, std::size_t rulenr, const char* rule_descr) const;
	void TransitionError(std::size_t state_id) const;

	void SetSemanticRules(const std::vector<t_semanticrule>* rules);
	t_symbol Parse(const std::vector<t_token>& input);

protected:
	void PrintSymbols() const;
	void GetNextLookahead();

%%DECLARE_CLOSURES%%

private:
	// semantic rules
	const std::vector<t_semanticrule>* m_semantics{};
	const std::vector<t_token>* m_input{};     // input tokens
	std::stack<t_symbol> m_symbols{};          // currently active symbols

	t_token m_lookahead{nullptr};              // lookahead token
	std::size_t m_lookahead_id{0};             // lookahead identifier
	int m_lookahead_idx{-1};                   // index into input token array

	bool m_debug{false};                       // output debug infos
	bool m_accepted{false};                    // input was accepted

	// number of function returns between reduction and performing jump / non-terminal transition
	std::size_t m_dist_to_jump{0};

	// end symbol id
	static constexpr const std::size_t s_end_id{%%END_ID%%};
};

#endif)raw";


	// output cpp file stub
	std::string outfile_cpp = R"raw(/*
 * Parser created on %%TIME_STAMP%% using liblalr1 by Tobias Weber, 2020-2022.
 * DOI: https://doi.org/10.5281/zenodo.6987396
 */
%%INCLUDE_HEADER%%
#include <exception>
#include <string>
#include <iostream>
#include <sstream>

void %%PARSER_CLASS%%::PrintSymbols() const
{
	std::stack<t_symbol> symbols = m_symbols;

	std::cout << symbols.size() << " symbols: ";
	while(symbols.size())
	{
		std::cout << symbols.top()->GetId() << ", ";
		symbols.pop();
	}
	std::cout << std::endl;
}

void %%PARSER_CLASS%%::GetNextLookahead()
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

void %%PARSER_CLASS%%::SetDebug(bool b)
{
	m_debug = b;
}

void %%PARSER_CLASS%%::DebugMessageState(std::size_t state_id, const char* state_name) const
{
	if(m_debug)
	{
		std::cout << "\nRunning state " << state_id
			<< " function \"" << state_name << "\"..."
			<< std::endl;
		if(m_lookahead)
		{
			std::cout << "Lookahead [\"" << m_lookahead_idx << "\"]: \""
				<< m_lookahead_id
				<< std::endl;
		}
		PrintSymbols();
	}
}

void %%PARSER_CLASS%%::DebugMessageReturn(std::size_t state_id) const
{
	if(m_debug)
	{
		std::cout << "Returning from state " << state_id
			<< ", distance to jump: " << m_dist_to_jump << "."
			<< std::endl;
	}
}

void %%PARSER_CLASS%%::DebugMessageReduce(std::size_t num_rhs,
	std::size_t rulenr, const char* rule_descr) const
{
	if(m_debug)
	{
		std::cout << "Reducing " << num_rhs
			<< " symbol(s) using rule " << rulenr
			<< " (" << rule_descr << ")."
			<< std::endl;
	}
}

void %%PARSER_CLASS%%::TransitionError(std::size_t state_id) const
{
	std::ostringstream ostr;
	ostr << "No transition from state " << state_id << ", ";

	if(m_symbols.size())
	{
		const t_symbol& topsym = m_symbols.top();
		bool is_term = topsym->IsTerminal();
		std::size_t sym_id = topsym->GetId();

		ostr << "top-level " << (is_term ? "terminal" : "non-terminal")
			<< " " << sym_id << ", ";
	}

	ostr << "and look-ahead terminal " << m_lookahead_id << ".";

	throw std::runtime_error(ostr.str());
}

void %%PARSER_CLASS%%::SetSemanticRules(const std::vector<t_semanticrule>* rules)
{
	m_semantics = rules;
}

%%PARSER_CLASS%%::t_symbol %%PARSER_CLASS%%::Parse(const std::vector<t_token>& input)
{
	m_input = &input;
	m_lookahead_idx = -1;
	m_lookahead_id = 0;
	m_lookahead = nullptr;
	m_dist_to_jump = 0;
	m_accepted = false;
	while(!m_symbols.empty())
		m_symbols.pop();

	GetNextLookahead();
	state_0();

	if(m_symbols.size() && m_accepted)
		return m_symbols.top();
	return nullptr;
}

%%DEFINE_CLOSURES%%)raw";


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
	bool use_col_saved = g_options.GetUseColour();

	std::ostringstream ostr_h, ostr_cpp;
	g_options.SetUseColour(false);

	for(const ClosurePtr& closure : m_collection)
	{
		std::optional<Terminal::t_terminalset> lookbacks;

		// write comment
		ostr_cpp << "/*\n" << *closure;

		if(Terminal::t_terminalset lookbacks = GetLookbackTerminals(closure);
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

				ostr_cpp << "\t- to state " << closure_to->GetId()
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

				ostr_cpp << "\t- to state " << closure_to->GetId()
					<< " via symbol " << symTrans->GetStrId()
					<< " (id = " << symTrans->GetId() << ")"
					<< "\n";
			}
		}

		ostr_cpp << "*/\n";  // end comment


		// write closure function
		ostr_cpp << "void " << class_name << "::state_" << closure->GetId() << "()\n";
		ostr_cpp << "{\n";

		if(m_genDebugCode)
		{
			ostr_cpp << "\tif(m_debug)\n";
			ostr_cpp << "\t\tDebugMessageState(" << closure->GetId()
				<< ", __PRETTY_FUNCTION__);\n";
		}


		// shift actions
		std::unordered_map<std::size_t, std::string> shifts;

		for(const t_transition& transition : GetTransitions(closure, true))
		{
			const ClosurePtr& closure_to = std::get<1>(transition);
			const SymbolPtr& symTrans = std::get<2>(transition);
			//const Collection::t_elements& elemsFrom = std::get<3>(transition);

			if(symTrans->IsEps() || !symTrans->IsTerminal())
				continue;

			std::ostringstream ostr_shift;
			if(symTrans->GetId() == g_end->GetId())
			{
				ostr_shift << "\t\tcase s_end_id:\n";
			}
			else if(m_useOpChar && std::isprint(symTrans->GetId()))
			{
				ostr_shift << "\t\tcase \'"
					<< char(symTrans->GetId()) << "\':\n";
			}
			else
			{
				ostr_shift << "\t\tcase " << symTrans->GetId() << ": // "
					<< symTrans->GetStrId() << "\n";
			}
			ostr_shift << "\t\t{\n";
			ostr_shift << "\t\t\tm_symbols.push(m_lookahead);\n";
			ostr_shift << "\t\t\tGetNextLookahead();\n";
			ostr_shift << "\t\t\tstate_" << closure_to->GetId() << "();\n";
			ostr_shift << "\t\t\tbreak;\n";
			ostr_shift << "\t\t}\n";  // end case

			shifts.emplace(std::make_pair(symTrans->GetId(), ostr_shift.str()));
		}


		// reduce actions
		std::vector<std::unordered_set<SymbolPtr>> reduces_lookaheads;
		std::vector<std::string> reduces;

		for(const ElementPtr& elem : closure->GetElements())
		{
			std::optional<std::size_t> rulenr = elem->GetSemanticRule();

			// cursor at end -> reduce a completely parsed rule
			if(elem->IsCursorAtEnd())
			{
				if(!rulenr)
				{
					std::cerr << "Error: No semantic rule assigned to element "
						<< (*elem) << "." << std::endl;
					continue;
				}

				const Terminal::t_terminalset& lookaheads = elem->GetLookaheads();
				if(lookaheads.size())
				{
					std::ostringstream ostr_reduce;

					std::unordered_set<SymbolPtr> reduce_lookaheads;
					for(const TerminalPtr& la : lookaheads)
						reduce_lookaheads.insert(la);
					reduces_lookaheads.emplace_back(std::move(reduce_lookaheads));
					ostr_reduce << "\t\t{\n";

					// in extended grammar, first production (rule 0) is of the form start -> ...
					if(*rulenr == 0)
					{
						ostr_reduce << "\t\t\tm_accepted = true;\n";
						ostr_reduce << "\t\t\tbreak;\n";
					}
					else
					{
						std::ostringstream rule_descr;
						rule_descr << (*elem->GetLhs()) << " -> " << (*elem->GetRhs());

						std::size_t num_rhs = elem->GetRhs()->NumSymbols(false);
						ostr_reduce << "\t\t\tm_dist_to_jump = " << num_rhs << ";\n";

						if(m_genDebugCode)
						{
							ostr_reduce << "\t\t\tif(m_debug)\n";
							ostr_reduce << "\t\t\t\tDebugMessageReduce("
								<< num_rhs << ", "
								<< *rulenr << ", "
								<< "\"" << rule_descr.str() << "\");\n";
						}


						// take the symbols from the stack and create an argument vector for the semantic rule
						ostr_reduce << "\t\t\tstd::vector<t_symbol> args(" << num_rhs << ");\n";
						if(num_rhs)
						{
							ostr_reduce << "\t\t\tfor(std::size_t arg=0; arg<" << num_rhs << "; ++arg)\n";
							ostr_reduce << "\t\t\t{\n";
							ostr_reduce << "\t\t\t\targs[" << num_rhs << "-arg-1] = std::move(m_symbols.top());\n";
							ostr_reduce << "\t\t\t\tm_symbols.pop();\n";
							ostr_reduce << "\t\t\t}\n";  // end for
						}

						// execute semantic rule
						ostr_reduce << "\t\t\t// semantic rule: " << rule_descr.str() << ".\n";
						ostr_reduce << "\t\t\tm_symbols.emplace((*m_semantics)[" << *rulenr << "](true, args));\n";
						ostr_reduce << "\t\t\tbreak;\n";
					}

					ostr_reduce << "\t\t}\n";  // end case

					reduces.emplace_back(ostr_reduce.str());
				}
			}

			// cursor not at end -> partially parsed rule
			else
			{
				if(!rulenr)  // no semantic rule assigned
					continue;

				// TODO
			}
		}


		// try to solve shift/reduce conflicts
		for(std::size_t reduce_idx = 0; reduce_idx < reduces_lookaheads.size(); ++reduce_idx)
		{
			auto& reduce_lookaheads = reduces_lookaheads[reduce_idx];

			for(auto iter_la = reduce_lookaheads.begin(); iter_la != reduce_lookaheads.end();)
			{
				bool already_incremented_la = false;
				const SymbolPtr& la = *iter_la;

				// does the same token also exist in the shifts?
				auto iter_shift = shifts.find(la->GetId());
				if(iter_shift == shifts.end())
				{
					++iter_la;
					continue;
				}

				if(ElementPtr conflictelem = closure->GetElementWithCursorAtSymbol(la); conflictelem)
				{
					if(!lookbacks)
						lookbacks = GetLookbackTerminals(closure);

					std::size_t shift_val = 0, reduce_val = 0;  // dummy values (only need to check for ERROR_VAL)
					if(SolveConflict(la, *lookbacks, &shift_val, &reduce_val))
					{
						if(shift_val == ERROR_VAL && reduce_val != ERROR_VAL)
						{
							// keep reduce, remove shift
							iter_shift = shifts.erase(iter_shift);
						}
						else if(shift_val != ERROR_VAL && reduce_val == ERROR_VAL)
						{
							// keep shift, remove reduce
							iter_la = reduce_lookaheads.erase(iter_la);
							already_incremented_la = true;
						}
					}
					else
					{
						std::ostringstream ostrErr;
						ostrErr << "Shift/reduce conflict detected"
							<< " in state " << closure->GetId();
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
						ostrErr << " and look-ahead terminal " << la->GetId();
						ostrErr << ".";

						if(m_stopOnConflicts)
							throw std::runtime_error(ostrErr.str());
						else
							std::cerr << ostrErr.str() << std::endl;
					}
				}

				if(!already_incremented_la)
					++iter_la;
			}
		}


		// write shift and reduce codes
		ostr_cpp << "\tswitch(m_lookahead_id)\n";
		ostr_cpp << "\t{\n";

		for(const auto& shift : shifts)
			ostr_cpp << shift.second;

		for(std::size_t reduce_idx = 0; reduce_idx < reduces_lookaheads.size(); ++reduce_idx)
		{
			const auto& reduce_lookaheads = reduces_lookaheads[reduce_idx];
			const auto& reduce = reduces[reduce_idx];

			if(!reduce_lookaheads.size())
				continue;

			for(const SymbolPtr& la : reduce_lookaheads)
			{
				if(la->GetId() == g_end->GetId())
				{
					ostr_cpp << "\t\tcase s_end_id:\n";
				}
				else if(m_useOpChar && std::isprint(la->GetId()))
				{
					ostr_cpp << "\t\tcase \'"
						<< char(la->GetId())
						<< "\':\n";
				}
				else
				{
					ostr_cpp << "\t\tcase " << la->GetId() << ": // "
						<< la->GetStrId() << "\n";
				}
			}
			ostr_cpp << reduce;
		}

		if(m_genErrorCode)
		{
			// default: error
			ostr_cpp << "\t\tdefault:\n";
			ostr_cpp << "\t\t{\n";
			ostr_cpp << "\t\t\tTransitionError(" << closure->GetId() << ");\n";
			ostr_cpp << "\t\t\tbreak;\n";
			ostr_cpp << "\t\t}\n";
		}

		ostr_cpp << "\t}\n";        // end switch


		// jump to new closure
		std::ostringstream ostr_cpp_while;
		ostr_cpp_while << "\twhile(!m_dist_to_jump && m_symbols.size() && !m_accepted)\n";
		ostr_cpp_while << "\t{\n";

		ostr_cpp_while << "\t\tconst t_symbol& topsym = m_symbols.top();\n";
		ostr_cpp_while << "\t\tif(topsym->IsTerminal())\n\t\t\tbreak;\n";

		ostr_cpp_while << "\t\tswitch(topsym->GetId())\n";
		ostr_cpp_while << "\t\t{\n";

		bool while_has_entries = false;
		for(const t_transition& transition : GetTransitions(closure, false))
		{
			const ClosurePtr& closure_to = std::get<1>(transition);
			const SymbolPtr& symTrans = std::get<2>(transition);
			//const Collection::t_elements& elemsFrom = std::get<3>(transition);

			if(symTrans->IsEps() || symTrans->IsTerminal())
				continue;

			ostr_cpp_while << "\t\t\tcase " << symTrans->GetId() << ":\n";
			ostr_cpp_while << "\t\t\t\tstate_" << closure_to->GetId() << "();\n";
			ostr_cpp_while << "\t\t\t\tbreak;\n";

			while_has_entries = true;
		}

		if(m_genErrorCode)
		{
			ostr_cpp_while << "\t\t\tdefault:\n";
			ostr_cpp_while << "\t\t\t\tTransitionError(" << closure->GetId() << ");\n";
			ostr_cpp_while << "\t\t\t\tbreak;\n";
		}

		ostr_cpp_while << "\t\t}\n";  // end switch

		ostr_cpp_while << "\t}\n";    // end while
		if(while_has_entries)
			ostr_cpp << ostr_cpp_while.str();

		// return from closure -> decrement distance counter
		ostr_cpp << "\tif(m_dist_to_jump > 0)\n\t\t--m_dist_to_jump;\n";

		if(m_genDebugCode)
		{
			ostr_cpp << "\tif(m_debug)\n";
			ostr_cpp << "\t\tDebugMessageReturn(" << closure->GetId() << ");\n";
		}

		// end closure function
		ostr_cpp << "}\n\n";

		ostr_h << "\tvoid state_" << closure->GetId() << "();\n";
	}


	// write output files
	std::string incl = "#include \"" + filename_h + "\"";
	std::string time_stamp = get_timestamp();

	boost::replace_all(outfile_cpp, "%%PARSER_CLASS%%", class_name);
	boost::replace_all(outfile_h, "%%PARSER_CLASS%%", class_name);
	boost::replace_all(outfile_cpp, "%%INCLUDE_HEADER%%", incl);
	boost::replace_all(outfile_cpp, "%%DEFINE_CLOSURES%%", ostr_cpp.str());
	boost::replace_all(outfile_h, "%%DECLARE_CLOSURES%%", ostr_h.str());
	boost::replace_all(outfile_h, "%%END_ID%%", std::to_string(g_end->GetId()));
	boost::replace_all(outfile_cpp, "%%TIME_STAMP%%", time_stamp);
	boost::replace_all(outfile_h, "%%TIME_STAMP%%", time_stamp);

	file_cpp << outfile_cpp << std::endl;
	file_h << outfile_h << std::endl;

	// restore options
	g_options.SetUseColour(use_col_saved);
	return true;
}
