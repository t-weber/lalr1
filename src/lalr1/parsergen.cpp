/**
 * generates an lalr(1) recursive ascent parser from a collection
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date aug-2022
 * @license see 'LICENSE' file
 *
 * Principal reference:
 *	- https://doi.org/10.1016/0020-0190(88)90061-0
 * Further references:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/lr1.html
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/slr1table.html
 *	- https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 *	- https://en.wikipedia.org/wiki/LR_parser
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
#include "lalr1/stack.h"

#include <vector>
#include <unordered_set>

class %%PARSER_CLASS%%
{
public:
	using t_token = t_toknode;                    // token data type
	using t_symbol = t_lalrastbaseptr;            // symbol data type
	using t_stack = ParseStack<t_symbol>;         // symbol stack

	%%PARSER_CLASS%%() = default;
	~%%PARSER_CLASS%%() = default;
	%%PARSER_CLASS%%(const %%PARSER_CLASS%%&) = delete;
	%%PARSER_CLASS%%& operator=(const %%PARSER_CLASS%%&) = delete;

	void SetDebug(bool b);

	void SetSemanticRules(const std::vector<t_semanticrule>* rules);
	t_symbol Parse(const std::vector<t_token>& input);

protected:
	void PrintSymbols() const;
	void GetNextLookahead();
	void PushLookahead();

	static t_semanticargs GetArguments(t_stack& symbols, std::size_t num_rhs);
	t_semanticargs GetCopyArguments(std::size_t num_rhs) const;

	std::size_t GetPartialRuleHash(std::size_t rule, std::size_t len, std::size_t state_hash) const;
	void ApplyPartialRule(std::size_t rule_nr, std::size_t rule_len, std::size_t state_hash);
	void ApplyRule(std::size_t rule_nr, std::size_t rule_len);

	void DebugMessageState(std::size_t state_id, const char* state_func, std::size_t state_hash) const;
	void DebugMessageReturn(std::size_t state_id) const;
	void DebugMessageReduce(std::size_t num_rhs, std::size_t rulenr, const char* rule_descr) const;
	void DebugMessagePartialRule(std::size_t rulelen, std::size_t rulenr, std::size_t state_hash) const;
	void TransitionError(std::size_t state_id) const;

%%DECLARE_CLOSURES%%
private:
	// semantic rules
	const std::vector<t_semanticrule>* m_semantics{};
	const std::vector<t_token>* m_input{};        // input tokens
	t_stack m_symbols{};                          // currently active symbols

	t_token m_lookahead{nullptr};                 // lookahead token
	std::size_t m_lookahead_id{0};                // lookahead identifier
	int m_lookahead_idx{-1};                      // index into input token array
	std::unordered_set<std::size_t> m_partials{}; // already seen partial rule hashes

	bool m_debug{false};                          // output debug infos
	bool m_accepted{false};                       // input was accepted

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
#include <boost/functional/hash.hpp>

/**
 * print the symbol stack
 */
void %%PARSER_CLASS%%::PrintSymbols() const
{
	std::cout << "Symbol stack [" << m_symbols.size() << "]: ";
	std::size_t i = 0;
	for(auto iter = m_symbols.rbegin(); iter != m_symbols.rend(); std::advance(iter, 1))
	{
		const t_lalrastbaseptr& sym = *iter;

		std::cout << sym->GetId();
		if(sym->IsTerminal() && std::isprint(sym->GetId()))
			std::cout << " ('" << char(sym->GetId()) << "')";

		if(sym->IsTerminal())
			std::cout << " [t]";
		else
			std::cout << " [nt]";

		if(i < m_symbols.size()-1)
			std::cout << ", ";
		else
			std::cout << ".";
		++i;
	}

	std::cout << std::endl;
}

/**
 * get the next lookahead terminal symbol
 */
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

/**
 * shift the next lookahead terminal symbol onto the stack
 */
void %%PARSER_CLASS%%::PushLookahead()
{
	m_symbols.push(m_lookahead);
	GetNextLookahead();
}

/**
 * take the symbols from the stack and create an argument vector for the semantic rule
 */
t_semanticargs %%PARSER_CLASS%%::GetArguments(t_stack& symbols, std::size_t num_rhs)
{
	num_rhs = std::min(symbols.size(), num_rhs);
	t_semanticargs args(num_rhs);

	for(std::size_t arg=0; arg<num_rhs; ++arg)
	{
		args[num_rhs-arg-1] = std::move(symbols.top());
		symbols.pop();
	}

	return args;
}

/**
 * get a vector of the elements on the symbol stack, leaving the stack unchanged
 */
t_semanticargs %%PARSER_CLASS%%::GetCopyArguments(std::size_t num_rhs) const
{
	//t_stack symbols = m_symbols;
	//return GetArguments(symbols, num_rhs);
	return m_symbols.topN<std::deque>(num_rhs);
}

/**
 * enable/disable debug messages
 */
void %%PARSER_CLASS%%::SetDebug(bool b)
{
	m_debug = b;
}

/**
 * debug message when a new state becomes active
 */
void %%PARSER_CLASS%%::DebugMessageState(std::size_t state_id, const char* state_name, std::size_t state_hash) const
{
	std::cout << "\nRunning state " << state_id
		<< " function \"" << state_name << "\""
		<< " having hash value " << std::hex << state_hash << std::dec
		<< "..." << std::endl;
	if(m_lookahead)
	{
		std::cout << "Lookahead [" << m_lookahead_idx << "]: " << m_lookahead_id;
		if(std::isprint(m_lookahead_id))
			std::cout << " = '" << char(m_lookahead_id) << "'";
		std::cout << "." << std::endl;
	}

	PrintSymbols();
}

/**
 * debug message when a state becomes inactive
 */
void %%PARSER_CLASS%%::DebugMessageReturn(std::size_t state_id) const
{
	std::cout << "Returning from state " << state_id
		<< ", distance to jump: " << m_dist_to_jump << "."
		<< std::endl;
}

/**
 * debug message when symbols are reduced using a semantic rule
 */
void %%PARSER_CLASS%%::DebugMessageReduce(std::size_t num_rhs,
	std::size_t rulenr, const char* rule_descr) const
{
	std::cout << "Reducing " << num_rhs
		<< " symbol(s) using rule " << rulenr
		<< " (" << rule_descr << ")."
		<< std::endl;
}

/**
 * debug message when a partial semantic rule is applied
 */
void %%PARSER_CLASS%%::DebugMessagePartialRule(std::size_t rulelen, std::size_t rulenr, std::size_t state_hash) const
{
	if(std::size_t hash = GetPartialRuleHash(rulenr, rulelen, state_hash); m_partials.contains(hash))
		return;

	std::cout << "Partially matched rule " << rulenr
		<< " of length " << rulelen
		<< "." << std::endl;
}

/**
 * grammar error: invalid state transition
 */
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

	ostr << "and lookahead terminal " << m_lookahead_id << ".";

	throw std::runtime_error(ostr.str());
}

/**
 * sets a vector of semantic rules to apply when reducing
 */
void %%PARSER_CLASS%%::SetSemanticRules(const std::vector<t_semanticrule>* rules)
{
	m_semantics = rules;
}

/**
 * start parsing
 */
%%PARSER_CLASS%%::t_symbol %%PARSER_CLASS%%::Parse(const std::vector<t_token>& input)
{
	m_input = &input;
	m_lookahead_idx = -1;
	m_lookahead_id = 0;
	m_lookahead = nullptr;
	m_dist_to_jump = 0;
	m_accepted = false;
	m_partials.clear();
	while(!m_symbols.empty())
		m_symbols.pop();

	GetNextLookahead();
	state_0(0xffffffff /* random seed value */);

	if(m_symbols.size() && m_accepted)
		return m_symbols.top();
	return nullptr;
}

/**
 * get a unique identifier for a partial rule
 */
std::size_t %%PARSER_CLASS%%::GetPartialRuleHash(std::size_t rule, std::size_t len, std::size_t state_hash) const
{
	std::size_t total_hash = std::hash<std::size_t>{}(rule);
	boost::hash_combine(total_hash, std::hash<std::size_t>{}(len));
	boost::hash_combine(total_hash, state_hash);

	for(const auto& symbol : m_symbols)
		boost::hash_combine(total_hash, symbol->hash());

	return total_hash;
}

/**
 * execute a partially recognised semantic rule
 */
void %%PARSER_CLASS%%::ApplyPartialRule(std::size_t rule_nr, std::size_t rule_len, std::size_t state_hash)
{
	if(!m_semantics || rule_nr >= m_semantics->size())
	{
		std::cerr << "Error: No semantic rule #" << rule_nr << " defined." << std::endl;
		return;
	}

	if(std::size_t hash = GetPartialRuleHash(rule_nr, rule_len, state_hash); !m_partials.contains(hash))
	{
		m_partials.insert(hash);

		const t_semanticrule& rule = (*m_semantics)[rule_nr];
		if(!rule)
		{
			std::cerr << "Error: Invalid semantic rule #" << rule_nr << "." << std::endl;
			return;
		}

		t_semanticargs args = GetCopyArguments(rule_len);
		rule(false, args);
	}
}

/**
 * apply a fully recognised semantic rule
 */
void %%PARSER_CLASS%%::ApplyRule(std::size_t rule_nr, std::size_t rule_len)
{
	if(!m_semantics || rule_nr >= m_semantics->size())
	{
		std::cerr << "Error: No semantic rule #" << rule_nr << " defined." << std::endl;
		return;
	}

	const t_semanticrule& rule = (*m_semantics)[rule_nr];
	if(!rule)
	{
		std::cerr << "Error: No semantic rule #" << rule_nr << " defined." << std::endl;
		return;
	}

	t_semanticargs args = GetArguments(m_symbols, rule_len);
	m_symbols.emplace(rule(true, args));
}

%%DEFINE_CLOSURES%%)raw";


	std::string filename_h = boost::replace_last_copy(filename_cpp, ".cpp", ".h");

	// open output files
	std::ofstream file_cpp(filename_cpp);
	std::ofstream file_h(filename_h);

	if(!file_cpp || !file_h)
	{
		std::cerr << "Error: Cannot open output files \"" << filename_cpp
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
		ostr_cpp << "/**\n" << *closure;

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
		ostr_cpp << "void " << class_name << "::state_" << closure->GetId() << "(std::size_t state_hash)\n";
		ostr_cpp << "{\n";
		std::size_t hash_id = std::hash<std::size_t>{}(closure->GetId());
		ostr_cpp << "\tboost::hash_combine(state_hash, " << hash_id << ");\n";

		if(m_genDebugCode)
		{
			ostr_cpp << "\tif(m_debug)\n";
			ostr_cpp << "\t\tDebugMessageState(" << closure->GetId()
				<< ", __PRETTY_FUNCTION__"
				<< ", state_hash);\n";
		}


		// shift actions
		std::unordered_map<std::size_t, std::string> shifts;

		for(const t_transition& transition : GetTransitions(closure, true))
		{
			const ClosurePtr& closure_to = std::get<1>(transition);
			const SymbolPtr& symTrans = std::get<2>(transition);
			const Collection::t_elements& elemsFrom = std::get<3>(transition);

			// transition symbol has to be a terminal
			if(symTrans->IsEps() || !symTrans->IsTerminal())
				continue;

			// unique partial match
			bool has_partial = false;
			std::ostringstream ostr_partial;
			if(m_genPartialMatches)
			{
				if(auto [uniquematch, rulenr, rulelen] = GetUniquePartialMatch(elemsFrom); uniquematch)
				{
					if(m_genDebugCode)
					{
						ostr_partial << "\t\t\tif(m_debug)\n";
						ostr_partial << "\t\t\t\tDebugMessagePartialRule("
							<< rulelen << ", " << rulenr << ", state_hash);\n";
					}

					// execute partial semantic rule
					ostr_partial << "\t\t\t// partial semantic rule "
						<< rulenr << " with " << rulelen << " recognised argument(s)\n";
					ostr_partial << "\t\t\tApplyPartialRule("
						<< rulenr << ", " << rulelen << ", state_hash);\n";
					has_partial = true;
				}
			}

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
			if(has_partial)
				ostr_shift << ostr_partial.str();
			ostr_shift << "\t\t\tnext_state = &" << class_name
				<< "::state_" << closure_to->GetId() << ";\n";
			ostr_shift << "\t\t\tbreak;\n";
			ostr_shift << "\t\t}\n";  // end case

			shifts.emplace(std::make_pair(symTrans->GetId(), ostr_shift.str()));
		}


		// reduce actions
		std::vector<std::unordered_set<SymbolPtr>> reduces_lookaheads{};
		std::vector<std::string> reduces{};

		for(const ElementPtr& elem : closure->GetElements())
		{
			// cursor at end -> reduce a completely parsed rule
			if(!elem->IsCursorAtEnd())
				continue;

			std::optional<std::size_t> rulenr = elem->GetSemanticRule();
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

					// execute semantic rule
					ostr_reduce << "\t\t\t// semantic rule " << *rulenr << ": " << rule_descr.str() << "\n";
					ostr_reduce << "\t\t\tApplyRule(" << *rulenr << ", " << num_rhs << ");\n";
					ostr_reduce << "\t\t\tbreak;\n";
				}

				ostr_reduce << "\t\t}\n";  // end case

				reduces.emplace_back(ostr_reduce.str());
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
						ostrErr << " and lookahead terminal " << la->GetId();
						ostrErr << ".";

						if(m_stopOnConflicts)
							throw std::runtime_error(ostrErr.str());
						else
							std::cerr << "Error: " << ostrErr.str() << std::endl;
					}
				}

				if(!already_incremented_la)
					++iter_la;
			}
		}


		// write shift and reduce codes
		if(shifts.size())
			ostr_cpp << "\tvoid(" << class_name << "::*next_state)(std::size_t) = nullptr;\n";

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

		if(shifts.size())
		{
			ostr_cpp << "\tif(next_state)\n";
			ostr_cpp << "\t{\n";
			ostr_cpp << "\t\tPushLookahead();\n";
			ostr_cpp << "\t\t(this->*next_state)(state_hash);\n";
			ostr_cpp << "\t}\n";
		}


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
			const Collection::t_elements& elemsFrom = std::get<3>(transition);

			// transition symbol has to be a non-terminal
			if(symTrans->IsEps() || symTrans->IsTerminal())
				continue;

			// unique partial match
			bool has_partial = false;
			std::ostringstream ostr_partial;
			if(m_genPartialMatches)
			{
				if(auto [uniquematch, rulenr, rulelen] = GetUniquePartialMatch(elemsFrom); uniquematch)
				{
					if(m_genDebugCode)
					{
						ostr_partial << "\t\t\t\tif(m_debug)\n";
						ostr_partial << "\t\t\t\t\tDebugMessagePartialRule("
							<< rulelen << ", " << rulenr << ", state_hash);\n";
					}

					// execute partial semantic rule
					ostr_partial << "\t\t\t\t// partial semantic rule "
						<< rulenr << " with " << rulelen << " arguments\n";
					ostr_partial << "\t\t\t\tApplyPartialRule("
						<< rulenr << ", " << rulelen << ", state_hash);\n";
					has_partial = true;
				}
			}

			ostr_cpp_while << "\t\t\tcase " << symTrans->GetId() << ":\n";
			ostr_cpp_while << "\t\t\t{\n";
			if(has_partial)
				ostr_cpp_while << ostr_partial.str();
			ostr_cpp_while << "\t\t\t\tstate_" << closure_to->GetId() << "(state_hash);\n";
			ostr_cpp_while << "\t\t\t\tbreak;\n";
			ostr_cpp_while << "\t\t\t}\n";

			while_has_entries = true;
		}

		if(m_genErrorCode)
		{
			ostr_cpp_while << "\t\t\tdefault:\n";
			ostr_cpp_while << "\t\t\t{\n";
			ostr_cpp_while << "\t\t\t\tTransitionError(" << closure->GetId() << ");\n";
			ostr_cpp_while << "\t\t\t\tbreak;\n";
			ostr_cpp_while << "\t\t\t}\n";
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

		ostr_h << "\tvoid state_" << closure->GetId() << "(std::size_t state_hash);\n";
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
