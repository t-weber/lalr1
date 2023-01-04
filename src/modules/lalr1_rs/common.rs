/*
 * common/interface types
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 12-nov-2022
 * @license see 'LICENSE' file
 */

use types;


#[derive(Default, Clone, Debug)]
pub struct Symbol
{
	pub is_term : bool,
	pub id : types::TSymbolId,
	pub val : types::TLVal,
	pub strval : Option<String>,
}


#[derive(Default, Clone, Debug)]
pub struct ActiveRule
{
	pub seen_tokens : usize,
	pub handle : isize,
	pub retval : types::TLVal,
}


impl ActiveRule
{
	pub fn new() -> ActiveRule
	{
		let rule : ActiveRule = ActiveRule
		{
			seen_tokens : 0,
			handle : -1,
			retval : 0 as types::TLVal,
		};

		rule
	}
}


pub type TSemantics = fn(Vec<Symbol>, bool, types::TLVal)
	-> types::TLVal;


pub trait Parsable
{
	fn set_semantics(&mut self, sema : &[(types::TSemanticId, TSemantics)]);
	fn set_input(&mut self, input: &[Symbol]);
	fn set_debug(&mut self, debug : bool);
	fn set_partials(&mut self, use_partials : bool);

	fn get_end_id(&self) -> types::TSymbolId;
	fn get_top_symbol(&self) -> Option<&Symbol>;

	fn reset(&mut self);
	fn parse(&mut self) -> bool;
}
