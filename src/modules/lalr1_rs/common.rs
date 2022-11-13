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
}


pub type TSemantics = fn(Vec<Symbol>) -> types::TLVal;


pub trait Parsable
{
	fn set_semantics(&mut self, sema : &[(types::TSemanticId, TSemantics)]);
	fn set_input(&mut self, input: &[Symbol]);

	fn get_top_symbol(&self) -> Option<&Symbol>;

	fn reset(&mut self);
	fn parse(&mut self) -> bool;
}
