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

#include "tableexport.h"
#include "options.h"
#include "timer.h"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <type_traits>

#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>


TableExporter::TableExporter(const Collection* coll)
	: m_collection{coll}
{ }


/**
 * save the parsing tables to C++ code
 */
bool TableExporter::SaveParseTablesCXX(const std::string& file) const
{
	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	ofstr << "/*\n";
	ofstr << " * Parsing tables created on " << get_timestamp();
	ofstr << " using liblalr1 by Tobias Weber, 2020-2022.\n";
	ofstr << " * DOI: https://doi.org/10.5281/zenodo.6987396\n";
	ofstr << " */\n\n";

	ofstr << "#ifndef __LALR1_TABLES__\n";
	ofstr << "#define __LALR1_TABLES__\n\n";

	ofstr << "namespace _lalr1_tables {\n\n";

	// save constants
	ofstr << "const constexpr t_index err = 0x" << std::hex << ERROR_VAL << std::dec;
	if constexpr(std::is_unsigned_v<t_index>)
		ofstr << "u";
	ofstr << ";\n";

	ofstr << "const constexpr t_index acc = 0x" << std::hex << ACCEPT_VAL << std::dec;
	if constexpr(std::is_unsigned_v<t_index>)
		ofstr << "u";
	ofstr << ";\n";

	ofstr << "const constexpr t_symbol_id eps = 0x" << std::hex << EPS_IDENT << std::dec;
	if constexpr(std::is_unsigned_v<t_symbol_id>)
		ofstr << "u";
	ofstr << ";\n";

	ofstr << "const constexpr t_symbol_id end = 0x" << std::hex << END_IDENT << std::dec;
	if constexpr(std::is_unsigned_v<t_symbol_id>)
		ofstr << "u";
	ofstr << ";\n";

	ofstr << "\n";

	// save lalr(1) tables
	const t_table& tabActionShift = m_collection->GetShiftTable();
	const t_table& tabActionReduce = m_collection->GetReduceTable();
	const t_table& tabJump = m_collection->GetJumpTable();

	tabActionShift.SaveCXX(ofstr, "tab_action_shift", "state", "terminal");
	tabActionReduce.SaveCXX(ofstr, "tab_action_reduce", "state", "lookahead");
	tabJump.SaveCXX(ofstr, "tab_jump", "state", "nonterminal");

	// save partial match tables
	if(m_collection->GetGenPartialMatches())
	{
		const t_table& tabPartialRuleTerm = m_collection->GetPartialsRuleTerm();
		const t_table& tabPartialRuleNonterm = m_collection->GetPartialsRuleNonterm();
		const t_table& tabPartialMatchLenTerm = m_collection->GetPartialsMatchLengthTerm();
		const t_table& tabPartialMatchLenNonterm = m_collection->GetPartialsMatchLengthNonterm();

		tabPartialRuleTerm.SaveCXX(ofstr, "tab_partials_rule_term", "state", "terminal");
		tabPartialMatchLenTerm.SaveCXX(ofstr, "tab_partials_matchlen_term", "state", "terminal");
		tabPartialRuleNonterm.SaveCXX(ofstr, "tab_partials_rule_nonterm", "state", "nonterminal");
		tabPartialMatchLenNonterm.SaveCXX(ofstr, "tab_partials_matchlen_nonterm", "state", "nonterminal");
	}

	// terminal symbol indices
	const t_mapIdIdx& mapTermIdx = m_collection->GetTermIndexMap();
	const t_mapIdStrId& mapTermStrIds = m_collection->GetTermStringIdMap();
	ofstr << "const t_mapIdIdx map_term_idx\n{{\n";
	for(const auto& [id, idx] : mapTermIdx)
	{
		ofstr << "\t{ ";
		if(id == EPS_IDENT)
			ofstr << "eps";
		else if(id == END_IDENT)
			ofstr << "end";
		else if(m_collection->GetUseOpChar() && isprintable(id))
			ofstr << "'" << char(id) << "'";
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
	ofstr << "}};\n\n";

	// non-terminal symbol indices
	const t_mapIdIdx& mapNonTermIdx = m_collection->GetNontermIndexMap();
	const t_mapIdStrId& mapNonTermStrIds = m_collection->GetNontermStringIdMap();
	ofstr << "const t_mapIdIdx map_nonterm_idx\n{{\n";
	for(const auto& [id, idx] : mapNonTermIdx)
	{
		ofstr << "\t{ " << id << ", " << idx << " },";

		// get string identifier
		if(auto iterStrId = mapNonTermStrIds.find(id);
			iterStrId != mapNonTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "}};\n\n";

	// semantic rule indices
	const t_mapIdIdx& mapSemanticIdx = m_collection->GetSemanticIndexMap();
	ofstr << "const t_mapSemanticIdIdx map_semantic_idx\n{{\n";
	for(const auto& [id, idx] : mapSemanticIdx)
		ofstr << "\t{ " << id << ", " << idx << " },\n";
	ofstr << "}};\n\n";

	// number of symbols on right-hand side of rule
	const auto& numRhsSymsPerRule = m_collection->GetNumRhsSymbolsPerRule();
	ofstr << "const t_vecIdx vec_num_rhs_syms{{ ";
	for(const auto& val : numRhsSymsPerRule)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	// index of lhs nonterminal in rule
	const auto& ruleLhsIdx = m_collection->GetRuleLhsIndices();
	ofstr << "const t_vecIdx vec_lhs_idx{{ ";
	for(const auto& val : ruleLhsIdx)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	ofstr << "}\n\n\n";  // end namespace


	// lalr(1) tables getter
	ofstr << "static\nstd::tuple<const t_table*, const t_table*, const t_table*,\n";
	ofstr << "\tconst t_vecIdx*, const t_vecIdx*>\n";
	ofstr << "get_lalr1_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t&_lalr1_tables::tab_action_shift, &_lalr1_tables::tab_action_reduce, &_lalr1_tables::tab_jump,\n";
	ofstr << "\t\t&_lalr1_tables::vec_num_rhs_syms, &_lalr1_tables::vec_lhs_idx);\n";
	ofstr << "}\n\n";

	// partial match tables getter
	ofstr << "[[maybe_unused]] static\nstd::tuple<const t_table*, const t_table*,\n";
	ofstr << "\tconst t_table*, const t_table*>\n";
	ofstr << "get_lalr1_partials_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	if(m_collection->GetGenPartialMatches())
	{
		ofstr << "\t\t&_lalr1_tables::tab_partials_rule_term, &_lalr1_tables::tab_partials_matchlen_term,\n";
		ofstr << "\t\t&_lalr1_tables::tab_partials_rule_nonterm, &_lalr1_tables::tab_partials_matchlen_nonterm);\n";
	}
	else
	{
		ofstr << "\t\tnullptr, nullptr,\n";
		ofstr << "\t\tnullptr, nullptr);\n";
	}
	ofstr << "}\n\n";

	// index maps getter
	ofstr << "static\nstd::tuple<const t_mapIdIdx*, const t_mapIdIdx*, const t_mapSemanticIdIdx*>\n";
	ofstr << "get_lalr1_table_indices()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t&_lalr1_tables::map_term_idx, &_lalr1_tables::map_nonterm_idx, &_lalr1_tables::map_semantic_idx);\n";
	ofstr << "}\n\n";

	// constants getter
	ofstr << "static constexpr\nstd::tuple<t_index, t_index, t_symbol_id, t_symbol_id>\n";
	ofstr << "get_lalr1_constants()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t_lalr1_tables::err, _lalr1_tables::acc, _lalr1_tables::eps, _lalr1_tables::end);\n";
	ofstr << "}\n\n";

	ofstr << "\n#endif" << std::endl;
	return true;
}


/**
 * save the parsing tables to Java code
 */
bool TableExporter::SaveParseTablesJava(const std::string& file) const
{
	const bool map_special_vals_to_negative = g_options.GetUseNegativeTableValues();

	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	std::string stem = std::filesystem::path{file}.stem().string();

	ofstr << "/*\n";
	ofstr << " * Parsing tables created on " << get_timestamp();
	ofstr << " using liblalr1 by Tobias Weber, 2020-2022.\n";
	ofstr << " * DOI: https://doi.org/10.5281/zenodo.6987396\n";
	ofstr << " */\n\n";

	ofstr << "public class " << stem << " implements lalr1_java.ParsingTableInterface" << "\n{\n";

	// save constants
	if(map_special_vals_to_negative)
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

	ofstr << "\n";

	// save lalr(1) tables
	const t_table& tabActionShift = m_collection->GetShiftTable();
	const t_table& tabActionReduce = m_collection->GetReduceTable();
	const t_table& tabJump = m_collection->GetJumpTable();

	tabActionShift.SaveJava(ofstr, "tab_action_shift", "state", "terminal", "private", 1);
	tabActionReduce.SaveJava(ofstr, "tab_action_reduce", "state", "lookahead", "private", 1);
	tabJump.SaveJava(ofstr, "tab_jump", "state", "nonterminal", "private", 1);

	// save partial match tables
	if(m_collection->GetGenPartialMatches())
	{
		const t_table& tabPartialRuleTerm = m_collection->GetPartialsRuleTerm();
		const t_table& tabPartialRuleNonterm = m_collection->GetPartialsRuleNonterm();
		const t_table& tabPartialMatchLenTerm = m_collection->GetPartialsMatchLengthTerm();
		const t_table& tabPartialMatchLenNonterm = m_collection->GetPartialsMatchLengthNonterm();

		tabPartialRuleTerm.SaveJava(ofstr, "tab_partials_rule_term", "state", "terminal", "private", 1);
		tabPartialMatchLenTerm.SaveJava(ofstr, "tab_partials_matchlen_term", "state", "terminal", "private", 1);
		tabPartialRuleNonterm.SaveJava(ofstr, "tab_partials_rule_nonterm", "state", "nonterminal", "private", 1);
		tabPartialMatchLenNonterm.SaveJava(ofstr, "tab_partials_matchlen_nonterm", "state", "nonterminal", "private", 1);
	}

	// terminal symbol indices
	const t_mapIdIdx& mapTermIdx = m_collection->GetTermIndexMap();
	const t_mapIdStrId& mapTermStrIds = m_collection->GetTermStringIdMap();
	ofstr << "\tprivate final int[][] map_term_idx =\n\t{\n";
	for(const auto& [id, idx] : mapTermIdx)
	{
		ofstr << "\t\t{ ";
		if(id == EPS_IDENT)
			ofstr << "eps";
		else if(id == END_IDENT)
			ofstr << "end";
		else if(m_collection->GetUseOpChar() && isprintable(id))
			ofstr << "'" << char(id) << "'";
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
	const t_mapIdIdx& mapNonTermIdx = m_collection->GetNontermIndexMap();
	const t_mapIdStrId& mapNonTermStrIds = m_collection->GetNontermStringIdMap();
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

	// semantic rule indices
	const t_mapIdIdx& mapSemanticIdx = m_collection->GetSemanticIndexMap();
	ofstr << "\tprivate final int[][] map_semantic_idx =\n\t{\n";
	for(const auto& [id, idx] : mapSemanticIdx)
		ofstr << "\t\t{ " << id << ", " << idx << " },\n";
	ofstr << "\t};\n\n";

	// number of symbols on right-hand side of rule
	const auto& numRhsSymsPerRule = m_collection->GetNumRhsSymbolsPerRule();
	ofstr << "\tprivate final int[] vec_num_rhs_syms =\n\t{\n\t\t";
	for(const auto& val : numRhsSymsPerRule)
		ofstr << val << ", ";
	ofstr << "\n\t};\n\n";

	// index of lhs nonterminal in rule
	const auto& ruleLhsIdx = m_collection->GetRuleLhsIndices();
	ofstr << "\tprivate final int[] vec_lhs_idx =\n\t{\n\t\t";
	for(const auto& val : ruleLhsIdx)
		ofstr << val << ", ";
	ofstr << "\n\t};\n\n";

	// getter functions
	ofstr << "\t@Override public int GetErrConst() { return err; }\n";
	ofstr << "\t@Override public int GetAccConst() { return acc; }\n";
	ofstr << "\t@Override public int GetEndConst() { return end; }\n";
	ofstr << "\t@Override public int GetEpsConst() { return eps; }\n";

	ofstr << "\t@Override public int[][] GetShiftTab() { return tab_action_shift; }\n";
	ofstr << "\t@Override public int[][] GetReduceTab() { return tab_action_reduce; }\n";
	ofstr << "\t@Override public int[][] GetJumpTab() { return tab_jump; }\n";

	ofstr << "\t@Override public int[][] GetTermIndexMap() { return map_term_idx; }\n";
	ofstr << "\t@Override public int[][] GetNontermIndexMap() { return map_nonterm_idx; }\n";
	ofstr << "\t@Override public int[][] GetSemanticIndexMap() { return map_semantic_idx; }\n";

	ofstr << "\t@Override public int[] GetNumRhsSymbols() { return vec_num_rhs_syms; }\n";
	ofstr << "\t@Override public int[] GetLhsIndices() { return vec_lhs_idx; }\n";

	ofstr << "\t@Override public int[][] GetPartialsRuleTerm() { return tab_partials_rule_term; }\n";
	ofstr << "\t@Override public int[][] GetPartialsRuleNonterm() { return tab_partials_rule_nonterm; }\n";
	ofstr << "\t@Override public int[][] GetPartialsMatchLengthTerm() { return tab_partials_matchlen_term; }\n";
	ofstr << "\t@Override public int[][] GetPartialsMatchLengthNonterm() { return tab_partials_matchlen_nonterm; }\n";

	ofstr << "}\n";  // end class
	return true;
}


/**
 * save the parsing tables to json
 * @see https://en.wikipedia.org/wiki/JSON
 */
bool TableExporter::SaveParseTablesJSON(const std::string& file) const
{
	const bool map_special_vals_to_negative = g_options.GetUseNegativeTableValues();

	std::unordered_map<t_index, int> special_values
	{
		{ ERROR_VAL, -1 },
		{ ACCEPT_VAL, -2 },
	};
	std::unordered_map<t_symbol_id, int> special_idents
	{
		{ END_IDENT, -1 },
		{ EPS_IDENT, -2 },
	};

	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	ofstr << "{\n";

	// meta infos
	ofstr << "\"infos\" : ";
	ofstr << "\"Parsing tables created on " << get_timestamp();
	ofstr << " using liblalr1 by Tobias Weber, 2020-2022";
	ofstr << " (DOI: https://doi.org/10.5281/zenodo.6987396).\",\n";

	// constants
	ofstr << "\n\"consts\" : {\n";
	//ofstr << "\t\"acc_rule\" : " << m_accepting_rule << ",\n";

	if(map_special_vals_to_negative)
	{
		ofstr << "\t\"err\" : " << special_values[ERROR_VAL] << ",\n";
		ofstr << "\t\"acc\" : " << special_values[ACCEPT_VAL] << ",\n";
		ofstr << "\t\"eps\" : " << special_idents[EPS_IDENT] << ",\n";
		ofstr << "\t\"end\" : " << special_idents[END_IDENT] << "\n";
	}
	else
	{
		/*ofstr << "\t\"err\" : 0x" << std::hex << ERROR_VAL << std::dec << ",\n";
		ofstr << "\t\"acc\" : 0x" << std::hex << ACCEPT_VAL << std::dec << ",\n";
		ofstr << "\t\"eps\" : 0x" << std::hex << EPS_IDENT << std::dec << ",\n";
		ofstr << "\t\"end\" : 0x" << std::hex << END_IDENT << std::dec << "\n";*/
		ofstr << "\t\"err\" : " << ERROR_VAL << ",\n";
		ofstr << "\t\"acc\" : " << ACCEPT_VAL << ",\n";
		ofstr << "\t\"eps\" : " << EPS_IDENT << ",\n";
		ofstr << "\t\"end\" : " << END_IDENT << "\n";
	}

	ofstr << "},\n\n";

	// lalr(1) tables
	const t_table& tabActionShift = m_collection->GetShiftTable();
	const t_table& tabActionReduce = m_collection->GetReduceTable();
	const t_table& tabJump = m_collection->GetJumpTable();

	tabActionShift.SaveJSON(ofstr, "shift", "state", "terminal", &special_values);
	ofstr << ",\n\n";
	tabActionReduce.SaveJSON(ofstr, "reduce", "state", "lookahead", &special_values);
	ofstr << ",\n\n";
	tabJump.SaveJSON(ofstr, "jump", "state", "nonterminal", &special_values);
	ofstr << ",\n\n";

	// partial match tables
	if(m_collection->GetGenPartialMatches())
	{
		const t_table& tabPartialRuleTerm = m_collection->GetPartialsRuleTerm();
		const t_table& tabPartialRuleNonterm = m_collection->GetPartialsRuleNonterm();
		const t_table& tabPartialMatchLenTerm = m_collection->GetPartialsMatchLengthTerm();
		const t_table& tabPartialMatchLenNonterm = m_collection->GetPartialsMatchLengthNonterm();

		tabPartialRuleTerm.SaveJSON(ofstr,
			"partials_rule_term", "state", "terminal", &special_values);
		ofstr << ",\n\n";
		tabPartialMatchLenTerm.SaveJSON(ofstr,
			"partials_matchlen_term", "state", "terminal");
		ofstr << ",\n\n";
		tabPartialRuleNonterm.SaveJSON(ofstr,
			"partials_rule_nonterm", "state", "nonterminal", &special_values);
		ofstr << ",\n\n";
		tabPartialMatchLenNonterm.SaveJSON(ofstr,
			"partials_matchlen_nonterm", "state", "nonterminal");
		ofstr << ",\n";
	}

	// terminal symbol indices
	const t_mapIdIdx& mapTermIdx = m_collection->GetTermIndexMap();
	const t_mapIdStrId& mapTermStrIds = m_collection->GetTermStringIdMap();
	ofstr << "\n\"term_idx\" : [\n";
	for(auto iter = mapTermIdx.begin(); iter != mapTermIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;

		ofstr << "\t[ ";
		if(auto iter_special = special_idents.find(id); iter_special != special_idents.end())
		{
			ofstr << iter_special->second;
		}
		else
		{
			if(m_collection->GetUseOpChar() && isprintable(id))
				ofstr << "\"" << char(id) << "\"";
			else
				ofstr << id;
		}
		ofstr << ", " << idx;

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id); iterStrId != mapTermStrIds.end())
			ofstr << ", \"" << iterStrId->second << "\"";

		ofstr << " ]";

		if(std::next(iter, 1) != mapTermIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "],\n";

	// non-terminal symbol indices
	const t_mapIdIdx& mapNonTermIdx = m_collection->GetNontermIndexMap();
	const t_mapIdStrId& mapNonTermStrIds = m_collection->GetNontermStringIdMap();
	ofstr << "\n\"nonterm_idx\" : [\n";
	for(auto iter = mapNonTermIdx.begin(); iter != mapNonTermIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;
		ofstr << "\t[ " << id << ", " << idx;

		// get string identifier
		if(auto iterStrId = mapNonTermStrIds.find(id); iterStrId != mapNonTermStrIds.end())
			ofstr << ", \"" << iterStrId->second << "\"";

		ofstr << " ]";

		if(std::next(iter, 1) != mapNonTermIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "],\n";

	// semantic rule indices
	const t_mapIdIdx& mapSemanticIdx = m_collection->GetSemanticIndexMap();
	ofstr << "\n\"semantic_idx\" : [\n";
	for(auto iter = mapSemanticIdx.begin(); iter != mapSemanticIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;
		ofstr << "\t[ " << id << ", " << idx << " ]";
		if(std::next(iter, 1) != mapSemanticIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "],\n";

	// number of symbols on right-hand side of rule
	const auto& numRhsSymsPerRule = m_collection->GetNumRhsSymbolsPerRule();
	ofstr << "\n\"num_rhs_syms\" : [ ";
	for(auto iter = numRhsSymsPerRule.begin(); iter != numRhsSymsPerRule.end(); std::advance(iter, 1))
	{
		ofstr << *iter;
		if(std::next(iter, 1) != numRhsSymsPerRule.end())
			ofstr << ",";
		ofstr << " ";
	}
	ofstr << "],\n";

	// index of lhs nonterminal in rule
	const auto& ruleLhsIdx = m_collection->GetRuleLhsIndices();
	ofstr << "\n\"lhs_idx\" : [ ";
	for(auto iter = ruleLhsIdx.begin(); iter != ruleLhsIdx.end(); std::advance(iter, 1))
	{
		ofstr << *iter;
		if(std::next(iter, 1) != ruleLhsIdx.end())
			ofstr << ",";
		ofstr << " ";
	}
	ofstr << "]\n";

	ofstr << "}" << std::endl;
	return true;
}
