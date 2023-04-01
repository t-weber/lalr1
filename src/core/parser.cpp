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


namespace lalr1 {


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
	m_starting_state = parser.m_starting_state;
	m_accepting_rule = parser.m_accepting_rule;
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
static void print_stacks(
	const ParseStack<t_state_id>& states,
	const ParseStack<t_astbaseptr>& symbols,
#ifndef LALR1_DONT_USE_SYMBOL_EXP
	const ParseStack<t_index>& symbols_exp,
#endif
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
#ifndef LALR1_DONT_USE_SYMBOL_EXP
	auto iter_exp = symbols_exp.rbegin();
#endif
	for(auto iter = symbols.rbegin(); iter != symbols.rend();
		std::advance(iter, 1)
#ifndef LALR1_DONT_USE_SYMBOL_EXP
		, std::advance(iter_exp, 1)
#endif
	)
	{
		const t_astbaseptr& sym = (*iter);
		if(!sym)
		{
#ifndef LALR1_DONT_USE_SYMBOL_EXP
			const t_index exp_sym_id = *iter_exp;
			ostr << exp_sym_id << " [exp nt], ";
#else
			std::cout << "nullptr [exp nt], ";
#endif
			continue;
		}

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
	} // end symbols loop
	ostr << "." << std::endl;
}


/**
 * print a terminal
 */
static void print_token(const t_astbaseptr& tok, std::ostream& ostr,
	std::optional<t_symbol_id> end = std::nullopt)
{
	t_symbol_id tok_id = tok->GetId();
	if(end && tok_id == *end)
		ostr << "end";
	else
		ostr << tok_id;
	if(isprintable(tok_id))
		ostr << " = '" << char(tok_id) << "'";
	ostr << " (terminal index " << tok->GetTableIndex() << ")";
}


/**
 * debug output of current token
 */
static void print_input_token(t_index inputidx, const t_toknode& curtok,
	std::ostream& ostr, std::optional<t_symbol_id> end = std::nullopt)
{
	ostr << "\tCurrent token [" << inputidx-1 << "]" << ": ";
	print_token(curtok, ostr, end);
	ostr << "." << std::endl;
}


/**
 * get the top terminal from the symbols stack (this serves as lookback symbol)
 */
static t_astbaseptr get_top_term(const ParseStack<t_astbaseptr>& symbols)
{
	for(auto iter = symbols.rbegin(); iter != symbols.rend(); std::advance(iter, 1))
	{
		const t_astbaseptr& sym = (*iter);
		if(!sym)
			continue;

		if(sym->IsTerminal())
			return sym;
	}

	return nullptr;
}


/**
 * get the semantic rule index and the matching length of a partial match
 */
std::tuple<std::optional<t_index>, std::optional<std::size_t>>
Parser::GetPartialRule(
	t_state_id topstate,
	const t_toknode& curtok,
	const ParseStack<t_astbaseptr>& symbols,
#ifndef LALR1_DONT_USE_SYMBOL_EXP
	const ParseStack<t_index>& symbols_exp,
#endif
	bool term) const
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
	else if(symbols.size())
	{
		// otherwise look for nonterminal transitions
		bool topsym_isterm;
		t_index topsym_idx;

		const t_astbaseptr& topsym = symbols.top();
		if(topsym)
		{
			topsym_isterm = topsym->IsTerminal();
			topsym_idx = topsym->GetTableIndex();
		}
		else
		{
#ifndef LALR1_DONT_USE_SYMBOL_EXP
			topsym_isterm = false;
			topsym_idx = symbols_exp.top();
#else
			throw std::runtime_error("No lhs symbol id available in state "
				+ std::to_string(topstate) + ".");
#endif
		}

		if(!topsym_isterm)
		{
			partialrule_idx = (*m_tabPartialsRulesNonTerm)(topstate, topsym_idx);
			partialmatchlen = (*m_tabPartialsMatchLenNonTerm)(topstate, topsym_idx);
		}
	}

	return std::make_tuple(partialrule_idx, partialmatchlen);
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
	if(idx == ACCEPT_VAL)
		idx = m_accepting_rule;

	if(auto iter = m_mapSemanticIdx_inv.find(idx); iter != m_mapSemanticIdx_inv.end())
		return iter->second;

	// just return the original index if no corresponding id is found
	return static_cast<t_semantic_id>(idx);
}


/**
 * parse the input tokens using the lalr(1) tables
 */
t_astbaseptr Parser::Parse(const t_toknodes& input) const
{
	// parser stacks
	// Note that m_symbols_exp is only needed in the case a semantic rule
	// returns a nullptr and thus doesn't provide a symbol id.
	// One could alternatively fully separate the symbol id tracking and the
	// user-provided symbol lvalue (like in the external scripting modules).
	ParseStack<t_state_id> states;     // state number stack
	ParseStack<t_astbaseptr> symbols;  // symbol stack
#ifndef LALR1_DONT_USE_SYMBOL_EXP
	ParseStack<t_index> symbols_exp;   // expected symbol table index stack
#endif

	// starting state
	states.push(m_starting_state);
	t_index inputidx = 0;

	// corresponding partial rule matches have the same handle
	t_index cur_rule_handle = 0;

	// next token
	t_toknode curtok = input[inputidx++];

	// already seen partial rules
	t_active_rules active_rules;

	// run the shift-reduce parser
	while(true)
	{
		// current state and rule
		t_state_id topstate = states.top();
		t_state_id newstate = (*m_tabActionShift)(topstate, curtok->GetTableIndex());
		t_index rule_idx = (*m_tabActionReduce)(topstate, curtok->GetTableIndex());
		t_semantic_id rule_id = GetRuleId(rule_idx);

		// debug-print the current parser state
		auto print_active_state = [this,
			&states, &symbols,
#ifndef LALR1_DONT_USE_SYMBOL_EXP
			&symbols_exp,
#endif
			&topstate, &inputidx, &curtok](std::ostream& ostr)
		{
			// print state
			ostr << "\nState " << topstate << " active." << std::endl;

			// print lookahead
			print_input_token(inputidx, curtok, ostr, m_end);

			// print lookback
			if(t_astbaseptr lookback = get_top_term(symbols); lookback)
			{
				ostr << "\tLookback token: ";
				print_token(lookback, ostr, m_end);
				ostr << "." << std::endl;
			}

			// print stacks
			print_stacks(states, symbols,
#ifndef LALR1_DONT_USE_SYMBOL_EXP
			symbols_exp,
#endif
			ostr);
		};

		// run a partial rule related to either a terminal or a nonterminal transition
		auto apply_partial_rule = [this, &topstate, &curtok, &symbols,
#ifndef LALR1_DONT_USE_SYMBOL_EXP
			&symbols_exp,
#endif
			&active_rules, &cur_rule_handle](bool is_term)
		{
			bool before_shift = is_term;  // before jump otherwise

			auto [partialrule_idx, partialmatchlen] = this->GetPartialRule(
				topstate, curtok, symbols,
#ifndef LALR1_DONT_USE_SYMBOL_EXP
				symbols_exp,
#endif
				is_term);

			std::size_t arglen = partialmatchlen ? *partialmatchlen : 0;
			// directly count the following lookahead terminal
			if(before_shift && partialmatchlen)
				++*partialmatchlen;

			if(!partialrule_idx || *partialrule_idx == ERROR_VAL)
				return;

			t_semantic_id partialrule_id = GetRuleId(*partialrule_idx);
			bool already_seen_active_rule = false;
			bool insert_new_active_rule = false;
			int seen_tokens_old = -1;

			t_active_rules::iterator iter_active_rule = active_rules.find(partialrule_id);
			if(iter_active_rule != active_rules.end())
			{
				t_active_rule_stack& rulestack = iter_active_rule->second;
				if(!rulestack.empty())
				{
					ActiveRule& active_rule = rulestack.top();
					seen_tokens_old = int(active_rule.seen_tokens);

					if(before_shift)
					{
						if(active_rule.seen_tokens < *partialmatchlen)
							active_rule.seen_tokens = *partialmatchlen;  // update seen length
						else
							insert_new_active_rule = true;               // start of new rule
					}
					else  // before jump
					{
						// new active rules are only pushed before shifts, (possibly of length 0),
						// not after reductions / before jumps.
						// TODO: verify that this is actually always correct
						if(active_rule.seen_tokens == *partialmatchlen)
							already_seen_active_rule = true;
						else
							active_rule.seen_tokens = *partialmatchlen;  // update seen length
					}
				}
				else
				{
					// no active rule yet
					insert_new_active_rule = true;
				}
			}
			else
			{
				// no active rule yet
				iter_active_rule = active_rules.emplace(
					std::make_pair(partialrule_id, t_active_rule_stack{})).first;
				insert_new_active_rule = true;
			}

			if(insert_new_active_rule)
			{
				seen_tokens_old = -1;

				ActiveRule active_rule{
					.seen_tokens = *partialmatchlen,
					.handle = cur_rule_handle++,
				};

				t_active_rule_stack& rulestack = iter_active_rule->second;
				rulestack.emplace(std::move(active_rule));
			}

			if(!already_seen_active_rule)
			{
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

				t_active_rule_stack& rulestack = iter_active_rule->second;
				ActiveRule& active_rule = rulestack.top();

				// get the arguments for the semantic rule
				std::deque<t_astbaseptr> args = symbols.topN<std::deque>(arglen);

				if(!before_shift || seen_tokens_old < int(*partialmatchlen) - 1)
				{
					// run the semantic rule
					active_rule.retval = rule(false, args, active_rule.retval);
				}

				if(before_shift)
				{
					// since we already know the next terminal in a shift, include it directly
					args.push_back(curtok);

					// run the semantic rule again
					active_rule.retval = rule(false, args, active_rule.retval);
				}

				if(m_debug)
				{
					std::cout << "\tPartially matched rule #" << partialrule_id
						<< " (handle id " << active_rule.handle << ")"
						<< " of length " << *partialmatchlen;
					if(before_shift)
					{
						if(seen_tokens_old < int(*partialmatchlen) - 1)
							std::cout << " and length " << (*partialmatchlen - 1);
						std::cout << " (before terminal)";
					}
					else
					{
						std::cout << " (before non-terminal)";
					}
					std::cout << "." << std::endl;
				}
			}
		};  // apply_partial_rule()

		if(m_debug)
			print_active_state(std::cout);

		bool accepted = false;
		t_toknode accepted_topnode;


		// neither a shift nor a reduce defined
		if(newstate == ERROR_VAL && rule_idx == ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Undefined shift and reduce entries"
				<< " from state " << topstate << ".";

			ostrErr << " Current token id is ";
			print_token(curtok, ostrErr, m_end);
			ostrErr << get_line_numbers(curtok) << ".";

			if(t_astbaseptr lookback = get_top_term(symbols); lookback)
			{
				ostrErr << " Lookback token id is ";
				print_token(lookback, ostrErr, m_end);
				ostrErr << get_line_numbers(lookback) << ".";
			}

			throw std::runtime_error(ostrErr.str());
		}

		// both a shift and a reduce would be possible
		else if(newstate != ERROR_VAL && rule_idx != ERROR_VAL)
		{
			std::ostringstream ostrErr;
			ostrErr << "Shift/reduce conflict between shift"
				<< " from state " << topstate << " to state " << newstate
				<< " and reduce using rule " << rule_id << ".";

			ostrErr << " Current token id is ";
			print_token(curtok, ostrErr, m_end);
			ostrErr << get_line_numbers(curtok) << ".";

			if(t_astbaseptr lookback = get_top_term(symbols); lookback)
			{
				ostrErr << " Lookback token id is ";
				print_token(lookback, ostrErr, m_end);
				ostrErr << get_line_numbers(lookback) << ".";
			}

			throw std::runtime_error(ostrErr.str());
		}

		// accept
		else if(rule_idx == ACCEPT_VAL)
		{
			if(m_debug)
				std::cout << "\tAccepting." << std::endl;

			accepted = true;
			accepted_topnode = symbols.top();
			rule_idx = m_accepting_rule;
		}

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

			// partial rules
			apply_partial_rule(true);

			states.push(newstate);
			symbols.emplace(std::move(curtok));
#ifndef LALR1_DONT_USE_SYMBOL_EXP
			symbols_exp.emplace(t_index{}); // placeholder, as we only track nonterminals with symbols_exp
#endif

			// next token
			curtok = input[inputidx++];
		}

		// reduce
		else if(rule_idx != ERROR_VAL)
		{
			// remove fully reduced rule from active rule stack
			ActiveRule* active_rule = nullptr;
			if(t_active_rules::iterator iter_active_rule = active_rules.find(rule_id);
			   iter_active_rule != active_rules.end())
			{
				t_active_rule_stack& rulestack = iter_active_rule->second;
				if(!rulestack.empty())
				{
					t_active_rule_stack& rulestack = iter_active_rule->second;
					ActiveRule& _active_rule = rulestack.top();
					active_rule = &_active_rule;

					rulestack.pop();
				}
			}

			std::size_t numSyms = (*m_numRhsSymsPerRule)[rule_idx];
			if(m_debug)
			{
				std::cout << "\tReducing " << numSyms
					<< " symbol(s) via rule #" << rule_id;
				if(active_rule)
					std::cout << " (handle id " << active_rule->handle << ")";
				std::cout << " (popping " << numSyms << " element(s) from stacks,"
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
#ifndef LALR1_DONT_USE_SYMBOL_EXP
				symbols_exp.pop();
#endif
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

			t_astbaseptr retval = nullptr;
			if(active_rule)
				retval = active_rule->retval;
			t_astbaseptr reducedSym = rule(true, args, retval);

			// set return symbol type
			const t_index lhs_expected_index = (*m_vecLhsIndices)[rule_idx];
			t_index lhs_index = lhs_expected_index;
			if(reducedSym)
			{
				lhs_index = reducedSym->GetTableIndex();
				if(lhs_index != lhs_expected_index)
				{
					/*if(m_debug)
					{
						std::cerr
							<< "Warning: Expected return symbol index " << lhs_expected_index
							<< " in semantic rule #" << rule_idx
							<< ", but received index " << lhs_index << "."
							<< std::endl;
					}*/

					lhs_index = lhs_expected_index;
					reducedSym->SetTableIndex(lhs_index);
				}
			}

			// no more needed if the grammar has already been accepted
			if(!accepted)
			{
				topstate = states.top();
				if(m_debug)
					print_active_state(std::cout);

				symbols.emplace(std::move(reducedSym));
#ifndef LALR1_DONT_USE_SYMBOL_EXP
				symbols_exp.emplace(lhs_expected_index);
#endif

				// partial rules
				apply_partial_rule(false);

				t_state_id jumpstate = (*m_tabJump)(topstate, lhs_index);
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

		if(accepted)
			return accepted_topnode;
	}

	// parsing failed
	if(m_debug)
		std::cout << "\tNot accepting." << std::endl;
	return nullptr;
}

} // namespace lalr1
