/* pkl-anal.c - Analysis phases for the poke compiler.  */

/* Copyright (C) 2019, 2020, 2021, 2022 Jose E. Marchesi */

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

#include <config.h>
#include <string.h>
#include <stdlib.h>

#include "pk-utils.h"

#include "pkl.h"
#include "pkl-diag.h"
#include "pkl-ast.h"
#include "pkl-pass.h"
#include "pkl-anal.h"

/* This file implements several analysis compiler phases, which can
   raise errors and/or warnings, and update annotations in nodes, but
   won't alter the structure of the AST.  These phases are
   restartable.

   `anal1' is run immediately after trans1.
   `anal2' is run after constant folding.

   `analf' is run in the backend pass, right before gen.  Its main
   purpose is to determine that every node that is traversed
   optionally in do_pass but that is required by the code generator
   exists.  This avoids the codegen to generate invalid code silently.

   See the handlers below for detailed information about what these
   phases check for.  */

#define PKL_ANAL_PAYLOAD ((pkl_anal_payload) PKL_PASS_PAYLOAD)

#define PKL_ANAL_CONTEXT                                                \
  (PKL_ANAL_PAYLOAD->next_context == 0                                  \
   ? -1                                                                 \
   : PKL_ANAL_PAYLOAD->context[PKL_ANAL_PAYLOAD->next_context - 1])

#define PKL_ANAL_PUSH_CONTEXT(CTX)                                      \
  do                                                                    \
    {                                                                   \
      assert (PKL_ANAL_PAYLOAD->next_context < PKL_ANAL_MAX_CONTEXT_NEST); \
      PKL_ANAL_PAYLOAD->context[PKL_ANAL_PAYLOAD->next_context++]       \
        = (CTX);                                                        \
    } while (0)

#define PKL_ANAL_POP_CONTEXT                            \
  do                                                    \
    {                                                   \
      assert (PKL_ANAL_PAYLOAD->next_context > 0);      \
      PKL_ANAL_PAYLOAD->next_context--;                 \
    } while (0)

/* The following handler is used in all anal phases, and initializes
   the phase payload.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal_pr_program)
{
  /* No errors initially.  */
  PKL_ANAL_PAYLOAD->errors = 0;
}
PKL_PHASE_END_HANDLER

/* Sanity check for context management.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal_ps_program)
{
  assert (PKL_ANAL_PAYLOAD->next_context == 0);
}
PKL_PHASE_END_HANDLER

/* The following handler is used in all anal phases, and handles
   changing the source file for diagnostics.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal_ps_src)
{
  pkl_ast_node src = PKL_PASS_NODE;
  char *filename = PKL_AST_SRC_FILENAME (src);

  free (PKL_PASS_AST->filename);
  PKL_PASS_AST->filename = filename ? strdup (filename) : NULL;
}
PKL_PHASE_END_HANDLER

/* In struct literals, make sure that the names of its elements are
   unique in the structure.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_struct)
{
  pkl_ast_node node = PKL_PASS_NODE;
  pkl_ast_node elems = PKL_AST_STRUCT_FIELDS (node);
  pkl_ast_node t;

  for (t = elems; t; t = PKL_AST_CHAIN (t))
    {
      pkl_ast_node ename = PKL_AST_STRUCT_FIELD_NAME (t);

      if (ename != NULL)
        {
          pkl_ast_node u;

          for (u = elems; u != t; u = PKL_AST_CHAIN (u))
            {
              pkl_ast_node uname = PKL_AST_STRUCT_FIELD_NAME (u);

              if (uname != NULL)
                if (STREQ (PKL_AST_IDENTIFIER_POINTER (ename),
                           PKL_AST_IDENTIFIER_POINTER (uname)))
                  {
                    PKL_ERROR (PKL_AST_LOC (u),
                               "duplicated struct element '%s'",
                               PKL_AST_IDENTIFIER_POINTER (uname));
                    PKL_ANAL_PAYLOAD->errors++;
                    PKL_PASS_ERROR;
                    /* Do not report more duplicates in this struct.  */
                    break;
                  }
            }
        }
    }
}
PKL_PHASE_END_HANDLER

/* Type structs introduce a context.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_pr_type_struct)
{
  PKL_ANAL_PUSH_CONTEXT (PKL_ANAL_CONTEXT_STRUCT_TYPE);
}
PKL_PHASE_END_HANDLER

/* In struct TYPE nodes, check that no duplicated named element are
   declared in the type.  This covers both declared entities and
   struct fields.

   Also, declarations in unions are only allowed before any of the
   alternatives, but methods can appear anywhere.

   Integral structs cannot be pinned.

   Also, pop the analysis context.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_type_struct)
{
  pkl_ast_node struct_type = PKL_PASS_NODE;
  pkl_ast_node struct_type_elems
    = PKL_AST_TYPE_S_ELEMS (struct_type);
  pkl_ast_node t;

  if (PKL_AST_TYPE_S_UNION_P (struct_type))
    {
      int found_field = 0;

      for (t = struct_type_elems; t; t = PKL_AST_CHAIN (t))
        {
          if (found_field
              && PKL_AST_CODE (t) != PKL_AST_STRUCT_TYPE_FIELD
              && !(PKL_AST_CODE (t) == PKL_AST_DECL
                   && PKL_AST_DECL_KIND (t) == PKL_AST_DECL_KIND_FUNC
                   && PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (t))))
            {
              PKL_ERROR (PKL_AST_LOC (t),
                         "declarations are not supported after union fields");
              PKL_ANAL_PAYLOAD->errors++;
              PKL_PASS_ERROR;
            }
          else
            found_field = 1;
        }
    }

  if (PKL_AST_TYPE_S_ITYPE (struct_type)
      && PKL_AST_TYPE_S_PINNED_P (struct_type))
    {
      PKL_ERROR (PKL_AST_LOC (PKL_AST_TYPE_S_ITYPE (struct_type)),
                 "integral structs cannot be pinned");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }

  for (t = struct_type_elems; t; t = PKL_AST_CHAIN (t))
    {
      pkl_ast_node tname
        = (PKL_AST_CODE (t) == PKL_AST_STRUCT_TYPE_FIELD
           ? PKL_AST_STRUCT_TYPE_FIELD_NAME (t)
           : PKL_AST_DECL_NAME (t));
      pkl_ast_node u;

      if (PKL_AST_TYPE_S_UNION_P (struct_type) && !tname)
        {
          PKL_ERROR (PKL_AST_LOC (t),
                     "anonymous members are not allowed in unions");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }

      for (u = struct_type_elems; u != t; u = PKL_AST_CHAIN (u))
        {
          pkl_ast_node uname
            = (PKL_AST_CODE (u) == PKL_AST_STRUCT_TYPE_FIELD
               ? PKL_AST_STRUCT_TYPE_FIELD_NAME (u)
               : PKL_AST_DECL_NAME (u));

          if (uname
              && tname
              && STREQ (PKL_AST_IDENTIFIER_POINTER (uname),
                        PKL_AST_IDENTIFIER_POINTER (tname)))
            {
              PKL_ERROR (PKL_AST_LOC (u),
                         "duplicated struct element '%s'",
                         PKL_AST_IDENTIFIER_POINTER (uname));
              PKL_ANAL_PAYLOAD->errors++;
              PKL_PASS_ERROR;
            }
        }
    }

  PKL_ANAL_POP_CONTEXT;
}
PKL_PHASE_END_HANDLER

/* Builtin compound statements can't contain statements
   themselves.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_comp_stmt)
{
  pkl_ast_node comp_stmt = PKL_PASS_NODE;

  if (PKL_AST_COMP_STMT_BUILTIN (comp_stmt) != PKL_AST_BUILTIN_NONE
      && PKL_AST_COMP_STMT_STMTS (comp_stmt) != NULL)
    {
      PKL_ICE (PKL_AST_LOC (comp_stmt),
               "builtin comp-stmt contains statements");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* Every node in the AST should have a valid location after parsing.
   This handler is used in both anal1 and anal2.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal_ps_default)
{
#if 0
  if (!PKL_AST_LOC_VALID (PKL_AST_LOC (PKL_PASS_NODE)))
    {
      PKL_ICE (PKL_AST_NOLOC,
               "node #%" PRIu64 " with code %d has no location",
               PKL_AST_UID (PKL_PASS_NODE), PKL_AST_CODE (PKL_PASS_NODE));
      PKL_PASS_ERROR;
    }
#endif
}
PKL_PHASE_END_HANDLER

/* The arguments to a funcall should be either all named, or none
   named.  Also, it is not allowed to specify the same argument
   twice.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_funcall)
{
  pkl_ast_node funcall = PKL_PASS_NODE;
  pkl_ast_node funcall_arg;
  int some_named = 0;
  int some_unnamed = 0;

  /* Check that all arguments are either named or unnamed.  */
  for (funcall_arg = PKL_AST_FUNCALL_ARGS (funcall);
       funcall_arg;
       funcall_arg = PKL_AST_CHAIN (funcall_arg))
    {
      if (PKL_AST_FUNCALL_ARG_NAME (funcall_arg))
        some_named = 1;
      else
        some_unnamed = 1;
    }

  if (some_named && some_unnamed)
    {
      PKL_ERROR (PKL_AST_LOC (funcall),
                 "found named and not-named arguments mixed in funcall");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }

  /* If arguments are named, check that there are not arguments named
     twice.  */
  if (some_named)
    {
      for (funcall_arg = PKL_AST_FUNCALL_ARGS (funcall);
           funcall_arg;
           funcall_arg = PKL_AST_CHAIN (funcall_arg))
        {
          pkl_ast_node aa;
          for (aa = PKL_AST_CHAIN (funcall_arg); aa; aa = PKL_AST_CHAIN (aa))
            {
              pkl_ast_node identifier1 = PKL_AST_FUNCALL_ARG_NAME (funcall_arg);
              pkl_ast_node identifier2 = PKL_AST_FUNCALL_ARG_NAME (aa);

              if (STREQ (PKL_AST_IDENTIFIER_POINTER (identifier1),
                         PKL_AST_IDENTIFIER_POINTER (identifier2)))
                {
                  PKL_ERROR (PKL_AST_LOC (aa),
                             "duplicated argument in funcall");
                  PKL_ANAL_PAYLOAD->errors++;
                  PKL_PASS_ERROR;
                }
            }
        }
    }
}
PKL_PHASE_END_HANDLER

/* Methods introduce an analysis context.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_pr_func)
{
  if (PKL_AST_FUNC_METHOD_P (PKL_PASS_NODE))
    PKL_ANAL_PUSH_CONTEXT (PKL_ANAL_CONTEXT_METHOD);
}
PKL_PHASE_END_HANDLER

/* Check that all optional formal arguments in a function specifier
   are at the end of the arguments list, and other checks.

   Also, pop the analysis context if this was a method.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_func)
{
  pkl_ast_node func = PKL_PASS_NODE;
  pkl_ast_node fa;
  int in_opt_args = 0;

  for (fa = PKL_AST_FUNC_ARGS (func);
       fa;
       fa = PKL_AST_CHAIN (fa))
    {
      if (fa == PKL_AST_FUNC_FIRST_OPT_ARG (func))
        in_opt_args = 1;

      /* All optional formal arguments in a function specifier should
         be at the end of the arguments list.  */
      if (in_opt_args && !PKL_AST_FUNC_ARG_INITIAL (fa))
        {
          PKL_ERROR (PKL_AST_LOC (fa),
                     "non-optional argument after optional arguments");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }

      /* If there is a vararg argument, it should be at the end of the
         list of arguments.  Also, it should be unique.  */
      if (PKL_AST_FUNC_ARG_VARARG (fa) == 1 && PKL_AST_CHAIN (fa) != NULL)
        {
          PKL_ERROR (PKL_AST_LOC (fa),
                     "vararg argument should be the last argument");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
    }

  if (PKL_AST_FUNC_METHOD_P (PKL_PASS_NODE))
    PKL_ANAL_POP_CONTEXT;
}
PKL_PHASE_END_HANDLER

/* In function type specifier arguments, only one vararg argument can
   exist, and it should be the last argument in the type.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_type_function)
{
  pkl_ast_node func_type = PKL_PASS_NODE;
  pkl_ast_node arg;

  for (arg = PKL_AST_TYPE_F_ARGS (func_type);
       arg;
       arg = PKL_AST_CHAIN (arg))
    {
      if (PKL_AST_FUNC_TYPE_ARG_VARARG (arg)
          && PKL_AST_CHAIN (arg) != NULL
          && PKL_AST_LOC_VALID (PKL_AST_LOC (arg)))
        {
          PKL_ERROR (PKL_AST_LOC (arg),
                     "vararg argument should be the last argument");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
    }
}
PKL_PHASE_END_HANDLER

/* Make sure every BREAK statement has an associated entity.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_break_stmt)
{
  pkl_ast_node break_stmt = PKL_PASS_NODE;

  if (!PKL_AST_BREAK_STMT_ENTITY (break_stmt))
    {
      PKL_ERROR (PKL_AST_LOC (break_stmt),
                 "`break' statement without containing statement");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* Make sure every CONTINUE statement has an associated entity.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_continue_stmt)
{
  pkl_ast_node continue_stmt = PKL_PASS_NODE;

  if (!PKL_AST_CONTINUE_STMT_ENTITY (continue_stmt))
    {
      PKL_ERROR (PKL_AST_LOC (continue_stmt),
                 "`continue' statement without containing statement");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* Every return statement should be associated with a containing
   function.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_return_stmt)
{
  pkl_ast_node return_stmt = PKL_PASS_NODE;

  if (PKL_AST_RETURN_STMT_FUNCTION (return_stmt) == NULL)
    {
      PKL_ERROR (PKL_AST_LOC (return_stmt),
                 "`return' statement without containing function");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* Arrays of `void' are not allowed.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_type_array)
{
  pkl_ast_node type = PKL_PASS_NODE;
  pkl_ast_node etype = PKL_AST_TYPE_A_ETYPE (type);

  if (PKL_AST_TYPE_CODE (etype) == PKL_TYPE_VOID)
    {
      PKL_ERROR (PKL_AST_LOC (etype),
                 "arrays of type `void' are not allowed");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* If the unit in an offset type specifier is specified using an
   integral constant, this constant should be bigger than zero.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_type_offset)
{
  pkl_ast_node offset_type = PKL_PASS_NODE;
  pkl_ast_node unit = PKL_AST_TYPE_O_UNIT (offset_type);

  if (PKL_AST_CODE (unit) == PKL_AST_INTEGER
      && PKL_AST_INTEGER_VALUE (unit) == 0)
    {
      PKL_ERROR (PKL_AST_LOC (unit),
                 "the unit in offset types shall be bigger than zero");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* The unit of an offset literal, if expressed as an integral, shall
   be bigger than zero.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_offset)
{
  pkl_ast_node node = PKL_PASS_NODE;
  pkl_ast_node unit = PKL_AST_OFFSET_UNIT (node);

  if (PKL_AST_CODE (unit) == PKL_AST_INTEGER
      && PKL_AST_INTEGER_VALUE (unit) == 0)
    {
      PKL_ERROR (PKL_AST_LOC (unit),
                 "the unit in offsets shall be bigger than zero");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }

}
PKL_PHASE_END_HANDLER

/* The bit count operator in left shift operations can't be equal or
   higher than the number of bits of the shifted operand.  Check here
   the cases where the bit count is constant.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_op_sl)
{
  pkl_ast_node op = PKL_PASS_NODE;
  pkl_ast_node value = PKL_AST_EXP_OPERAND (op, 0);
  pkl_ast_node count = PKL_AST_EXP_OPERAND (op, 1);
  pkl_ast_node value_type = PKL_AST_TYPE (value);

  assert (value_type != NULL);

  /* The operand may be an integral struct, and promo hasn't been
     performed yet for this node.  */
  if (PKL_AST_TYPE_CODE (value_type) == PKL_TYPE_STRUCT
      && PKL_AST_TYPE_S_ITYPE (value_type))
    value_type = PKL_AST_TYPE_S_ITYPE (value_type);

  if (PKL_AST_CODE (count) == PKL_AST_INTEGER
      && PKL_AST_INTEGER_VALUE (count) >= PKL_AST_TYPE_I_SIZE (value_type))
    {
      PKL_ERROR (PKL_AST_LOC (count),
                 "count in left bit shift too big");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* Methods can only be defined in a struct type.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_pr_decl)
{
  pkl_ast_node decl = PKL_PASS_NODE;

  if (PKL_AST_DECL_KIND (decl) == PKL_AST_DECL_KIND_FUNC)
    {
      pkl_ast_node func = PKL_AST_DECL_INITIAL (decl);

      if (PKL_AST_FUNC_METHOD_P (func)
          && (!PKL_PASS_PARENT
              || (PKL_AST_CODE (PKL_PASS_PARENT) != PKL_AST_TYPE
                  || PKL_AST_TYPE_CODE (PKL_PASS_PARENT) != PKL_TYPE_STRUCT)))
        {
          pkl_ast_node decl_name = PKL_AST_DECL_NAME (decl);

          PKL_ERROR (PKL_AST_LOC (decl_name),
                     "methods are only allowed inside struct types");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
    }
}
PKL_PHASE_END_HANDLER

/* The initializing expressions in unit declarations should be integer
   nodes.  Note this handler runs after the unit is
   constant-folded.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_decl)
{
  pkl_ast_node decl = PKL_PASS_NODE;

  if (PKL_AST_DECL_KIND (decl) == PKL_AST_DECL_KIND_UNIT)
    {
      pkl_ast_node initial = PKL_AST_DECL_INITIAL (decl);

      if (PKL_AST_CODE (initial) != PKL_AST_INTEGER)
        {
          PKL_ERROR (PKL_AST_LOC (initial),
                     "expected constant integral value for unit");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
    }
}
PKL_PHASE_END_HANDLER

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_var)
{
  pkl_ast_node var = PKL_PASS_NODE;
  pkl_ast_node var_decl = PKL_AST_VAR_DECL (var);
  pkl_ast_node var_function = PKL_AST_VAR_FUNCTION (var);

  const int in_method_p
    = var_function && PKL_AST_FUNC_METHOD_P (var_function);
  const int var_is_method_p
    = (PKL_AST_DECL_KIND (var_decl) == PKL_AST_DECL_KIND_FUNC
       && PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (var_decl)));
  const int var_is_field_p
    = PKL_AST_DECL_STRUCT_FIELD_P (var_decl);

  /* Only methods can call other methods.  */
  if (var_is_method_p && !in_method_p)
    {
      PKL_ERROR (PKL_AST_LOC (var),
                 "only methods can directly call other methods");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }

  /* A method can only refer to struct fields and methods defined in
     the same struct.  */
  if (in_method_p
      && (var_is_field_p || var_is_method_p))
    {
      const char *what
        = var_is_method_p ? "method" : "field";

      int back = PKL_AST_VAR_BACK (var);
      int function_back = PKL_AST_VAR_FUNCTION_BACK (var);

      if (back != (function_back + 1))
        {
          PKL_ERROR (PKL_AST_LOC (var),
                     "referred %s not in this struct", what);
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
    }
}
PKL_PHASE_END_HANDLER

/* It is an error to set a struct field as a variable if we are not in
   a method.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_ass_stmt)
{
  pkl_ast_node ass_stmt = PKL_PASS_NODE;
  pkl_ast_node ass_stmt_lvalue = PKL_AST_ASS_STMT_LVALUE (ass_stmt);

  if (PKL_AST_CODE (ass_stmt_lvalue) == PKL_AST_VAR)
    {
      pkl_ast_node var = ass_stmt_lvalue;
      pkl_ast_node var_decl = PKL_AST_VAR_DECL (var);
      pkl_ast_node var_function = PKL_AST_VAR_FUNCTION (var);

      if (var_function
          && PKL_AST_DECL_STRUCT_FIELD_P (var_decl)
          && !PKL_AST_FUNC_METHOD_P (var_function))
        {
          PKL_ERROR (PKL_AST_LOC (var),
                     "invalid assignment to struct field");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
    }
}
PKL_PHASE_END_HANDLER

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_cons)
{
  pkl_ast_node cons = PKL_PASS_NODE;
  pkl_ast_node cons_type = PKL_AST_CONS_TYPE (cons);
  pkl_ast_node cons_value = PKL_AST_CONS_VALUE (cons);

  /* Different kind of constructors require different number and kind
     of arguments.  Check it here.  */

  switch (PKL_AST_TYPE_CODE (cons_type))
    {
    case PKL_TYPE_STRUCT:
      /* Struct constructors accept exactly one argument.  This is
         enforced by the syntax.  */
      if (pkl_ast_chain_length (cons_value) != 1)
        {
          PKL_ICE (PKL_AST_LOC (cons),
                   "struct constructor requires exactly one argument");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
      break;
    case PKL_TYPE_ARRAY:
      /* Array constructors accept zero or one arguments.  */
      if (pkl_ast_chain_length (cons_value) > 1)
        {
          PKL_ERROR (PKL_AST_LOC (cons),
                     "struct constructor requires exactly one argument");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
      break;
    default:
      assert (0);
    }
}
PKL_PHASE_END_HANDLER

/* Some attributes are binary and thus require a second argument.
   Make sure they have it.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal1_ps_op_attr)
{
  pkl_ast_node exp = PKL_PASS_NODE;
  enum pkl_ast_attr attr = PKL_AST_EXP_ATTR (exp);

  assert (attr != PKL_AST_ATTR_NONE);

  switch (attr)
    {
    case PKL_AST_ATTR_ELEM:
    case PKL_AST_ATTR_EOFFSET:
    case PKL_AST_ATTR_ESIZE:
    case PKL_AST_ATTR_ENAME:
      if (PKL_AST_EXP_NUMOPS (exp) != 2)
        {
          PKL_ERROR (PKL_AST_LOC (exp),
                     "attribute requires an argument");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
      break;
    default:
      if (PKL_AST_EXP_NUMOPS (exp) != 1)
        {
          PKL_ERROR (PKL_AST_LOC (exp),
                     "attribute doesn't take any argument");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
      break;
    }
}
PKL_PHASE_END_HANDLER

struct pkl_phase pkl_phase_anal1 =
  {
   PKL_PHASE_PS_HANDLER (PKL_AST_SRC, pkl_anal_ps_src),
   PKL_PHASE_PR_HANDLER (PKL_AST_PROGRAM, pkl_anal_pr_program),
   PKL_PHASE_PS_HANDLER (PKL_AST_PROGRAM, pkl_anal_ps_program),
   PKL_PHASE_PS_HANDLER (PKL_AST_STRUCT, pkl_anal1_ps_struct),
   PKL_PHASE_PS_HANDLER (PKL_AST_COMP_STMT, pkl_anal1_ps_comp_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_BREAK_STMT, pkl_anal1_ps_break_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_CONTINUE_STMT, pkl_anal1_ps_continue_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_FUNCALL, pkl_anal1_ps_funcall),
   PKL_PHASE_PR_HANDLER (PKL_AST_FUNC, pkl_anal1_pr_func),
   PKL_PHASE_PS_HANDLER (PKL_AST_FUNC, pkl_anal1_ps_func),
   PKL_PHASE_PS_HANDLER (PKL_AST_RETURN_STMT, pkl_anal1_ps_return_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_OFFSET, pkl_anal1_ps_offset),
   PKL_PHASE_PR_HANDLER (PKL_AST_DECL, pkl_anal1_pr_decl),
   PKL_PHASE_PS_HANDLER (PKL_AST_DECL, pkl_anal1_ps_decl),
   PKL_PHASE_PS_HANDLER (PKL_AST_VAR, pkl_anal1_ps_var),
   PKL_PHASE_PS_HANDLER (PKL_AST_ASS_STMT, pkl_anal1_ps_ass_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_CONS, pkl_anal1_ps_cons),
   PKL_PHASE_PR_TYPE_HANDLER (PKL_TYPE_STRUCT, pkl_anal1_pr_type_struct),
   PKL_PHASE_PS_TYPE_HANDLER (PKL_TYPE_STRUCT, pkl_anal1_ps_type_struct),
   PKL_PHASE_PS_TYPE_HANDLER (PKL_TYPE_FUNCTION, pkl_anal1_ps_type_function),
   PKL_PHASE_PS_TYPE_HANDLER (PKL_TYPE_OFFSET, pkl_anal1_ps_type_offset),
   PKL_PHASE_PS_TYPE_HANDLER (PKL_TYPE_ARRAY, pkl_anal1_ps_type_array),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_SL, pkl_anal1_ps_op_sl),
   PKL_PHASE_PS_OP_HANDLER (PKL_AST_OP_ATTR, pkl_anal1_ps_op_attr),
   PKL_PHASE_PS_DEFAULT_HANDLER (pkl_anal_ps_default),
  };



/* Every expression, array and struct node should be annotated with a
   type, and the type's completeness should have been determined.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal2_ps_checktype)
{
  pkl_ast_node node = PKL_PASS_NODE;
  pkl_ast_node type = PKL_AST_TYPE (node);

  if (type == NULL)
    {
      PKL_ICE (PKL_AST_LOC (node),
               "node #%" PRIu64 " has no type",
               PKL_AST_UID (node));
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }

  if (PKL_AST_TYPE_COMPLETE (type)
      == PKL_AST_TYPE_COMPLETE_UNKNOWN)
    {
      PKL_ICE (PKL_AST_LOC (type),
               "type completeness is unknown in node #%" PRIu64,
               PKL_AST_UID (node));
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* The magnitude in offset literals should be an integral expression.
   Also, it must have a type and its completeness should be known.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal2_ps_offset)
{
  pkl_ast_node node = PKL_PASS_NODE;
  pkl_ast_node magnitude = PKL_AST_OFFSET_MAGNITUDE (node);
  pkl_ast_node magnitude_type = PKL_AST_TYPE (magnitude);
  pkl_ast_node type = PKL_AST_TYPE (node);

  if (PKL_AST_TYPE_CODE (magnitude_type)
      != PKL_TYPE_INTEGRAL)
    {
      PKL_ERROR (PKL_AST_LOC (magnitude),
                 "expected integer expression in offset");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }

  if (type == NULL)
    {
      PKL_ICE (PKL_AST_LOC (node),
               "node #% " PRIu64 " has no type",
               PKL_AST_UID (node));
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }

  if (PKL_AST_TYPE_COMPLETE (type)
      == PKL_AST_TYPE_COMPLETE_UNKNOWN)
    {
      PKL_ICE (PKL_AST_LOC (type),
               "type completeness is unknown in node #%" PRIu64,
               PKL_AST_UID (node));
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* A return statement returning a value is not allowed in a void
   function.  Also, an expressionless return statement is invalid in a
   non-void function.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal2_ps_return_stmt)
{
  pkl_ast_node return_stmt = PKL_PASS_NODE;
  pkl_ast_node exp = PKL_AST_RETURN_STMT_EXP (return_stmt);
  pkl_ast_node function = PKL_AST_RETURN_STMT_FUNCTION (return_stmt);

  if (exp
      && PKL_AST_TYPE_CODE (PKL_AST_FUNC_RET_TYPE (function)) == PKL_TYPE_VOID)
    {
      PKL_ERROR (PKL_AST_LOC (exp),
                 "returning a value in a void function");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
  else if (!exp
           && PKL_AST_TYPE_CODE (PKL_AST_FUNC_RET_TYPE (function)) != PKL_TYPE_VOID)
    {
      PKL_ERROR (PKL_AST_LOC (return_stmt),
                 "the function expects a return value");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* A funcall to a void function is only allowed in an "expression
   statement.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal2_ps_funcall)
{
  pkl_ast_node funcall = PKL_PASS_NODE;
  pkl_ast_node funcall_function = PKL_AST_FUNCALL_FUNCTION (funcall);
  pkl_ast_node function_type = PKL_AST_TYPE (funcall_function);

  if (PKL_AST_TYPE_F_RTYPE (function_type) == NULL
      && PKL_PASS_PARENT
      && PKL_AST_CODE (PKL_PASS_PARENT) != PKL_AST_EXP_STMT)
    {
      /* Note that this error is catched earlier in
         pkl_typify1_ps_funcall */
      PKL_ERROR (PKL_AST_LOC (funcall_function),
                 "call to void function in expression");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* Emit a warning when an endianness field annotation is useless due
   to lexical structure of the code.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal2_ps_struct_type_field)
{
  pkl_ast_node field = PKL_PASS_NODE;
  pkl_ast_node type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (field);

  if (PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (field) != PKL_AST_ENDIAN_DFL
      && (PKL_AST_TYPE_CODE (type) == PKL_TYPE_STRUCT
          || PKL_AST_TYPE_CODE (type) == PKL_TYPE_ARRAY)
      && PKL_AST_TYPE_NAME (type))
    {
      const char *msg = "useless endianness annotation in field";

      if (pkl_error_on_warning (PKL_PASS_COMPILER))
        {
          PKL_ERROR (PKL_AST_LOC (field), msg);
          PKL_ANAL_PAYLOAD->errors ++;
          PKL_PASS_ERROR;
        }
      else
        PKL_WARNING (PKL_AST_LOC (field), msg);
    }
}
PKL_PHASE_END_HANDLER

/* In unions, alternatives appearing after an alternative with no
   constraint expression, or a constant expression known to be true,
   are unreachable.  Also, if an union alternative has a constraint
   known to be false, it is never taken.  Warning about these two
   situations.

   Optional fields are not supported in unions.  */

PKL_PHASE_BEGIN_HANDLER (pkl_anal2_ps_type_struct)
{
  pkl_ast_node struct_type = PKL_PASS_NODE;
  pkl_ast_node struct_type_elems
    = PKL_AST_TYPE_S_ELEMS (struct_type);
  pkl_ast_node t;
  pkl_ast_node last_unconditional_alternative = NULL;

  if (!PKL_AST_TYPE_S_UNION_P (struct_type))
    PKL_PASS_DONE;

  for (t = struct_type_elems; t; t = PKL_AST_CHAIN (t))
    {
      /* Process only struct type fields.  */
      if (PKL_AST_CODE (t) == PKL_AST_STRUCT_TYPE_FIELD)
        {
          pkl_ast_node constraint = PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (t);
          pkl_ast_node optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (t);
          pkl_ast_node elem_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (t);

          if (optcond)
            {
              PKL_ERROR (PKL_AST_LOC (t),
                         "optional fields are not allowed in unions");
              PKL_ANAL_PAYLOAD->errors++;
              PKL_PASS_ERROR;
            }

          if (last_unconditional_alternative)
            {
              const char *msg = "unreachable alternative in union";

              if (pkl_error_on_warning (PKL_PASS_COMPILER))
                {
                  PKL_ERROR (PKL_AST_LOC (t), msg);
                  PKL_ANAL_PAYLOAD->errors ++;
                  PKL_PASS_ERROR;
                }
              else
                PKL_WARNING (PKL_AST_LOC (t), msg);
              break;
            }

          if (!constraint
              && PKL_AST_TYPE_CODE (elem_type) != PKL_TYPE_STRUCT)
            last_unconditional_alternative = t;

          if (constraint
              && (PKL_AST_CODE (constraint) == PKL_AST_INTEGER
                  && PKL_AST_INTEGER_VALUE (constraint) != 0))
            last_unconditional_alternative = t;

          if (constraint
              && PKL_AST_CODE (constraint) == PKL_AST_INTEGER
              && PKL_AST_INTEGER_VALUE (constraint) == 0)
            {
              const char *msg = "unreachable alternative in union";

              if (pkl_error_on_warning (PKL_PASS_COMPILER))
                {
                  PKL_ERROR (PKL_AST_LOC (t), msg);
                  PKL_ANAL_PAYLOAD->errors ++;
                  PKL_PASS_ERROR;
                }
              else
                PKL_WARNING (PKL_AST_LOC (t), msg);
              break;
            }
        }
    }
}
PKL_PHASE_END_HANDLER

struct pkl_phase pkl_phase_anal2 =
  {
   PKL_PHASE_PS_HANDLER (PKL_AST_SRC, pkl_anal_ps_src),
   PKL_PHASE_PR_HANDLER (PKL_AST_PROGRAM, pkl_anal_pr_program),
   PKL_PHASE_PS_HANDLER (PKL_AST_PROGRAM, pkl_anal_ps_program),
   PKL_PHASE_PS_HANDLER (PKL_AST_EXP, pkl_anal2_ps_checktype),
   PKL_PHASE_PS_HANDLER (PKL_AST_ARRAY, pkl_anal2_ps_checktype),
   PKL_PHASE_PS_HANDLER (PKL_AST_STRUCT, pkl_anal2_ps_checktype),
   PKL_PHASE_PS_HANDLER (PKL_AST_OFFSET, pkl_anal2_ps_offset),
   PKL_PHASE_PS_HANDLER (PKL_AST_RETURN_STMT, pkl_anal2_ps_return_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_FUNCALL, pkl_anal2_ps_funcall),
   PKL_PHASE_PS_HANDLER (PKL_AST_STRUCT_TYPE_FIELD, pkl_anal2_ps_struct_type_field),
   PKL_PHASE_PS_TYPE_HANDLER (PKL_TYPE_STRUCT, pkl_anal2_ps_type_struct),
   PKL_PHASE_PS_DEFAULT_HANDLER (pkl_anal_ps_default),
  };



/* Make sure that every array initializer features an index at this
   point.  */

PKL_PHASE_BEGIN_HANDLER (pkl_analf_ps_array_initializer)
{
  if (!PKL_AST_ARRAY_INITIALIZER_INDEX (PKL_PASS_NODE))
    {
      PKL_ICE (PKL_AST_NOLOC,
               "array initializer node #%" PRIu64 " has no index",
               PKL_AST_UID (PKL_PASS_NODE));
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* Make sure that the left-hand of an assignment expression is of the
   right kind.  */

PKL_PHASE_BEGIN_HANDLER (pkl_analf_ps_ass_stmt)
{
  pkl_ast_node ass_stmt = PKL_PASS_NODE;
  pkl_ast_node ass_stmt_lvalue = PKL_AST_ASS_STMT_LVALUE (ass_stmt);

  if (!pkl_ast_lvalue_p (ass_stmt_lvalue))
    {
      PKL_ERROR (PKL_AST_LOC (ass_stmt_lvalue),
                 "invalid l-value in assignment");
      PKL_ANAL_PAYLOAD->errors++;
      PKL_PASS_ERROR;
    }
}
PKL_PHASE_END_HANDLER

/* Make sure that the argument to an incrdecr operator is of the right
   kind, i.e. it is a valid lvalue.  */

PKL_PHASE_BEGIN_HANDLER (pkl_analf_ps_incrdecr)
{
  pkl_ast_node incrdecr = PKL_PASS_NODE;
  pkl_ast_node incrdecr_exp = PKL_AST_INCRDECR_EXP (incrdecr);

  if (!pkl_ast_lvalue_p (incrdecr_exp))
    {
      int incrdecr_order = PKL_AST_INCRDECR_ORDER (incrdecr);
      int incrdecr_sign = PKL_AST_INCRDECR_SIGN (incrdecr);

      if (!pkl_ast_lvalue_p (incrdecr_exp))
        {
          /* Note that this is already checked in ass_stmt.  */
          PKL_ERROR (PKL_AST_LOC (incrdecr_exp),
                     "invalid operand to %s%s",
                     incrdecr_order == PKL_AST_ORDER_PRE ? "pre" : "post",
                     incrdecr_sign == PKL_AST_SIGN_INCR ? "increment" : "decrement");
          PKL_ANAL_PAYLOAD->errors++;
          PKL_PASS_ERROR;
        }
    }
}
PKL_PHASE_END_HANDLER

struct pkl_phase pkl_phase_analf =
  {
   PKL_PHASE_PS_HANDLER (PKL_AST_SRC, pkl_anal_ps_src),
   PKL_PHASE_PR_HANDLER (PKL_AST_PROGRAM, pkl_anal_pr_program),
   PKL_PHASE_PS_HANDLER (PKL_AST_PROGRAM, pkl_anal_ps_program),
   PKL_PHASE_PS_HANDLER (PKL_AST_OFFSET, pkl_analf_ps_array_initializer),
   PKL_PHASE_PS_HANDLER (PKL_AST_ASS_STMT, pkl_analf_ps_ass_stmt),
   PKL_PHASE_PS_HANDLER (PKL_AST_INCRDECR, pkl_analf_ps_incrdecr),
  };
