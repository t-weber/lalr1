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

#include <boost/functional/hash.hpp>


// special terminal symbols
const TerminalPtr g_eps = std::make_shared<Terminal>(EPS_IDENT, "eps", true, false);
const TerminalPtr g_end = std::make_shared<Terminal>(END_IDENT, "end", false, true);


// ----------------------------------------------------------------------------
// symbol base class
// ----------------------------------------------------------------------------

Symbol::Symbol(std::size_t id, const std::string& strid, bool bEps, bool bEnd)
	: std::enable_shared_from_this<Symbol>{},
	  m_id{id}, m_strid{strid}, m_iseps{bEps}, m_isend{bEnd}
{
	// if no string id is given, use the numeric id
	if(m_strid == "")
		m_strid = std::to_string(id);
}


Symbol::~Symbol()
{
}


bool Symbol::operator==(const Symbol& other) const
{
	return this->hash() == other.hash();
}


std::size_t Symbol::HashSymbol::operator()(const SymbolPtr& sym) const
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

Terminal::Terminal(std::size_t id, const std::string& strid, bool bEps, bool bEnd)
	: Symbol{id, strid, bEps, bEnd}
{}


Terminal::~Terminal()
{
}


/**
 * get the semantic rule index
 */
std::optional<std::size_t> Terminal::GetSemanticRule() const
{
	return m_semantic;
}


/**
 * set the semantic rule index
 */
void Terminal::SetSemanticRule(std::optional<std::size_t> semantic)
{
	m_semantic = semantic;
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


std::size_t Terminal::hash() const
{
	if(m_hash)
		return *m_hash;

	std::size_t hashId = std::hash<std::size_t>{}(GetId());
	std::size_t hashEps = std::hash<bool>{}(IsEps());
	std::size_t hashEnd = std::hash<bool>{}(IsEnd());

	boost::hash_combine(hashId, hashEps);
	boost::hash_combine(hashId, hashEnd);

	m_hash = hashId;
	return hashId;
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// non-terminal symbol
// ----------------------------------------------------------------------------

NonTerminal::NonTerminal(std::size_t id, const std::string& strid)
	: Symbol{id, strid}
{}


NonTerminal::~NonTerminal()
{
}


/**
 * adds multiple alternative production rules
 */
void NonTerminal::AddRule(const WordPtr& rule, std::optional<std::size_t> semanticruleidx)
{
	m_rules.emplace_back(rule);
	m_semantics.push_back(semanticruleidx);
}


/**
 * adds multiple alternative production rules
 */
void NonTerminal::AddRule(const Word& _rule, std::optional<std::size_t> semanticruleidx)
{
	WordPtr rule = std::make_shared<Word>(_rule);
	AddRule(rule, semanticruleidx);
}


/**
 * adds multiple alternative production rules
 */
void NonTerminal::AddRule(const Word& rule, const std::size_t* semanticruleidx)
{
	if(semanticruleidx)
		AddRule(rule, *semanticruleidx);
	else
		AddRule(rule);
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
const WordPtr& NonTerminal::GetRule(std::size_t i) const
{
	return m_rules[i];
}


/**
 * clears rules
 */
void NonTerminal::ClearRules()
{
	m_rules.clear();
}


/**
 * gets a semantic rule index for a given rule number
 */
std::optional<std::size_t> NonTerminal::GetSemanticRule(std::size_t i) const
{
	return m_semantics[i];
}


/**
 * removes left recursion
 * returns possibly added non-terminal
 */
NonTerminalPtr NonTerminal::RemoveLeftRecursion(
	std::size_t newIdBegin, const std::string& primerule,
	std::size_t* semanticruleidx)
{
	std::vector<WordPtr> rulesWithLeftRecursion;
	std::vector<WordPtr> rulesWithoutLeftRecursion;

	for(std::size_t ruleidx=0; ruleidx<NumRules(); ++ruleidx)
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
		word.RemoveSymbol(0);           // remove "this" rule causing left-recursion
		word.AddSymbol(newNonTerm);     // make it right-recursive instead

		newNonTerm->AddRule(word, semanticruleidx);
		if(semanticruleidx)
			++*semanticruleidx;
	}

	newNonTerm->AddRule({g_eps}, semanticruleidx);
	if(semanticruleidx)
		++*semanticruleidx;

	this->ClearRules();
	for(const WordPtr& _word : rulesWithoutLeftRecursion)
	{
		Word word = *_word;
		word.AddSymbol(newNonTerm);     // make it right-recursive instead

		this->AddRule(word, semanticruleidx);
		if(semanticruleidx)
			++*semanticruleidx;
	}

	return newNonTerm;
}


std::size_t NonTerminal::hash() const
{
	if(m_hash)
		return *m_hash;

	std::size_t hashId = std::hash<std::size_t>{}(GetId());

	m_hash = hashId;
	return hashId;
}


void NonTerminal::print(std::ostream& ostr, bool bnf) const
{
	std::string lhsrhssep = bnf ? "\t ::=" :  " ->\n";
	std::string rulesep = bnf ? "\t  |  " :  "\t| ";
	std::string rule0sep = bnf ? " " :  "\t  ";

	ostr << GetStrId() << lhsrhssep;
	for(std::size_t i=0; i<NumRules(); ++i)
	{
		if(i==0) ostr << rule0sep; else ostr << rulesep;

		if(GetSemanticRule(i) && !bnf)
			ostr << "[rule " << *GetSemanticRule(i) << "] ";

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
void NonTerminal::CalcFirst(t_map_first& _first, t_map_first_perrule* _first_perrule) const
{
	const NonTerminalPtr& nonterm =
		std::dynamic_pointer_cast<NonTerminal>(
			std::const_pointer_cast<Symbol>(
				shared_from_this()));
	if(!nonterm)
		return;

	// set already calculated?
	if(_first.find(nonterm) != _first.end())
		return;

	Terminal::t_terminalset first;
	std::vector<Terminal::t_terminalset> first_perrule;
	first_perrule.resize(nonterm->NumRules());

	// iterate rules
	for(std::size_t iRule=0; iRule<nonterm->NumRules(); ++iRule)
	{
		const WordPtr& rule = nonterm->GetRule(iRule);

		// iterate RHS of rule
		for(std::size_t iSym=0; iSym<rule->NumSymbols(); ++iSym)
		{
			const SymbolPtr& sym = (*rule)[iSym];

			// reached terminal symbol -> end
			if(sym->IsTerminal())
			{
				first.insert(std::dynamic_pointer_cast<Terminal>(sym));
				first_perrule[iRule].insert(std::dynamic_pointer_cast<Terminal>(sym));
				break;
			}

			// non-terminal
			else
			{
				const NonTerminalPtr& symnonterm =
					std::dynamic_pointer_cast<NonTerminal>(sym);

				// if the rule is left-recursive, ignore calculating the same symbol again
				if(*symnonterm != *nonterm)
					symnonterm->CalcFirst(_first, _first_perrule);

				// add first set except eps
				bool has_eps = false;
				for(const TerminalPtr& symprod : _first[symnonterm])
				{
					bool insert = true;
					if(symprod->IsEps())
					{
						has_eps = true;

						// if last non-terminal is reached -> add epsilon
						insert = (iSym == rule->NumSymbols()-1);
					}

					if(insert)
					{
						first.insert(std::dynamic_pointer_cast<Terminal>(symprod));
						first_perrule[iRule].insert(std::dynamic_pointer_cast<Terminal>(symprod));
					}
				}

				// no epsilon in production -> end
				if(!has_eps)
					break;
			}
		}
	}

	_first[nonterm] = first;

	if(_first_perrule)
		(*_first_perrule)[nonterm] = first_perrule;
}


/**
 * calculates the follow set of a nonterminal
 * @see https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 */
void NonTerminal::CalcFollow(const std::vector<NonTerminalPtr>& allnonterms,
	const NonTerminalPtr& start,
	const t_map_first& _first, t_map_follow& _follow) const
{
	const NonTerminalPtr& nonterm =
		std::dynamic_pointer_cast<NonTerminal>(
			std::const_pointer_cast<Symbol>(
				shared_from_this()));
	if(!nonterm)
		return;

	// set already calculated?
	if(_follow.find(nonterm) != _follow.end())
		return;

	Terminal::t_terminalset follow;

	// add end symbol as follower to start rule
	if(nonterm == start)
		follow.insert(g_end);

	// find current nonterminal in RHS of all rules (to get following symbols)
	for(const NonTerminalPtr& _nonterm : allnonterms)
	{
		// iterate rules
		for(std::size_t iRule=0; iRule<_nonterm->NumRules(); ++iRule)
		{
			const WordPtr& rule = _nonterm->GetRule(iRule);

			// iterate RHS of rule
			for(std::size_t iSym=0; iSym<rule->NumSymbols(); ++iSym)
			{
				// nonterm is in RHS of _nonterm rules
				if(*(*rule)[iSym] == *nonterm)
				{
					// add first set of following symbols except eps
					for(std::size_t _iSym=iSym+1; _iSym < rule->NumSymbols(); ++_iSym)
					{
						// add terminal to follow set
						if((*rule)[_iSym]->IsTerminal() && !(*rule)[_iSym]->IsEps())
						{
							follow.insert(std::dynamic_pointer_cast<Terminal>(
								(*rule)[_iSym]));
							break;
						}

						// non-terminal
						else
						{
							const auto& iterFirst = _first.find((*rule)[_iSym]);

							for(const TerminalPtr& symfirst : iterFirst->second)
							{
								if(!symfirst->IsEps())
									follow.insert(symfirst);
							}

							if(!std::dynamic_pointer_cast<NonTerminal>(
								(*rule)[_iSym])->HasEpsRule())
							{
								break;
							}
						}
					}


					// last symbol in rule?
					bool bLastSym = (iSym+1 == rule->NumSymbols());

					// ... or only epsilon productions afterwards?
					std::size_t iNextSym = iSym+1;
					for(; iNextSym<rule->NumSymbols(); ++iNextSym)
					{
						// terminal
						if((*rule)[iNextSym]->IsTerminal()
							&& !(*rule)[iNextSym]->IsEps())
						{
							break;
						}

						// non-terminal
						if(!std::dynamic_pointer_cast<NonTerminal>(
							(*rule)[iNextSym])->HasEpsRule())
						{
							break;
						}
					}

					if(bLastSym || iNextSym==rule->NumSymbols())
					{
						if(_nonterm != nonterm)
						{
							_nonterm->CalcFollow(
								allnonterms, start,
								_first, _follow);
						}
						const auto& __follow = _follow[_nonterm];
						follow.insert(__follow.begin(), __follow.end());
					}
				}
			}
		}
	}

	_follow[nonterm] = follow;
}

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// word (collection of symbols)
// ----------------------------------------------------------------------------

Word::Word(const std::initializer_list<SymbolPtr>& init)
	: std::enable_shared_from_this<Word>{},
	  m_syms{init}
{}


Word::Word(const Word& other)
	: std::enable_shared_from_this<Word>{},
	  m_syms{other.m_syms}
{}


Word::Word() : std::enable_shared_from_this<Word>{}
{}


/**
 * adds a symbol to the word
 */
std::size_t Word::AddSymbol(const SymbolPtr& sym)
{
	m_syms.push_back(sym);
	m_hash = std::nullopt;

	// return index
	return m_syms.size() - 1;
}


/**
 * removes a symbol from the word
 */
void Word::RemoveSymbol(std::size_t idx)
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
const SymbolPtr& Word::GetSymbol(const std::size_t i) const
{
	return m_syms[i];
}


const SymbolPtr& Word::operator[](const std::size_t i) const
{
	return GetSymbol(i);
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

		for(std::size_t i=0; i<m_syms.size(); ++i)
		{
			if(!m_syms[i]->IsEps())
				++num;
		}

		return num;
	}
}


bool Word::operator==(const Word& other) const
{
	if(this->NumSymbols() != other.NumSymbols())
		return false;

	return this->hash() == other.hash();
}


std::size_t Word::hash() const
{
	if(m_hash)
		return *m_hash;

	std::size_t hash = 0;

	for(std::size_t i=0; i<NumSymbols(); ++i)
	{
		std::size_t hashSym = m_syms[i]->hash();
		boost::hash_combine(hash, hashSym);
	}

	m_hash = hash;
	return hash;
}


std::ostream& operator<<(std::ostream& ostr, const Word& word)
{
	for(std::size_t i=0; i<word.NumSymbols(); ++i)
		ostr << word.GetSymbol(i)->GetStrId() << " ";

	return ostr;
}
// ----------------------------------------------------------------------------
