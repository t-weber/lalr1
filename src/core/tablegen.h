/**
 * exports lalr(1) tables to various formats
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 07-jun-2020
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/lr1.html
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/slr1table.html
 *	- https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 *	- https://en.wikipedia.org/wiki/LR_parser
 *	- https://doi.org/10.1016/0020-0190(88)90061-0
 */

#ifndef __LALR1_TABLEGEN_H__
#define __LALR1_TABLEGEN_H__

#include "collection.h"
#include "genoptions.h"


namespace lalr1 {

class TableGen;
using TableGenPtr = std::shared_ptr<TableGen>;


class TableGen : public GenOptions
{
public:
	TableGen(const CollectionPtr& coll);
	TableGen() = delete;
	~TableGen() = default;

	bool CreateParseTables();

	t_index GetTableIndex(t_symbol_id id, IndexTableKind table_kind) const;

	// getters
	const t_table& GetShiftTable() const { return m_tabActionShift; }
	const t_table& GetReduceTable() const { return m_tabActionReduce; }
	const t_table& GetJumpTable() const { return m_tabJump; }

	const t_mapIdStrId& GetNontermStringIdMap() const { return m_mapNonTermStrIds; }
	const t_mapIdStrId& GetTermStringIdMap() const { return m_mapTermStrIds; }

	const t_table& GetPartialsRuleTerm() const { return m_tabPartialRuleTerm; }
	const t_table& GetPartialsRuleNonterm() const { return m_tabPartialRuleNonterm; }
	const t_table& GetPartialsMatchLengthTerm() const { return m_tabPartialMatchLenTerm; }
	const t_table& GetPartialsMatchLengthNonterm() const { return m_tabPartialMatchLenNonterm; }
	const t_table& GetPartialsNontermLhsId() const { return m_tabPartialNontermLhsId; }

	const t_mapIdIdx& GetTermIndexMap() const { return m_mapTermIdx; }
	const t_mapIdIdx& GetNontermIndexMap() const { return m_mapNonTermIdx; }
	const t_mapIdIdx& GetSemanticIndexMap() const { return m_mapSemanticIdx; }

	const t_mapIdPrec& GetTermPrecMap() const { return m_mapTermPrec; }
	const t_mapIdAssoc& GetTermAssocMap() const { return m_mapTermAssoc; }

	const std::vector<std::size_t>& GetNumRhsSymbolsPerRule() const { return m_numRhsSymsPerRule; }
	const std::vector<t_index>& GetRuleLhsIndices() const { return m_ruleLhsIdx; }

	bool GetStopOnConflicts() const;


	// save the parsing tables to C++ code
	bool SaveParseTablesCXX(const std::string& file) const;

	// save the parsing tables to Java code
	bool SaveParseTablesJava(const std::string& file) const;

	// save the parsing tables to json
	// @see https://en.wikipedia.org/wiki/JSON
	bool SaveParseTablesJSON(const std::string& file) const;

	// save the parsing tables to rs code
	bool SaveParseTablesRS(const std::string& file) const;


protected:
	void CreateTableIndices();
	void CreateTerminalPrecedences();


private:
	const CollectionPtr m_collection{};         // collection of LALR(1) closures

	t_mapIdIdx m_mapTermIdx{};                  // maps the terminal ids to table indices
	t_mapIdIdx m_mapNonTermIdx{};               // maps the non-terminal ids to table indices
	t_mapIdIdx m_mapSemanticIdx{};              // maps the semantic ids to tables indices

	t_mapIdPrec m_mapTermPrec{};                // terminal operator precedences
	t_mapIdAssoc m_mapTermAssoc{};              // terminal operator associativities

	t_mapIdStrId m_mapNonTermStrIds{};          // maps the non-terminal ids to the respective string identifiers
	t_mapIdStrId m_mapTermStrIds{};             // maps the terminal ids to the respective string identifiers

	t_table m_tabActionShift{};                 // lalr(1) tables
	t_table m_tabActionReduce{};
	t_table m_tabJump{};

	t_table m_tabPartialRuleTerm{};             // partial match tables
	t_table m_tabPartialMatchLenTerm{};
	t_table m_tabPartialRuleNonterm{};
	t_table m_tabPartialMatchLenNonterm{};
	t_table m_tabPartialNontermLhsId{};

	std::vector<std::size_t> m_numRhsSymsPerRule{}; // number of symbols on rhs of a production rule
	std::vector<t_index> m_ruleLhsIdx{};            // nonterminal index of the rule's result type

	std::vector<TerminalPtr> m_seen_terminals{};    // collection of all possible terminal symbols
};

} // namespace lalr1

#endif
