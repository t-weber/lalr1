/*
 * expression parser
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 12-nov-2022
 * @license see 'LICENSE' file
 */

use std::convert::TryInto;

mod expr;
mod idents;
mod types;
mod common;
//mod parser;
mod generated_parser;

//use expr::lalr1_tables::END;
use common::{Parsable, Symbol, TSemantics};
use types::*;
use idents::*;
//use parser::Parser;
use generated_parser::Parser;


fn main()
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

	let mut parser = Parser::new();
	parser.set_debug(true);
	parser.set_semantics(&SEMANTICS);
	let end = parser.get_end_id();
	//let end = END;

	parser.set_input(&[
		Symbol{is_term:true, id:1001, val:123},
		Symbol{is_term:true, id:'+' as usize, val:0},
		Symbol{is_term:true, id:1001, val:987},
		Symbol{is_term:true, id:end, val:0},
	]);

	if parser.parse()
	{
		let topsym = parser.get_top_symbol().unwrap();
		println!("{:?}", topsym);
	}
	else
	{
		println!("Error: Parsing failed.");
	}
}
