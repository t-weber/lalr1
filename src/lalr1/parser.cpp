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
#include <optional>
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
static void print_stacks(const ParseStack<t_state_id>& states,
	const ParseStack<t_lalrastbaseptr>& symbols,
	std::ostream& ostr)
{
	ostr << "\tState stack [" << states.size() << "]: ";
	t_index i = 0;
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
		if(sym->IsTerminal() && isprintable(sym->GetId()))
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
static void print_input_token(t_index inputidx, const t_toknode& curtok,
	std::ostream& ostr, std::optional<t_symbol_id> end = std::nullopt)
{
	t_symbol_id curtok_id = curtok->GetId();
	ostr << "\tCurrent token [" << inputidx-1 << "]" << ": ";
	if(end && curtok_id == *end)
		ostr << "end";
	else
		ostr << curtok_id;
	if(isprintable(curtok_id))
		ostr << " = '" << char(curtok_id) << "'";
	ostr << " (terminal index " << curtok->GetTableIndex() << ")."
		<< std::endl;
};


/**
 * get the semantic rule index and the matching length of a partial match
 */
std::tuple<std::optional<t_index>, std::optional<std::size_t>>
Parser::GetPartialRules(t_state_id topstate, const t_toknode& curtok,
	const ParseStack<t_lalrastbaseptr>& symbols, bool term) const
{
	const bool has_partial_tables =
		m_tabPartialsRulesTerm && m_tabPartialsMatchLenTerm &&
		m_tabPartialsRulesNonTerm && m_tabPartialsMatchLenNonTerm;
	if(!has_partial_tables)
		return std::make_tuple(std::nullopt, std::nullopt);

	std::optional<t_index> partialrule_idx;
	std::optional<std::size_t> partialmatchlen;

	if(term)
	{
		// look for terminal transitions
		partialrule_idx = (*m_tabPartialsRulesTerm)(topstate, curtok->GetTableIndex());
		partialmatchlen = (*m_tabPartialsMatchLenTerm)(topstate, curtok->GetTableIndex());
	}
	else
	{
		// otherwise look for nonterminal transitions
		if(symbols.size() && !symbols.top()->IsTerminal())
		{
			const t_index idx = symbols.top()->GetTableIndex();
			partialrule_idx = (*m_tabPartialsRulesNonTerm)(topstate, idx);
			partialmatchlen = (*m_tabPartialsMatchLenNonTerm)(topstate, idx);
		}
	}

	return std::make_tuple(partialrule_idx, partialmatchlen);
}


/**
 * get a unique identifier for a partial rule
 */
t_hash Parser::GetPartialRuleHash(t_index rule_idx, std::size_t len,
	const ParseStack<t_state_id>& states,
	const ParseStack<t_lalrastbaseptr>& symbols) const
{
	t_hash total_hash = std::hash<t_index>{}(rule_idx);
	boost::hash_combine(total_hash, std::hash<std::size_t>{}(len));

	for(t_state_id state : states)
		boost::hash_combine(total_hash, std::hash<t_state_id>{}(state));

	for(const auto& symbol : symbols)
		boost::hash_combine(total_hash, symbol->hash());

	return total_hash;
}


/**
 * invert a map, exchanging keys and values
 */
template<class t_key, class t_val, template<class ...> class t_map>
t_map<t_val, t_key> invert_map(const t_map<t_key, t_val>& map)
{
	t_map<t_val, t_key> map_inv;

	for(const auto& [key, val] : map)
		map_inv.emplace(std::make_pair(val, key));

	return map_inv;
}


void Parser::SetSemanticIdxMap(const t_mapSemanticIdIdx* map)
{
	m_mapSemanticIdx = map;
	m_mapSemanticIdx_inv.clear();

	if(m_mapSemanticIdx)
		m_mapSemanticIdx_inv = invert_map(*m_mapSemanticIdx);
}


/**
 * get a rule id from a rule index
 */
t_semantic_id Parser::GetRuleId(t_index idx) const
{
	if(auto iter = m_mapSemanticIdx_inv.find(idx); iter != m_mapSemanticIdx_inv.end())
		return iter->second;

	// just return the original index if no corresponding id is found
	return static_cast<t_semantic_id>(idx);
}


/**
 * parse the input tokens using the lalr(1) tables
 */
t_lalrastbaseptr Parser::Parse(const t_toknodes& input) const
{
	// parser stacks
	ParseStack<t_state_id> states;
	ParseStack<t_lalrastbaseptr> symbols;

	// starting state
	states.push(0);
	t_index inputidx = 0;

	// next token
	t_toknode curtok = input[inputidx++];

	// already seen partial rules
	std::unordered_set<t_hash> already_seen_partials;

	// run the shift-reduce parser
	while(true)
	{
		// current state and rule
		t_state_id topstate = states.top();
		t_state_id newstate = (*m_tabActionShift)(topstate, curtok->GetTableIndex());
		t_index rule_idx = (*m_tabActionReduce)(topstate, curtok->GetTableIndex());
		t_semantic_id rule_id = GetRuleId(rule_idx);

		// debug-print the current parser state
		auto print_active_state = [&topstate, &inputidx, &curtok,
			&states, &symbols, this](std::ostream& ostr)
		{
			ostr << "\nState " << topstate << " active." << std::endl;
			print_input_token(inputidx, curtok, ostr, m_end);
			print_stacks(states, symbols, ostr);
		};

		// run a partial rule related to either a terminal or a nonterminal transition
		auto apply_partial_rule = [this, &topstate, &curtok, &symbols, &states,
			&already_seen_partials](bool term)
		{
			auto [partialrule_idx, partialmatchlen] = this->GetPartialRules(
				topstate, curtok, symbols, term);

			if(!partialrule_idx || *partialrule_idx == ERROR_VAL)
				return;

			if(t_hash hash_partial = GetPartialRuleHash(
				*partialrule_idx, *partialmatchlen, states, symbols);
				!already_seen_partials.contains(hash_partial))
			{
				t_semantic_id partialrule_id = GetRuleId(*partialrule_idx);

				if(!m_semantics || !m_semantics->contains(partialrule_id))
				{
					throw std::runtime_error("No semantic rule #" +
						std::to_string(partialrule_id) + " defined.");
				}

				// execute partial semantic rule if this hasn't been done before
				const t_semanticrule& rule = m_semantics->at(partialrule_id);
				if(!rule)
				{
					throw std::runtime_error("Invalid semantic rule #" +
						std::to_string(partialrule_id) + ".");
				}
				rule(false, symbols.topN<std::deque>(*partialmatchlen));

				already_seen_partials.insert(hash_partial);

				if(m_debug)
				{
					std::cout << "\tPartially matched rule #" << partialrule_id
						<< " of length " << *partialmatchlen
						<< "." << std::endl;
				}
			}
		};

		if(m_debug)
			print_active_state(std::cout);

		// neither a shift nor a reduce defined
		if(newstate == ERROR_VAL && rule_idx == ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Undefined shift and reduce entries"
				<< " from state " << topstate << "."
				<< " Current token id is " << curtok->GetId();
			if(isprintable(curtok->GetId()))
				ostrErr << " = '" << char(curtok->GetId()) << "'";
			ostrErr << get_line_numbers(curtok) << ".";

			throw std::runtime_error(ostrErr.str());
		}

		// both a shift and a reduce would be possible
		else if(newstate != ERROR_VAL && rule_idx != ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Shift/reduce conflict between shift"
				<< " from state " << topstate << " to state " << newstate
				<< " and reduce using rule " << rule_id << "."
				<< " Current token id is " << curtok->GetId();
			if(isprintable(curtok->GetId()))
				ostrErr << " = '" << char(curtok->GetId()) << "'";
			ostrErr << get_line_numbers(curtok) << ".";

			throw std::runtime_error(ostrErr.str());
		}

		// accept
		else if(rule_idx == ACCEPT_VAL)
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
		else if(rule_idx != ERROR_VAL)
		{
			std::size_t numSyms = (*m_numRhsSymsPerRule)[rule_idx];
			if(m_debug)
			{
				std::cout << "\tReducing " << numSyms
					<< " symbol(s) via rule #" << rule_id
					<< " (popping " << numSyms << " element(s) from stacks,"
					<< " pushing result to symbol stack)"
					<< "." << std::endl;
			}

			// take the symbols from the stack and create an
			// argument vector for the semantic rule
			t_semanticargs args;
			//args.reserve(numSyms);
			for(t_index arg=0; arg<numSyms; ++arg)
			{
				args.emplace_front(std::move(symbols.top()));

				symbols.pop();
				states.pop();
			}
			//if(args.size() > 1)
			//	std::reverse(args.begin(), args.end());

			if(!m_semantics || !m_semantics->contains(rule_id))
			{
				throw std::runtime_error("No semantic rule #" +
					std::to_string(rule_id) + " defined.");
			}

			// execute semantic rule
			const t_semanticrule& rule = m_semantics->at(rule_id);
			if(!rule)
			{
				throw std::runtime_error("Invalid semantic rule #" +
					std::to_string(rule_id) + ".");
			}
			t_lalrastbaseptr reducedSym = rule(true, args);

			// set return symbol type
			reducedSym->SetTableIndex((*m_vecLhsIndices)[rule_idx]);

			topstate = states.top();
			if(m_debug)
				print_active_state(std::cout);

			t_state_id jumpstate = (*m_tabJump)(topstate, reducedSym->GetTableIndex());
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
