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

#include "parsergen.h"
#include "timer.h"
#include "options.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <type_traits>
#include <regex>

#include <boost/algorithm/string.hpp>


ParserGen::ParserGen(const CollectionPtr& coll) : m_collection{coll}
{ }



/**
 * export an explicit recursive ascent parser
 * @see https://doi.org/10.1016/0020-0190(88)90061-0
 * @see https://en.wikipedia.org/wiki/Recursive_ascent_parser
 */
bool ParserGen::SaveParser(const std::string& filename_cpp, const std::string& class_name) const
{
	// --------------------------------------------------------------------------------
	// output header file stub
	// --------------------------------------------------------------------------------
	std::string outfile_h = R"raw(/**
 * Parser created on %%TIME_STAMP%% using liblalr1 by Tobias Weber, 2020-2022.
 * DOI: https://doi.org/10.5281/zenodo.6987396
 */

#ifndef __LALR1_PARSER_REC_ASC_H__
#define __LALR1_PARSER_REC_ASC_H__

#include "core/ast.h"
#include "core/common.h"
#include "core/stack.h"

#include <unordered_set>
#include <optional>

class %%PARSER_CLASS%%
{
public:
	using t_token = t_toknode;                    // token data type
	using t_tokens = std::vector<t_toknode>;      // token vector data type
	using t_symbol = t_lalrastbaseptr;            // symbol data type
	using t_stack = ParseStack<t_symbol>;         // symbol stack

	%%PARSER_CLASS%%() = default;
	~%%PARSER_CLASS%%() = default;
	%%PARSER_CLASS%%(const %%PARSER_CLASS%%&) = delete;
	%%PARSER_CLASS%%& operator=(const %%PARSER_CLASS%%&) = delete;

	void SetDebug(bool b);

	void SetSemanticRules(const t_semanticrules* rules);
	t_symbol Parse(const t_tokens& input);

protected:
	void PrintSymbols() const;
	void GetNextLookahead();
	void PushLookahead();

	static t_semanticargs GetArguments(t_stack& symbols, std::size_t num_rhs);
	t_semanticargs GetCopyArguments(std::size_t num_rhs) const;

	const ActiveRule* GetActiveRule(t_semantic_id rule_id) const;
	ActiveRule* GetActiveRule(t_semantic_id rule_id);
	std::optional<t_index> GetActiveRuleHandle(t_semantic_id rule_id) const;

	bool ApplyPartialRule(bool before_shift, t_semantic_id rule_id, std::size_t rule_len);
	bool ApplyRule(t_semantic_id rule_id, std::size_t rule_len);

	void DebugMessageState(t_state_id state_id, const char* state_func) const;
	void DebugMessageReturn(t_state_id state_id) const;
	void DebugMessageReduce(std::size_t num_rhs, t_semantic_id rule_id, const char* rule_descr) const;
	void DebugMessagePartialRule(std::size_t rulelen, t_semantic_id rule_id) const;
	void TransitionError(t_state_id state_id) const;

%%DECLARE_CLOSURES%%
private:
	// semantic rules
	const t_semanticrules* m_semantics{};         // semantic rules
	const t_tokens* m_input{};                    // input tokens
	t_stack m_symbols{};                          // currently active symbols

	t_active_rules m_active_rules{};              // active partial rules
	std::size_t m_cur_rule_handle{0};             // the same for corresponding partial rules

	t_token m_lookahead{nullptr};                 // lookahead token
	t_symbol_id m_lookahead_id{0};                // lookahead identifier
	int m_lookahead_idx{-1};                      // index into input token array

	bool m_debug{false};                          // output debug infos
	bool m_accepted{false};                       // input was accepted

	std::size_t m_dist_to_jump{0};                // return count between reduction and jump / non-terminal transition

	static constexpr const t_symbol_id s_end_id{%%END_ID%%}; // end symbol id
};

#endif)raw";
	// --------------------------------------------------------------------------------


	// --------------------------------------------------------------------------------
	// output cpp file stub
	// --------------------------------------------------------------------------------
	std::string outfile_cpp = R"raw(/**
 * Parser created on %%TIME_STAMP%% using liblalr1 by Tobias Weber, 2020-2022.
 * DOI: https://doi.org/10.5281/zenodo.6987396
 */

%%INCLUDE_HEADER%%
#include <exception>
#include <string>
#include <iostream>
#include <sstream>

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
		if(sym->IsTerminal() && isprintable(sym->GetId()))
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
 * take the symbols from the stack and create an argument container for the semantic rule
 */
t_semanticargs %%PARSER_CLASS%%::GetArguments(t_stack& symbols, std::size_t num_rhs)
{
	num_rhs = std::min(symbols.size(), num_rhs);
	t_semanticargs args(num_rhs);

	for(t_index arg=0; arg<num_rhs; ++arg)
	{
		args[num_rhs-arg-1] = std::move(symbols.top());
		symbols.pop();
	}

	return args;
}

/**
 * get elements on the symbol stack, leaving the stack unchanged
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
void %%PARSER_CLASS%%::DebugMessageState(t_state_id state_id, const char* state_name) const
{
	std::cout << "\nRunning state " << state_id
		<< " function \"" << state_name << "\"" << "..." << std::endl;
	if(m_lookahead)
	{
		std::cout << "Lookahead [" << m_lookahead_idx << "]: ";
		if(m_lookahead_id == s_end_id)
			std::cout << "end";
		else
			std::cout << m_lookahead_id;
		if(isprintable(m_lookahead_id))
			std::cout << " = '" << char(m_lookahead_id) << "'";
		std::cout << "." << std::endl;
	}

	PrintSymbols();
}

/**
 * debug message when a state becomes inactive
 */
void %%PARSER_CLASS%%::DebugMessageReturn(t_state_id state_id) const
{
	std::cout << "Returning from state " << state_id
		<< ", distance to jump: " << m_dist_to_jump << "."
		<< std::endl;
}

/**
 * debug message when symbols are reduced using a semantic rule
 */
void %%PARSER_CLASS%%::DebugMessageReduce(std::size_t num_rhs,
	t_semantic_id rule_id, const char* rule_descr) const
{
	std::optional<t_index> rule_handle = GetActiveRuleHandle(rule_id);

	std::cout << "Reducing " << num_rhs
		<< " symbol(s) using rule #" << rule_id;
	if(rule_handle)
		std::cout << " (handle id " << *rule_handle << ")";
	std::cout << " (" << rule_descr << ")." << std::endl;
}

/**
 * debug message when a partial semantic rule is applied
 */
void %%PARSER_CLASS%%::DebugMessagePartialRule(std::size_t rulelen,
	t_semantic_id rule_id) const
{
	std::optional<t_index> rule_handle = GetActiveRuleHandle(rule_id);

	std::cout << "Partially matched rule #" << rule_id;
	if(rule_handle)
		std::cout << " (handle id " << *rule_handle << ")";
	std::cout << " of length " << rulelen << "." << std::endl;
}

/**
 * grammar error: invalid state transition
 */
void %%PARSER_CLASS%%::TransitionError(t_state_id state_id) const
{
	std::ostringstream ostr;
	ostr << "No transition from state " << state_id << ", ";

	if(m_symbols.size())
	{
		const t_symbol& topsym = m_symbols.top();
		bool is_term = topsym->IsTerminal();
		t_symbol_id sym_id = topsym->GetId();

		ostr << "top-level " << (is_term ? "terminal" : "non-terminal")
			<< " " << sym_id << ", ";
	}

	ostr << "and lookahead terminal " << m_lookahead_id << ".";

	throw std::runtime_error(ostr.str());
}

/**
 * get active (partial) rule
 */
const ActiveRule* %%PARSER_CLASS%%::GetActiveRule(t_semantic_id rule_id) const
{
	if(t_active_rules::const_iterator iter_active_rule = m_active_rules.find(rule_id);
		iter_active_rule != m_active_rules.end())
	{
		const t_active_rule_stack& rulestack = iter_active_rule->second;
		if(!rulestack.empty())
		{
			const t_active_rule_stack& rulestack = iter_active_rule->second;
			const ActiveRule& active_rule = rulestack.top();
			return &active_rule;
		}
	}

	return nullptr;
}

/**
 * get active (partial) rule
 */
ActiveRule* %%PARSER_CLASS%%::GetActiveRule(t_semantic_id rule_id)
{
	const %%PARSER_CLASS%%* const_this = this;
	return const_cast<ActiveRule*>(const_this->GetActiveRule(rule_id));
}

/**
 * get active (partial) rule handle
 */
std::optional<t_index> %%PARSER_CLASS%%::GetActiveRuleHandle(t_semantic_id rule_id) const
{
	std::optional<t_index> rule_handle;
	if(const ActiveRule* active_rule = GetActiveRule(rule_id); active_rule)
		rule_handle = active_rule->handle;
	return rule_handle;
}

/**
 * sets the semantic rules to apply when reducing
 */
void %%PARSER_CLASS%%::SetSemanticRules(const t_semanticrules* rules)
{
	m_semantics = rules;
}

/**
 * start parsing
 */
%%PARSER_CLASS%%::t_symbol %%PARSER_CLASS%%::Parse(const t_tokens& input)
{
	m_input = &input;
	m_lookahead_idx = -1;
	m_lookahead_id = 0;
	m_lookahead = nullptr;
	m_cur_rule_handle = 0;
	m_dist_to_jump = 0;
	m_accepted = false;
	m_active_rules.clear();
	while(!m_symbols.empty())
		m_symbols.pop();

	GetNextLookahead();
	%%START_STATE%%();

	if(m_symbols.size() && m_accepted)
		return m_symbols.top();
	return nullptr;
}

/**
 * execute a partially recognised semantic rule
 */
bool %%PARSER_CLASS%%::ApplyPartialRule(bool before_shift, t_semantic_id rule_id, std::size_t rule_len)
{
	if(!m_semantics || !m_semantics->contains(rule_id))
	{
		std::cerr << "Error: No semantic rule #" << rule_id << " defined." << std::endl;
		return false;
	}

	bool already_seen_active_rule = false;
	bool insert_new_active_rule = false;

	t_active_rules::iterator iter_active_rule = m_active_rules.find(rule_id);
	if(iter_active_rule != m_active_rules.end())
	{
		t_active_rule_stack& rulestack = iter_active_rule->second;
		if(!rulestack.empty())
		{
			ActiveRule& active_rule = rulestack.top();
			if(before_shift)
			{
				if(active_rule.seen_tokens < rule_len)
					active_rule.seen_tokens = rule_len; // update seen length
				else
					insert_new_active_rule = true;      // start of new rule
			}
			else  // before jump
			{
				if(active_rule.seen_tokens == rule_len)
					already_seen_active_rule = true;
				else
					active_rule.seen_tokens = rule_len; // update seen length
			}
		}
		else
		{
			insert_new_active_rule = true;  // no active rule yet
		}
	}
	else
	{
		iter_active_rule = m_active_rules.emplace(
			std::make_pair(rule_id, t_active_rule_stack{})).first;
		insert_new_active_rule = true;          // no active rule yet
	}

	if(insert_new_active_rule)
	{
		ActiveRule active_rule{
			.seen_tokens = rule_len,
			.handle = m_cur_rule_handle++,
		};

		t_active_rule_stack& rulestack = iter_active_rule->second;
		rulestack.emplace(std::move(active_rule));
	}

	if(!already_seen_active_rule)
	{
		const t_semanticrule& rule = m_semantics->at(rule_id);
		if(!rule)
		{
			std::cerr << "Error: Invalid semantic rule #" << rule_id << "." << std::endl;
			return false;
		}

		t_semanticargs args = GetCopyArguments(rule_len);
		t_lalrastbaseptr retval = nullptr;
		ActiveRule *active_rule = GetActiveRule(rule_id);
		if(active_rule)
			retval = active_rule->retval;

		retval = rule(false, args, retval);

		if(active_rule)
			active_rule->retval = retval;

		return true;
	}

	return false;
}

/**
 * apply a fully recognised semantic rule
 */
bool %%PARSER_CLASS%%::ApplyRule(t_semantic_id rule_id, std::size_t rule_len)
{
	if(t_active_rules::iterator iter_active_rule = m_active_rules.find(rule_id);
		iter_active_rule != m_active_rules.end())
	{
		t_active_rule_stack& rulestack = iter_active_rule->second;
		if(!rulestack.empty())
		{
			t_active_rule_stack& rulestack = iter_active_rule->second;
			rulestack.pop();
		}
	}

	if(!m_semantics || !m_semantics->contains(rule_id))
	{
		std::cerr << "Error: No semantic rule #" << rule_id << " defined." << std::endl;
		return false;
	}

	const t_semanticrule& rule = m_semantics->at(rule_id);
	if(!rule)
	{
		std::cerr << "Error: No semantic rule #" << rule_id << " defined." << std::endl;
		return false;
	}

	t_semanticargs args = GetArguments(m_symbols, rule_len);
	t_lalrastbaseptr retval = nullptr;
	if(ActiveRule *active_rule = GetActiveRule(rule_id); active_rule)
		retval = active_rule->retval;

	m_symbols.emplace(rule(true, args, retval));
	return true;
}

%%DEFINE_CLOSURES%%)raw";
	// --------------------------------------------------------------------------------


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
	g_options.SetUseColour(false);

	std::ostringstream ostr_h, ostr_cpp;

	// create the names of the closure functions
	std::regex regex_ident("[_A-Za-z][_A-Za-z0-9]*");
	std::unordered_map<t_state_id, std::string> closure_names;
	std::unordered_map<std::string, std::size_t> name_counts;

	using t_transitions = Collection::t_transitions;
	using t_transition = Collection::t_transition;
	using t_closures = Collection::t_closures;

	const t_closures& closures = m_collection->GetClosures();

	for(const ClosurePtr& closure : closures)
	{
		bool name_valid = false;
		std::ostringstream closure_name;

		const t_state_id closure_id = closure->GetId();
		const Closure::t_elements& elems = closure->GetElements();

		if(GetUseStateNames() && elems.size())
		{
			// name the state function
			const std::string& lhsname = (*elems.begin())->GetLhs()->GetStrId();
			std::size_t name_count = 0;

			if(auto iter_ctr = name_counts.find(lhsname); iter_ctr != name_counts.end())
			{
				++iter_ctr->second;
				name_count = iter_ctr->second;
			}
			else
			{
				name_counts.emplace(std::make_pair(lhsname, 0));
				name_count = 0;
			}

			closure_name << lhsname << "_" << name_count;
			// checks if the name is a valid C++ identifier
			name_valid = std::regex_match(closure_name.str(), regex_ident);
		}

		if(!name_valid)
		{
			// use an anonymous state function name
			closure_name = std::ostringstream{};
			closure_name << "state_" << closure_id;
		}

		closure_names[closure_id] = closure_name.str();
	}

	// create closure functions
	for(const ClosurePtr& closure : closures)
	{
		const t_state_id closure_id = closure->GetId();
		const std::string& closure_name = closure_names[closure_id];

		std::optional<Terminal::t_terminalset> lookbacks;

		// write comment
		ostr_cpp << "/**\n" << *closure;

		if(Terminal::t_terminalset lookbacks =
			m_collection->GetLookbackTerminals(closure);
			lookbacks.size())
		{
			ostr_cpp << "Lookback terminals:";
			for(const TerminalPtr& lookback : lookbacks)
				ostr_cpp << " " << lookback->GetStrId();
			ostr_cpp << "\n";
		}

		if(t_transitions terminal_transitions =
			m_collection->GetTransitions(closure, true);
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

		if(t_transitions nonterminal_transitions =
			m_collection->GetTransitions(closure, false);
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
		ostr_cpp << "void " << class_name << "::" << closure_name << "()\n";
		ostr_cpp << "{\n";

		if(GetGenDebugCode())
		{
			ostr_cpp << "\tif(m_debug)\n";
			ostr_cpp << "\t\tDebugMessageState(" << closure_id
				<< ", __PRETTY_FUNCTION__);\n";
		}


		// shift actions
		std::unordered_map<t_symbol_id, std::string> shifts;

		for(const t_transition& transition : m_collection->GetTransitions(closure, true))
		{
			const ClosurePtr& closure_to = std::get<1>(transition);
			const SymbolPtr& symTrans = std::get<2>(transition);
			const Collection::t_elements& elemsFrom = std::get<3>(transition);

			// shift => transition symbol has to be a terminal
			if(symTrans->IsEps() || !symTrans->IsTerminal())
				continue;

			// unique partial match
			bool has_partial = false;
			std::ostringstream ostr_partial;
			if(GetGenPartialMatches())
			{
				if(auto [uniquematch, rule_id, rulelen] =
					m_collection->GetUniquePartialMatch(elemsFrom, true); uniquematch)
				{
					// execute partial semantic rule
					ostr_partial << "\t\t\t// partial semantic rule "
						<< rule_id << " with " << rulelen << " recognised argument(s)\n";
					ostr_partial << "\t\t\tbool applied = ApplyPartialRule(true, "
						<< rule_id << ", " << rulelen << ");\n";
					has_partial = true;

					if(GetGenDebugCode())
					{
						ostr_partial << "\t\t\tif(m_debug && applied)\n";
						ostr_partial << "\t\t\t\tDebugMessagePartialRule("
							<< rulelen << ", " << rule_id << ");\n";
					}
				}
			}

			std::ostringstream ostr_shift;
			if(symTrans->GetId() == g_end->GetId())
			{
				ostr_shift << "\t\tcase s_end_id:\n";
			}
			else if(GetUseOpChar() && isprintable(symTrans->GetId()))
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

			const std::string& closure_to_name = closure_names[closure_to->GetId()];
			ostr_shift << "\t\t\tnext_state = &" << class_name
				<< "::" << closure_to_name << ";\n";
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

			std::optional<t_semantic_id> rule_id = elem->GetSemanticRule();
			if(!rule_id)
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
				if(*rule_id == GetAcceptingRule())
				{
					ostr_reduce << "\t\t\tm_accepted = true;\n";
					ostr_reduce << "\t\t\tbreak;\n";
				}
				else
				{
					std::ostringstream rule_descr;
					rule_descr << (*elem->GetLhs()) << " -> " << (*elem->GetRhs());

					// in the table-based parser, num_rhs states are popped,
					// here we have to count function returns to get to the new top state function
					std::size_t num_rhs = elem->GetRhs()->NumSymbols(false);
					ostr_reduce << "\t\t\tm_dist_to_jump = " << num_rhs << ";\n";

					if(GetGenDebugCode())
					{
						ostr_reduce << "\t\t\tif(m_debug)\n";
						ostr_reduce << "\t\t\t\tDebugMessageReduce("
							<< num_rhs << ", "
							<< *rule_id << ", "
							<< "\"" << rule_descr.str() << "\");\n";
					}

					// execute semantic rule
					ostr_reduce << "\t\t\t// semantic rule " << *rule_id << ": " << rule_descr.str() << "\n";
					ostr_reduce << "\t\t\tApplyRule(" << *rule_id << ", " << num_rhs << ");\n";
					ostr_reduce << "\t\t\tbreak;\n";
				}

				ostr_reduce << "\t\t}\n";  // end case

				reduces.emplace_back(ostr_reduce.str());
			}
		}


		// try to solve shift/reduce conflicts
		for(t_index reduce_idx = 0; reduce_idx < reduces_lookaheads.size(); ++reduce_idx)
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
						lookbacks = m_collection->GetLookbackTerminals(closure);

					t_index shift_val = 0, reduce_val = 0;  // dummy values (only need to check for ERROR_VAL)
					if(m_collection->SolveConflict(la, *lookbacks, &shift_val, &reduce_val))
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
							<< " in state " << closure_id;
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

						if(GetStopOnConflicts())
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
			ostr_cpp << "\tvoid(" << class_name << "::*next_state)() = nullptr;\n";

		ostr_cpp << "\tswitch(m_lookahead_id)\n";
		ostr_cpp << "\t{\n";

		for(const auto& shift : shifts)
			ostr_cpp << shift.second;

		for(t_index reduce_idx = 0; reduce_idx < reduces_lookaheads.size(); ++reduce_idx)
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
				else if(GetUseOpChar() && isprintable(la->GetId()))
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

		if(GetGenErrorCode())
		{
			// default: error
			ostr_cpp << "\t\tdefault:\n";
			ostr_cpp << "\t\t{\n";
			ostr_cpp << "\t\t\tTransitionError(" << closure_id << ");\n";
			ostr_cpp << "\t\t\tbreak;\n";
			ostr_cpp << "\t\t}\n";
		}

		ostr_cpp << "\t}\n";        // end switch

		if(shifts.size())
		{
			ostr_cpp << "\tif(next_state)\n";
			ostr_cpp << "\t{\n";
			ostr_cpp << "\t\tPushLookahead();\n";
			ostr_cpp << "\t\t(this->*next_state)();\n";
			ostr_cpp << "\t}\n";
		}


		// jump to new closure after a reduce
		std::ostringstream ostr_cpp_while;
		ostr_cpp_while << "\twhile(!m_dist_to_jump && m_symbols.size() && !m_accepted)\n";
		ostr_cpp_while << "\t{\n";

		ostr_cpp_while << "\t\tconst t_symbol& topsym = m_symbols.top();\n";
		ostr_cpp_while << "\t\tif(topsym->IsTerminal())\n\t\t\tbreak;\n";

		ostr_cpp_while << "\t\tswitch(topsym->GetId())\n";
		ostr_cpp_while << "\t\t{\n";

		bool while_has_entries = false;
		for(const t_transition& transition : m_collection->GetTransitions(closure, false))
		{
			const ClosurePtr& closure_to = std::get<1>(transition);
			const SymbolPtr& symTrans = std::get<2>(transition);
			const Collection::t_elements& elemsFrom = std::get<3>(transition);

			// jump => transition symbol has to be a non-terminal
			if(symTrans->IsEps() || symTrans->IsTerminal())
				continue;

			// unique partial match
			bool has_partial = false;
			std::ostringstream ostr_partial;
			if(GetGenPartialMatches())
			{
				if(auto [uniquematch, rule_id, rulelen] =
					m_collection->GetUniquePartialMatch(elemsFrom, false); uniquematch)
				{
					// execute partial semantic rule
					ostr_partial << "\t\t\t\t// partial semantic rule "
						<< rule_id << " with " << rulelen << " arguments\n";
					ostr_partial << "\t\t\t\tbool applied = ApplyPartialRule(false, "
						<< rule_id << ", " << rulelen << ");\n";
					has_partial = true;

					if(GetGenDebugCode())
					{
						ostr_partial << "\t\t\t\tif(m_debug && applied)\n";
						ostr_partial << "\t\t\t\t\tDebugMessagePartialRule("
							<< rulelen << ", " << rule_id << ");\n";
					}
				}
			}

			ostr_cpp_while << "\t\t\tcase " << symTrans->GetId() << ":\n";
			ostr_cpp_while << "\t\t\t{\n";
			if(has_partial)
				ostr_cpp_while << ostr_partial.str();

			const std::string& closure_to_name = closure_names[closure_to->GetId()];
			ostr_cpp_while << "\t\t\t\t" << closure_to_name << "();\n";
			ostr_cpp_while << "\t\t\t\tbreak;\n";
			ostr_cpp_while << "\t\t\t}\n";

			while_has_entries = true;
		}

		if(GetGenErrorCode())
		{
			ostr_cpp_while << "\t\t\tdefault:\n";
			ostr_cpp_while << "\t\t\t{\n";
			ostr_cpp_while << "\t\t\t\tTransitionError(" << closure_id << ");\n";
			ostr_cpp_while << "\t\t\t\tbreak;\n";
			ostr_cpp_while << "\t\t\t}\n";
		}

		ostr_cpp_while << "\t\t}\n";  // end switch

		ostr_cpp_while << "\t}\n";    // end while
		if(while_has_entries)
			ostr_cpp << ostr_cpp_while.str();

		// return from closure -> decrement distance counter
		ostr_cpp << "\tif(m_dist_to_jump > 0)\n\t\t--m_dist_to_jump;\n";

		if(GetGenDebugCode())
		{
			ostr_cpp << "\tif(m_debug)\n";
			ostr_cpp << "\t\tDebugMessageReturn(" << closure_id << ");\n";
		}

		// end closure function
		ostr_cpp << "}\n\n";

		ostr_h << "\tvoid " << closure_name << "();\n";
	}


	// write output files
	std::string incl = "#include \"" + filename_h + "\"";
	std::string time_stamp = get_timestamp();

	t_symbol_id end_id = g_end->GetId();
	std::ostringstream end_id_ostr;
	end_id_ostr << "0x" << std::hex << end_id;
	if constexpr(std::is_unsigned_v<t_symbol_id>)
		end_id_ostr << "u";

	boost::replace_all(outfile_cpp, "%%PARSER_CLASS%%", class_name);
	boost::replace_all(outfile_h, "%%PARSER_CLASS%%", class_name);
	boost::replace_all(outfile_cpp, "%%INCLUDE_HEADER%%", incl);
	boost::replace_all(outfile_cpp, "%%DEFINE_CLOSURES%%", ostr_cpp.str());
	boost::replace_all(outfile_h, "%%DECLARE_CLOSURES%%", ostr_h.str());
	boost::replace_all(outfile_h, "%%END_ID%%", end_id_ostr.str());
	boost::replace_all(outfile_cpp, "%%START_STATE%%", closure_names[GetStartingState()]);
	boost::replace_all(outfile_cpp, "%%TIME_STAMP%%", time_stamp);
	boost::replace_all(outfile_h, "%%TIME_STAMP%%", time_stamp);

	file_cpp << outfile_cpp << std::endl;
	file_h << outfile_h << std::endl;

	// restore options
	g_options.SetUseColour(use_col_saved);
	return true;
}


bool ParserGen::GetStopOnConflicts() const
{
	return m_collection->GetStopOnConflicts();
}
