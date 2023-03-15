/**
 * symbols
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 07-jun-2020
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 *	- https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 */

#include "symbol.h"
#include "common.h"
#include "options.h"

#include <boost/functional/hash.hpp>


namespace lalr1 {


// special terminal symbols
const TerminalPtr g_eps = std::make_shared<Terminal>(EPS_IDENT, "\xce\xb5", true, false);
const TerminalPtr g_end = std::make_shared<Terminal>(END_IDENT, "\xcf\x89", false, true);


// ----------------------------------------------------------------------------
// symbol base class
// ----------------------------------------------------------------------------

Symbol::Symbol(t_symbol_id id, const std::string& strid, bool bEps, bool bEnd)
	: std::enable_shared_from_this<Symbol>{},
	  m_id{id}, m_strid{strid}, m_iseps{bEps}, m_isend{bEnd}
{
	// if no string id is given, use the numeric id
	if(m_strid == "")
		m_strid = std::to_string(id);
}

Symbol::Symbol(const Symbol& other) : std::enable_shared_from_this<Symbol>{}
{
	this->operator=(other);
}


Symbol::~Symbol()
{}


const Symbol& Symbol::operator=(const Symbol& other)
{
	this->m_id = other.m_id;
	this->m_strid = other.m_strid;
	this->m_iseps = other.m_iseps;
	this->m_isend = other.m_isend;

	return *this;
}


bool Symbol::operator==(const Symbol& other) const
{
	return this->hash() == other.hash();
}


t_hash Symbol::HashSymbol::operator()(const SymbolPtr& sym) const
{
	return sym->hash();
}


bool Symbol::CompareSymbolsEqual::operator()(
	const SymbolPtr& sym1, const SymbolPtr& sym2) const
{
	return sym1->hash() == sym2->hash();
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// terminal symbol
// ----------------------------------------------------------------------------

Terminal::Terminal(t_symbol_id id, const std::string& strid, bool bEps, bool bEnd)
	: Symbol{id, strid, bEps, bEnd}
{}


Terminal::Terminal(const Terminal& other) : Symbol(other)
{
	this->operator=(other);
}


Terminal::~Terminal()
{}


const Terminal& Terminal::operator=(const Terminal& other)
{
	static_cast<Symbol*>(this)->operator=(*static_cast<const Symbol*>(&other));
	this->m_precedence = other.m_precedence;
	this->m_associativity = other.m_associativity;
	this->m_hash = other.m_hash;

	return *this;
}


void Terminal::SetPrecedence(std::size_t prec)
{
	m_precedence = prec;
}


void Terminal::SetAssociativity(char assoc)
{
	m_associativity = assoc;
}


void Terminal::SetPrecedence(std::size_t prec, char assoc)
{
	SetPrecedence(prec);
	SetAssociativity(assoc);
}


std::optional<std::size_t> Terminal::GetPrecedence() const
{
	return m_precedence;
}


std::optional<char> Terminal::GetAssociativity() const
{
	return m_associativity;
}


void Terminal::print(std::ostream& ostr, bool /*bnf*/) const
{
	ostr << GetStrId();
}


/**
 * calculates a unique hash for the terminal symbol
 */
t_hash Terminal::hash() const
{
	if(m_hash)
		return *m_hash;

	t_hash hashId = std::hash<t_symbol_id>{}(GetId());
	t_hash hashEps = std::hash<bool>{}(IsEps());
	t_hash hashEnd = std::hash<bool>{}(IsEnd());

	m_hash = 0;
	boost::hash_combine(*m_hash, hashId);
	boost::hash_combine(*m_hash, hashEps);
	boost::hash_combine(*m_hash, hashEnd);
	return *m_hash;
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// non-terminal symbol
// ----------------------------------------------------------------------------

NonTerminal::NonTerminal(t_symbol_id id, const std::string& strid)
	: Symbol{id, strid}
{}


NonTerminal::NonTerminal(const NonTerminal& other) : Symbol(other)
{
	this->operator=(other);
}


NonTerminal::~NonTerminal()
{}


const NonTerminal& NonTerminal::operator=(const NonTerminal& other)
{
	static_cast<Symbol*>(this)->operator=(
		*static_cast<const Symbol*>(&other));
	this->m_rules = other.m_rules;
	this->m_semantics = other.m_semantics;
	this->m_hash = other.m_hash;

	return *this;
}


/**
 * adds multiple alternative production rules
 */
void NonTerminal::AddRule(const WordPtr& rule, std::optional<t_semantic_id> semantic_id)
{
	m_rules.emplace_back(rule);
	m_semantics.push_back(semantic_id);
}


/**
 * adds multiple alternative production rules
 */
void NonTerminal::AddRule(const Word& _rule, std::optional<t_semantic_id> semantic_id)
{
	WordPtr rule = std::make_shared<Word>(_rule);
	AddRule(rule, semantic_id);
}


/**
 * adds multiple alternative production rules
 * (non-overloaded helper for scripting interface)
 */
void NonTerminal::AddARule(const WordPtr& rule, t_semantic_id semantic_id)
{
	AddRule(rule, semantic_id);
}


/**
 * number of rules
 */
std::size_t NonTerminal::NumRules() const
{
	return m_rules.size();
}


/**
 * gets a production rule
 */
const WordPtr& NonTerminal::GetRule(t_index idx) const
{
	return m_rules[idx];
}


/*
 * gets a production rule from a semantic id
 */
WordPtr NonTerminal::GetRuleFromSemanticId(t_semantic_id semantic_id) const
{
	for(t_index ruleidx = 0; ruleidx < NumRules(); ++ruleidx)
	{
		std::optional<t_semantic_id> id = GetSemanticRule(ruleidx);
		if(!id)
			continue;

		if(*id == semantic_id)
			return GetRule(ruleidx);
	}

	return nullptr;
}


/**
 * clears rules
 */
void NonTerminal::ClearRules()
{
	m_rules.clear();
}


/**
 * gets a semantic rule id for a given rule number
 */
std::optional<t_semantic_id> NonTerminal::GetSemanticRule(t_index idx) const
{
	return m_semantics[idx];
}


/**
 * removes left recursion
 * returns possibly added non-terminal
 */
NonTerminalPtr NonTerminal::RemoveLeftRecursion(
	t_symbol_id newIdBegin, const std::string& primerule,
	std::optional<t_semantic_id> semantic_id)
{
	std::deque<WordPtr> rulesWithLeftRecursion;
	std::deque<WordPtr> rulesWithoutLeftRecursion;

	for(t_index ruleidx=0; ruleidx<NumRules(); ++ruleidx)
	{
		const WordPtr& rule = this->GetRule(ruleidx);
		if(rule->NumSymbols() >= 1 && rule->GetSymbol(0)->hash() == this->hash())
			rulesWithLeftRecursion.push_back(rule);
		else
			rulesWithoutLeftRecursion.push_back(rule);
	}

	if(rulesWithLeftRecursion.size() == 0)  // no left-recursive productions
		return nullptr;

	NonTerminalPtr newNonTerm = std::make_shared<NonTerminal>(
		this->GetId()+newIdBegin, this->GetStrId()+primerule);

	for(const WordPtr& _word : rulesWithLeftRecursion)
	{
		Word word = *_word;
		word.RemoveSymbol(0);       // remove "this" rule causing left-recursion
		word.AddSymbol(newNonTerm); // make it right-recursive instead

		newNonTerm->AddRule(word, semantic_id);
		if(semantic_id)
			++*semantic_id;
	}

	newNonTerm->AddRule({ g_eps }, semantic_id);
	if(semantic_id)
		++*semantic_id;

	this->ClearRules();
	for(const WordPtr& _word : rulesWithoutLeftRecursion)
	{
		Word word = *_word;
		word.AddSymbol(newNonTerm); // make it right-recursive instead

		this->AddRule(word, semantic_id);
		if(semantic_id)
			++*semantic_id;
	}

	return newNonTerm;
}


/**
 * calculates a unique hash for the non-terminal symbol
 */
t_hash NonTerminal::hash() const
{
	if(m_hash)
		return *m_hash;

	m_hash = std::hash<t_symbol_id>{}(GetId());
	return *m_hash;
}


void NonTerminal::print(std::ostream& ostr, bool bnf) const
{
	std::string lhsrhssep = bnf ? "\t ::=" :  (" " + g_options.GetArrowChar() + "\n");
	std::string rulesep = bnf ? "\t  |  " :  "\t| ";
	std::string rule0sep = bnf ? " " :  "\t  ";

	ostr << GetStrId() << lhsrhssep;
	for(t_index i=0; i<NumRules(); ++i)
	{
		if(i==0)
			ostr << rule0sep;
		else
			ostr << rulesep;

		if(auto rule = GetSemanticRule(i); rule && !bnf)
			ostr << "[rule " << *rule << "] ";

		ostr << (*GetRule(i));
		ostr << "\n";
	}
}


/**
 * does this non-terminal have a rule which produces epsilon?
 */
bool NonTerminal::HasEpsRule() const
{
	for(const WordPtr& rule : m_rules)
	{
		if(rule->NumSymbols()==1 && (*rule)[0]->IsEps())
			return true;
	}
	return false;
}


/**
 * calculates the first set of a nonterminal
 * @see https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 */
t_map_first NonTerminal::CalcFirst(t_map_first_perrule* first_perrule) const
{
	t_map_first map;
	CalcFirst(map, first_perrule);
	return map;
}


/**
 * calculates the first set of a nonterminal
 * @see https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 */
void NonTerminal::CalcFirst(t_map_first& map_first,
	t_map_first_perrule* map_first_perrule,
	std::size_t recurse_depth) const
{
	const NonTerminalPtr& nonterm =
		std::dynamic_pointer_cast<NonTerminal>(
			std::const_pointer_cast<Symbol>(
				shared_from_this()));
	if(!nonterm)
		return;

	// set already calculated?
	// always recalculate if recursive depth is zero, because the FIRST
	// set might be incomplete in case of loops in the production rules
	if(recurse_depth && map_first.contains(nonterm))
		return;

	Terminal::t_terminalset set_first;
	std::deque<Terminal::t_terminalset> set_first_perrule;
	set_first_perrule.resize(nonterm->NumRules());

	// iterate rules
	for(t_index rule_idx=0; rule_idx<nonterm->NumRules(); ++rule_idx)
	{
		const WordPtr& rule = nonterm->GetRule(rule_idx);

		// iterate RHS of rule
		const std::size_t num_all_symbols = rule->NumSymbols();
		for(t_index sym_idx=0; sym_idx<num_all_symbols; ++sym_idx)
		{
			const SymbolPtr& sym = (*rule)[sym_idx];

			// reached terminal symbol -> end
			if(sym->IsTerminal())
			{
				set_first.insert(std::dynamic_pointer_cast<Terminal>(sym));
				set_first_perrule[rule_idx].insert(
					std::dynamic_pointer_cast<Terminal>(sym));
				break;
			}

			// non-terminal
			else
			{
				const NonTerminalPtr& symnonterm =
					std::dynamic_pointer_cast<NonTerminal>(sym);

				// if the rule is left-recursive, ignore calculating the same symbol again
				if(*symnonterm != *nonterm)
					symnonterm->CalcFirst(map_first, map_first_perrule);

				// add first set except eps
				bool has_eps = false;
				for(const TerminalPtr& symprod : map_first[symnonterm])
				{
					bool insert = true;
					if(symprod->IsEps())
					{
						has_eps = true;

						// if last non-terminal is reached -> add epsilon
						insert = (sym_idx == num_all_symbols-1);
					}

					if(insert)
					{
						set_first.insert(std::dynamic_pointer_cast<Terminal>(symprod));
						set_first_perrule[rule_idx].insert(std::dynamic_pointer_cast<Terminal>(symprod));
					}
				}

				// no epsilon in production -> end
				if(!has_eps)
					break;
			}
		}
	}

	map_first[nonterm] = set_first;

	if(map_first_perrule)
		(*map_first_perrule)[nonterm] = set_first_perrule;
}


/**
 * calculates the follow set of a nonterminal
 * @see https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 */
void NonTerminal::CalcFollow(const std::vector<NonTerminalPtr>& allnonterms,
	const NonTerminalPtr& start,
	const t_map_first& map_first, t_map_follow& map_follow,
	std::size_t recurse_depth) const
{
	const NonTerminalPtr& cur_nonterm =
		std::dynamic_pointer_cast<NonTerminal>(
			std::const_pointer_cast<Symbol>(shared_from_this()));
	if(!cur_nonterm)
		return;

	// set already calculated?
	// always recalculate if recursive depth is zero, because the FOLLOW
	// set might be incomplete in case of loops in the production rules
	if(recurse_depth && map_follow.contains(cur_nonterm))
		return;

	Terminal::t_terminalset set_follow;

	// add end symbol as follower to start rule
	if(cur_nonterm == start)
		set_follow.insert(g_end);

	// find current nonterminal in RHS of all production rules (to get following symbols)
	for(const NonTerminalPtr& lhs : allnonterms)
	{
		// iterate rules
		for(t_index rule_idx=0; rule_idx<lhs->NumRules(); ++rule_idx)
		{
			const WordPtr& rule = lhs->GetRule(rule_idx);

			// iterate RHS of rule
			for(t_index sym_idx=0; sym_idx<rule->NumSymbols(); ++sym_idx)
			{
				// cur_nonterm is in RHS of lhs rules
				if(*(*rule)[sym_idx] == *cur_nonterm)
				{
					// add first set of following symbols except eps
					for(t_index next_sym_idx = sym_idx + 1; next_sym_idx < rule->NumSymbols(); ++next_sym_idx)
					{
						const SymbolPtr& sym = (*rule)[next_sym_idx];

						// add terminal to follow set
						if(sym->IsTerminal() && !sym->IsEps())
						{
							set_follow.insert(std::dynamic_pointer_cast<Terminal>(sym));
							break;
						}

						// non-terminal
						else
						{
							const auto& iterFirst = map_first.find(sym);

							for(const TerminalPtr& symfirst : iterFirst->second)
							{
								if(!symfirst->IsEps())
									set_follow.insert(symfirst);
							}

							if(!std::dynamic_pointer_cast<NonTerminal>(sym)->HasEpsRule())
								break;
						}
					}

					// last symbol in rule?
					t_index next_sym_idx = sym_idx + 1;
					bool is_last_sym = (next_sym_idx >= rule->NumSymbols());

					// ... or only epsilon productions afterwards?
					for(; next_sym_idx<rule->NumSymbols(); ++next_sym_idx)
					{
						const SymbolPtr& sym = (*rule)[next_sym_idx];

						// terminal
						if(sym->IsTerminal() && !sym->IsEps())
							break;

						// non-terminal
						if(!std::dynamic_pointer_cast<NonTerminal>(sym)->HasEpsRule())
							break;
					}

					if(is_last_sym || next_sym_idx >= rule->NumSymbols())
					{
						if(lhs != cur_nonterm)
							lhs->CalcFollow(allnonterms, start, map_first, map_follow, recurse_depth+1);

						const auto& follow_lhs = map_follow[lhs];
						set_follow.insert(follow_lhs.begin(), follow_lhs.end());
					}
				}
			}
		}
	}

	map_follow[cur_nonterm] = set_follow;
}

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// word (collection of symbols)
// ----------------------------------------------------------------------------

Word::Word(const std::initializer_list<SymbolPtr>& init)
	: std::enable_shared_from_this<Word>{},
	  m_syms{init}
{}


Word::Word(const Word& other) : std::enable_shared_from_this<Word>{}
{
	this->operator=(other);
}


Word::Word() : std::enable_shared_from_this<Word>{}
{}


const Word& Word::operator=(const Word& other)
{
	this->m_syms = other.m_syms;
	this->m_hash = other.m_hash;
	this->m_cached_firsts = other.m_cached_firsts;

	return *this;
}


/**
 * adds a symbol to the word
 */
t_index Word::AddSymbol(const SymbolPtr& sym)
{
	m_syms.push_back(sym);
	m_hash = std::nullopt;

	// return index
	return m_syms.size() - 1;
}


/**
 * removes a symbol from the word
 */
void Word::RemoveSymbol(t_index idx)
{
	m_syms.erase(std::next(m_syms.begin(), idx));
	m_hash = std::nullopt;
}


std::size_t Word::size() const
{
	return NumSymbols();
}


/**
 * gets a symbol in the word
 */
const SymbolPtr& Word::GetSymbol(t_index idx) const
{
	return *std::next(m_syms.begin(), idx);
}


const SymbolPtr& Word::operator[](t_index idx) const
{
	return GetSymbol(idx);
}


/**
 * number of symbols in the word
 */
std::size_t Word::NumSymbols(bool count_eps) const
{
	if(count_eps)
	{
		return m_syms.size();
	}
	else
	{
		std::size_t num = 0;

		for(const SymbolPtr& sym : m_syms)
		{
			if(!sym->IsEps())
				++num;
		}

		return num;
	}
}


/**
 * calculates the first set of a symbol string
 * @see https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 */
const Terminal::t_terminalset& Word::CalcFirst(
	const TerminalPtr& additional_sym, t_index offs) const
{
	t_hash hashval = 0;
	boost::hash_combine(hashval, this->hash());

	if(additional_sym)
	{
		t_hash hashSym = additional_sym->hash();
		boost::hash_combine(hashval, hashSym);
	}

	t_hash hashOffs = std::hash<t_index>{}(offs);
	boost::hash_combine(hashval, hashOffs);

	// terminal set already cached?
	auto iter = m_cached_firsts.find(hashval);
	if(iter != m_cached_firsts.end())
		return iter->second;

	Terminal::t_terminalset first;

	// iterate RHS of rule
	std::size_t num_rule_symbols = NumSymbols();
	std::size_t num_all_symbols = num_rule_symbols;

	// add an additional symbol to the end of the rules
	if(additional_sym && additional_sym.get())
		++num_all_symbols;

	t_map_first first_nonterms;

	for(t_index sym_idx=offs; sym_idx<num_all_symbols; ++sym_idx)
	{
		const SymbolPtr& sym = sym_idx < num_rule_symbols ? (*this)[sym_idx] : additional_sym;

		// reached terminal symbol -> end
		if(sym->IsTerminal())
		{
			first.insert(std::dynamic_pointer_cast<Terminal>(sym));
			break;
		}

		// non-terminal
		else
		{
			const NonTerminalPtr& symnonterm = std::dynamic_pointer_cast<NonTerminal>(sym);
			symnonterm->CalcFirst(first_nonterms);

			// add first set except eps
			bool has_eps = false;
			for(const TerminalPtr& symprod : first_nonterms[symnonterm])
			{
				bool insert = true;
				if(symprod->IsEps())
				{
					has_eps = true;

					// if last non-terminal is reached -> add epsilon
					insert = (sym_idx == num_all_symbols-1);
				}

				if(insert)
					first.insert(std::dynamic_pointer_cast<Terminal>(symprod));
			}

			// no epsilon in production -> end
			if(!has_eps)
				break;
		}
	}

	std::tie(iter, std::ignore) = m_cached_firsts.emplace(
		std::make_pair(hashval, std::move(first)));
	return iter->second;
}


bool Word::operator==(const Word& other) const
{
	if(this->NumSymbols() != other.NumSymbols())
		return false;

	return this->hash() == other.hash();
}


/**
 * calculates a unique hash for the symbol string
 */
t_hash Word::hash() const
{
	if(m_hash)
		return *m_hash;

	m_hash = 0;
	for(const SymbolPtr& sym : m_syms)
	{
		t_hash hashSym = sym->hash();
		boost::hash_combine(*m_hash, hashSym);
	}

	return *m_hash;
}


std::ostream& operator<<(std::ostream& ostr, const Word& word)
{
	const std::size_t num_syms = word.NumSymbols();
	for(t_index idx=0; idx<num_syms; ++idx)
	{
		ostr << word.GetSymbol(idx)->GetStrId();
		if(idx < num_syms - 1)
			ostr << " ";
	}

	return ostr;
}
// ----------------------------------------------------------------------------

} // namespace lalr1
