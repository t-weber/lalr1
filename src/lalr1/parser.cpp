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
#include <sstream>


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
	std::stack<std::size_t> states;
	std::stack<t_lalrastbaseptr> symbols;

	// debug output of the stacks
	auto print_stacks = [&states, &symbols](std::ostream& ostr)
	{
		ostr << "\tState stack: ";
		auto _states = states;
		while(!_states.empty())
		{
			ostr << _states.top();
			_states.pop();

			if(_states.size() > 0)
				ostr << ", ";
		}
		ostr << "." << std::endl;

		ostr << "\tSymbol stack: ";
		auto _symbols = symbols;
		while(!_symbols.empty())
		{
			ostr << _symbols.top()->GetTableIdx();
			_symbols.pop();

			if(_symbols.size() > 0)
				ostr << ", ";
		}
		ostr << "." << std::endl;
	};

	// starting state
	states.push(0);
	std::size_t inputidx = 0;

	t_toknode curtok = input[inputidx++];
	std::size_t curtokidx = curtok->GetTableIdx();

	// debug output of current token
	auto print_input_token = [&inputidx, &curtok, &curtokidx](std::ostream& ostr)
	{
		ostr << "\tCurrent token [" << inputidx-1 << "]"
			<< ": " << curtok->GetId()
			<< " (table index " << curtokidx << ")."
			<< std::endl;
	};

	while(true)
	{
		std::size_t topstate = states.top();
		std::size_t newstate = m_tabActionShift(topstate, curtokidx);
		std::size_t newrule = m_tabActionReduce(topstate, curtokidx);

		if(m_debug)
		{
			std::cout << "\n";
			std::cout << "State " << topstate << " active." << std::endl;
			print_input_token(std::cout);
			print_stacks(std::cout);
		}

		if(newstate == ERROR_VAL && newrule == ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Undefined shift and reduce entries"
				<< " from state " << topstate << "."
				<< " Current token id is " << curtok->GetId()
				<< get_line_numbers(curtok)
				<< ".";

			throw std::runtime_error(ostrErr.str());
		}
		else if(newstate != ERROR_VAL && newrule != ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Shift/reduce conflict between shift"
				<< " from state " << topstate << " to state " << newstate
				<< " and reduce using rule " << newrule << "."
				<< " Current token id is " << curtok->GetId()
				<< get_line_numbers(curtok)
				<< ".";

			throw std::runtime_error(ostrErr.str());
		}

		// accept
		else if(newrule == ACCEPT_VAL)
		{
			if(m_debug)
			{
				std::cout << "\tAccepting." << std::endl;
			}
			return symbols.top();
		}

		// shift
		else if(newstate != ERROR_VAL)
		{
			if(m_debug)
			{
				std::cout << "\tShifting state " << newstate
					<< " (pushing to state stack)"
					<< "." << std::endl;
			}

			states.push(newstate);
			symbols.push(curtok);

			if(inputidx >= input.size())
			{
				std::ostringstream ostrErr;
				ostrErr << "Input buffer underflow"
					<< get_line_numbers(curtok)
					<< ".";
				throw std::runtime_error(ostrErr.str());
			}

			curtok = input[inputidx++];
			curtokidx = curtok->GetTableIdx();
		}

		// reduce
		else if(newrule != ERROR_VAL)
		{
			std::size_t numSyms = m_numRhsSymsPerRule[newrule];
			if(m_debug)
			{
				std::cout << "\tReducing " << numSyms
					<< " symbol(s) via rule " << newrule
					<< " (popping " << numSyms << " element(s) from stacks,"
					<< " pushing result to symbol stack)"
					<< "." << std::endl;
			}

			// take the symbols from the stack and create an
			// argument vector for the semantic rule
			std::vector<t_lalrastbaseptr> args;
			for(std::size_t arg=0; arg<numSyms; ++arg)
			{
				args.emplace_back(std::move(symbols.top()));

				symbols.pop();
				states.pop();
			}

			if(args.size() > 1)
				std::reverse(args.begin(), args.end());

			// execute semantic rule
			t_lalrastbaseptr reducedSym = m_semantics[newrule](args);
			symbols.push(reducedSym);


			topstate = states.top();

			if(m_debug)
			{
				std::cout << "\n";
				std::cout << "State " << topstate << " active." << std::endl;
				print_input_token(std::cout);
				print_stacks(std::cout);
			}

			std::size_t jumpstate = m_tabJump(topstate, reducedSym->GetTableIdx());
			states.push(jumpstate);

			if(m_debug)
			{
				std::cout << "\tJumping from state " << topstate
					<< " to state " << jumpstate
					<< " (pushing jump state " << jumpstate << ")"
					<< "." << std::endl;
			}
		}
	}

	return nullptr;
}
