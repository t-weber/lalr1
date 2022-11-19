/**
 * generates a recursive ascent parser from parsing tables
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 25-oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 * 	- https://doi.org/10.1016/0020-0190(88)90061-0
 * 	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 * 	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

package lalr1_java;

import java.util.HashMap;
import java.util.Vector;
import java.io.FileWriter;
import java.io.BufferedWriter;
import java.io.PrintWriter;
import java.io.IOException;


public class ParserGen
{
	// parsing tables
	protected ParsingTableInterface m_tables;

	// table index and id maps
	protected HashMap<Integer, Integer> m_map_term_id;
	protected HashMap<Integer, Integer> m_map_nonterm_id;
	protected HashMap<Integer, Integer> m_map_semantic_id;

	protected int m_err_token = -1;
	protected int m_acc_token = -2;
	protected int m_end_token = -1;
	protected int m_start_idx = 0;


	public ParserGen(ParsingTableInterface tables)
	{
		m_tables = tables;

		// create terminal map from index to id
		m_map_term_id = new HashMap<Integer, Integer>();
		int[][] term_idx = m_tables.GetTermIndexMap();
		for(int i=0; i<term_idx.length; ++i)
			m_map_term_id.put(term_idx[i][1], term_idx[i][0]);

		// create nonterminal map from index to id
		m_map_nonterm_id = new HashMap<Integer, Integer>();
		int[][] nonterm_idx = m_tables.GetNontermIndexMap();
		for(int i=0; i<nonterm_idx.length; ++i)
			m_map_nonterm_id.put(nonterm_idx[i][1], nonterm_idx[i][0]);

		// create semantic rule map from index to id
		m_map_semantic_id = new HashMap<Integer, Integer>();
		int[][] semantic_idx = m_tables.GetSemanticIndexMap();
		for(int i=0; i<semantic_idx.length; ++i)
			m_map_semantic_id.put(semantic_idx[i][1], semantic_idx[i][0]);

		// get special token values
		m_err_token = m_tables.GetErrConst();
		m_acc_token = m_tables.GetAccConst();
		m_end_token = m_tables.GetEndConst();
		m_start_idx = m_tables.GetStartConst();
	}


	/**
	 * write an individual state function to the file
	 */
	protected void CreateState(PrintWriter pw, int state_idx)
	{
		int[] shift = m_tables.GetShiftTab()[state_idx];
		int[] reduce = m_tables.GetReduceTab()[state_idx];
		int[] jump = m_tables.GetJumpTab()[state_idx];

		int num_terms = shift.length;
		int num_nonterms = jump.length;

		boolean has_shift_entry = HasTableEntry(shift);
		boolean has_jump_entry = HasTableEntry(jump);

		pw.print("\tprotected void State" + state_idx + "()\n\t{\n");

		if(has_shift_entry)
			pw.print("\t\tRunnable next_state = null;\n");

		pw.print("\t\tswitch(m_lookahead.GetId())\n\t\t{\n");

		// map of rules and their terminal ids
		HashMap<Integer, Vector<Integer>> rules_term_id = new HashMap<Integer, Vector<Integer>>();
		// terminal ids for accepting
		Vector<Integer> acc_term_id = new Vector<Integer>();

		for(int term_idx=0; term_idx<num_terms; ++term_idx)
		{
			int term_id = GetTermTableId(term_idx);
			int newstate_idx = shift[term_idx];
			int rule_idx = reduce[term_idx];

			if(newstate_idx != m_err_token)
			{
				pw.print("\t\t\tcase " + term_id + ":" + GetCharComment(term_id) + "\n");
				pw.print("\t\t\t\tnext_state = this::State" + newstate_idx + ";\n");
				pw.print("\t\t\t\tbreak;\n");
			}
			else if(rule_idx != m_err_token)
			{
				if(rule_idx == m_acc_token)
				{
					acc_term_id.add(term_id);
				}
				else
				{
					if(!rules_term_id.containsKey(rule_idx))
						rules_term_id.put(rule_idx, new Vector<Integer>());
					rules_term_id.get(rule_idx).add(term_id);
				}
			}
		}

		// create cases for rule application
		for(Integer rule_idx : rules_term_id.keySet())
		{
			Vector<Integer> term_ids = rules_term_id.get(rule_idx);
			int num_rhs = m_tables.GetNumRhsSymbols()[rule_idx];
			int lhs_id = GetNontermTableId(m_tables.GetLhsIndices()[rule_idx]);
			int rule_id = GetSemanticTableId(rule_idx);

			boolean created_cases = false;
			for(Integer term_id : term_ids)
			{
				pw.print("\t\t\tcase " + term_id + ":" + GetCharComment(term_id) + "\n");
				created_cases = true;
			}

			if(created_cases)
			{
				pw.print("\t\t\t\tApplyRule(" + rule_id + ", " + num_rhs + ", " + lhs_id + ");\n");
				pw.print("\t\t\t\tbreak;\n");
			}
		}

		// create accepting case
		if(acc_term_id.size() > 0)
		{
			for(Integer term_id : acc_term_id)
				pw.print("\t\t\tcase " + term_id + ":" + GetCharComment(term_id) + "\n");

			pw.print("\t\t\t\tm_accepted = true;\n");
			pw.print("\t\t\t\tbreak;\n");
		}

		// default to error
		pw.print("\t\t\tdefault:\n");
		pw.print("\t\t\t\tthrow new RuntimeException(\"Invalid terminal transition \" + m_lookahead.GetId() + \" from state " + state_idx + ".\");\n");
		//pw.print("\t\t\t\tbreak;\n");

		pw.print("\t\t}\n");  // end switch

		if(has_shift_entry)
		{
			pw.print("\t\tif(next_state != null)\n\t\t{\n");
			pw.print("\t\t\tPushLookahead();\n");
			pw.print("\t\t\tnext_state.run();\n");
			pw.print("\t\t}\n");  // end if
		}

		if(has_jump_entry)
		{
			pw.print("\t\twhile(m_dist_to_jump == 0 && m_symbols.size() > 0 && !m_accepted)\n\t\t{\n");
			pw.print("\t\t\tSymbol<t_lval> topsym = m_symbols.peek();\n");
			pw.print("\t\t\tif(topsym.IsTerm())\n\t\t\t\tbreak;\n");
			pw.print("\t\t\tswitch(topsym.GetId())\n\t\t\t{\n");

			for(int nonterm_idx = 0; nonterm_idx < num_nonterms; ++nonterm_idx)
			{
				int jump_state_idx = jump[nonterm_idx];
				if(jump_state_idx != m_err_token)
				{
					int nonterm_id = GetNontermTableId(nonterm_idx);
					pw.print("\t\t\t\tcase " + nonterm_id + ":\n");
					pw.print("\t\t\t\t\tState" + jump_state_idx + "();\n");
					pw.print("\t\t\t\t\tbreak;\n");
				}
			}

			// default to error
			pw.print("\t\t\t\tdefault:\n");
			pw.print("\t\t\t\t\tthrow new RuntimeException(\"Invalid nonterminal transition \" + topsym.GetId() + \" from state " + state_idx + ".\");\n");
			//pw.print("\t\t\t\t\tbreak;\n");

			pw.print("\t\t\t}\n");  // end switch
			pw.print("\t\t}\n");  // end while
		}

		pw.print("\t\t--m_dist_to_jump;\n");
		pw.print("\t}\n\n");  // end state function
	}


	/**
	 * write the individual state functions to the file
	 */
	protected boolean CreateStates(PrintWriter pw)
	{
		int num_states = m_tables.GetShiftTab().length;
		if(num_states == 0)
			return false;

		// state function
		for(int state_idx=0; state_idx<num_states; ++state_idx)
			CreateState(pw, state_idx);

		return true;
	}


	/**
	 * write the parser class to the file
	 */
	public boolean CreateParser(String parser_name)
	{
		boolean ok = true;

		try
		{
			FileWriter fw = new FileWriter(parser_name + ".java", false);
			BufferedWriter bw = new BufferedWriter(fw);
			PrintWriter pw = new PrintWriter(bw);

			pw.print("/**\n");
			pw.print(" * Parser created using liblalr1 by Tobias Weber, 2020-2022.\n");
			pw.print(" * DOI: https://doi.org/10.5281/zenodo.6987396\n");
			pw.print(" */\n\n");

			pw.print("import lalr1_java.Symbol;\n");
			pw.print("import lalr1_java.ParserInterface;\n");
			pw.print("import lalr1_java.SemanticRuleInterface;\n");

			pw.print("import java.util.Vector;\n");
			pw.print("import java.util.Stack;\n");
			pw.print("import java.util.HashMap;\n\n");

			pw.print("public class " + parser_name + "<t_lval> implements ParserInterface<t_lval>\n{\n");

			// member variables
			pw.print("\tprotected Vector<Symbol<t_lval>> m_input;\n");
			pw.print("\tprotected Stack<Symbol<t_lval>> m_symbols;\n");
			pw.print("\tprotected HashMap<Integer, SemanticRuleInterface<t_lval>> m_semantics;\n");
			pw.print("\tprotected int m_input_index;\n");
			pw.print("\tprotected Symbol<t_lval> m_lookahead;\n");
			pw.print("\tprotected int m_dist_to_jump;\n");
			pw.print("\tprotected boolean m_accepted;\n\n");

			// NextLookahead()
			pw.print("\tprotected void NextLookahead()\n\t{\n");
			pw.print("\t\t++m_input_index;\n");
			pw.print("\t\tm_lookahead = m_input.elementAt(m_input_index);\n");
			pw.print("\t}\n\n");  // end NextLookahead()

			// PushLookahead()
			pw.print("\tprotected void PushLookahead()\n\t{\n");
			pw.print("\t\tm_symbols.push(m_lookahead);\n");
			pw.print("\t\tNextLookahead();\n");
			pw.print("\t}\n\n");  // end PushLookahead()

			// ApplyRule()
			pw.print("\tprotected void ApplyRule(int rule_id, int num_rhs, int lhs_id)\n\t{\n");
			pw.print("\t\tm_dist_to_jump = num_rhs;\n");
			pw.print("\t\tVector<Symbol<t_lval>> args = new Vector<Symbol<t_lval>>();\n");
			pw.print("\t\tfor(int i=0; i<num_rhs; ++i)\n");
			pw.print("\t\t\targs.add(m_symbols.elementAt(m_symbols.size() - num_rhs + i));\n");
			pw.print("\t\tfor(int i=0; i<num_rhs; ++i)\n");
			pw.print("\t\t\tm_symbols.pop();\n");
			pw.print("\t\tt_lval retval = null;\n");
			pw.print("\t\tif(m_semantics != null && m_semantics.containsKey(rule_id))\n");
			pw.print("\t\t\tretval = m_semantics.get(rule_id).call(args);\n");
			pw.print("\t\telse\n");
			pw.print("\t\t\tSystem.err.println(\"Semantic rule \" + rule_id + \" is not defined.\");\n");
			pw.print("\t\tm_symbols.push(new Symbol<t_lval>(false, lhs_id, retval));\n");
			pw.print("\t}\n\n");  // end ApplyRule()

			// constructor
			pw.print("\tpublic " + parser_name + "()\n\t{\n");
			pw.print("\t\tm_input = null;\n");
			pw.print("\t\tm_semantics = null;\n");
			pw.print("\t\tm_symbols = new Stack<Symbol<t_lval>>();\n");
			pw.print("\t\tReset();\n");
			pw.print("\t}\n\n");  // end constructor

			// Reset()
			pw.print("\t@Override\n");
			pw.print("\tpublic void Reset()\n\t{\n");
			pw.print("\t\tm_symbols.clear();\n");
			pw.print("\t\tm_input_index = -1;\n");
			pw.print("\t\tm_lookahead = null;\n");
			pw.print("\t\tm_dist_to_jump = 0;\n");
			pw.print("\t\tm_accepted = false;\n");
			pw.print("\t}\n\n");  // end Reset()

			// SetInput()
			pw.print("\t@Override\n");
			pw.print("\tpublic void SetInput(Vector<Symbol<t_lval>> input)\n\t{\n");
			pw.print("\t\tm_input = input;\n");
			pw.print("\t}\n\n");  // end SetInput()

			// SetSemantics()
			pw.print("\t@Override\n");
			pw.print("\tpublic void SetSemantics(HashMap<Integer, SemanticRuleInterface<t_lval>> semantics)\n\t{\n");
			pw.print("\t\tm_semantics = semantics;\n");
			pw.print("\t}\n\n");  // end SetSemantics()

			// GetAccepted()
			pw.print("\t@Override\n");
			pw.print("\tpublic boolean GetAccepted()\n\t{\n");
			pw.print("\t\treturn m_accepted;\n");
			pw.print("\t}\n\n");  // end GetAccepted()

			// GetEndConst()
			pw.print("\t@Override\n");
			pw.print("\tpublic int GetEndConst()\n\t{\n");
			pw.print("\t\treturn " + m_end_token + ";\n");
			pw.print("\t}\n\n");  // end GetEndConst()

			// Parse()
			pw.print("\t@Override\n");
			pw.print("\tpublic Symbol<t_lval> Parse()\n\t{\n");
			pw.print("\t\tReset();\n");
			pw.print("\t\tNextLookahead();\n");
			pw.print("\t\tState" + m_start_idx + "();\n");
			pw.print("\t\tif(m_symbols.size() >= 1)\n");
			pw.print("\t\t\treturn m_symbols.peek();\n");
			pw.print("\t\treturn null;\n");
			pw.print("\t}\n\n");  // end Parse()

			ok = CreateStates(pw);

			pw.print("}\n");  // end class

			bw.flush();
			fw.close();
		}
		catch(IOException ex)
		{
			System.err.println(ex.toString());
			return false;
		}

		return ok;
	}


	/**
	 * get a comment string for a char representation of the given token
	 */
	protected String GetCharComment(Integer tok)
	{
		String comment_str = "";

		if(tok >= 0 && tok < 256)
		{
			char ch = (char) tok.intValue();
			if(!Character.isISOControl(ch))
				comment_str = " // '" + ch + "'";
		}

		return comment_str;
	}


	/**
	 * get the table id from its index
	 */
	protected Integer GetTableId(HashMap<Integer, Integer> map, int idx)
	{
		return map.get(idx);
	}


	/**
	 * get the semantic table id from its index
	 */
	protected int GetSemanticTableId(int idx)
	{
		Integer id = GetTableId(m_map_semantic_id, idx);

		if(id == null)
			throw new RuntimeException("Invalid semantic index: " + idx + ".");
		return id;
	}


	/**
	 * get the nonterminal table id from its index
	 */
	protected int GetNontermTableId(int idx)
	{
		Integer id = GetTableId(m_map_nonterm_id, idx);

		if(id == null)
			throw new RuntimeException("Invalid nonterminal index: " + idx + ".");
		return id;
	}


	/**
	 * get the terminal table id from its index
	 */
	protected int GetTermTableId(int idx)
	{
		Integer id = GetTableId(m_map_term_id, idx);

		if(id == null)
			throw new RuntimeException("Invalid terminal index: " + idx + ".");
		return id;
	}


	/**
	 * check if the given table row has at least one non-error entry
	 */
	protected boolean HasTableEntry(int[] tab)
	{
		for(int entry : tab)
		{
			if(entry != m_err_token)
				return true;
		}

		return false;
	}


	public static void main(String[] args)
	{
		if(args.length == 0)
		{
			System.err.println("Please give a parsing table class name.");
			return;
		}

		try
		{
			String class_name = args[0];
			String parser_name = class_name.replace("Tab", "") + "Parser";
			System.out.println("Creating parser \"" + class_name + ".class\" -> \"" + parser_name + ".java\".");

			// create parsing tables
			Class<?> tab_class = Class.forName(class_name);
			ParsingTableInterface tab = (ParsingTableInterface)
				tab_class.getConstructor().newInstance();

			// create parser generator
			ParserGen parsergen = new ParserGen(tab);
			parsergen.CreateParser(parser_name);
		}
		catch(Exception ex)
		{
			System.err.println(ex.toString());
			return;
		}
	}
}
