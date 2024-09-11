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
	protected HashMap<Integer, Integer> m_map_nonterm_id, m_map_nonterm_idx;
	protected HashMap<Integer, Integer> m_map_semantic_id;

	protected int m_err_token = -1;
	protected int m_acc_token = -2;
	protected int m_end_token = -1;
	protected int m_start_idx = 0;

	protected boolean m_gen_partials = true;


	public ParserGen(ParsingTableInterface tables)
	{
		m_tables = tables;

		// create terminal map from index to id
		m_map_term_id = new HashMap<Integer, Integer>();
		int[][] term_idx = m_tables.GetTermIndexMap();
		for(int i=0; i<term_idx.length; ++i)
			m_map_term_id.put(term_idx[i][1], term_idx[i][0]);

		// create nonterminal map from index to id (and vice-versa)
		m_map_nonterm_id = new HashMap<Integer, Integer>();
		m_map_nonterm_idx = new HashMap<Integer, Integer>();
		int[][] nonterm_idx = m_tables.GetNontermIndexMap();
		for(int i=0; i<nonterm_idx.length; ++i)
		{
			m_map_nonterm_id.put(nonterm_idx[i][1], nonterm_idx[i][0]);
			m_map_nonterm_idx.put(nonterm_idx[i][0], nonterm_idx[i][1]);
		}

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


	public void SetGenPartials(boolean gen_partials)
	{
		m_gen_partials = gen_partials;
	}


	/**
	 * write an individual state function to the file
	 */
	protected void CreateState(PrintWriter pw, int state_idx)
	{
		// main parsing tables
		int[] shift = m_tables.GetShiftTab()[state_idx];
		int[] reduce = m_tables.GetReduceTab()[state_idx];
		int[] jump = m_tables.GetJumpTab()[state_idx];

		// partial rule tables
		final int[] part_term = m_tables.GetPartialsRuleTerm()[state_idx];
		final int[] part_nonterm = m_tables.GetPartialsRuleNonterm()[state_idx];
		final int[] part_term_len = m_tables.GetPartialsMatchLengthTerm()[state_idx];
		final int[] part_nonterm_len = m_tables.GetPartialsMatchLengthNonterm()[state_idx];
		final int[] part_nonterm_lhs = m_tables.GetPartialsLhsIdNonterm()[state_idx];

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

			// shift
			if(newstate_idx != m_err_token)
			{
				pw.print("\t\t\tcase " + term_id + ":" + GetCharComment(term_id) + "\n");
				pw.print("\t\t\t\tnext_state = this::State" + newstate_idx + ";\n");
				if(m_gen_partials)
				{
					int partial_idx = part_term[term_idx];
					if(partial_idx != m_err_token)
					{
						int partial_id = GetSemanticTableId(partial_idx);
						int partial_len = part_term_len[term_idx];

						pw.print("\t\t\t\tif(m_use_partials)\n");
						pw.print("\t\t\t\t\tApplyPartialRule(" + partial_id + ", " + partial_len + ", true);\n");
					}
				}
				pw.print("\t\t\t\tbreak;\n");
			}
			else if(rule_idx != m_err_token)
			{
				if(rule_idx == m_acc_token)
				{
					// accept
					acc_term_id.add(term_id);
				}
				else
				{
					// reduce
					if(!rules_term_id.containsKey(rule_idx))
						rules_term_id.put(rule_idx, new Vector<Integer>());
					rules_term_id.get(rule_idx).add(term_id);
				}
			}
		}

		// reduce; create cases for rule application
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

		// shift
		if(has_shift_entry)
		{
			pw.print("""
				if(next_state != null)
				{
					PushLookahead();
					next_state.run();
				}
		""");
		}

		// jump
		if(has_jump_entry)
		{
			pw.print("""
				while(m_dist_to_jump == 0 && m_symbols.size() > 0 && !m_accepted)
				{
					Symbol<t_lval> topsym = m_symbols.peek();
					if(topsym.IsTerm())
						break;
					switch(topsym.GetId())
					{
		""");


			for(int nonterm_idx = 0; nonterm_idx < num_nonterms; ++nonterm_idx)
			{
				int jump_state_idx = jump[nonterm_idx];
				if(jump_state_idx != m_err_token)
				{
					int nonterm_id = GetNontermTableId(nonterm_idx);
					pw.print("\t\t\t\tcase " + nonterm_id + ":\n");

					if(m_gen_partials)
					{
						int lhs_id = part_nonterm_lhs[nonterm_idx];
						if(lhs_id != m_err_token)
						{
							int lhs_idx = GetNontermTableIdx(lhs_id);
							int partial_idx = part_nonterm[lhs_idx];
							if(partial_idx != m_err_token)
							{
								int partial_id = GetSemanticTableId(partial_idx);
								int partial_len = part_nonterm_len[lhs_idx];

								pw.print("\t\t\t\tif(m_use_partials)\n");
								pw.print("\t\t\t\t\tApplyPartialRule(" + partial_id + ", " + partial_len + ", false);\n");
							}
						}
					}

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

			pw.print("""
			/**
			 * Parser created using liblalr1 by Tobias Weber, 2020-2024.
			 * DOI: https://doi.org/10.5281/zenodo.6987396
			 */

			import lalr1_java.Symbol;
			import lalr1_java.ParserInterface;
			import lalr1_java.SemanticRuleInterface;
			import lalr1_java.ActiveRule;

			import java.util.Vector;
			import java.util.Stack;
			import java.util.HashMap;

			""");

			pw.print("public class " + parser_name + "<t_lval> implements ParserInterface<t_lval>\n");

			pw.print("""
			{
				protected boolean m_debug;
				protected boolean m_use_partials;
				protected Vector<Symbol<t_lval>> m_input;
				protected Stack<Symbol<t_lval>> m_symbols;
				protected HashMap<Integer, SemanticRuleInterface<t_lval>> m_semantics;
				protected int m_input_index;
				protected Symbol<t_lval> m_lookahead;
				protected int m_dist_to_jump;
				protected boolean m_accepted;
			""");

			if(m_gen_partials)
			{
				pw.print("""
					protected HashMap<Integer, Stack<ActiveRule<t_lval>>> m_active_rules;
					protected int m_cur_rule_handle;
				""");
			}

			pw.print("""

				protected void NextLookahead()
				{
					++m_input_index;
					m_lookahead = m_input.elementAt(m_input_index);
				}

				protected void PushLookahead()
				{
					m_symbols.push(m_lookahead);
					NextLookahead();
				}

				@Override
				public void Reset()
				{
					m_symbols.clear();
					m_input_index = -1;
					m_lookahead = null;
					m_dist_to_jump = 0;
					m_accepted = false;
			""");
			if(m_gen_partials)
			{
				pw.print("""
					m_active_rules.clear();
					m_cur_rule_handle = 0;
			""");
			}
			pw.print("""
				}

				@Override
				public void SetDebug(boolean debug)
				{
					m_debug = debug;
				}

				@Override
				public void SetPartials(boolean use_partials)
				{
					m_use_partials = use_partials;
				}

				@Override
				public void SetInput(Vector<Symbol<t_lval>> input)
				{
					m_input = input;
				}

				@Override
				public void SetSemantics(HashMap<Integer, SemanticRuleInterface<t_lval>> semantics)
				{
					m_semantics = semantics;
				}

				@Override
				public boolean GetAccepted()
				{
					return m_accepted;
				}

				protected void ApplyRule(int rule_id, int num_rhs, int lhs_id)
				{
					t_lval retval = null;
					int handle = -1;
			""");
			if(m_gen_partials)
			{
				pw.print("""
					if(m_use_partials)
					{
						Stack<ActiveRule<t_lval>> rulestack = m_active_rules.get(rule_id);
						if(rulestack != null && !rulestack.empty())
						{
							ActiveRule<t_lval> active_rule = rulestack.peek();
							retval = active_rule.retval;
							handle = active_rule.handle;
							rulestack.pop();
						}
					}
			""");
			}
			pw.print("""
					if(m_debug)
					{
						System.out.print("Applying rule " + rule_id + " with " + num_rhs + " arguments");
						if(handle >= 0)
							System.out.print(" (handle " + handle + ")");
						System.out.println(".");
					}

					m_dist_to_jump = num_rhs;
					Vector<Symbol<t_lval>> args = new Vector<Symbol<t_lval>>();
					for(int i=0; i<num_rhs; ++i)
						args.add(m_symbols.elementAt(m_symbols.size() - num_rhs + i));
					for(int i=0; i<num_rhs; ++i)
						m_symbols.pop();
					if(m_semantics != null && m_semantics.containsKey(rule_id))
						retval = m_semantics.get(rule_id).call(args, true, retval);
					else
						System.err.println("Semantic rule " + rule_id + " is not defined.");
					m_symbols.push(new Symbol<t_lval>(false, lhs_id, retval));
				}

			""");

			if(m_gen_partials)
			{
				pw.print("""
				protected void ApplyPartialRule(int rule_id, int rule_len, boolean before_shift)
				{
					int arg_len = rule_len;
					if(before_shift)
						++rule_len;

					boolean already_seen_active_rule = false;
					boolean insert_new_active_rule = false;
					int seen_tokens_old = -1;

					Stack<ActiveRule<t_lval>> rulestack = m_active_rules.get(rule_id);
					if(rulestack != null)
					{
						if(!rulestack.empty())
						{
							ActiveRule<t_lval> active_rule = rulestack.peek();
							seen_tokens_old = active_rule.seen_tokens;

							if(before_shift)
							{
								if(active_rule.seen_tokens < rule_len)
									active_rule.seen_tokens = rule_len;
								else
									insert_new_active_rule = true;
							}
							else // before jump
							{
								if(active_rule.seen_tokens == rule_len)
									already_seen_active_rule = true;
								else
									active_rule.seen_tokens = rule_len;
							}
						}
						else
						{
							insert_new_active_rule = true;
						}
					}
					else
					{
						rulestack = new Stack<ActiveRule<t_lval>>();
						m_active_rules.put(rule_id, rulestack);
						insert_new_active_rule = true;
					}

					if(insert_new_active_rule)
					{
						seen_tokens_old = -1;

						ActiveRule<t_lval> active_rule = new ActiveRule<t_lval>();
						active_rule.seen_tokens = rule_len;
						active_rule.handle = m_cur_rule_handle++;

						rulestack.push(active_rule);
					}

					if(!already_seen_active_rule)
					{
						if(m_semantics == null || !m_semantics.containsKey(rule_id))
						{
							System.err.println("Semantic rule " + rule_id + " is not defined.");
							return;
						}

						ActiveRule<t_lval> active_rule = rulestack.peek();

						Vector<Symbol<t_lval>> args = new Vector<Symbol<t_lval>>();
						for(int i=0; i<arg_len; ++i)
							args.add(m_symbols.elementAt(m_symbols.size() - arg_len + i));

						if(!before_shift || seen_tokens_old < rule_len - 1)
						{
							if(m_debug)
							{
								System.out.println("Applying partial rule " + rule_id +
									" with " + arg_len + " arguments" +
									" (handle " + active_rule.handle + ")" +
									" Before shift: " + before_shift + ".");
							}
							active_rule.retval = m_semantics.get(rule_id).call(
								args, false, active_rule.retval);
						}

						if(before_shift)
						{
							args.add(m_lookahead);

							if(m_debug)
							{
								System.out.println("Applying partial rule " + rule_id +
									" with " + rule_len + " arguments" +
									" (handle " + active_rule.handle + ")" +
									" Before shift: " + before_shift + ".");
							}
							active_rule.retval = m_semantics.get(rule_id).call(
								args, false, active_rule.retval);
						}
					}
				}

			""");
			}

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

			// constructor
			pw.print("\tpublic " + parser_name + "()\n\t{\n");
			pw.print("\t\tm_debug = false;\n");
			pw.print("\t\tm_use_partials = true;\n");
			pw.print("\t\tm_input = null;\n");
			pw.print("\t\tm_semantics = null;\n");
			pw.print("\t\tm_symbols = new Stack<Symbol<t_lval>>();\n");
			if(m_gen_partials)
				pw.print("\t\tm_active_rules = new HashMap<Integer, Stack<ActiveRule<t_lval>>>();\n");
			pw.print("\t\tReset();\n");
			pw.print("\t}\n\n");  // end constructor

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
	 * get the table id from its index (or also vice-versa)
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
	 * get the nonterminal index from its table id
	 */
	protected int GetNontermTableIdx(int id)
	{
		Integer idx = GetTableId(m_map_nonterm_idx, id);

		if(idx == null)
			throw new RuntimeException("Invalid nonterminal id: " + id + ".");
		return idx;
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
