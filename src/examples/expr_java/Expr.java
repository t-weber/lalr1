/**
 * expression parser
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24-oct-2022
 * @license see 'LICENSE' file
 */

// table-based version:
// javac -classpath "../../modules:." Expr.java Lexer.java Ids.java ExprTab.java -Xlint:unchecked
// java -classpath "../../modules:." Expr

// recursive-ascent version
// java -classpath "../../modules:." lalr1_java.ParserGen ExprTab
// javac -classpath "../../modules:." Expr.java Lexer.java Ids.java ExprParser.java -Xlint:unchecked
// java -classpath "../../modules:." Expr


import java.util.Vector;
import java.util.HashMap;
import java.util.Random;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.IOException;

import lalr1_java.*;


public class Expr
{
	public void run()
	{
		// semantic rules
		SemanticRuleInterface<Double> sem_start = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(0).GetVal();
		};
		SemanticRuleInterface<Double> sem_brackets = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(1).GetVal();
		};

		// arithmetics
		SemanticRuleInterface<Double> sem_add = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(0).GetVal() + args.elementAt(2).GetVal();
		};
		SemanticRuleInterface<Double> sem_sub = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(0).GetVal() - args.elementAt(2).GetVal();
		};
		SemanticRuleInterface<Double> sem_mul = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(0).GetVal() * args.elementAt(2).GetVal();
		};
		SemanticRuleInterface<Double> sem_div = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(0).GetVal() / args.elementAt(2).GetVal();
		};
		SemanticRuleInterface<Double> sem_mod = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(0).GetVal() % args.elementAt(2).GetVal();
		};
		SemanticRuleInterface<Double> sem_pow = (args, done, retval) ->
		{
			if(!done) return null;
			return Math.pow(
				args.elementAt(0).GetVal(),
				args.elementAt(2).GetVal());
		};
		SemanticRuleInterface<Double> sem_uadd = (args, done, retval) ->
		{
			if(!done) return null;
			return +args.elementAt(1).GetVal();
		};
		SemanticRuleInterface<Double> sem_usub = (args, done, retval) ->
		{
			if(!done) return null;
			return -args.elementAt(1).GetVal();
		};

		// numerical constants
		SemanticRuleInterface<Double> sem_real = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(0).GetVal();
		};
		SemanticRuleInterface<Double> sem_int = (args, done, retval) ->
		{
			if(!done) return null;
			return args.elementAt(0).GetVal();
		};

		// variables
		HashMap<String, Double> vars = new HashMap<String, Double>();
		vars.put("pi", Math.PI);
		SemanticRuleInterface<Double> sem_ident = (args, done, retval) ->
		{
			if(!done) return null;
			return vars.get(args.elementAt(0).GetStrVal());
		};

		// 0-argument functions
		HashMap<String, Func0Args<Double>> funcs0
			= new HashMap<String, Func0Args<Double>>();
		Random rnd = new Random();
		funcs0.put("rand", () ->
		{
			return rnd.nextDouble();
		});
		SemanticRuleInterface<Double> sem_func0 = (args, done, retval) ->
		{
			if(!done) return null;
			return funcs0.get(args.elementAt(0).GetStrVal()).call();
		};

		// 1-argument functions
		HashMap<String, Func1Arg<Double>> funcs1
			= new HashMap<String, Func1Arg<Double>>();
		funcs1.put("sin", (arg) -> Math.sin(arg));
		funcs1.put("cos", (arg) -> Math.cos(arg));
		funcs1.put("tan", (arg) -> Math.tan(arg));
		funcs1.put("sqrt", (arg) -> Math.sqrt(arg));
		SemanticRuleInterface<Double> sem_func1 = (args, done, retval) ->
		{
			if(!done) return null;
			return funcs1.get(args.elementAt(0).GetStrVal()).call(
				args.elementAt(2).GetVal());
		};

		// 2-argument functions
		HashMap<String, Func2Args<Double>> funcs2
			= new HashMap<String, Func2Args<Double>>();
		funcs2.put("pow", (arg1, arg2) -> Math.pow(arg1, arg2));
		SemanticRuleInterface<Double> sem_func2 = (args, done, retval) ->
		{
			if(!done) return null;
			return funcs2.get(args.elementAt(0).GetStrVal()).call(
				args.elementAt(2).GetVal(),
				args.elementAt(4).GetVal());
		};

		HashMap<Integer, SemanticRuleInterface<Double>> rules =
			new HashMap<Integer, SemanticRuleInterface<Double>>();
		rules.put(Ids.sem_start_id, sem_start);
		rules.put(Ids.sem_brackets_id, sem_brackets);
		rules.put(Ids.sem_add_id, sem_add);
		rules.put(Ids.sem_sub_id, sem_sub);
		rules.put(Ids.sem_mul_id, sem_mul);
		rules.put(Ids.sem_div_id, sem_div);
		rules.put(Ids.sem_mod_id, sem_mod);
		rules.put(Ids.sem_pow_id, sem_pow);
		rules.put(Ids.sem_uadd_id, sem_uadd);
		rules.put(Ids.sem_usub_id, sem_usub);
		rules.put(Ids.sem_real_id, sem_real);
		rules.put(Ids.sem_int_id, sem_int);
		rules.put(Ids.sem_ident_id, sem_ident);
		rules.put(Ids.sem_call0_id, sem_func0);
		rules.put(Ids.sem_call1_id, sem_func1);
		rules.put(Ids.sem_call2_id, sem_func2);

		// parser (select either table-based or recursive-ascent version)
		ParserInterface<Double> parser = new Parser<Double>(new ExprTab());
		//ParserInterface<Double> parser = new ExprParser<Double>();
		parser.SetSemantics(rules);
		parser.SetDebug(false);
		parser.SetPartials(false);

		// lexer
		InputStreamReader isr = new InputStreamReader(System.in); // byte -> char
		BufferedReader br = new BufferedReader(isr);
		Lexer<Double> lexer = new Lexer<Double>();

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

			Vector<Symbol<Double>> syms = lexer.GetTokens(line);
			syms.add(new Symbol<Double>(true, parser.GetEndConst(), null));

			/*for(int i=0; i<syms.size(); ++i)
			{
				Symbol<Double> sym = syms.elementAt(i);
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


	public static void main(String[] prog_args)
	{
		Expr expr = new Expr();
		expr.run();
	}
}
