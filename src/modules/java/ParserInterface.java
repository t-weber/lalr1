/**
 * LALR(1) parser interface
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 23-oct-2022
 * @license see 'LICENSE' file
 */

import java.util.Vector;
import java.util.Stack;
import java.util.HashMap;


public interface ParserInterface<t_lval>
{
	/**
	 * reset the parser
	 */
	public void Reset();

	/**
	 * set the input tokens
	 */
	public void SetInput(Vector<Symbol<t_lval>> input);

	/**
	 * set the semantic rule functions
	 */
	public void SetSemantics(HashMap<Integer, SemanticRuleInterface<t_lval>> semantics);

	/**
	 * get the token id representing the end of the input
	 */
	public int GetEndConst();

	/**
	 * has the input been accepted?
	 */
	public boolean GetAccepted();

	/**
	 * parse the input
	 */
	public Symbol<t_lval> Parse();
}
