/**
 * expression lexer
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24-oct-2022
 * @license see 'LICENSE' file
 */

import java.util.Vector;
import java.util.regex.Pattern;
import java.util.regex.Matcher;


interface Converter<t_lval>
{
	Symbol<t_lval> call(int id, String strval);
}


public class Lexer<t_lval>
{
	public Lexer()
	{
		Pattern re_int = Pattern.compile("[0-9]+");
		Pattern re_real = Pattern.compile("[0-9]+(\\.[0-9]*)?");
		Pattern re_ident = Pattern.compile("[A-Za-z]+[A-Za-z0-9]*");

		m_regexes = new Vector<Pattern>();
		m_regexes.add(re_int);
		//m_regexes.add(re_real);
		m_regexes.add(re_ident);

		m_ids = new Vector<Integer>();
		m_ids.add(Ids.tok_int_id);
		//m_ids.add(Ids.tok_real_id);
		m_ids.add(Ids.tok_ident_id);

		Converter<t_lval> conv_int = (id, strval) ->
		{
			// TODO: correct warning in conversion
			t_lval val = null;
			val = (t_lval)Integer.valueOf(Integer.parseInt(strval));
			Symbol<t_lval> sym = new Symbol<t_lval>(true, id, val, strval);
			return sym;
		};

		/*Converter<t_lval> conv_real = (id, strval) ->
		{
			t_lval val = null;
			val = (t_lval)Double.valueOf(Double.parseDouble(strval));
			Symbol<t_lval> sym = new Symbol<t_lval>(true, id, val, strval);
			return sym;
		};*/

		Converter<t_lval> conv_ident = (id, strval) ->
		{
			Symbol<t_lval> sym = new Symbol<t_lval>(true, id, null, strval);
			return sym;
		};

		m_conv = new Vector<Converter<t_lval>>();
		m_conv.add(conv_int);
		//m_conv.add(conv_real);
		m_conv.add(conv_ident);
	}


	/**
	 * get the possibly matching tokens in a string
	 */
	public Vector<Symbol<t_lval>> GetMatches(String str)
	{
		Vector<Symbol<t_lval>> matches = new Vector<Symbol<t_lval>>();

		// get all possible matches
		for(int i=0; i<m_regexes.size(); ++i)
		{
			Pattern regex = m_regexes.elementAt(i);
			Matcher matcher = regex.matcher(str);

			if(matcher != null && matcher.lookingAt())
			{
				int start = matcher.start();
				int end = matcher.end();
				String strval = str.substring(start, end);

				Converter<t_lval> conv = m_conv.elementAt(i);
				int id = m_ids.elementAt(i);
				Symbol<t_lval> sym = conv.call(id, strval);
				matches.add(sym);
			}
		}

		// sort by match length
		matches.sort((sym1, sym2) ->
			sym2.GetStrVal().length() - sym1.GetStrVal().length());
		return matches;
	}


	/**
	 * get the next token in a string
	 */
	public Symbol<t_lval> GetToken(String str)
	{
		if(str.length() == 0)
			return null;

		// match pre-compiled regexes
		Vector<Symbol<t_lval>> matches = GetMatches(str);
		if(matches.size() > 0)
		{
			return matches.elementAt(0);
		}

		// match single-char operators
		char c = str.charAt(0);
		if(c=='+' || c=='-' ||
			c=='*' || c=='/' ||
			c=='%' || c=='^' ||
			c=='(' || c==')' ||
			c==',')
		{
			return new Symbol<t_lval>(true, (int)c, null, String.valueOf(c));
		}

		return null;
	}


	/**
	 * get the next token in a string
	 */
	public Vector<Symbol<t_lval>> GetTokens(String str)
	{
		Vector<Symbol<t_lval>> tokens = new Vector<Symbol<t_lval>>();

		while(true)
		{
			str = str.trim();
			Symbol<t_lval> token = GetToken(str);

			// end of string?
			if(token == null)
				break;

			tokens.add(token);

			// rest of the string
			str = str.substring(token.GetStrVal().length());
		}

		return tokens;
	}


	protected Vector<Pattern> m_regexes;        // pre-compiled regexes
	protected Vector<Integer> m_ids;            // corresponding token ids
	protected Vector<Converter<t_lval>> m_conv; // string to lvalue converter
}
