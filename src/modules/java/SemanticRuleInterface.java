
/**
 * semantic rule function
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 23-oct-2022
 * @license see 'LICENSE' file
 */

import java.util.Vector;


public interface SemanticRuleInterface<t_lval>
{
	t_lval call(Vector<Symbol<t_lval>> args);
}


interface Func0Args<t_lval>
{
	t_lval call();
}


interface Func1Arg<t_lval>
{
	t_lval call(t_lval arg);
}


interface Func2Args<t_lval>
{
	t_lval call(t_lval arg1, t_lval arg2);
}
