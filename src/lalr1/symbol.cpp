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


Symbol::Symbol(std::size_t id, const std::string& strid, bool bEps, bool bEnd)
	: m_id{id}, m_strid{strid}, m_iseps{bEps}, m_isend{bEnd}
{
	// if no string id is given, use the numeric id
	if(m_strid == "")
		m_strid = std::to_string(id);
}


bool Symbol::operator==(const Symbol& other) const
{
	return this->hash() == other.hash();
}


std::size_t Symbol::HashSymbol::operator()(
	const SymbolPtr sym) const
{
	return sym->hash();
}


bool Symbol::CompareSymbolsLess::operator()(
	const SymbolPtr sym1, const SymbolPtr sym2) const
{
	/*
	 const std::string& id1 = sym1->GetStrId();
	 const std::string& id2 = sym2->GetStrId();
	 return std::lexicographical_compare(id1.begin(), id1.end(), id2.begin(), id2.end());
	 */
	return sym1->hash() < sym2->hash();
}


bool Symbol::CompareSymbolsEqual::operator()(
	const SymbolPtr sym1, const SymbolPtr sym2) const
{
	return sym1->hash() == sym2->hash();
}


// ----------------------------------------------------------------------------



void Terminal::print(std::ostream& ostr, bool /*bnf*/) const
{
	ostr << GetStrId();
}


std::size_t Terminal::hash() const
{
	//std::size_t hashId = std::hash<std::string>{}(GetStrId());
	std::size_t hashId = std::hash<std::size_t>{}(GetId());
	std::size_t hashEps = std::hash<bool>{}(IsEps());
	std::size_t hashEnd = std::hash<bool>{}(IsEnd());

	boost::hash_combine(hashId, hashEps);
	boost::hash_combine(hashId, hashEnd);

	return hashId;
}


// ----------------------------------------------------------------------------


/**
 * remove left recursion
 * returns possibly added non-terminal
 */
NonTerminalPtr NonTerminal::RemoveLeftRecursion(
	std::size_t newIdBegin, const std::string& primerule,
	std::size_t* semanticruleidx)
{
	std::vector<Word> rulesWithLeftRecursion;
	std::vector<Word> rulesWithoutLeftRecursion;

	for(std::size_t ruleidx=0; ruleidx<NumRules(); ++ruleidx)
	{
		const Word& rule = this->GetRule(ruleidx);
		if(rule.NumSymbols() >= 1 && rule.GetSymbol(0)->hash() == this->hash())
			rulesWithLeftRecursion.push_back(rule);
		else
			rulesWithoutLeftRecursion.push_back(rule);
	}

	// no left-recursive productions
	if(rulesWithLeftRecursion.size() == 0)
		return nullptr;

	NonTerminalPtr newNonTerm = std::make_shared<NonTerminal>(
		this->GetId()+newIdBegin, this->GetStrId()+primerule);

	for(Word word : rulesWithLeftRecursion)
	{
		word.RemoveSymbol(0);		// remove "this" rule causing left-recursion
		word.AddSymbol(newNonTerm);	// make it right-recursive instead

		newNonTerm->AddRule(word, semanticruleidx);
		if(semanticruleidx)
			++*semanticruleidx;
	}

	newNonTerm->AddRule({g_eps}, semanticruleidx);
	if(semanticruleidx)
		++*semanticruleidx;

	this->ClearRules();
	for(Word word : rulesWithoutLeftRecursion)
	{
		word.AddSymbol(newNonTerm);	// make it right-recursive instead
		this->AddRule(word, semanticruleidx);
		if(semanticruleidx)
			++*semanticruleidx;
	}

	return newNonTerm;
}


std::size_t NonTerminal::hash() const
{
	//std::size_t hashId = std::hash<std::string>{}(GetStrId());
	std::size_t hashId = std::hash<std::size_t>{}(GetId());
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

		ostr << GetRule(i);
		ostr << "\n";
	}
}


/**
 * does this non-terminal have a rule which produces epsilon?
 */
bool NonTerminal::HasEpsRule() const
{
	for(const auto& rule : m_rules)
	{
		if(rule.NumSymbols()==1 && rule[0]->IsEps())
			return true;
	}
	return false;
}


// ----------------------------------------------------------------------------


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

	bool match = true;
	for(std::size_t i=0; i<NumSymbols(); ++i)
	{
		if(*m_syms[i] != *other.m_syms[i])
		{
			match = false;
			break;
		}
	}

	return match;
}


std::size_t Word::hash() const
{
	std::size_t hash = 0;

	for(std::size_t i=0; i<NumSymbols(); ++i)
	{
		std::size_t hashSym = m_syms[i]->hash();
		boost::hash_combine(hash, hashSym);
	}

	return hash;
}


std::ostream& operator<<(std::ostream& ostr, const Word& word)
{
	for(std::size_t i=0; i<word.NumSymbols(); ++i)
	{
		//word.GetSymbol(i)->print(ostr);
		ostr << word.GetSymbol(i)->GetStrId() << " ";
	}

	return ostr;
}


// ----------------------------------------------------------------------------


/**
 * calculates the first set of a nonterminal
 * @see https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 */
void calc_first(const NonTerminalPtr nonterm, t_map_first& _first, t_map_first_perrule* _first_perrule)
{
	// set already calculated?
	if(_first.find(nonterm) != _first.end())
		return;

	Terminal::t_terminalset first;
	std::vector<Terminal::t_terminalset> first_perrule;
	first_perrule.resize(nonterm->NumRules());

	// iterate rules
	for(std::size_t iRule=0; iRule<nonterm->NumRules(); ++iRule)
	{
		const Word& rule = nonterm->GetRule(iRule);

		// iterate RHS of rule
		for(std::size_t iSym=0; iSym<rule.NumSymbols(); ++iSym)
		{
			const SymbolPtr& sym = rule[iSym];

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
				const NonTerminalPtr symnonterm = std::dynamic_pointer_cast<NonTerminal>(sym);

				// if the rule is left-recursive, ignore calculating the same symbol again
				if(*symnonterm != *nonterm)
					calc_first(symnonterm, _first, _first_perrule);

				// add first set except eps
				bool has_eps = false;
				for(const TerminalPtr& symprod : _first[symnonterm])
				{
					bool insert = true;
					if(symprod->IsEps())
					{
						has_eps = true;

						// if last non-terminal is reached -> add epsilon
						insert = (iSym == rule.NumSymbols()-1);
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
void calc_follow(const std::vector<NonTerminalPtr>& allnonterms,
	const NonTerminalPtr& start, const NonTerminalPtr nonterm,
	const t_map_first& _first, t_map_follow& _follow)
{
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
			const Word& rule = _nonterm->GetRule(iRule);

			// iterate RHS of rule
			for(std::size_t iSym=0; iSym<rule.NumSymbols(); ++iSym)
			{
				// nonterm is in RHS of _nonterm rules
				if(*rule[iSym] == *nonterm)
				{
					// add first set of following symbols except eps
					for(std::size_t _iSym=iSym+1; _iSym < rule.NumSymbols(); ++_iSym)
					{
						// add terminal to follow set
						if(rule[_iSym]->IsTerminal() && !rule[_iSym]->IsEps())
						{
							follow.insert(std::dynamic_pointer_cast<Terminal>(
								rule[_iSym]));
							break;
						}

						// non-terminal
						else
						{
							const auto& iterFirst = _first.find(rule[_iSym]);

							for(const TerminalPtr& symfirst : iterFirst->second)
							{
								if(!symfirst->IsEps())
									follow.insert(symfirst);
							}

							if(!std::dynamic_pointer_cast<NonTerminal>(
								rule[_iSym])->HasEpsRule())
							{
								break;
							}
						}
					}


					// last symbol in rule?
					bool bLastSym = (iSym+1 == rule.NumSymbols());

					// ... or only epsilon productions afterwards?
					std::size_t iNextSym = iSym+1;
					for(; iNextSym<rule.NumSymbols(); ++iNextSym)
					{
						// terminal
						if(rule[iNextSym]->IsTerminal() && !rule[iNextSym]->IsEps())
						{
							break;
						}

						// non-terminal
						if(!std::dynamic_pointer_cast<NonTerminal>(
							rule[iNextSym])->HasEpsRule())
						{
							break;
						}
					}

					if(bLastSym || iNextSym==rule.NumSymbols())
					{
						if(_nonterm != nonterm)
						{
							calc_follow(allnonterms, start,
								_nonterm, _first, _follow);
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
