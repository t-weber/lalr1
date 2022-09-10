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

	// next token
	t_toknode curtok = input[inputidx++];

	// debug output of current token
	auto print_input_token = [&inputidx, &curtok](std::ostream& ostr)
	{
		ostr << "\tCurrent token [" << inputidx-1 << "]"
			<< ": " << curtok->GetId();
		if(std::isprint(curtok->GetId()))
			ostr << " = '" << char(curtok->GetId()) << "'";
		ostr << " (table index " << curtok->GetTableIdx() << ")."
			<< std::endl;
	};

	const bool has_partial_tables =
		m_tabPartialsRulesTerm && m_tabPartialsMatchLenTerm &&
		m_tabPartialsRulesNonTerm && m_tabPartialsMatchLenNonTerm;

	while(true)
	{
		// current state and rule
		std::size_t topstate = states.top();
		std::size_t newstate = (*m_tabActionShift)(topstate, curtok->GetTableIdx());
		std::size_t newrule = (*m_tabActionReduce)(topstate, curtok->GetTableIdx());

		// partial match
		std::optional<std::size_t> partialrule, partialmatchlen;

		auto get_partial_rules = [this, &has_partial_tables,
			&partialrule, &partialmatchlen, &topstate, &curtok, &symbols]()
		{
			if(has_partial_tables)
			{
				// look for terminal transitions
				partialrule = (*m_tabPartialsRulesTerm)(topstate, curtok->GetTableIdx());
				partialmatchlen = (*m_tabPartialsMatchLenTerm)(topstate, curtok->GetTableIdx());

				// otherwise look for nonterminal transitions
				if(partialrule == ERROR_VAL && symbols.size() && !symbols.top()->IsTerminal())
				{
					partialrule = (*m_tabPartialsRulesNonTerm)(topstate, symbols.top()->GetTableIdx());
					partialmatchlen = (*m_tabPartialsMatchLenNonTerm)(topstate, symbols.top()->GetTableIdx());
				}
			}
		};

		auto apply_partial_rules = [this, &partialrule, &partialmatchlen, &symbols]()
		{
			if(!partialrule || *partialrule == ERROR_VAL)
				return;
			if(m_debug)
			{
				std::cout << "\tPartially matched rule " << *partialrule
					<< " of length " << *partialmatchlen
					<< "." << std::endl;
			}

			// take the symbols from the stack and create an
			// argument vector for the semantic rule
			std::stack<t_lalrastbaseptr> symbols_cp = symbols;
			std::vector<t_lalrastbaseptr> args;
			args.reserve(*partialmatchlen);
			for(std::size_t arg=0; arg<*partialmatchlen; ++arg)
			{
				args.emplace_back(std::move(symbols_cp.top()));
				symbols_cp.pop();
			}

			// execute partial semantic rule
			(*m_semantics)[*partialrule](false, args);
		};


		auto print_active_state = [this, &topstate, &print_input_token, &print_stacks]
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
		get_partial_rules();
		apply_partial_rules();

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
			reducedSym->SetTableIdx((*m_vecLhsIndices)[newrule]);

			topstate = states.top();
			if(m_debug)
				print_active_state(std::cout);

			std::size_t jumpstate = (*m_tabJump)(topstate, reducedSym->GetTableIdx());
			symbols.emplace(std::move(reducedSym));

			// partial rules
			get_partial_rules();
			apply_partial_rules();

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
