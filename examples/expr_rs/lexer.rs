/*
 * expression lexer
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 19-nov-2022
 * @license see 'LICENSE' file
 */

#![allow(unused)]

use types::*;
use idents::*;
use common::*;


fn match_int(str : &str, base : u32) -> bool
{
	for ch in str.chars()
	{
		if !ch.is_digit(base)
		{
			return false;
		}
	}

	true
}


fn match_real(str : &str, base : u32) -> bool
{
	let mut had_point : bool = false;
	let mut had_exp : bool = false;
	let mut exp_idx : usize = 0;

	for (idx, ch) in str.chars().enumerate()
	{
		if ch.is_digit(base)
		{
			continue;
		}

		// decimal point
		if ch == '.' && !had_point && !had_exp
		{
			had_point = true;
			continue;
		}

		// exp
		if ch == 'e' || ch == 'E' && !had_exp
		{
			had_exp = true;
			exp_idx = idx;
			continue;
		}

		// exp, "e+" or "e-"
		if (ch == '+' || ch == '-') && had_exp && idx == exp_idx+1
		{
			continue;
		}

		return false;
	}

	true
}


fn match_ident(str : &str) -> bool
{
	for (idx, ch) in str.chars().enumerate()
	{
		if idx==0 && !ch.is_alphabetic()
		{
			return false;
		}
		else if idx>0 && !ch.is_alphanumeric()
		{
			return false;
		}
	}

	true
}



fn match_str(str : &str) -> bool
{
	let mut str_opened : bool = false;
	let mut str_closed : bool = false;

	for (idx, ch) in str.chars().enumerate()
	{
		// beyond end of string
		if str_closed
		{
			return false;
		}

		// opening '"'
		if idx == 0 && ch == '\"'
		{
			str_opened = true;
			continue;
		}

		// closing '"'
		if ch == '\"' && str_opened
		{
			str_closed = true;
			continue;
		}
	}

	str_closed
}


/*
 * match an entire string against the possible tokens
 */
fn get_match(str : &str) -> Option<Symbol>
{
	// match integer
	if match_int(str, 10)
	{
		let lval = str.parse::<TLVal>().unwrap();
		return Some(Symbol{
			is_term : true,
			id : TOK_INT_ID,
			val : lval,
			strval : Some(str.to_string())
		});
	}

	// match real
	else if match_real(str, 10)
	{
		let lval = str.parse::<TLVal>().unwrap();
		return Some(Symbol{
			is_term : true,
			id : TOK_REAL_ID,
			val : lval,
			strval : Some(str.to_string())
		});
	}

	// match identifier
	else if match_ident(str)
	{
		return Some(Symbol{
			is_term : true,
			id : TOK_IDENT_ID,
			val : 0 as TLVal,
			strval : Some(str.to_string())
		});
	}

	// match single-char operators
	else if str.len() == 1
	{
		let ch : char = str.chars().nth(0).unwrap();
		if ch=='+' || ch=='-' || ch=='*' || ch=='/' || ch=='%' || ch=='^'
			|| ch=='(' || ch==')' || ch==','
		{
			return Some(Symbol{
				is_term : true,
				id : ch as TSymbolId,
				val : 0 as TLVal,
				strval : Some(str.to_string())
			});
		}
		else
		{
			return None;
		}
	}

	// no match
	None
}


/*
 * get the longest matching string and its end index
 */
fn get_longest_match(str : &str) -> (Option<Symbol>, usize)
{
	let len : usize = str.len();
	if len == 0
	{
		return (None, 0);
	}

	let mut last_match : Option<Symbol> = None;
	let mut match_found : bool = false;

	//for idx in (1..len).rev()
	for idx in (0..len)
	{
		let substr = str[0..=idx].to_string();
		let new_match : Option<Symbol> = get_match(&substr);
		if new_match.is_some()
		{
			last_match = new_match;
			match_found = true;
		}
		else if match_found
		{
			// longest match found
			return (last_match, idx);
		}
	}

	(last_match, len)
}


/*
 * get the longest matching string and its end index
 */
pub fn get_all_matches(str : &str) -> Vec<Symbol>
{
	let len : usize = str.len();
	let mut substr = str.trim().to_string();
	let mut syms : Vec<Symbol> = Vec::<Symbol>::new();

	loop
	{
		let (sym, idx) = get_longest_match(&substr);
		//println!("{:?}, {:?} {:?}", substr, sym, idx);
		if sym.is_none()
		{
			// no match
			break;
		}

		syms.push(sym.unwrap());

		if idx >= len
		{
			// last match
			break;
		}

		substr = substr[idx..].trim().to_string();
	}

	syms
}
