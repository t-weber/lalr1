/**
 * symbols
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 07-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_SYMBOL_H__
#define __LALR1_SYMBOL_H__


#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>

#include "types.h"


namespace lalr1 {

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
	Symbol() = delete;
	virtual ~Symbol();

	Symbol(t_symbol_id id, const std::string& strid = "",
		bool bEps = false, bool bEnd = false);
	Symbol(const Symbol& other);

	const Symbol& operator=(const Symbol& other);

	virtual bool IsTerminal() const = 0;

	void SetStrId(const std::string& str) { m_strid = str; }
	const std::string& GetStrId() const { return m_strid; }
	t_symbol_id GetId() const { return m_id; }

	bool IsEps() const { return m_iseps; }
	bool IsEnd() const { return m_isend; }

	bool operator==(const Symbol& other) const;
	bool operator!=(const Symbol& other) const { return !operator==(other); }

	virtual void print(std::ostream& ostr, bool bnf = false) const = 0;
	virtual t_hash hash() const = 0;

	friend std::ostream& operator<<(std::ostream& ostr, const Symbol& sym)
	{
		ostr << sym.GetStrId();
		return ostr;
	}


private:
	t_symbol_id m_id{ };      // numeric identifier of the symbol
	std::string m_strid{ };   // string identifier of the symbol

	bool m_iseps{ false };    // the symbol is the epsilon transition
	bool m_isend{ false };    // the symbol is the end marker


public:
	// hash function for terminals
	struct HashSymbol
	{
		t_hash operator()(const SymbolPtr& sym) const;
	};

	// comparator for terminals
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
	Terminal() = delete;
	virtual ~Terminal();

	Terminal(t_symbol_id id, const std::string& strid = "",
		bool bEps = false, bool bEnd = false);
	Terminal(const Terminal& other);

	const Terminal& operator=(const Terminal& other);

	virtual bool IsTerminal() const override { return true; }

	virtual void print(std::ostream& ostr, bool bnf = false) const override;
	virtual t_hash hash() const override;

	void SetPrecedence(std::size_t prec);
	void SetAssociativity(char assoc);
	void SetPrecedence(std::size_t prec, char assoc);

	std::optional<t_precedence> GetPrecedence() const;
	std::optional<t_associativity> GetAssociativity() const;


public:
	using t_terminalset = std::unordered_set<TerminalPtr,
		Symbol::HashSymbol, Symbol::CompareSymbolsEqual>;
	template<class t_val>
	using t_terminalmap = std::unordered_map<TerminalPtr, t_val,
		Symbol::HashSymbol, Symbol::CompareSymbolsEqual>;


private:
	// operator precedence and associativity
	std::optional<t_precedence> m_precedence{};
	std::optional<t_associativity> m_associativity{};  // 'l' or 'r'

	// cached hash value
	mutable std::optional<t_hash> m_hash{ std::nullopt };
};



using t_map_first = std::unordered_map<
	SymbolPtr, Terminal::t_terminalset,
	Symbol::HashSymbol, Symbol::CompareSymbolsEqual>;
using t_map_first_perrule = std::unordered_map<
	SymbolPtr, std::deque<Terminal::t_terminalset>,
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
	NonTerminal() = delete;
	virtual ~NonTerminal();

	NonTerminal(t_symbol_id id, const std::string& strid);
	NonTerminal(const NonTerminal& other);

	const NonTerminal& operator=(const NonTerminal& other);

	virtual bool IsTerminal() const override { return false; }

	// adds multiple alternative production rules
	void AddRule(const WordPtr& rule,
		std::optional<t_semantic_id> semantic_id = std::nullopt);

	// adds multiple alternative production rules
	void AddRule(const Word& rule,
		std::optional<t_semantic_id> semantic_id = std::nullopt);

	// adds multiple alternative production rules
	// (non-overloaded helper for scripting interface)
	void AddARule(const WordPtr& rule, t_semantic_id semantic_id);

	// number of rules
	std::size_t NumRules() const;

	// gets a production rule
	const WordPtr& GetRule(t_index idx) const;

	// gets a production rule from a semantic id
	WordPtr GetRuleFromSemanticId(t_semantic_id semantic_id) const;

	// clears rules
	void ClearRules();

	// gets a semantic rule id for a given rule number
	std::optional<t_semantic_id> GetSemanticRule(t_index idx) const;

	// does this non-terminal have a rule which produces epsilon?
	bool HasEpsRule() const;

	// removes left recursion
	NonTerminalPtr RemoveLeftRecursion(
		t_symbol_id newIdBegin = 1000,
		const std::string& primerule = "'",
		std::optional<t_semantic_id> semantic_id = std::nullopt);

	virtual void print(std::ostream& ostr, bool bnf = false) const override;
	virtual t_hash hash() const override;

	// calculates the first set of a nonterminal
	t_map_first CalcFirst(t_map_first_perrule* first_perrule = nullptr) const;
	void CalcFirst(t_map_first& map_first,
		t_map_first_perrule* first_perrule = nullptr,
		std::size_t recurse_depth = 0) const;

	// calculates the follow set of a nonterminal
	void CalcFollow(const std::vector<NonTerminalPtr>& allnonterms,
		const NonTerminalPtr& start,
		const t_map_first& _first, t_map_follow& _follow,
		std::size_t recurse_depth = 0) const;


private:
	// production syntactic rules
	std::vector<WordPtr> m_rules{};

	// production semantic rule ids
	std::vector<std::optional<t_semantic_id>> m_semantics{};

	// cached hash value
	mutable std::optional<t_hash> m_hash{ std::nullopt };
};



/**
 * a collection of terminals and non-terminals
 */
class Word : public std::enable_shared_from_this<Word>
{
public:
	using t_symbols = std::list<SymbolPtr>;


public:
	Word();
	~Word() = default;

	Word(const std::initializer_list<SymbolPtr>& init);
	Word(const Word& other);

	const Word& operator=(const Word& word);

	// adds a symbol to the word
	t_index AddSymbol(const SymbolPtr& sym);

	// removes a symbol from the word
	void RemoveSymbol(t_index idx);

	// number of symbols in the word
	std::size_t NumSymbols(bool count_eps = true) const;
	std::size_t size() const;

	const Terminal::t_terminalset& CalcFirst(
		const TerminalPtr& additional_sym = nullptr,
		t_index offs = 0) const;

	// gets a symbol in the word
	const SymbolPtr& GetSymbol(t_index idx) const;
	const SymbolPtr& operator[](t_index idx) const;

	// tests for equality
	bool operator==(const Word& other) const;
	bool operator!=(const Word& other) const { return !operator==(other); }

	t_hash hash() const;
	t_hash hash(t_index offs, const TerminalPtr& additional_sym = nullptr) const;
	friend std::ostream& operator<<(std::ostream& ostr, const Word& word);


private:
	// string of symbols
	t_symbols m_syms{};

	// cached hash value
	mutable std::optional<t_hash> m_hash{ std::nullopt };

	// cached first calculations (key: hash)
	mutable std::unordered_map<t_hash, Terminal::t_terminalset> m_cached_firsts{};
};



// ----------------------------------------------------------------------------


/**
 * epsilon and end symbols
 */
extern const TerminalPtr g_eps, g_end;


// ----------------------------------------------------------------------------

} // namespace lalr1

#endif
