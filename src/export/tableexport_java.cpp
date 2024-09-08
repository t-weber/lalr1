/**
 * exports lalr(1) tables to java code
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

#include "tableexport_java.h"
#include "options.h"
#include "timer.h"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>


namespace lalr1 {


/**
 * save a parsing table to java code
 */
void TableExportJava::SaveParseTable(const t_table& tab,
	std::ostream& ostr, const std::string& var,
	const std::string& row_label, const std::string& col_label,
	const std::string& elem_label,
	const std::string& acc_level, std::size_t indent)
{
	auto do_indent = [&ostr, indent]()
	{
		for(std::size_t i = 0; i < indent; ++i)
			ostr << "\t";
	};

	do_indent();
	ostr << acc_level << " final int[";
	//ostr << " " << tab.size1();
	if(row_label != "")
		ostr << " /*" << row_label << "*/ ";
	ostr << "][";
	//ostr << " " << tab.size2();
	if(col_label != "")
		ostr << " /*" << col_label << "*/ ";
	ostr << "] " << var << " =\n";

	do_indent();
	ostr << "{ /*" << elem_label << "*/\n";
	for(std::size_t row = 0; row < tab.size1(); ++row)
	{
		do_indent();
		ostr << "\t{ ";
		for(std::size_t col = 0; col < tab.size2(); ++col)
		{
			const typename t_table::value_type& entry = tab(row, col);

			if(entry == tab.GetErrorVal())
				ostr << "err, ";
			else if(entry == tab.GetAcceptVal())
				ostr << "acc, ";
			else
				ostr << entry << ", ";
		}
		ostr << "}, // " << row_label << " " << row << "\n";
	}

	do_indent();
	ostr << "};\n\n";
}



/**
 * save the parsing tables to java code
 */
bool TableExportJava::SaveParseTables(const TableGen& tab, const std::string& file)
{
	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	std::string stem = std::filesystem::path{file}.stem().string();

	ofstr << "/*\n";
	ofstr << " * Parsing tables created on " << get_timestamp();
	ofstr << " using liblalr1 by Tobias Weber, 2020-2023.\n";
	ofstr << " * DOI: https://doi.org/10.5281/zenodo.6987396\n";
	ofstr << " */\n\n";

	ofstr << "public class " << stem << " implements lalr1_java.ParsingTableInterface" << "\n{\n";

	// save constants
	if(tab.GetUseNegativeTableValues())
	{
		ofstr << "\tprivate final int err = -1;\n";
		ofstr << "\tprivate final int acc = -2;\n";
		ofstr << "\tprivate final int end = -1;\n";
		ofstr << "\tprivate final int eps = -2;\n";
	}
	else
	{
		ofstr << "\tprivate final int err = 0x" << std::hex << ERROR_VAL << std::dec;
		if constexpr(std::is_unsigned_v<t_index>)
			ofstr << "u";
		ofstr << ";\n";

		ofstr << "\tprivate final int acc = 0x" << std::hex << ACCEPT_VAL << std::dec;
		if constexpr(std::is_unsigned_v<t_index>)
			ofstr << "u";
		ofstr << ";\n";

		ofstr << "\tprivate final int eps = 0x" << std::hex << EPS_IDENT << std::dec;
		if constexpr(std::is_unsigned_v<t_symbol_id>)
			ofstr << "u";
		ofstr << ";\n";

		ofstr << "\tprivate final int end = 0x" << std::hex << END_IDENT << std::dec;
		if constexpr(std::is_unsigned_v<t_symbol_id>)
			ofstr << "u";
		ofstr << ";\n";
	}

	t_index acc_rule_idx = tab.GetTableIndex(tab.GetAcceptingRule(), IndexTableKind::SEMANTIC);
	ofstr << "\tprivate final int accept = " << acc_rule_idx << ";\n";
	ofstr << "\tprivate final int start = " << tab.GetStartingState() << ";\n";

	ofstr << "\n";

	// save lalr(1) tables
	const t_table& tabActionShift = tab.GetShiftTable();
	const t_table& tabActionReduce = tab.GetReduceTable();
	const t_table& tabJump = tab.GetJumpTable();

	SaveParseTable(tabActionShift, ofstr,
		"tab_action_shift", "state", "terminal", "state", "private", 1);
	SaveParseTable(tabActionReduce, ofstr,
		"tab_action_reduce", "state", "lookahead", "rule index", "private", 1);
	SaveParseTable(tabJump, ofstr,
		"tab_jump", "state", "nonterminal", "state", "private", 1);

	// save partial match tables
	if(tab.GetGenPartialMatches())
	{
		const t_table& tabPartialRuleTerm = tab.GetPartialsRuleTerm();
		const t_table& tabPartialRuleNonterm = tab.GetPartialsRuleNonterm();
		const t_table& tabPartialMatchLenTerm = tab.GetPartialsMatchLengthTerm();
		const t_table& tabPartialMatchLenNonterm = tab.GetPartialsMatchLengthNonterm();
		const t_table& tabPartialNontermLhs = tab.GetPartialsNontermLhsId();

		SaveParseTable(tabPartialRuleTerm, ofstr,
			"tab_partials_rule_term", "state", "terminal", "rule index", "private", 1);
		SaveParseTable(tabPartialMatchLenTerm, ofstr,
			"tab_partials_matchlen_term", "state", "terminal", "length", "private", 1);
		SaveParseTable(tabPartialRuleNonterm, ofstr,
			"tab_partials_rule_nonterm", "state", "nonterminal", "rule index", "private", 1);
		SaveParseTable(tabPartialMatchLenNonterm, ofstr,
			"tab_partials_matchlen_nonterm", "state", "nonterminal", "length", "private", 1);
		SaveParseTable(tabPartialNontermLhs, ofstr,
			"tab_partials_lhs_nonterm", "state", "nonterminal", "lhs nonterminal id", "private", 1);
	}

	// terminal symbol indices
	const t_mapIdIdx& mapTermIdx = tab.GetTermIndexMap();
	const t_mapIdStrId& mapTermStrIds = tab.GetTermStringIdMap();
	ofstr << "\tprivate final int[][] map_term_idx =\n\t{\n";
	for(const auto& [id, idx] : mapTermIdx)
	{
		ofstr << "\t\t{ ";
		if(id == EPS_IDENT)
			ofstr << "eps";
		else if(id == END_IDENT)
			ofstr << "end";
		else if(tab.GetUseOpChar() && isprintable(id))
			ofstr << "'" << get_escaped_char(char(id)) << "'";
		else
			ofstr << id;
		ofstr << ", " << idx << " },";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id);
			iterStrId != mapTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "\t};\n\n";

	// non-terminal symbol indices
	const t_mapIdIdx& mapNonTermIdx = tab.GetNontermIndexMap();
	const t_mapIdStrId& mapNonTermStrIds = tab.GetNontermStringIdMap();
	ofstr << "\tprivate final int[][] map_nonterm_idx =\n\t{\n";
	for(const auto& [id, idx] : mapNonTermIdx)
	{
		ofstr << "\t\t{ " << id << ", " << idx << " },";

		// get string identifier
		if(auto iterStrId = mapNonTermStrIds.find(id);
			iterStrId != mapNonTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "\t};\n\n";

	// terminal operator precedences
	const t_mapIdPrec& mapTermPrec = tab.GetTermPrecMap();
	ofstr << "\tprivate final int[][] map_term_prec =\n\t{\n";
	for(const auto& [id, prec] : mapTermPrec)
	{
		ofstr << "\t\t{ " << id << ", " << prec << " },";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id);
			iterStrId != mapTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "\t};\n\n";

	// terminal operator associativities
	const t_mapIdAssoc& mapTermAssoc = tab.GetTermAssocMap();
	ofstr << "\tprivate final int[][] map_term_assoc =\n\t{\n";
	for(const auto& [id, assoc] : mapTermAssoc)
	{
		ofstr << "\t\t{ " << id << ", '" << assoc << "' },";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id);
			iterStrId != mapTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "\t};\n\n";

	// semantic rule indices
	const t_mapIdIdx& mapSemanticIdx = tab.GetSemanticIndexMap();
	ofstr << "\tprivate final int[][] map_semantic_idx =\n\t{\n";
	for(const auto& [id, idx] : mapSemanticIdx)
		ofstr << "\t\t{ " << id << ", " << idx << " },\n";
	ofstr << "\t};\n\n";

	// number of symbols on right-hand side of rule
	const auto& numRhsSymsPerRule = tab.GetNumRhsSymbolsPerRule();
	ofstr << "\tprivate final int[] vec_num_rhs_syms =\n\t{\n\t\t";
	for(const auto& val : numRhsSymsPerRule)
		ofstr << val << ", ";
	ofstr << "\n\t};\n\n";

	// index of lhs nonterminal in rule
	const auto& ruleLhsIdx = tab.GetRuleLhsIndices();
	ofstr << "\tprivate final int[] vec_lhs_idx =\n\t{\n\t\t";
	for(const auto& val : ruleLhsIdx)
		ofstr << val << ", ";
	ofstr << "\n\t};\n\n";

	// getter functions
	ofstr << "\t@Override public int GetErrConst() { return err; }\n";
	ofstr << "\t@Override public int GetAccConst() { return acc; }\n";
	ofstr << "\t@Override public int GetEndConst() { return end; }\n";
	ofstr << "\t@Override public int GetEpsConst() { return eps; }\n";
	ofstr << "\t@Override public int GetStartConst() { return start; }\n";

	ofstr << "\t@Override public int[][] GetShiftTab() { return tab_action_shift; }\n";
	ofstr << "\t@Override public int[][] GetReduceTab() { return tab_action_reduce; }\n";
	ofstr << "\t@Override public int[][] GetJumpTab() { return tab_jump; }\n";

	ofstr << "\t@Override public int[][] tab.GetTermIndexMap() { return map_term_idx; }\n";
	ofstr << "\t@Override public int[][] tab.GetNontermIndexMap() { return map_nonterm_idx; }\n";
	ofstr << "\t@Override public int[][] tab.GetSemanticIndexMap() { return map_semantic_idx; }\n";

	ofstr << "\t@Override public int[] GetNumRhsSymbols() { return vec_num_rhs_syms; }\n";
	ofstr << "\t@Override public int[] GetLhsIndices() { return vec_lhs_idx; }\n";

	ofstr << "\t@Override public int[][] tab.GetPartialsRuleTerm() { return tab_partials_rule_term; }\n";
	ofstr << "\t@Override public int[][] tab.GetPartialsRuleNonterm() { return tab_partials_rule_nonterm; }\n";
	ofstr << "\t@Override public int[][] tab.GetPartialsMatchLengthTerm() { return tab_partials_matchlen_term; }\n";
	ofstr << "\t@Override public int[][] tab.GetPartialsMatchLengthNonterm() { return tab_partials_matchlen_nonterm; }\n";
	ofstr << "\t@Override public int[][] GetPartialsLhsIdNonterm() { return tab_partials_lhs_nonterm; }\n";

	ofstr << "\t@Override public int[][] GetPrecedences() { return map_term_prec; }\n";
	ofstr << "\t@Override public int[][] GetAssociativities() { return map_term_assoc; }\n";

	ofstr << "}\n";  // end class
	return true;
}

} // namespace lalr1
