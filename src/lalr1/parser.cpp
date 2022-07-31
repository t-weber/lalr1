/**
 * lalr(1) parser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-jun-2020
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

#include "parser.h"

#include <stack>


Parser::Parser(
	const std::tuple<
		t_table,                 // 0: shift table
		t_table,                 // 1: reduce table
		t_table,                 // 2: jump table
		t_mapIdIdx,              // 3: terminal indices
		t_mapIdIdx,              // 4: nonterminal indices
		t_vecIdx>& init,         // 5: semantic rule indices
	const std::vector<t_semanticrule>& rules)
	: m_tabActionShift{std::get<0>(init)},
		m_tabActionReduce{std::get<1>(init)},
		m_tabJump{std::get<2>(init)},
		m_mapTermIdx{std::get<3>(init)},
		m_mapNonTermIdx{std::get<4>(init)},
		m_numRhsSymsPerRule{std::get<5>(init)},
		m_semantics{rules}
{}


Parser::Parser(
	const std::tuple<
		const t_table*,          // 0: shift table
		const t_table*,          // 1: reduce table
		const t_table*,          // 2: jump table
		const t_mapIdIdx*,       // 3: terminal indices
		const t_mapIdIdx*,       // 4: nonterminal indices
		const t_vecIdx*>& init,  // 5: semantic rule indices
	const std::vector<t_semanticrule>& rules)
	: m_tabActionShift{*std::get<0>(init)},
		m_tabActionReduce{*std::get<1>(init)},
		m_tabJump{*std::get<2>(init)},
		m_mapTermIdx{*std::get<3>(init)},
		m_mapNonTermIdx{*std::get<4>(init)},
		m_numRhsSymsPerRule{*std::get<5>(init)},
		m_semantics{rules}
{}


template<class t_toknode>
std::string get_line_numbers(const t_toknode& node)
{
	std::ostringstream ostr;

	if(auto lines = node->GetLineRange(); lines)
	{
		auto line_start = std::get<0>(*lines);
		auto line_end = std::get<1>(*lines);

		if(line_start == line_end)
			ostr << " (line " << line_start << ")";
		else
			ostr << " (lines " << line_start << "..." << line_end << ")";
	}

	return ostr.str();
}


t_lalrastbaseptr Parser::Parse(const std::vector<t_toknode>& input) const
{
	constexpr bool debug = false;

	std::stack<std::size_t> states;
	std::stack<t_lalrastbaseptr> symbols;

	// starting state
	states.push(0);
	std::size_t inputidx = 0;

	t_toknode curtok = input[inputidx++];
	std::size_t curtokidx = curtok->GetTableIdx();

	while(true)
	{
		std::size_t topstate = states.top();
		std::size_t newstate = m_tabActionShift(topstate, curtokidx);
		std::size_t newrule = m_tabActionReduce(topstate, curtokidx);

		if(newstate == ERROR_VAL && newrule == ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Undefined shift and reduce entries"
				<< " from state " << topstate << ".";
			ostrErr << " Current token id is " << curtok->GetId();
			ostrErr << get_line_numbers(curtok);
			ostrErr << ".";

			throw std::runtime_error(ostrErr.str());
		}
		else if(newstate != ERROR_VAL && newrule != ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Shift/reduce conflict between shift"
				<< " from state " << topstate << " to state " << newstate
				<< " and reduce using rule " << newrule << ".";
			ostrErr << " Current token id is " << curtok->GetId();
			ostrErr << get_line_numbers(curtok);
			ostrErr << ".";

			throw std::runtime_error(ostrErr.str());
		}

		// accept
		else if(newrule == ACCEPT_VAL)
		{
			if constexpr(debug)
				std::cout << "accepting" << std::endl;
			return symbols.top();
		}

		// shift
		else if(newstate != ERROR_VAL)
		{
			if constexpr(debug)
				std::cout << "shifting state " << newstate << std::endl;

			states.push(newstate);
			symbols.push(curtok);

			if(inputidx >= input.size())
			{
				std::ostringstream ostrErr;
				ostrErr << "Input buffer underflow";
				ostrErr << get_line_numbers(curtok);
				ostrErr << ".";
				throw std::runtime_error(ostrErr.str());
			}

			curtok = input[inputidx++];
			curtokidx = curtok->GetTableIdx();
		}

		// reduce
		else if(newrule != ERROR_VAL)
		{
			std::size_t numSyms = m_numRhsSymsPerRule[newrule];
			if constexpr(debug)
			{
				std::cout << "reducing " << numSyms
					<< " symbol(s) via rule " << newrule
					<< ", top state: " << topstate
					<< std::endl;
			}

			// take the symbols from the stack and create an argument vector for the semantic rule
			std::vector<t_lalrastbaseptr> args;
			for(std::size_t arg=0; arg<numSyms; ++arg)
			{
				args.push_back(symbols.top());

				symbols.pop();
				states.pop();
			}

			if(args.size() > 1)
				std::reverse(args.begin(), args.end());

			// execute semantic rule
			t_lalrastbaseptr reducedSym = m_semantics[newrule](args);
			symbols.push(reducedSym);

			topstate = states.top();
			std::size_t jumpstate = m_tabJump(topstate, reducedSym->GetTableIdx());
			states.push(jumpstate);

			if constexpr(debug)
			{
				std::cout << "jumping from state " << topstate
					<< " to state " << jumpstate
					<< std::endl;
			}
		}
	}

	return nullptr;
}
