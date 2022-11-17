/*
 * expression parser types
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 12-nov-2022
 * @license see 'LICENSE' file
 */

#![allow(unused)]
pub use expr::lalr1_tables;


pub type TLVal = i64;
pub type TIndex = lalr1_tables::TIndex;
pub type TSymbolId = lalr1_tables::TSymbolId;
pub type TSemanticId = lalr1_tables::TSemanticId;
