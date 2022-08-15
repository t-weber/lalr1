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
#include <list>
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
class Symbol : public std::enable_shared_from_this<Symbol>
{
public:
	Symbol(std::size_t id, const std::string& strid = "",
		bool bEps = false, bool bEnd = false);
	Symbol() = delete;

	virtual ~Symbol();

	virtual bool IsTerminal() const = 0;

	const std::string& GetStrId() const { return m_strid; }
	std::size_t GetId() const { return m_id; }

	bool IsEps() const { return m_iseps; }
	bool IsEnd() const { return m_isend; }

	bool operator==(const Symbol& other) const;
	bool operator!=(const Symbol& other) const { return !operator==(other); }

	virtual void print(std::ostream& ostr, bool bnf = false) const = 0;
	virtual std::size_t hash() const = 0;


private:
	std::size_t m_id{ 0 };    // numeric identifier of the symbol
	std::string m_strid{ };   // string identifier of the symbol

	bool m_iseps{ false };    // the symbol is the epsilon transition
	bool m_isend{ false };    // the symbol is the end marker


public:
	/**
	 * hash function for terminals
	 */
	struct HashSymbol
	{
		std::size_t operator()(const SymbolPtr& sym) const;
	};

	/**
	 * comparator for terminals
	 */
	struct CompareSymbolsEqual
	{
		bool operator()(const SymbolPtr& sym1, const SymbolPtr& sym2) const;
	};
};



/**
 * terminal symbols
 */
class Terminal : public Symbol
{
public:
	Terminal(std::size_t id, const std::string& strid = "",
		bool bEps = false, bool bEnd = false);
	Terminal() = delete;
	virtual ~Terminal();

	virtual bool IsTerminal() const override { return true; }

	// get the semantic rule index
	std::optional<std::size_t> GetSemanticRule() const;

	// set the semantic rule index
	void SetSemanticRule(std::optional<std::size_t> semantic = std::nullopt);

	virtual void print(std::ostream& ostr, bool bnf = false) const override;
	virtual std::size_t hash() const override;

	void SetPrecedence(std::size_t prec);
	void SetAssociativity(char assoc);
	void SetPrecedence(std::size_t prec, char assoc);

	std::optional<std::size_t> GetPrecedence() const;
	std::optional<char> GetAssociativity() const;


public:
	using t_terminalset = std::unordered_set<TerminalPtr,
		Symbol::HashSymbol, Symbol::CompareSymbolsEqual>;


private:
	// semantic rule
	std::optional<std::size_t> m_semantic{};

	// operator precedence and associativity
	std::optional<std::size_t> m_precedence{};
	std::optional<char> m_associativity{};     // 'l' or 'r'

	// cached hash value
	mutable std::optional<std::size_t> m_hash{ std::nullopt };
};



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
 * nonterminal symbols
 */
class NonTerminal : public Symbol
{
public:
	NonTerminal(std::size_t id, const std::string& strid);
	NonTerminal() = delete;
	virtual ~NonTerminal();

	virtual bool IsTerminal() const override { return false; }

	// adds multiple alternative production rules
	void AddRule(const WordPtr& rule,
		std::optional<std::size_t> semanticruleidx = std::nullopt);

	// adds multiple alternative production rules
	void AddRule(const Word& rule,
		std::optional<std::size_t> semanticruleidx = std::nullopt);

	// adds multiple alternative production rules
	void AddRule(const Word& rule, const std::size_t* semanticruleidx);

	// number of rules
	std::size_t NumRules() const;

	// gets a production rule
	const WordPtr& GetRule(std::size_t i) const;

	// clears rules
	void ClearRules();

	// gets a semantic rule index for a given rule number
	std::optional<std::size_t> GetSemanticRule(std::size_t i) const;

	// does this non-terminal have a rule which produces epsilon?
	bool HasEpsRule() const;

	// removes left recursion
	NonTerminalPtr RemoveLeftRecursion(
		std::size_t newIdBegin = 1000, const std::string& primerule = "'",
		std::size_t* semanticruleidx = nullptr);

	virtual void print(std::ostream& ostr, bool bnf = false) const override;
	virtual std::size_t hash() const override;

	// calculates the first set of a nonterminal
	t_map_first CalcFirst(t_map_first_perrule* first_perrule = nullptr) const;
	void CalcFirst(t_map_first& map_first, t_map_first_perrule* first_perrule = nullptr) const;

	// calculates the follow set of a nonterminal
	void CalcFollow(const std::vector<NonTerminalPtr>& allnonterms,
		const NonTerminalPtr& start,
		const t_map_first& _first, t_map_follow& _follow) const;


private:
	// production syntactic rules
	std::vector<WordPtr> m_rules{};

	// production semantic rule indices
	std::vector<std::optional<std::size_t>> m_semantics{};

	// cached hash value
	mutable std::optional<std::size_t> m_hash{ std::nullopt };
};



/**
 * a collection of terminals and non-terminals
 */
class Word : protected std::enable_shared_from_this<Word>
{
public:
	using t_symbols = std::list<SymbolPtr>;


public:
	Word(const std::initializer_list<SymbolPtr>& init);
	Word(const Word& other);
	Word();

	// adds a symbol to the word
	std::size_t AddSymbol(const SymbolPtr& sym);

	// removes a symbol from the word
	void RemoveSymbol(std::size_t idx);

	// number of symbols in the word
	std::size_t NumSymbols(bool count_eps = true) const;
	std::size_t size() const;

	Terminal::t_terminalset CalcFirst(TerminalPtr additional_sym = nullptr) const;

	// gets a symbol in the word
	const SymbolPtr& GetSymbol(const std::size_t i) const;
	const SymbolPtr& operator[](const std::size_t i) const;

	// tests for equality
	bool operator==(const Word& other) const;
	bool operator!=(const Word& other) const { return !operator==(other); }

	std::size_t hash() const;

	friend std::ostream& operator<<(std::ostream& ostr, const Word& word);


private:
	// string of symbols
	t_symbols m_syms{};

	// cached hash value
	mutable std::optional<std::size_t> m_hash{ std::nullopt };
};



// ----------------------------------------------------------------------------


/**
 * epsilon and end symbols
 */
extern const TerminalPtr g_eps, g_end;


// ----------------------------------------------------------------------------


#endif
