#
# expression parser ids
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 7-sep-2024
# @license see 'LICENSE' file
#


#
# semantic ids
#
sem_start_id    ::Int = 100
sem_brackets_id ::Int = 101
sem_add_id      ::Int = 200
sem_sub_id      ::Int = 201
sem_mul_id      ::Int = 202
sem_div_id      ::Int = 203
sem_mod_id      ::Int = 204
sem_pow_id      ::Int = 205
sem_uadd_id     ::Int = 210
sem_usub_id     ::Int = 211
sem_call0_id    ::Int = 300
sem_call1_id    ::Int = 301
sem_call2_id    ::Int = 302
sem_real_id     ::Int = 400
sem_int_id      ::Int = 401
sem_ident_id    ::Int = 410
sem_assign_id   ::Int = 500


#
# token ids
#
tok_real_id     ::Int = 1000
tok_int_id      ::Int = 1001
tok_str_id      ::Int = 1002
tok_ident_id    ::Int = 1003


#
# nonterminal ids
#
nonterm_start   ::Int = 10
nonterm_expr    ::Int = 20
