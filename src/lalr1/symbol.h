/**
 * symbols
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 07-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LR1_SYM_H__
#define __LR1_SYM_H__


#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>
#include <iostream>


class Symbol;
class Terminal;
class NonTerminal;
class Word;

using SymbolPtr = std::shared_ptr<Symbol>;
using TerminalPtr = std::shared_ptr<Terminal>;
using NonTerminalPtr = std::shared_ptr<NonTerminal>;
using WordPtr = std::shared_ptr<Word>;



/**
 * symbol base class
 */
class Symbol
{
public:
	Symbol(std::size_t id, const std::string& strid="", bool bEps=false, bool bEnd=false);
	Symbol() = delete;
	virtual ~Symbol() = default;

	virtual bool IsTerminal() const = 0;

	const std::string& GetStrId() const { return m_strid; }
	std::size_t GetId() const { return m_id; }

	bool IsEps() const { return m_iseps; }
	bool IsEnd() const { return m_isend; }

	bool operator==(const Symbol& other) const;
	bool operator!=(const Symbol& other) const { return !operator==(other); }

	virtual void print(std::ostream& ostr, bool bnf=false) const = 0;
	virtual std::size_t hash() const = 0;


private:
	std::size_t m_id{0};
	std::string m_strid{};

	bool m_iseps{false};
	bool m_isend{false};


public:
	/**
	 * hash function for terminals
	 */
	struct HashSymbol
	{
		std::size_t operator()(const SymbolPtr sym) const;
	};

	/**
	 * comparator for terminals
	 */
	struct CompareSymbolsLess
	{
		bool operator()(const SymbolPtr sym1, const SymbolPtr sym2) const;
	};

	/**
	 * comparator for terminals
	 */
	struct CompareSymbolsEqual
	{
		bool operator()(const SymbolPtr sym1, const SymbolPtr sym2) const;
	};
};



/**
 * terminal symbols
 */
class Terminal : public Symbol
{
public:
	Terminal(std::size_t id, const std::string& strid="", bool bEps=false, bool bEnd=false)
		: Symbol{id, strid, bEps, bEnd}, m_semantic{} {}
	Terminal() = delete;
	virtual ~Terminal() = default;

	virtual bool IsTerminal() const override { return true; }

	/**
	 * get the semantic rule index
	 */
	std::optional<std::size_t> GetSemanticRule() const { return m_semantic; }

	/**
	 * set the semantic rule index
	 */
	void SetSemanticRule(std::optional<std::size_t> semantic=std::nullopt)
	{ m_semantic = semantic; }

	virtual void print(std::ostream& ostr, bool bnf=false) const override;
	virtual std::size_t hash() const override;

	void SetPrecedence(std::size_t prec) { m_precedence = prec; }
	void SetAssociativity(char assoc) { m_associativity = assoc; }
	void SetPrecedence(std::size_t prec, char assoc)
	{
		SetPrecedence(prec);
		SetAssociativity(assoc);
	}

	std::optional<std::size_t> GetPrecedence() const { return m_precedence; }
	std::optional<char> GetAssociativity() const { return m_associativity; }


public:
	//using t_terminalset = std::set<TerminalPtr, Symbol::CompareSymbolsLess>;
	using t_terminalset = std::unordered_set<TerminalPtr,
		Symbol::HashSymbol, Symbol::CompareSymbolsEqual>;


private:
	// semantic rule
	std::optional<std::size_t> m_semantic{};

	// operator precedence and associativity
	std::optional<std::size_t> m_precedence{};
	std::optional<char> m_associativity{};     // 'l' or 'r'
};



/**
 * nonterminal symbols
 */
class NonTerminal : public Symbol
{
public:
	NonTerminal(std::size_t id, const std::string& strid="")
		: Symbol{id, strid}, m_rules{}, m_semantics{} {}
	NonTerminal() = delete;
	virtual ~NonTerminal() = default;

	virtual bool IsTerminal() const override { return false; }

	/**
	 * add multiple alternative production rules
	 */
	void AddRule(const Word& rule,
		std::optional<std::size_t> semanticruleidx = std::nullopt)
	{
		m_rules.push_back(rule);
		m_semantics.push_back(semanticruleidx);
	}

	/**
	 * add multiple alternative production rules
	 */
	void AddRule(const Word& rule, const std::size_t* semanticruleidx)
	{
		if(semanticruleidx)
			AddRule(rule, *semanticruleidx);
		else
			AddRule(rule);
	}

	/**
	 * number of rules
	 */
	std::size_t NumRules() const { return m_rules.size(); }

	/**
	 * get a production rule
	 */
	const Word& GetRule(std::size_t i) const { return m_rules[i]; }

	/**
	 * clear rules
	 */
	void ClearRules() { m_rules.clear(); }

	/**
	 * get a semantic rule index for a given rule number
	 */
	std::optional<std::size_t> GetSemanticRule(std::size_t i) const
	{ return m_semantics[i]; }

	/**
	 * does this non-terminal have a rule which produces epsilon?
	 */
	bool HasEpsRule() const;

	/**
	 * remove left recursion (for future ll(1) support)
	 * returns possibly added non-terminal
	 */
	NonTerminalPtr RemoveLeftRecursion(
		std::size_t newIdBegin = 1000, const std::string& primerule = "'",
		std::size_t* semanticruleidx = nullptr);

	virtual void print(std::ostream& ostr, bool bnf=false) const override;
	virtual std::size_t hash() const override;


private:
	// production syntactic rules
	std::vector<Word> m_rules{};

	// production semantic rule indices
	std::vector<std::optional<std::size_t>> m_semantics{};
};



/**
 * a collection of terminals and non-terminals
 */
class Word
{
public:
	Word(const std::initializer_list<SymbolPtr> init) : m_syms{init} {}
	Word(const Word& other) : m_syms{other.m_syms} {}
	Word() : m_syms{} {}

	/**
	 * add a symbol to the word
	 */
	void AddSymbol(SymbolPtr sym)
	{
		m_syms.push_back(sym);
	}

	/**
	 * remove a symbol from the word
	 */
	void RemoveSymbol(std::size_t idx)
	{
		m_syms.erase(std::next(m_syms.begin(), idx));
	}

	/**
	 * number of symbols in the word
	 */
	std::size_t NumSymbols(bool count_eps = true) const;
	std::size_t size() const { return NumSymbols(); }

	/**
	 * get a symbol in the word
	 */
	const SymbolPtr GetSymbol(const std::size_t i) const { return m_syms[i]; }
	const SymbolPtr operator[](const std::size_t i) const { return GetSymbol(i); }

	/**
	 * test for equality
	 */
	bool operator==(const Word& other) const;
	bool operator!=(const Word& other) const { return !operator==(other); }

	std::size_t hash() const;

	friend std::ostream& operator<<(std::ostream& ostr, const Word& word);


private:
	std::vector<SymbolPtr> m_syms{};
};



// ----------------------------------------------------------------------------


/**
 * epsilon and end symbols
 */
extern const TerminalPtr g_eps, g_end;


// ----------------------------------------------------------------------------


/*
using t_map_first = std::map<
	SymbolPtr, Terminal::t_terminalset, Symbol::CompareSymbolsLess>;
using t_map_first_perrule = std::map<
	SymbolPtr, std::vector<Terminal::t_terminalset>, Symbol::CompareSymbolsLess>;
using t_map_follow = std::map<
	SymbolPtr, Terminal::t_terminalset, Symbol::CompareSymbolsLess>;
*/

using t_map_first = std::unordered_map<
	SymbolPtr, Terminal::t_terminalset,
	Symbol::HashSymbol, Symbol::CompareSymbolsEqual>;
using t_map_first_perrule = std::unordered_map<
	SymbolPtr, std::vector<Terminal::t_terminalset>,
	Symbol::HashSymbol, Symbol::CompareSymbolsEqual>;
using t_map_follow = std::unordered_map<
	SymbolPtr, Terminal::t_terminalset,
	Symbol::HashSymbol, Symbol::CompareSymbolsEqual>;


/**
 * calculates the first set of a nonterminal
 */
extern void calc_first(const NonTerminalPtr nonterm,
	t_map_first& _first, t_map_first_perrule* _first_per_rule=nullptr);


/**
 * calculates the follow set of a nonterminal
 */
extern void calc_follow(const std::vector<NonTerminalPtr>& allnonterms,
	const NonTerminalPtr& start, const NonTerminalPtr nonterm,
	const t_map_first&  _first, t_map_follow& _follow);


#endif
