/**
 * LALR(1) table interface
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 23-oct-2022
 * @license see 'LICENSE' file
 */

package lalr1_java;

public interface ParsingTableInterface
{
	public int GetErrConst();
	public int GetAccConst();
	public int GetEndConst();
	public int GetEpsConst();
	public int GetStartConst();

	public int[][] GetShiftTab();
	public int[][] GetReduceTab();
	public int[][] GetJumpTab();

	public int[][] GetTermIndexMap();
	public int[][] GetNontermIndexMap();
	public int[][] GetSemanticIndexMap();

	public int[] GetNumRhsSymbols();
	public int[] GetLhsIndices();

	public int[][] GetPartialsRuleTerm();
	public int[][] GetPartialsRuleNonterm();
	public int[][] GetPartialsMatchLengthTerm();
	public int[][] GetPartialsMatchLengthNonterm();
	public int[][] GetPartialsLhsIdNonterm();

	public int[][] GetPrecedences();
	public int[][] GetAssociativities();
}
