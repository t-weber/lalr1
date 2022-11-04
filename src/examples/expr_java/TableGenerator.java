/**
 * expression parser table generation
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 29-oct-2022
 * @license see 'LICENSE' file
 */

import java.util.Vector;
import java.io.File;

public class TableGenerator
{
	public static void main(String[] args)
	{
		// possible library names
		Vector<File> lalr1_libs = new Vector<File>();
		lalr1_libs.add(new File("liblalr1_java.jnilib"));
		lalr1_libs.add(new File("liblalr1_java.so"));

		for(File lalr1_lib : lalr1_libs)
		{
			try
			{
				File lalr1_abslib = lalr1_lib.getAbsoluteFile();
				System.load(lalr1_abslib.toString());
			}
			catch(UnsatisfiedLinkError ex)
			{
				// try next library
				continue;
			}

			// library loaded
			break;
		}

		// non-terminals
		NonTerminal start = lalr1.make_nonterminal(Ids.nonterm_start, "start");
		NonTerminal expr = lalr1.make_nonterminal(Ids.nonterm_expr, "expr");

		// terminals
		Terminal op_plus = lalr1.make_terminal('+', "+");
		Terminal op_minus = lalr1.make_terminal('-', "-");
		Terminal op_mul = lalr1.make_terminal('*', "*");
		Terminal op_div = lalr1.make_terminal('/', "/");
		Terminal op_mod = lalr1.make_terminal('%', "%");
		Terminal op_pow = lalr1.make_terminal('^', "^");
		Terminal bracket_open = lalr1.make_terminal('(', "(");
		Terminal bracket_close = lalr1.make_terminal(')', ")");
		Terminal comma = lalr1.make_terminal(',', ",");
		Terminal sym_real = lalr1.make_terminal(Ids.tok_real_id, "real");
		Terminal sym_int = lalr1.make_terminal(Ids.tok_int_id, "integer");
		Terminal ident = lalr1.make_terminal(Ids.tok_ident_id, "ident");

		// precedences and associativities
		op_plus.SetPrecedence(70, 'l');
		op_minus.SetPrecedence(70, 'l');
		op_mul.SetPrecedence(80, 'l');
		op_div.SetPrecedence(80, 'l');
		op_mod.SetPrecedence(80, 'l');
		op_pow.SetPrecedence(110, 'r');

		// rules
		SymbolVec start_prod = new SymbolVec();
		start_prod.add(expr);
		start.AddARule(lalr1.make_word(start_prod), Ids.sem_start_id);

		// unary plus
		SymbolVec expr_uadd = new SymbolVec();
		expr_uadd.add(op_plus); expr_uadd.add(expr);
		expr.AddARule(lalr1.make_word(expr_uadd), Ids.sem_uadd_id);

		// unary minus
		SymbolVec expr_usub = new SymbolVec();
		expr_usub.add(op_minus); expr_usub.add(expr);
		expr.AddARule(lalr1.make_word(expr_usub), Ids.sem_usub_id);

		// addition
		SymbolVec expr_add = new SymbolVec();
		expr_add.add(expr); expr_add.add(op_plus); expr_add.add(expr);
		expr.AddARule(lalr1.make_word(expr_add), Ids.sem_add_id);

		// subtraction
		SymbolVec expr_sub = new SymbolVec();
		expr_sub.add(expr); expr_sub.add(op_minus); expr_sub.add(expr);
		expr.AddARule(lalr1.make_word(expr_sub), Ids.sem_sub_id);

		// multiplication
		SymbolVec expr_mul = new SymbolVec();
		expr_mul.add(expr); expr_mul.add(op_mul); expr_mul.add(expr);
		expr.AddARule(lalr1.make_word(expr_mul), Ids.sem_mul_id);

		// division
		SymbolVec expr_div = new SymbolVec();
		expr_div.add(expr); expr_div.add(op_div); expr_div.add(expr);
		expr.AddARule(lalr1.make_word(expr_div), Ids.sem_div_id);

		// modulo
		SymbolVec expr_mod = new SymbolVec();
		expr_mod.add(expr); expr_mod.add(op_mod); expr_mod.add(expr);
		expr.AddARule(lalr1.make_word(expr_mod), Ids.sem_mod_id);

		// power
		SymbolVec expr_pow = new SymbolVec();
		expr_pow.add(expr); expr_pow.add(op_pow); expr_pow.add(expr);
		expr.AddARule(lalr1.make_word(expr_pow), Ids.sem_pow_id);

		// brackets
		SymbolVec expr_bracket = new SymbolVec();
		expr_bracket.add(bracket_open); expr_bracket.add(expr); expr_bracket.add(bracket_close);
		expr.AddARule(lalr1.make_word(expr_bracket), Ids.sem_brackets_id);

		// function call with no arguments
		SymbolVec expr_call0 = new SymbolVec();
		expr_call0.add(ident); expr_call0.add(bracket_open); expr_call0.add(bracket_close);
		expr.AddARule(lalr1.make_word(expr_call0), Ids.sem_call0_id);

		// function call with one argument
		SymbolVec expr_call1 = new SymbolVec();
		expr_call1.add(ident); expr_call1.add(bracket_open);
		expr_call1.add(expr); expr_call1.add(bracket_close);
		expr.AddARule(lalr1.make_word(expr_call1), Ids.sem_call1_id);

		// function call with two arguments
		SymbolVec expr_call2 = new SymbolVec();
		expr_call2.add(ident); expr_call2.add(bracket_open);
		expr_call2.add(expr); expr_call2.add(comma);
		expr_call2.add(expr); expr_call2.add(bracket_close);
		expr.AddARule(lalr1.make_word(expr_call2), Ids.sem_call2_id);

		// real constants
		SymbolVec expr_real = new SymbolVec();
		expr_real.add(sym_real);
		expr.AddARule(lalr1.make_word(expr_real), Ids.sem_real_id);

		// integer constants
		SymbolVec expr_int = new SymbolVec();
		expr_int.add(sym_int);
		expr.AddARule(lalr1.make_word(expr_int), Ids.sem_int_id);

		// variable identifier
		SymbolVec expr_ident = new SymbolVec();
		expr_ident.add(ident);
		expr.AddARule(lalr1.make_word(expr_ident), Ids.sem_ident_id);

		Collection coll = lalr1.make_collection(start);
		System.out.println("Calculating LALR(1) collection.");
		coll.DoTransitions();

		String tables_name = "ExprTab.java";
		TableGen tables = lalr1.make_tablegen(coll, start);
		if(tables.CreateParseTables())
		{
			tables.SaveParseTablesJava(tables_name);
			System.out.println("Created parsing tables \""
				+ tables_name + "\".");
		}
		else
		{
			System.err.println("Could not create parsing tables \""
				+ tables_name + "\".");
		}
	}
}
