/*
 * expression parser
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 12-nov-2022
 * @license see 'LICENSE' file
 */

use std::convert::TryInto;
use std::io::stdin;

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
			TLVal::pow(args[0].val, args[2].val.try_into().unwrap())
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

		( SEM_IDENT_ID, |_args : Vec<Symbol>| -> TLVal
		{
			//args[0].val
			0 // TODO
		} ),
		// ----------------------------------------------------------------------

		// ----------------------------------------------------------------------
		// functions
		( SEM_CALL0_ID, |_args : Vec<Symbol>| -> TLVal
		{
			0 // TODO
		} ),

		( SEM_CALL1_ID, |_args : Vec<Symbol>| -> TLVal
		{
			0 // TODO
		} ),

		( SEM_CALL2_ID, |_args : Vec<Symbol>| -> TLVal
		{
			0 // TODO
		} ),
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
		tokens.push(Symbol{is_term:true, id:end, val:0});
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
