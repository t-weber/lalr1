/**
 * active partial rule
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 1-january-2023
 * @license see 'LICENSE' file
 */

package lalr1_java;


public class ActiveRule<t_lval>
{
	public ActiveRule()
	{
		seen_tokens = 0;
		handle = -1;
		retval = null;
	}

	public int seen_tokens;
	public int handle;
	public t_lval retval;
}
