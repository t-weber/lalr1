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

import java.util.HashMap;


public class ParserGen
{
	// parsing tables
	protected ParsingTables m_tables;

	// table index and id maps
	protected HashMap<Integer, Integer> m_map_term_index;
	protected HashMap<Integer, Integer> m_map_nonterm_id;
	protected HashMap<Integer, Integer> m_map_semantic_id;


	public ParserGen(ParsingTables tables)
	{
		m_tables = tables;

		// create terminal map from id to index
		m_map_term_index = new HashMap<Integer, Integer>();
		int[][] term_idx = m_tables.GetTermIndexMap();
		for(int i=0; i<term_idx.length; ++i)
			m_map_term_index.put(term_idx[i][0], term_idx[i][1]);

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
	}


	public boolean CreateParser(String parser_name)
	{
		// TODO

		return true;
	}


	/**
	 * get the terminal table index from its id
	 */
	protected int GetTermTableIndex(int id)
	{
		HashMap<Integer, Integer> map = m_map_term_index;
		Integer idx = map.get(id);

		if(idx == null)
			throw new RuntimeException("Invalid terminal id: " + id + ".");
		return idx;
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



	public static void main(String[] args)
	{
		if(args.length == 0)
		{
			System.err.println("Please give a ParsingTables class name.");
			return;
		}

		try
		{
			String class_name = args[0];
			String parser_name = class_name.replace("Tab", "") + "Parser";
			System.out.println("Creating parser \"" + class_name + ".class\" -> \"" + parser_name + ".java\".");

			// create parsing tables
			Class<?> tab_class = Class.forName(class_name);
			ParsingTables tab = (ParsingTables)tab_class.getConstructor().newInstance();

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
