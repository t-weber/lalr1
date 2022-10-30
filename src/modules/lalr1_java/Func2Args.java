
/**
 * semantic rule function with two arguemnts
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 23-oct-2022
 * @license see 'LICENSE' file
 */

package lalr1_java;


public interface Func2Args<t_lval>
{
	t_lval call(t_lval arg1, t_lval arg2);
}
