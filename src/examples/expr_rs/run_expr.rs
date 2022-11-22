/*
 * expression parser
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 12-nov-2022
 * @license see 'LICENSE' file
 */

use std::convert::TryInto;
use std::io::stdin;
use std::f64::consts::PI;

mod expr;
mod idents;
mod types;
mod common;
mod lexer;
mod parser;
//mod generated_parser;

use common::{Parsable, Symbol, TSemantics};
use types::*;
use idents::*;
use parser::Parser;
//use generated_parser::Parser;

const SET_DEBUG : bool = false;


fn get_symbol(args : Vec<Symbol>) -> TLVal
{
	if args[0].strval.is_none()
	{
		return 0 as TLVal;
	}

	let ident : &str = &args[0].strval.as_ref().unwrap();
	match ident
	{
		"pi" => { PI as TLVal },
		_ =>
		{
			println!("Identifier \"{}\" is unknown.", ident);
			0 as TLVal
		}
	}
}


fn call_func1(args : Vec<Symbol>) -> TLVal
{
	if args[0].strval.is_none()
	{
		return 0 as TLVal;
	}

	let arg1 : TLVal = args[2].val;

	let ident : &str = &args[0].strval.as_ref().unwrap();
	match ident
	{
		"sqrt" => { arg1.sqrt() as TLVal },
		"sin" => { arg1.sin() as TLVal },
		"cos" => { arg1.cos() as TLVal },
		"tan" => { arg1.tan() as TLVal },
		_ =>
		{
			println!("Function \"{}\" is unknown.", ident);
			0 as TLVal
		}
	}
}


fn call_func2(args : Vec<Symbol>) -> TLVal
{
	if args[0].strval.is_none()
	{
		return 0 as TLVal;
	}

	let arg1 : TLVal = args[2].val;
	let arg2 : TLVal = args[4].val;

	let ident : &str = &args[0].strval.as_ref().unwrap();
	match ident
	{
		"pow" => { arg1.powf(arg2) as TLVal },
		_ =>
		{
			println!("Function \"{}\" is unknown.", ident);
			0 as TLVal
		}
	}
}


fn set_semantics(parser : &mut dyn Parsable)
{
	const SEMANTICS : [(TSemanticId, TSemantics); 16] =
	[
		// ----------------------------------------------------------------------
		( SEM_START_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[0].val
		} ),

		( SEM_BRACKETS_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[1].val
		} ),
		// ----------------------------------------------------------------------

		// ----------------------------------------------------------------------
		// arithmetics
		( SEM_ADD_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[0].val + args[2].val
		} ),

		( SEM_SUB_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[0].val - args[2].val
		} ),

		( SEM_MUL_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[0].val * args[2].val
		} ),

		( SEM_DIV_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[0].val / args[2].val
		} ),

		( SEM_MOD_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[0].val % args[2].val
		} ),

		( SEM_POW_ID, |args : Vec<Symbol>| -> TLVal
		{
			TLVal::powf(args[0].val, args[2].val.try_into().unwrap())
		} ),

		( SEM_UADD_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[1].val
		} ),

		( SEM_USUB_ID, |args : Vec<Symbol>| -> TLVal
		{
			-args[1].val
		} ),
		// ----------------------------------------------------------------------

		// ----------------------------------------------------------------------
		// symbols and constants
		( SEM_REAL_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[0].val
		} ),

		( SEM_INT_ID, |args : Vec<Symbol>| -> TLVal
		{
			args[0].val
		} ),

		( SEM_IDENT_ID, get_symbol),
		// ----------------------------------------------------------------------

		// ----------------------------------------------------------------------
		// functions
		( SEM_CALL0_ID, |_args : Vec<Symbol>| -> TLVal
		{
			0 as TLVal // TODO
		} ),

		( SEM_CALL1_ID, call_func1),
		( SEM_CALL2_ID, call_func2),
		// ----------------------------------------------------------------------
	];

	parser.set_semantics(&SEMANTICS);
}


fn run_parser(parser : &mut dyn Parsable)
{
	parser.set_debug(SET_DEBUG);
	let end = parser.get_end_id();

	loop
	{
		let mut line : String = String::new();
		stdin().read_line(&mut line).expect("Could not read input.");
		line = line.trim().to_string();
		if line.len() == 0
		{
			continue
		}

		let mut tokens = lexer::get_all_matches(&line);
		tokens.push(Symbol{
			is_term : true,
			id : end,
			val : 0 as TLVal,
			strval : Some("<end>".to_string())
		});
		parser.set_input(&tokens);
		if SET_DEBUG
		{
			println!("Tokens: {:?}.", tokens);
		}

		if parser.parse()
		{
			let topsym = parser.get_top_symbol().unwrap();
			println!("{}", topsym.val);
		}
		else
		{
			println!("Error: Parsing failed.");
		}
	}
}


fn main()
{
	let mut parser = Parser::new();
	set_semantics(&mut parser);
	run_parser(&mut parser);
}
