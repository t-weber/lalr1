/*
 * expression parser ids
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 12-nov-2022
 * @license see 'LICENSE' file
 */

#![allow(unused)]

use types;


type TSymbolId = types::TSymbolId;
type TSemanticId = types::TSemanticId;


// semantic ids
pub const SEM_START_ID : TSemanticId    = 100;
pub const SEM_BRACKETS_ID : TSemanticId = 101;
pub const SEM_ADD_ID : TSemanticId      = 200;
pub const SEM_SUB_ID : TSemanticId      = 201;
pub const SEM_MUL_ID : TSemanticId      = 202;
pub const SEM_DIV_ID : TSemanticId      = 203;
pub const SEM_MOD_ID : TSemanticId      = 204;
pub const SEM_POW_ID : TSemanticId      = 205;
pub const SEM_UADD_ID : TSemanticId     = 210;
pub const SEM_USUB_ID : TSemanticId     = 211;
pub const SEM_CALL0_ID : TSemanticId    = 300;
pub const SEM_CALL1_ID : TSemanticId    = 301;
pub const SEM_CALL2_ID : TSemanticId    = 302;
pub const SEM_REAL_ID : TSemanticId     = 400;
pub const SEM_INT_ID : TSemanticId      = 401;
pub const SEM_IDENT_ID : TSemanticId    = 410;
pub const SEM_ASSIGN_ID : TSemanticId   = 500;

// token ids
pub const TOK_REAL_ID : TSymbolId       = 1000;
pub const TOK_INT_ID : TSymbolId        = 1001;
pub const TOK_STR_ID : TSymbolId        = 1002;
pub const TOK_IDENT_ID : TSymbolId      = 1003;

// nonterminals
pub const NONTERM_START : TSymbolId     = 10;
pub const NONTERM_EXPR : TSymbolId      = 20;

