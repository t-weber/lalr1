/**
 * expression parser
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24-oct-2022
 * @license see 'LICENSE' file
 */

// javac -classpath "../../java/:." Expr.java Lexer.java Ids.java ../../java/*.java -Xlint:unchecked
// java -classpath "../../java/:." Expr


import java.util.Vector;
import java.util.HashMap;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.IOException;


public class Expr
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

		// parser
		ParserInterface<Integer> parser = new Parser<Integer>(new ExprTab());
		//ParserInterface<Integer> parser = new ExprParser<Integer>();
		parser.SetSemantics(rules);

		// lexer
		InputStreamReader isr = new InputStreamReader(System.in); // byte -> char
		BufferedReader br = new BufferedReader(isr);
		Lexer<Integer> lexer = new Lexer<Integer>();

		int cmd_nr = 1;
		while(true)
		{
			// read line
			String line;
			System.out.print("[in " + (cmd_nr++) + "] ");
			try
			{
				line = br.readLine();
				line = line.trim();

				// remove any possibly remaining input
				while(br.ready())
					br.skip(1);
			}
			catch(IOException ex)
			{
				System.err.println(ex.toString());
				continue;
			}

			if(line.length() == 0)
				continue;
			if(line.equals("quit") || line.equals("exit"))
				break;

			Vector<Symbol<Integer>> syms = lexer.GetTokens(line);
			syms.add(new Symbol<Integer>(true, parser.GetEndConst(), null));

			/*for(int i=0; i<syms.size(); ++i)
			{
				Symbol<Integer> sym = syms.elementAt(i);
				System.out.println(sym.GetStrVal());
			}*/

			parser.SetInput(syms);

			// run parser
			Symbol result = null;
			try
			{
				result = parser.Parse();
			}
			catch(RuntimeException ex)
			{
				System.err.println(ex.toString());
			}

			if(result != null)
			{
				String res_str = "[out " + (cmd_nr-1) + "] "
					+ result.GetVal();
				System.out.println(res_str);
			}
			else
			{
				System.err.println("Parsing error.");
			}
		}
	}
}
