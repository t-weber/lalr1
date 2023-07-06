#
# expression parser ids
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date sep-2022
# @license see 'LICENSE' file
#
# References:
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

#
# semantic ids
#
sem_start_id      = 100
sem_brackets_id   = 101
sem_add_id        = 200
sem_sub_id        = 201
sem_mul_id        = 202
sem_div_id        = 203
sem_mod_id        = 204
sem_pow_id        = 205
sem_uadd_id       = 210
sem_usub_id       = 211
sem_call0_id      = 300
sem_call1_id      = 301
sem_call2_id      = 302
sem_real_id       = 400
sem_int_id        = 401
sem_ident_id      = 410
sem_assign_id     = 500
sem_diff_id       = 600


#
# token ids
#
tok_real_id       = 1000
tok_int_id        = 1001
tok_str_id        = 1002
tok_ident_id      = 1003


#
# nonterminal ids
#
nonterm_start     = 10
nonterm_expr      = 20
nonterm_diff_expr = 30
