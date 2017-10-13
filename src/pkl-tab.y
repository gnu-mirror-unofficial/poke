/* pkl-tab.y - LARL(1) parser for Poke.  */

/* Copyright (C) 2017 Jose E. Marchesi.  */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

%define api.pure full
%define parse.lac full
%define parse.error verbose
%locations
%name-prefix "pkl_tab_"

%lex-param {void *scanner}
%parse-param {struct pkl_parser *pkl_parser}


%{
#include <config.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <xalloc.h>
#include <assert.h>

#include "pkl-ast.h"
#include "pkl-parser.h" /* For struct pkl_parser.  */
#include "pkl-tab.h"
#include "pkl-lex.h"

#ifdef PKL_DEBUG
# include "pkl-gen.h"
#endif

#define scanner (pkl_parser->scanner)
  
/* YYLLOC_DEFAULT -> default code for computing locations.  */
  
#define PKL_AST_CHILDREN_STEP 12

/* Error reporting function for pkl_tab_parse.

   When this function returns, the parser tries to recover the error.
   If it is unable to recover, then it returns with 1 (syntax error)
   immediately.  */
  
void
pkl_tab_error (YYLTYPE *llocp,
               struct pkl_parser *pkl_parser,
               char const *err)
{
  // XXX
  //  if (YYRECOVERING ())
  //    return;
  // XXX: store the line read and other info for pretty
  //      error printing in EXTRA.
  //  if (!pkl_parser->interactive)
    fprintf (stderr, "%s: %d: %s\n", pkl_parser->filename,
             llocp->first_line, err);
}

#if 0

/* The following functions are used in the actions in several grammar
   rules below.  This is to avoid replicating code in situations where
   the difference between rules are just a different permutation of
   its elements.

   All these functions return 1 if the action is executed
   successfully, or 0 if a syntax error should be raised at the
   grammar rule invoking the function.  */

static int
enum_specifier_action (struct pkl_parser *pkl_parser,
                       pkl_ast_node *enumeration,
                       pkl_ast_node tag, YYLTYPE *loc_tag,
                       pkl_ast_node enumerators, YYLTYPE *loc_enumerators,
                       pkl_ast_node docstr, YYLTYPE *loc_docstr)
{
  *enumeration = pkl_ast_make_enum (tag, enumerators, docstr);

  if (pkl_ast_register (pkl_parser->ast,
                        PKL_AST_IDENTIFIER_POINTER (tag),
                        *enumeration) == NULL)
    {
      pkl_tab_error (loc_tag, pkl_parser, "enum already defined");
      return 0;
    }

  return 1;
}

static int
struct_specifier_action (struct pkl_parser *pkl_parser,
                         pkl_ast_node *strct,
                         pkl_ast_node tag, YYLTYPE *loc_tag,
                         pkl_ast_node docstr, YYLTYPE *loc_docstr,
                         pkl_ast_node mem, YYLTYPE *loc_mem)
{
  *strct = pkl_ast_make_struct (tag, docstr, mem);

  if (pkl_ast_register (pkl_parser->ast,
                        PKL_AST_IDENTIFIER_POINTER (tag),
                        *strct) == NULL)
    {
      pkl_tab_error (loc_tag, pkl_parser, "struct already defined");
      return 0;
    }

  return 1;
}
#endif

static int
check_operand_unary (pkl_ast ast,
                     enum pkl_ast_op opcode,
                     pkl_ast_node *a)
{
  /* All unary operators require an integer as an argument.  */
  if (!PKL_AST_TYPE_INTEGRAL (PKL_AST_TYPE (*a)))
    return 0;

  return 1;
}

static int
promote_to_integral (size_t size, int sign,
                     pkl_ast ast, pkl_ast_node *a)
{
  if (PKL_AST_TYPE_INTEGRAL (PKL_AST_TYPE (*a)))
    {
      if (!(PKL_AST_TYPE_SIZE (*a) == size
            && PKL_AST_TYPE_SIGNED (*a) == sign))
        {
          pkl_ast_node desired_type
            = pkl_ast_search_std_type (ast, size, sign);

          if (!desired_type)
            desired_type = pkl_ast_make_type (0, sign, size,
                                              NULL /* enum */,
                                              NULL /* struct */);

          assert (desired_type);
          *a = pkl_ast_make_cast (desired_type, *a);
        }

      return 1;
    }

  return 0;

}

static int
promote_to_bool (pkl_ast ast, pkl_ast_node *a)
{
  return promote_to_integral (32, 1, ast, a);
}

static int
promote_to_ulong (pkl_ast ast, pkl_ast_node *a)
{
  return promote_to_integral (64, 0, ast, a);
}
 
static int
promote_operands_binary (pkl_ast ast,
                         pkl_ast_node *a,
                         pkl_ast_node *b,
                         int allow_strings,
                         int allow_arrays,
                         int allow_tuples)
{
  pkl_ast_node *to_promote_a = NULL;
  pkl_ast_node *to_promote_b = NULL;
  pkl_ast_node ta = PKL_AST_TYPE (*a);
  pkl_ast_node tb = PKL_AST_TYPE (*b);
  size_t size_a = PKL_AST_TYPE_SIZE (ta);
  size_t size_b = PKL_AST_TYPE_SIZE (tb);
  int sign_a = PKL_AST_TYPE_SIGNED (ta);
  int sign_b = PKL_AST_TYPE_SIGNED (tb);

  /* Both arguments should be either integrals, strings, arrays or
     tuples.  */

  if (PKL_AST_TYPE_INTEGRAL (ta) + PKL_AST_TYPE_INTEGRAL (tb) == 1)
    return 0;

  if ((!allow_tuples && ((PKL_AST_CODE (*a) == PKL_AST_TUPLE)
                         + (PKL_AST_CODE (*b) == PKL_AST_TUPLE) > 0))
      || (allow_tuples && ((PKL_AST_CODE (*a) == PKL_AST_TUPLE)
                           + (PKL_AST_CODE (*b) == PKL_AST_TUPLE) == 1)))
    return 0;

  if ((!allow_arrays && ((PKL_AST_CODE (*a) == PKL_AST_ARRAY)
                         + (PKL_AST_CODE (*b) == PKL_AST_ARRAY) > 0))
      || (allow_arrays && ((PKL_AST_CODE (*a) == PKL_AST_ARRAY)
                           + (PKL_AST_CODE (*b) == PKL_AST_ARRAY) == 1)))
    return 0;

  if ((!allow_strings && ((PKL_AST_TYPE_CODE (ta) == PKL_TYPE_STRING)
                          + (PKL_AST_TYPE_CODE (tb) == PKL_TYPE_STRING) > 0))
      || (allow_strings && ((PKL_AST_TYPE_CODE (ta) == PKL_TYPE_STRING)
                            + (PKL_AST_TYPE_CODE (tb) == PKL_TYPE_STRING) == 1)))
    return 0;

  if (!PKL_AST_TYPE_INTEGRAL (ta))
    return 1;

  /* Handle promotion of integer operands.  The rules are:

     - If one operand is narrower than the other, it is promoted to
       have the same width.  

     - If one operand is unsigned and the other signed, the signed
       operand is promoted to unsigned.  */

  if (size_a > size_b)
    {
      size_b = size_a;
      to_promote_b = b;
    }
  else if (size_a < size_b)
    {
      size_a = size_b;
      to_promote_a = a;
    }

  if (sign_a == 0 && sign_b == 1)
    {
      sign_b = 0;
      to_promote_b = b;
    }
  else if (sign_a == 1 && sign_b == 0)
    {
      sign_a = 0;
      to_promote_a = b;
    }

  if (to_promote_a != NULL)
    {
      pkl_ast_node t
        = pkl_ast_search_std_type (ast, size_a, sign_a);

      assert (t != NULL);

      *a = pkl_ast_make_cast (t, *a);
    }

  if (to_promote_b != NULL)
    {
      pkl_ast_node t
        = pkl_ast_search_std_type (ast, size_b, sign_b);

      assert (t != NULL);

      *b = pkl_ast_make_cast (t, *b);
    }

  return 1;
}

/* Inspect a list of array elements ELEMS and return the size of the
   array and the type of its element in *NELEM and *TYPE respectively.
   Print out diagnostic errors if appropriate.  */

static int
check_array_type (struct pkl_parser *parser,
                  YYLTYPE *llocp,
                  pkl_ast_node elems,
                  pkl_ast_node *type,
                  size_t *nelem)
{
  pkl_ast_node t;
  size_t index;

  *type = NULL;
  *nelem = 0;

  for (index = 0, t = elems; t; t = PKL_AST_CHAIN (t))
    {
      pkl_ast_node elem = PKL_AST_ARRAY_ELEM_EXP (t);
      size_t elem_index = PKL_AST_ARRAY_ELEM_INDEX (t);
      size_t elems_appended, effective_index;
      
      /* First check the type of the element.  */
      assert (PKL_AST_TYPE (elem));
      if (*type == NULL)
        *type = pkl_ast_type_dup (PKL_AST_TYPE (elem));
      else if (!pkl_ast_type_equal (PKL_AST_TYPE (elem), *type))
        {
          pkl_tab_error (llocp, parser,
                         "array element is of the wrong type.");
          return 0;
        }        
      
      /* Then set the index of the element.  */
      if (elem_index == PKL_AST_ARRAY_NOINDEX)
        {
          effective_index = index;
          elems_appended = 1;
        }
      else
        {
          if (elem_index < index)
            elems_appended = 0;
          else
            elems_appended = elem_index - index + 1;
          effective_index = elem_index;
        }
      
      PKL_AST_ARRAY_ELEM_INDEX (t) = effective_index;
      index += elems_appended;
      *nelem += elems_appended;
    }

  PKL_AST_TYPE_ARRAYOF (*type) += 1;
  return 1;
}
 
%}

%union {
  pkl_ast_node ast;
  enum pkl_ast_op opcode;
  int integer;
}

%destructor { /* pkl_ast_free ($$);*/ } <ast>

/* Primaries.  */

%token <ast> INTEGER
%token <ast> CHAR
%token <ast> STR
%token <ast> IDENTIFIER
%token <ast> TYPENAME

/* Reserved words.  */

%token ENUM
%token STRUCT
%token TYPEDEF
%token BREAK
%token CONST
%token CONTINUE
%token ELSE
%token FOR
%token WHILE
%token IF
%token SIZEOF
%token ASSERT
%token ERR

/* Operator tokens and their precedences, in ascending order.  */

%right '?' ':'
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left LE GE
%left SL SR
%left '+' '-'
%left '*' '/' '%'
%right UNARY INC DEC
%left HYPERUNARY
%left '.'

%token <opcode> MULA
%token <opcode> DIVA
%token <opcode> MODA
%token <opcode> ADDA
%token <opcode> SUBA
%token <opcode> SLA
%token <opcode> SRA
%token <opcode> BANDA
%token <opcode> XORA
%token <opcode> IORA

%token MSB LSB
%token SIGNED UNSIGNED

%type <opcode> unary_operator

%type <ast> program program_elem_list program_elem
%type <ast> expression primary
%type <ast> array_elem_list array_elem
%type <ast> type_specifier

%start program

%% /* The grammar follows.  */

program: program_elem_list
          	{
                  if (yynerrs > 0)
                    {
                      pkl_ast_node_free ($1);
                      YYERROR;
                    }
                  
                  $$ = pkl_ast_make_program ($1);
                  pkl_parser->ast->ast = ASTREF ($$);
                }
        ;

program_elem_list:
          %empty
		{
                  if (pkl_parser->what == PKL_PARSE_EXPRESSION)
                    /* We should parse exactly one expression.  */
                    YYERROR;
                  $$ = NULL;
                }
	| program_elem
        | program_elem_list program_elem
        	{
                  if (pkl_parser->what == PKL_PARSE_EXPRESSION)
                    /* We should parse exactly one expression.  */
                    YYERROR;
                  $$ = pkl_ast_chainon ($1, $2);
                }
	;

program_elem:
          expression
        	{
                  if (pkl_parser->what != PKL_PARSE_EXPRESSION)
                    /* Expressions are not valid top-level structures
                       in full poke programs.  */
                    YYERROR;
                  $$ = $1;
                }
	| expression ','
        	{
                  if (pkl_parser->what != PKL_PARSE_EXPRESSION)
                    /* Expressions are not valid top-level structures
                       in full poke programs.  */
                    YYERROR;
                  $$ = pkl_ast_make_program ($1);
                  pkl_parser->ast->ast = ASTREF ($$);
                  YYACCEPT;
                }
/*	  declaration
          	{
                  if (pkl_parser->what == PKL_PARSE_EXPRESSION)
                    YYERROR;
                  $$ = $1;
                  }*/
        ;

/*
 * Expressions.
 */

expression:
	  primary
        | unary_operator expression %prec UNARY
          	{
                  if (($1 == PKL_AST_OP_NOT
                       && !promote_to_bool (pkl_parser->ast, &$2))
                      || (!check_operand_unary (pkl_parser->ast, $1, &$2)))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operand to unary operator.");
                        YYERROR;
                    }
                  $$ = pkl_ast_make_unary_exp ($1, PKL_AST_TYPE ($2), $2);
                }
        | '(' type_specifier ')' expression %prec UNARY
        	{
                  $$ = pkl_ast_make_cast ($2, $4);
                  PKL_AST_TYPE ($$) = ASTREF ($2);
                }
        | SIZEOF expression %prec UNARY
        	{
                  $$ = pkl_ast_make_unary_exp (PKL_AST_OP_SIZEOF,
                                               PKL_AST_TYPE ($2), $2);
                }
        | SIZEOF '(' type_specifier ')' %prec HYPERUNARY
        	{
                  $$ = pkl_ast_make_unary_exp (PKL_AST_OP_SIZEOF,
                                               PKL_AST_TYPE ($3),
                                               $3);
                }
        | expression '+' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                1 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to '+'.");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_ADD,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '-' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to '-'.");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_SUB,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '*' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to '*'.");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_MUL,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '/' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to '/'.");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_DIV,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '%' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to '%'.");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_MOD,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression SL expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to <<");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_SL,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression SR expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to >>");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_SR,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression EQ expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                1 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to ==");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_EQ,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
	| expression NE expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                1 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to !=");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_NE,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '<' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                1 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to <");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_LT,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '>' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                1 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to >");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_GT,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression LE expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                1 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to <=");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_LE,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
	| expression GE expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                1 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to >=");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_GE,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '|' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to |");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_IOR,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '^' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to ^");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_XOR,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
	| expression '&' expression
        	{
                  if (!promote_operands_binary (pkl_parser->ast,
                                                &$1, &$3,
                                                0 /* allow_strings */,
                                                0 /* allow_arrays */,
                                                0 /* allow_tuples */))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to &");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_BAND,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression AND expression
        	{
                  if (!promote_to_bool (pkl_parser->ast, &$1)
                      || !promote_to_bool (pkl_parser->ast, &$3))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to &&");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_AND,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
	| expression OR expression
        	{
                  if (!promote_to_bool (pkl_parser->ast, &$1)
                      || !promote_to_bool (pkl_parser->ast, &$3))
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "invalid operators to ||");
                      YYERROR;
                    }
                  $$ = pkl_ast_make_binary_exp (PKL_AST_OP_OR,
                                                PKL_AST_TYPE ($1),
                                                $1, $3);
                }
        | expression '?' expression ':' expression
        	{ $$ = pkl_ast_make_cond_exp ($1, $3, $5); }
        ;

unary_operator:
	  '-'		{ $$ = PKL_AST_OP_NEG; }
	| '+'		{ $$ = PKL_AST_OP_POS; }
	| INC		{ $$ = PKL_AST_OP_PREINC; }
	| DEC		{ $$ = PKL_AST_OP_PREDEC; }
	| '~'		{ $$ = PKL_AST_OP_BNOT; }
	| '!'		{ $$ = PKL_AST_OP_NOT; }
	;

primary:
	  INTEGER
        | CHAR
        | STR
          /*        | IDENTIFIER */
        | '(' expression ')'
		{ $$ = $2; }
	| primary INC
        	{
                  $$ = pkl_ast_make_unary_exp (PKL_AST_OP_POSTINC,
                                               PKL_AST_TYPE ($1),
                                               $1);
                }
        | primary DEC
        	{
                  $$ = pkl_ast_make_unary_exp (PKL_AST_OP_POSTDEC,
                                               PKL_AST_TYPE ($1),
                                               $1);
                }
        | primary '[' expression ']' %prec '.'
        	{
                  if (PKL_AST_TYPE_ARRAYOF (PKL_AST_TYPE ($1)) < 1)
                    {
                      pkl_tab_error (&@1, pkl_parser,
                                     "operator to [] must be an array.");
                      YYERROR;
                    }
                  if (!promote_to_ulong (pkl_parser->ast, &$3))
                    {
                      pkl_tab_error (&@1, pkl_parser,
                                     "invalid index in array reference.");
                    }
                  $$ = pkl_ast_make_array_ref ($1, $3);
                }
        | '[' array_elem_list ']'
        	{
                  pkl_ast_node type;
                  size_t nelem;

                  if (!check_array_type (pkl_parser,
                                         &@2, $2,
                                         &type, &nelem))
                    YYERROR;
                  
                  $$ = pkl_ast_make_array (type, nelem, $2);
                }
	;

array_elem_list:
	  array_elem
        | array_elem_list ',' array_elem
          	{
                  $$ = pkl_ast_chainon ($1, $3);
                }
        ;

array_elem:
	  expression
          	{
                  $$ = pkl_ast_make_array_elem (PKL_AST_ARRAY_NOINDEX,
                                                $1);
                }
        | '.' '[' INTEGER ']' '=' expression
        	{
                  $$ = pkl_ast_make_array_elem (PKL_AST_INTEGER_VALUE ($3),
                                                $6);
                }
        ;

/*
 * Declarations.
 */

/*
declaration:
	  declaration_specifiers ';'
        ;

declaration_specifiers:
          typedef_specifier
          	| struct_specifier
                | enum_specifier 
        ;

*/

/*
 * Typedefs
 */

/*
typedef_specifier:
	  TYPEDEF type_specifier IDENTIFIER
          	{
                  const char *id = PKL_AST_IDENTIFIER_POINTER ($3);
                  pkl_ast_node type = pkl_ast_make_type (PKL_AST_TYPE_CODE ($2),
                                                         PKL_AST_TYPE_SIGNED ($2),
                                                         PKL_AST_TYPE_SIZE ($2),
                                                         PKL_AST_TYPE_ENUMERATION ($2),
                                                         PKL_AST_TYPE_STRUCT ($2));

                  if (pkl_ast_register (pkl_parser->ast, id, type) == NULL)
                    {
                      pkl_tab_error (&@2, pkl_parser, "type already defined");
                      YYERROR;
                    }

                  $$ = NULL;
                }
        ;
*/

type_specifier:
	  TYPENAME
          /*          	| STRUCT IDENTIFIER
        	{
                  pkl_ast_node strct
                    = pkl_ast_get_registered (pkl_parser->ast,
                                              PKL_AST_IDENTIFIER_POINTER ($2),
                                              PKL_AST_STRUCT);

                  if (!strct)
                    {
                      pkl_tab_error (&@2, pkl_parser,
                                     "expected struct tag");
                      YYERROR;
                    }
                  else
                    $$ = pkl_ast_make_type (PKL_TYPE_STRUCT, 0, 0, NULL, strct);
                }
        | ENUM IDENTIFIER
        	{
                  pkl_ast_node enumeration
                    = pkl_ast_get_registered (pkl_parser->ast,
                                              PKL_AST_IDENTIFIER_POINTER ($2),
                                              PKL_AST_ENUM);

                  if (!enumeration)
                    {
                      pkl_tab_error (&@2, pkl_parser, "expected enumeration tag");
                      YYERROR;
                    }
                  else
                    $$ = pkl_ast_make_type (PKL_TYPE_ENUM, 0, 32, enumeration, NULL);
                    }
        ;
*/

/*
 * Enumerations.
 */

/*
enum_specifier:
	  ENUM IDENTIFIER '{' enumerator_list '}'
          	{
                  if (! enum_specifier_action (pkl_parser,
                                               &$$,
                                               $IDENTIFIER, &@IDENTIFIER,
                                               $enumerator_list, &@enumerator_list,
                                               NULL, NULL))
                    YYERROR;
                }
	| ENUM IDENTIFIER DOCSTR '{' enumerator_list '}'
          	{
                  if (! enum_specifier_action (pkl_parser,
                                               &$$,
                                               $IDENTIFIER, &@IDENTIFIER,
                                               $enumerator_list, &@enumerator_list,
                                               $DOCSTR, &@DOCSTR))
                    YYERROR;
                }
        | ENUM DOCSTR IDENTIFIER '{' enumerator_list '}'
          	{
                  if (! enum_specifier_action (pkl_parser,
                                               &$$,
                                               $IDENTIFIER, &@IDENTIFIER,
                                               $enumerator_list, &@enumerator_list,
                                               $DOCSTR, &@DOCSTR))
                    YYERROR;
                }
        | ENUM IDENTIFIER '{' enumerator_list '}' DOCSTR
          	{
                  if (! enum_specifier_action (pkl_parser,
                                               &$$,
                                               $IDENTIFIER, &@IDENTIFIER,
                                               $enumerator_list, &@enumerator_list,
                                               $DOCSTR, &@DOCSTR))
                    YYERROR;
                }
        | DOCSTR ENUM IDENTIFIER '{' enumerator_list '}'
          	{
                  if (! enum_specifier_action (pkl_parser,
                                               &$$,
                                               $IDENTIFIER, &@IDENTIFIER,
                                               $enumerator_list, &@enumerator_list,
                                               $DOCSTR, &@DOCSTR))
                    YYERROR;
                }
        ;

enumerator_list:
	  enumerator
	| enumerator_list ',' enumerator
          	{ $$ = pkl_ast_chainon ($1, $3); }
	;

enumerator:
	  IDENTIFIER
                { $$ = pkl_ast_make_enumerator ($1, NULL, NULL); }
	| IDENTIFIER DOCSTR
                {
                  $$ = pkl_ast_make_enumerator ($1, NULL, $2);
                }
        | IDENTIFIER '=' constant_expression
                { $$ = pkl_ast_make_enumerator ($1, $3, NULL); }
        | IDENTIFIER '=' DOCSTR constant_expression
        	{
                  $$ = pkl_ast_make_enumerator ($1, $4, $3);
                }
        | IDENTIFIER '=' constant_expression DOCSTR
	        {
                  $$ = pkl_ast_make_enumerator ($1, $3, $4);
                }
        | DOCSTR IDENTIFIER '=' constant_expression
        	{
                  $$ = pkl_ast_make_enumerator ($2, $4, $1);
                }
	;

constant_expression:
	  conditional_expression
          	{
                  if (!PKL_AST_LITERAL_P ($1))
                    {
                      pkl_tab_error (&@1, pkl_parser,
                                     "expected constant expression");
                      YYERROR;
                    }

                  $$ = $1;
                }
        ;
*/
/*
 * Structs.
 */

/*
struct_specifier:
	  STRUCT IDENTIFIER mem_layout
			{
                          if (!struct_specifier_action (pkl_parser,
                                                        &$$,
                                                        $IDENTIFIER, &@IDENTIFIER,
                                                        NULL, NULL,
                                                        $mem_layout, &@mem_layout))
                            YYERROR;
                        }
	| DOCSTR STRUCT IDENTIFIER mem_layout
			{
                          if (!struct_specifier_action (pkl_parser,
                                                        &$$,
                                                        $IDENTIFIER, &@IDENTIFIER,
                                                        $DOCSTR, &@DOCSTR,
                                                        $mem_layout, &@mem_layout))
                            YYERROR;
                        }
        | STRUCT IDENTIFIER DOCSTR mem_layout
        	{
                  if (!struct_specifier_action (pkl_parser,
                                                &$$,
                                                $IDENTIFIER, &@IDENTIFIER,
                                                $DOCSTR, &@DOCSTR,
                                                $mem_layout, &@mem_layout))
                    YYERROR;
                }
        | STRUCT IDENTIFIER mem_layout DOCSTR
        		{
                          if (!struct_specifier_action (pkl_parser,
                                                        &$$,
                                                        $IDENTIFIER, &@IDENTIFIER,
                                                        $DOCSTR, &@DOCSTR,
                                                        $mem_layout, &@mem_layout))
                            YYERROR;
                        }
	;
*/
/*
 * Memory layouts.
 */

/*
mem_layout:
	  mem_endianness '{' mem_declarator_list '}'
          			{ $$ = pkl_ast_make_mem ($1, $3); }
	| '{' mem_declarator_list '}'
        			{ $$ = pkl_ast_make_mem (pkl_ast_default_endian (),
                                                         $2); }
        ;

mem_endianness:
	  MSB		{ $$ = PKL_AST_MSB; }
	| LSB		{ $$ = PKL_AST_LSB; }
	;

mem_declarator_list:
	  %empty
			{ $$ = NULL; }
	| mem_declarator ';'
        | mem_declarator_list  mem_declarator ';'
		        { $$ = pkl_ast_chainon ($1, $2); }
	;

mem_declarator:
	  mem_layout
        | mem_field_with_docstr
        | mem_cond
        | mem_loop
        | assignment_expression
        | assert
        ;

mem_field_with_docstr:
	  mem_field_with_endian
	| DOCSTR mem_field_with_endian
          		{
                          PKL_AST_FIELD_DOCSTR ($2) = ASTREF ($1);
                          $$ = $2;
                        }
        | mem_field_with_endian DOCSTR
		        {
                          PKL_AST_FIELD_DOCSTR ($1) = ASTREF ($2);
                          $$ = $1;
                        }
        ;

mem_field_with_endian:
	  mem_endianness mem_field_with_size
          		{
                          PKL_AST_FIELD_ENDIAN ($2) = $1;
                          $$ = $2;
                        }
        | mem_field_with_size
	;

mem_field_with_size:
	  mem_field
        | mem_field ':' expression
          		{
                          if (PKL_AST_TYPE_CODE (PKL_AST_FIELD_TYPE ($1))
                              == PKL_TYPE_STRUCT)
                            {
                              pkl_tab_error (&@1, pkl_parser,
                                             "fields of type struct can't have an\
 explicit size");
                              YYERROR;
                            }

                          * Discard the size inferred from the field
                             type and replace it with the
                             field width expression.  *
                          pkl_ast_node_free (PKL_AST_FIELD_SIZE ($1));
                          PKL_AST_FIELD_SIZE ($1) = ASTREF ($3);
                          $$ = $1;
                        }
        ;

mem_field:
	  type_specifier IDENTIFIER
          		{
                          pkl_ast_node one = pkl_ast_make_integer (1);
                          pkl_ast_node size = pkl_ast_make_integer (PKL_AST_TYPE_SIZE ($1));

                          $$ = pkl_ast_make_field ($2, $1, NULL,
                                                   pkl_ast_default_endian (),
                                                   one, size);
                        }
        | type_specifier IDENTIFIER '[' assignment_expression ']'
		        {
                          pkl_ast_node size = pkl_ast_make_integer (PKL_AST_TYPE_SIZE ($1));

                          $$ = pkl_ast_make_field ($2, $1, NULL,
                                                   pkl_ast_default_endian (),
                                                   $4, size);
                        }
	;

mem_cond:
	  IF '(' expression ')' mem_layout
          		{ $$ = pkl_ast_make_cond ($3, $5, NULL); }
        | IF '(' expression ')' mem_layout ELSE mem_layout
        		{ $$ = pkl_ast_make_cond ($3, $5, $7); }
        ;

mem_loop:
	  FOR '(' expression ';' expression ';' expression ')' mem_layout
          		{ $$ = pkl_ast_make_loop ($3, $5, $7, $9); }
        | WHILE '(' expression ')' mem_layout
        		{ $$ = pkl_ast_make_loop (NULL, $3, NULL, $5); }
	;

assert:
	  ASSERT expression
          		{ $$ = pkl_ast_make_assertion ($2); }
	;
*/

%%
