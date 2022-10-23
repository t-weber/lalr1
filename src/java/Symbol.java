/**
 * LALR(1) parser symbols
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 23-oct-2022
 * @license see 'LICENSE' file
 */

public class Symbol<t_lval>
{
	protected boolean m_is_term;  // symbol is a terminal
	protected int m_id;           // symbol identifier
	protected t_lval m_val;       // symbol lvalue

	public Symbol(boolean is_term, int id, t_lval val)
	{
		m_is_term = is_term;
		m_id = id;
		m_val = val;
	}

	public boolean IsTerm()
	{
		return m_is_term;
	}

	public int GetId()
	{
		return m_id;
	}

	public t_lval GetVal()
	{
		return m_val;
	}
}
