/**
 * exports lalr(1) tables to rs code
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

#include "tableexport_rs.h"
#include "options.h"
#include "timer.h"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>


namespace lalr1 {

/**
 * save a parsing table to rs code
 */
void TableExportRS::SaveParseTable(const t_table& tab,
	std::ostream& ostr, const std::string& var,
		const std::string& row_label, const std::string& col_label,
		const std::string& elem_label,
		std::optional<std::string> ty_override)
{
	// get the data type name
	std::string ty;
	if(ty_override)
		ty = *ty_override;
	else
		ty = get_rs_typename<typename t_table::value_type>();

	ostr << "pub const " << var;
	ostr << " : [[" << ty << "; " << tab.size2();
	if(col_label != "")
		ostr << " /* " << col_label << " */";
	ostr << "]; " << tab.size1();
	if(row_label != "")
		ostr << " /* " << row_label << " */";
	ostr << "]";
	ostr << " =\n[ /* " << elem_label << " */\n";

	for(std::size_t row = 0; row < tab.size1(); ++row)
	{
		ostr << "\t[ ";
		for(std::size_t col = 0; col < tab.size2(); ++col)
		{
			const typename t_table::value_type& entry = tab(row, col);
			if(entry == tab.GetErrorVal())
				ostr << "ERR";
			else if(entry == tab.GetAcceptVal())
				ostr << "ACC";
			else
				ostr << entry;

			if(col < tab.size2() - 1)
				ostr << ",";
			ostr << " ";
		}
		ostr << "]";
		if(row < tab.size1() - 1)
			ostr << ",";

		if(row_label != "")
			ostr << " // " << row_label << " " << row;

		ostr << "\n";
	}
	ostr << "];\n";
}



/**
 * save the parsing tables to rs code
 */
bool TableExportRS::SaveParseTables(const TableGen& tab, const std::string& file)
{
	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	// meta infos
	ofstr << "/*\n";
	ofstr << " * Parsing tables created on " << get_timestamp() << "\n";
	ofstr << " * using liblalr1 by Tobias Weber, 2020-2024\n";
	ofstr << " * (DOI: https://doi.org/10.5281/zenodo.6987396).\n";
	ofstr << " */\n\n";

	// table module
	ofstr << "#[allow(unused)]\n";
	ofstr << "pub mod lalr1_tables\n{\n";

	// basic data types
	ofstr << "pub type TIndex = " << get_rs_typename<t_index>() << ";\n";
	ofstr << "pub type TSymbolId = " << get_rs_typename<t_symbol_id>() << ";\n";
	ofstr << "pub type TSemanticId = " << get_rs_typename<t_semantic_id>() << ";\n";
	ofstr << "pub type TPrec = " << get_rs_typename<t_precedence>() << ";\n";
	ofstr << "pub type TAssoc = " << get_rs_typename<t_associativity>() << ";\n";
	std::string ty_idx = "TIndex";
	std::string ty_sym = "TSymbolId";
	std::string ty_sem = "TSemanticId";
	std::string ty_prec = "TPrec";
	std::string ty_assoc = "TAssoc";

	ofstr << "\n";

	// constants
	t_index acc_rule_idx = tab.GetTableIndex(tab.GetAcceptingRule(), IndexTableKind::SEMANTIC);
	ofstr << "pub const ERR : " << ty_idx << " = 0x" << std::hex << ERROR_VAL << std::dec << ";\n";
	ofstr << "pub const ACC : " << ty_idx << " = 0x" << std::hex << ACCEPT_VAL << std::dec << ";\n";
	ofstr << "pub const EPS : " << ty_sym << " = 0x" << std::hex << EPS_IDENT << std::dec << ";\n";
	ofstr << "pub const END : " << ty_sym << " = 0x" << std::hex << END_IDENT << std::dec << ";\n";
	ofstr << "pub const START : " << ty_idx << " = 0x" << std::hex << tab.GetStartingState() << std::dec << ";\n";
	ofstr << "pub const ACCEPT : " << ty_idx << " = 0x" << std::hex << acc_rule_idx << std::dec << ";\n";

	ofstr << "\n";

	// lalr(1) tables
	SaveParseTable(tab.GetShiftTable(), ofstr,
		"SHIFT", "state", "terminal", "state", ty_idx);
	SaveParseTable(tab.GetReduceTable(), ofstr,
		"REDUCE", "state", "lookahead", "rule index", ty_idx);
	SaveParseTable(tab.GetJumpTable(), ofstr,
		"JUMP", "state", "nonterminal", "state", ty_idx);
	ofstr << "\n";

	// partial match tables
	if(tab.GetGenPartialMatches())
	{
		SaveParseTable(tab.GetPartialsRuleTerm(),
			ofstr, "PARTIALS_RULE_TERM", "state", "terminal", "rule index", ty_idx);
		SaveParseTable(tab.GetPartialsMatchLengthTerm(),
			ofstr, "PARTIALS_MATCHLEN_TERM", "state", "terminal", "length", ty_idx);
		SaveParseTable(tab.GetPartialsRuleNonterm(),
			ofstr, "PARTIALS_RULE_NONTERM", "state", "nonterminal", "rule index", ty_idx);
		SaveParseTable(tab.GetPartialsMatchLengthNonterm(),
			ofstr, "PARTIALS_MATCHLEN_NONTERM", "state", "nonterminal", "length", ty_idx);
		SaveParseTable(tab.GetPartialsNontermLhsId(),
			ofstr, "PARTIALS_LHS_NONTERM", "state", "nonterminal", "lhs nonterminal id", ty_idx);
		ofstr << "\n";
	}

	// terminal symbol indices
	const t_mapIdIdx& mapTermIdx = tab.GetTermIndexMap();
	const t_mapIdStrId& mapTermStrIds = tab.GetTermStringIdMap();
	ofstr << "pub const TERM_IDX : [(" << ty_sym << ", " << ty_idx
		<< ", &str); " << mapTermIdx.size() << "] =\n[\n";
	for(auto iter = mapTermIdx.begin(); iter != mapTermIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;

		ofstr << "\t( ";
		if(id == END_IDENT)
			ofstr << "END";
		else if(id == EPS_IDENT)
			ofstr << "EPS";
		else if(tab.GetUseOpChar() && isprintable(id))
			ofstr << "'" << get_escaped_char(char(id)) << "' as " << ty_sym;
		else
			ofstr << id;
		ofstr << ", " << idx;

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id); iterStrId != mapTermStrIds.end())
			ofstr << ", \"" << iterStrId->second << "\"";
		ofstr << " )";

		if(std::next(iter, 1) != mapTermIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "];\n";

	// non-terminal symbol indices
	const t_mapIdIdx& mapNonTermIdx = tab.GetNontermIndexMap();
	const t_mapIdStrId& mapNonTermStrIds = tab.GetNontermStringIdMap();
	ofstr << "pub const NONTERM_IDX : [(" << ty_sym << ", " << ty_idx
		<< ", &str); " << mapNonTermIdx.size() << "] =\n[\n";
	for(auto iter = mapNonTermIdx.begin(); iter != mapNonTermIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;
		ofstr << "\t( " << id << ", " << idx;

		// get string identifier
		if(auto iterStrId = mapNonTermStrIds.find(id); iterStrId != mapNonTermStrIds.end())
			ofstr << ", \"" << iterStrId->second << "\"";
		ofstr << " )";

		if(std::next(iter, 1) != mapNonTermIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "];\n";

	// semantic rule indices
	const t_mapIdIdx& mapSemanticIdx = tab.GetSemanticIndexMap();
	ofstr << "pub const SEMANTIC_IDX : [(" << ty_sem << ", " << ty_idx
		<< "); " << mapSemanticIdx.size() << "] =\n[\n";
	for(auto iter = mapSemanticIdx.begin(); iter != mapSemanticIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;
		ofstr << "\t( " << id << ", " << idx << " )";
		if(std::next(iter, 1) != mapSemanticIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "];\n\n";

	// terminal operator precedences
	const t_mapIdPrec& mapTermPrec = tab.GetTermPrecMap();
	ofstr << "pub const TERM_PREC : [(" << ty_sym << ", " << ty_prec
		<< "); " << mapTermPrec.size() << "] =\n[\n";
	for(auto iter = mapTermPrec.begin(); iter != mapTermPrec.end(); std::advance(iter, 1))
	{
		const auto& [id, prec] = *iter;
		ofstr << "\t( " << id << ", " << prec << " )";

		if(std::next(iter, 1) != mapTermPrec.end())
			ofstr << ",";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id); iterStrId != mapTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "];\n";

	// terminal operator associativities
	const t_mapIdAssoc& mapTermAssoc = tab.GetTermAssocMap();

	ofstr << "pub const TERM_ASSOC : [(" << ty_sym << ", " << ty_assoc
		<< "); " << mapTermPrec.size() << "] =\n[\n";
	for(auto iter = mapTermAssoc.begin(); iter != mapTermAssoc.end(); std::advance(iter, 1))
	{
		const auto& [id, assoc] = *iter;
		ofstr << "\t( " << id << ", '" << assoc << "' as " << ty_assoc << " )";

		if(std::next(iter, 1) != mapTermAssoc.end())
			ofstr << ",";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id); iterStrId != mapTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "];\n\n";

	// number of symbols on right-hand side of rule
	const auto& numRhsSymsPerRule = tab.GetNumRhsSymbolsPerRule();
	ofstr << "pub const NUM_RHS_SYMS : [" << ty_idx << "; "
		<< numRhsSymsPerRule.size() << "] = [ ";
	for(auto iter = numRhsSymsPerRule.begin(); iter != numRhsSymsPerRule.end(); std::advance(iter, 1))
	{
		ofstr << *iter;
		if(std::next(iter, 1) != numRhsSymsPerRule.end())
			ofstr << ",";
		ofstr << " ";
	}
	ofstr << "];\n";

	// index of lhs nonterminal in rule
	const auto& ruleLhsIdx = tab.GetRuleLhsIndices();
	ofstr << "pub const LHS_IDX : [" << ty_idx << "; "
		<< ruleLhsIdx.size() << "] = [ ";
	for(auto iter = ruleLhsIdx.begin(); iter != ruleLhsIdx.end(); std::advance(iter, 1))
	{
		ofstr << *iter;
		if(std::next(iter, 1) != ruleLhsIdx.end())
			ofstr << ",";
		ofstr << " ";
	}
	ofstr << "];\n";

	ofstr << "}\n";  // end of module
	return true;
}

} // namespace lalr1
