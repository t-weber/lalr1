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

#include <deque>
#include <vector>
#include <stack>
#include <tuple>
#include <optional>
#include <sstream>


Parser::Parser(const Parser& parser)
{
	this->operator=(parser);
}


Parser& Parser::operator=(const Parser& parser)
{
	m_tabActionShift = parser.m_tabActionShift;
	m_tabActionReduce = parser.m_tabActionReduce;
	m_tabJump = parser.m_tabJump;
	m_tabPartialsRulesTerm = parser.m_tabPartialsRulesTerm;
	m_tabPartialsMatchLenTerm = parser.m_tabPartialsMatchLenTerm;
	m_tabPartialsRulesNonTerm = parser.m_tabPartialsRulesNonTerm;
	m_tabPartialsMatchLenNonTerm = parser.m_tabPartialsMatchLenNonTerm;
	m_numRhsSymsPerRule = parser.m_numRhsSymsPerRule;
	m_vecLhsIndices = parser.m_vecLhsIndices;
	m_semantics = parser.m_semantics;
	m_debug = parser.m_debug;

	return *this;
}


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


/**
 * stack class for parse states and symbols
 */
template<class t_elem, class t_cont = std::deque<t_elem>>
class ParseStack : public std::stack<t_elem, t_cont>
{
protected:
	// the underlying container
	using std::stack<t_elem, t_cont>::c;

public:
	/**
	 * get the top N elements on stack
	 */
	template<template<class...> class t_cont_ret>
	t_cont_ret<t_elem> topN(std::size_t N)
	{
		t_cont_ret<t_elem> cont;

		for(auto iter = c.rbegin(); iter != c.rend(); std::advance(iter, 1))
		{
			cont.push_back(*iter);
			if(cont.size() == N)
				break;
		}

		return cont;
	}
};


t_lalrastbaseptr Parser::Parse(const std::vector<t_toknode>& input) const
{
	ParseStack<std::size_t> states;
	ParseStack<t_lalrastbaseptr> symbols;

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
			t_lalrastbaseptr sym = _symbols.top();
			if(sym->IsTerminal())
				ostr << "term ";
			else
				ostr << "nonterm ";

			ostr << sym->GetTableIndex();
			if(sym->IsTerminal() && std::isprint(sym->GetId()))
				ostr << " ('" << char(sym->GetId()) << "')";

			_symbols.pop();

			if(_symbols.size() > 0)
				ostr << ", ";
		}
		ostr << "." << std::endl;
	};

	// starting state
	states.push(0);
	std::size_t inputidx = 0;

	// next token
	t_toknode curtok = input[inputidx++];

	// debug output of current token
	auto print_input_token = [&inputidx, &curtok](std::ostream& ostr)
	{
		ostr << "\tCurrent token [" << inputidx-1 << "]"
			<< ": " << curtok->GetId();
		if(std::isprint(curtok->GetId()))
			ostr << " = '" << char(curtok->GetId()) << "'";
		ostr << " (terminal index " << curtok->GetTableIndex() << ")."
			<< std::endl;
	};

	const bool has_partial_tables =
		m_tabPartialsRulesTerm && m_tabPartialsMatchLenTerm &&
		m_tabPartialsRulesNonTerm && m_tabPartialsMatchLenNonTerm;

	while(true)
	{
		// current state and rule
		std::size_t topstate = states.top();
		std::size_t newstate = (*m_tabActionShift)(topstate, curtok->GetTableIndex());
		std::size_t newrule = (*m_tabActionReduce)(topstate, curtok->GetTableIndex());

		// partial match
		auto get_partial_rules = [this, &has_partial_tables,
			&topstate, &curtok, &symbols](bool term)
			-> std::tuple<std::optional<std::size_t>, std::optional<std::size_t>>
		{
			if(!has_partial_tables)
				return std::make_tuple(std::nullopt, std::nullopt);

			std::optional<std::size_t> partialrule, partialmatchlen;

			if(term)
			{
				// look for terminal transitions
				partialrule = (*m_tabPartialsRulesTerm)(topstate, curtok->GetTableIndex());
				partialmatchlen = (*m_tabPartialsMatchLenTerm)(topstate, curtok->GetTableIndex());
			}
			else
			{
				// otherwise look for nonterminal transitions
				if(symbols.size() && !symbols.top()->IsTerminal())
				{
					const std::size_t idx = symbols.top()->GetTableIndex();
					partialrule = (*m_tabPartialsRulesNonTerm)(topstate, idx);
					partialmatchlen = (*m_tabPartialsMatchLenNonTerm)(topstate, idx);
				}
			}

			return std::make_tuple(partialrule, partialmatchlen);
		};

		auto apply_partial_rules = [this, &symbols, &get_partial_rules](bool term)
		{
			auto [partialrule, partialmatchlen] = get_partial_rules(term);

			if(!partialrule || *partialrule == ERROR_VAL)
				return;
			if(m_debug)
			{
				std::cout << "\tPartially matched rule " << *partialrule
					<< " of length " << *partialmatchlen
					<< "." << std::endl;
			}

			// execute partial semantic rule
			(*m_semantics)[*partialrule](false, symbols.topN<std::vector>(*partialmatchlen));
		};


		auto print_active_state = [&topstate, &print_input_token, &print_stacks]
			(std::ostream& ostr)
		{
			ostr << "\nState " << topstate << " active." << std::endl;
			print_input_token(ostr);
			print_stacks(ostr);
		};

		if(m_debug)
			print_active_state(std::cout);

		// neither a shift nor a reduce defined
		if(newstate == ERROR_VAL && newrule == ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Undefined shift and reduce entries"
				<< " from state " << topstate << "."
				<< " Current token id is " << curtok->GetId();
			if(std::isprint(curtok->GetId()))
				ostrErr << " = '" << char(curtok->GetId()) << "'";
			ostrErr << get_line_numbers(curtok) << ".";

			throw std::runtime_error(ostrErr.str());
		}

		// both a shift and a reduce would be possible
		else if(newstate != ERROR_VAL && newrule != ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Shift/reduce conflict between shift"
				<< " from state " << topstate << " to state " << newstate
				<< " and reduce using rule " << newrule << "."
				<< " Current token id is " << curtok->GetId();
			if(std::isprint(curtok->GetId()))
				ostrErr << " = '" << char(curtok->GetId()) << "'";
			ostrErr << get_line_numbers(curtok) << ".";

			throw std::runtime_error(ostrErr.str());
		}

		// accept
		else if(newrule == ACCEPT_VAL)
		{
			if(m_debug)
				std::cout << "\tAccepting." << std::endl;

			return symbols.top();
		}

		// partial rules
		apply_partial_rules(true);

		// shift
		if(newstate != ERROR_VAL)
		{
			if(m_debug)
			{
				std::cout << "\tShifting state " << newstate
					<< " (pushing to state stack)"
					<< "." << std::endl;
			}

			if(inputidx >= input.size())
			{
				std::ostringstream ostrErr;
				ostrErr << "Input buffer underflow"
					<< get_line_numbers(curtok)
					<< ".";
				throw std::runtime_error(ostrErr.str());
			}

			states.push(newstate);
			symbols.emplace(std::move(curtok));

			// next token
			curtok = input[inputidx++];
		}

		// reduce
		else if(newrule != ERROR_VAL)
		{
			std::size_t numSyms = (*m_numRhsSymsPerRule)[newrule];
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
			args.reserve(numSyms);
			for(std::size_t arg=0; arg<numSyms; ++arg)
			{
				args.emplace_back(std::move(symbols.top()));

				symbols.pop();
				states.pop();
			}

			if(args.size() > 1)
				std::reverse(args.begin(), args.end());

			// execute semantic rule
			t_lalrastbaseptr reducedSym = (*m_semantics)[newrule](true, args);
			reducedSym->SetTableIndex((*m_vecLhsIndices)[newrule]);

			topstate = states.top();
			if(m_debug)
				print_active_state(std::cout);

			std::size_t jumpstate = (*m_tabJump)(topstate, reducedSym->GetTableIndex());
			symbols.emplace(std::move(reducedSym));

			// partial rules
			apply_partial_rules(false);

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
