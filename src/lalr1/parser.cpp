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

#include <sstream>
#include <unordered_set>
#include <functional>

#include <boost/functional/hash.hpp>


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
 * debug output of the stacks
 */
static void print_stacks(const ParseStack<std::size_t>& states,
	const ParseStack<t_lalrastbaseptr>& symbols,
	std::ostream& ostr)
{
	ostr << "\tState stack [" << states.size() << "]: ";
	std::size_t i = 0;
	for(auto iter = states.rbegin(); iter != states.rend(); std::advance(iter, 1))
	{
		ostr << (*iter);
		if(i < states.size() - 1)
			ostr << ", ";
		++i;
	}
	ostr << "." << std::endl;

	ostr << "\tSymbol stack [" << symbols.size() << "]: ";
	i = 0;
	for(auto iter = symbols.rbegin(); iter != symbols.rend(); std::advance(iter, 1))
	{
		const t_lalrastbaseptr& sym = (*iter);

		ostr << sym->GetTableIndex();
		if(sym->IsTerminal() && std::isprint(sym->GetId()))
			ostr << " ('" << char(sym->GetId()) << "')";

		if(sym->IsTerminal())
			ostr << " [t]";
		else
			ostr << " [nt]";

		if(i < symbols.size() - 1)
			ostr << ", ";
		++i;
	}
	ostr << "." << std::endl;
}


/**
 * debug output of current token
 */
static void print_input_token(std::size_t inputidx, const t_toknode& curtok,
	std::ostream& ostr)
{
	ostr << "\tCurrent token [" << inputidx-1 << "]"
		<< ": " << curtok->GetId();
	if(std::isprint(curtok->GetId()))
		ostr << " = '" << char(curtok->GetId()) << "'";
	ostr << " (terminal index " << curtok->GetTableIndex() << ")."
		<< std::endl;
};


/**
 * get the rule number and the matching length of a partial match
 */
std::tuple<std::optional<std::size_t>, std::optional<std::size_t>>
Parser::GetPartialRules(std::size_t topstate, const t_toknode& curtok,
	const ParseStack<t_lalrastbaseptr>& symbols, bool term) const
{
	const bool has_partial_tables =
		m_tabPartialsRulesTerm && m_tabPartialsMatchLenTerm &&
		m_tabPartialsRulesNonTerm && m_tabPartialsMatchLenNonTerm;
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
}


/**
 * get a unique identifier for a partial rule
 */
std::size_t Parser::GetPartialRuleHash(std::size_t rule, std::size_t len,
	const ParseStack<std::size_t>& states, const ParseStack<t_lalrastbaseptr>& symbols) const
{
	std::size_t total_hash = std::hash<std::size_t>{}(rule);
	boost::hash_combine(total_hash, std::hash<std::size_t>{}(len));

	for(std::size_t state : states)
		boost::hash_combine(total_hash, std::hash<std::size_t>{}(state));

	for(const auto& symbol : symbols)
		boost::hash_combine(total_hash, symbol->hash());

	return total_hash;
}


/**
 * parse the input tokens using the lalr(1) tables
 */
t_lalrastbaseptr Parser::Parse(const std::vector<t_toknode>& input) const
{
	// parser stacks
	ParseStack<std::size_t> states;
	ParseStack<t_lalrastbaseptr> symbols;

	// starting state
	states.push(0);
	std::size_t inputidx = 0;

	// next token
	t_toknode curtok = input[inputidx++];

	// already seen partial rules
	std::unordered_set<std::size_t> already_seen_partials;

	// run the shift-reduce parser
	while(true)
	{
		// current state and rule
		std::size_t topstate = states.top();
		std::size_t newstate = (*m_tabActionShift)(topstate, curtok->GetTableIndex());
		std::size_t newrule = (*m_tabActionReduce)(topstate, curtok->GetTableIndex());

		// debug-print the current parser state
		auto print_active_state = [&topstate, &inputidx, &curtok,
			&states, &symbols](std::ostream& ostr)
		{
			ostr << "\nState " << topstate << " active." << std::endl;
			print_input_token(inputidx, curtok, ostr);
			print_stacks(states, symbols, ostr);
		};

		// run a partial rule related to either a terminal or a nonterminal transition
		auto apply_partial_rule = [this, &topstate, &curtok, &symbols, &states,
			&already_seen_partials](bool term)
		{
			auto [partialrule, partialmatchlen] = this->GetPartialRules(
				topstate, curtok, symbols, term);

			if(!partialrule || *partialrule == ERROR_VAL)
				return;

			if(std::size_t hash_partial = GetPartialRuleHash(*partialrule, *partialmatchlen, states, symbols);
				!already_seen_partials.contains(hash_partial))
			{
				if(!m_semantics || partialrule >= m_semantics->size())
				{
					throw std::runtime_error("No semantic rule #" +
						std::to_string(*partialrule) + " defined.");
				}

				// execute partial semantic rule if this hasn't been done before
				const t_semanticrule& rule = (*m_semantics)[*partialrule];
				if(!rule)
				{
					throw std::runtime_error("Invalid semantic rule #" +
						std::to_string(*partialrule) + ".");
				}
				rule(false, symbols.topN<std::deque>(*partialmatchlen));

				already_seen_partials.insert(hash_partial);

				if(m_debug)
				{
					std::cout << "\tPartially matched rule " << *partialrule
						<< " of length " << *partialmatchlen
						<< "." << std::endl;
				}
			}
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
		apply_partial_rule(true);

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
			t_semanticargs args;
			//args.reserve(numSyms);
			for(std::size_t arg=0; arg<numSyms; ++arg)
			{
				args.emplace_front(std::move(symbols.top()));

				symbols.pop();
				states.pop();
			}
			//if(args.size() > 1)
			//	std::reverse(args.begin(), args.end());

			if(!m_semantics || newrule >= m_semantics->size())
			{
				throw std::runtime_error("No semantic rule #" +
					std::to_string(newrule) + " defined.");
			}

			// execute semantic rule
			const t_semanticrule& rule = (*m_semantics)[newrule];
			if(!rule)
			{
				throw std::runtime_error("Invalid semantic rule #" +
					std::to_string(newrule) + ".");
			}
			t_lalrastbaseptr reducedSym = rule(true, args);

			// set return symbol type
			reducedSym->SetTableIndex((*m_vecLhsIndices)[newrule]);

			topstate = states.top();
			if(m_debug)
				print_active_state(std::cout);

			std::size_t jumpstate = (*m_tabJump)(topstate, reducedSym->GetTableIndex());
			symbols.emplace(std::move(reducedSym));

			// partial rules
			apply_partial_rule(false);

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

	// parsing failed
	if(m_debug)
		std::cout << "\tNot accepting." << std::endl;
	return nullptr;
}
