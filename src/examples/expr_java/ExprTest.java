/**
 * expression test
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24-oct-2022
 * @license see 'LICENSE' file
 */

// javac -classpath "../../modules/java:." ../../modules/java/*.java ExprTest.java ExprTab.java Ids.java
// java -classpath "../../modules/java:." ExprTest

import java.util.Vector;
import java.util.HashMap;


public class ExprTest
{
	public static void main(String[] prog_args)
	{
		// semantic rules
		SemanticRuleInterface<Integer> sem_start = (args) -> {
			return args.elementAt(0).GetVal(); };
		SemanticRuleInterface<Integer> sem_brackets = (args) -> {
			return args.elementAt(1).GetVal(); };
		SemanticRuleInterface<Integer> sem_add = (args) -> {
			return args.elementAt(0).GetVal() + args.elementAt(2).GetVal(); };
		SemanticRuleInterface<Integer> sem_sub = (args) -> {
			return args.elementAt(0).GetVal() - args.elementAt(2).GetVal(); };
		SemanticRuleInterface<Integer> sem_mul = (args) -> {
			return args.elementAt(0).GetVal() * args.elementAt(2).GetVal(); };
		SemanticRuleInterface<Integer> sem_div = (args) -> {
			return args.elementAt(0).GetVal() / args.elementAt(2).GetVal(); };
		SemanticRuleInterface<Integer> sem_mod = (args) -> {
			return args.elementAt(0).GetVal() % args.elementAt(2).GetVal(); };
		SemanticRuleInterface<Integer> sem_uadd = (args) -> {
			return +args.elementAt(1).GetVal(); };
		SemanticRuleInterface<Integer> sem_usub = (args) -> {
			return -args.elementAt(1).GetVal(); };
		SemanticRuleInterface<Integer> sem_real = (args) -> {
			return args.elementAt(0).GetVal(); };
		SemanticRuleInterface<Integer> sem_int = (args) -> {
			return args.elementAt(0).GetVal(); };

		HashMap<Integer, SemanticRuleInterface<Integer>> rules =
			new HashMap<Integer, SemanticRuleInterface<Integer>>();
		rules.put(Ids.sem_start_id, sem_start);
		rules.put(Ids.sem_brackets_id, sem_brackets);
		rules.put(Ids.sem_add_id, sem_add);
		rules.put(Ids.sem_sub_id, sem_sub);
		rules.put(Ids.sem_mul_id, sem_mul);
		rules.put(Ids.sem_div_id, sem_div);
		rules.put(Ids.sem_mod_id, sem_mod);
		rules.put(Ids.sem_uadd_id, sem_uadd);
		rules.put(Ids.sem_usub_id, sem_usub);
		rules.put(Ids.sem_real_id, sem_real);
		rules.put(Ids.sem_int_id, sem_int);

		// parsing tables
		ParsingTables tab = new ExprTab();

		// test input
		Vector<Symbol<Integer>> input = new Vector<Symbol<Integer>>();
		input.add(new Symbol<Integer>(true, Ids.tok_int_id, Integer.valueOf(1)));
		input.add(new Symbol<Integer>(true, (int)'+', null));
		input.add(new Symbol<Integer>(true, Ids.tok_int_id, Integer.valueOf(2)));
		input.add(new Symbol<Integer>(true, (int)'*', null));
		input.add(new Symbol<Integer>(true, Ids.tok_int_id, Integer.valueOf(3)));
		input.add(new Symbol<Integer>(true, tab.GetEndConst(), null));

		// parser
		Parser<Integer> parser = new Parser<Integer>(tab);
		parser.SetSemantics(rules);
		parser.SetInput(input);

		// run parser
		Symbol result = parser.Parse();
		if(result != null)
			System.out.println(result.GetVal());
	}
}
