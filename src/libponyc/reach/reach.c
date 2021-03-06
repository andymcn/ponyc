#include "reach.h"
#include "../codegen/genname.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../../libponyrt/ds/stack.h"
#include "../../libponyrt/mem/pool.h"
#include <stdio.h>
#include <assert.h>

DECLARE_STACK(reachable_method_stack, reachable_method_t);
DEFINE_STACK(reachable_method_stack, reachable_method_t);

static reachable_type_t* add_type(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* type);

static void reachable_method(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* type, const char* name, ast_t* typeargs);

static void reachable_expr(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* ast);

static uint64_t reachable_method_hash(reachable_method_t* m)
{
  return hash_ptr(m->name);
}

static bool reachable_method_cmp(reachable_method_t* a, reachable_method_t* b)
{
  return a->name == b->name;
}

static void reachable_method_free(reachable_method_t* m)
{
  ast_free(m->typeargs);
  ast_free(m->r_fun);
  POOL_FREE(reachable_method_t, m);
}

DEFINE_HASHMAP(reachable_methods, reachable_method_t, reachable_method_hash,
  reachable_method_cmp, pool_alloc_size, pool_free_size, reachable_method_free
  );

static uint64_t reachable_method_name_hash(reachable_method_name_t* m)
{
  return hash_ptr(m->name);
}

static bool reachable_method_name_cmp(reachable_method_name_t* a,
  reachable_method_name_t* b)
{
  return a->name == b->name;
}

static void reachable_method_name_free(reachable_method_name_t* m)
{
  reachable_methods_destroy(&m->r_methods);
  POOL_FREE(reachable_method_name_t, m);
}

DEFINE_HASHMAP(reachable_method_names, reachable_method_name_t,
  reachable_method_name_hash, reachable_method_name_cmp, pool_alloc_size,
  pool_free_size, reachable_method_name_free);

static uint64_t reachable_type_hash(reachable_type_t* t)
{
  return hash_ptr(t->name);
}

static bool reachable_type_cmp(reachable_type_t* a, reachable_type_t* b)
{
  return a->name == b->name;
}

static void reachable_type_free(reachable_type_t* t)
{
  ast_free(t->type);
  reachable_method_names_destroy(&t->methods);
  reachable_type_cache_destroy(&t->subtypes);
  POOL_FREE(reachable_type_t, t);
}

DEFINE_HASHMAP(reachable_types, reachable_type_t, reachable_type_hash,
  reachable_type_cmp, pool_alloc_size, pool_free_size, reachable_type_free);

DEFINE_HASHMAP(reachable_type_cache, reachable_type_t, reachable_type_hash,
  reachable_type_cmp, pool_alloc_size, pool_free_size, NULL);

static void add_rmethod(reachable_method_stack_t** s,
  reachable_type_t* t, reachable_method_name_t* n, ast_t* typeargs)
{
  const char* name = genname_fun(NULL, n->name, typeargs);
  reachable_method_t* m = reach_method(n, name);

  if(m == NULL)
  {
    m = POOL_ALLOC(reachable_method_t);
    m->name = name;
    m->typeargs = ast_dup(typeargs);
    m->vtable_index = (uint32_t)-1;

    ast_t* fun = lookup(NULL, NULL, t->type, n->name);

    if(typeargs != NULL)
    {
      // Reify the method with its typeargs, if it has any.
      AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error,
        body);

      ast_t* r_fun = reify(fun, fun, typeparams, typeargs);
      ast_free_unattached(fun);
      fun = r_fun;
    }

    m->r_fun = ast_dup(fun);
    ast_free_unattached(fun);

    reachable_methods_put(&n->r_methods, m);

    // Put on a stack of reachable methods to trace.
    *s = reachable_method_stack_push(*s, m);
  }
}

static void add_method(reachable_method_stack_t** s,
  reachable_type_t* t, const char* name, ast_t* typeargs)
{
  reachable_method_name_t* n = reach_method_name(t, name);

  if(n == NULL)
  {
    n = POOL_ALLOC(reachable_method_name_t);
    n->name = name;
    reachable_methods_init(&n->r_methods, 0);
    reachable_method_names_put(&t->methods, n);
  }

  add_rmethod(s, t, n, typeargs);

  // Add to subtypes if we're an interface or trait.
  ast_t* def = (ast_t*)ast_data(t->type);

  switch(ast_id(def))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
    {
      size_t i = HASHMAP_BEGIN;
      reachable_type_t* t2;

      while((t2 = reachable_type_cache_next(&t->subtypes, &i)) != NULL)
        add_method(s, t2, name, typeargs);

      break;
    }

    default: {}
  }
}

static void add_methods_to_type(reachable_method_stack_t** s,
  reachable_type_t* from, reachable_type_t* to)
{
  size_t i = HASHMAP_BEGIN;
  reachable_method_name_t* n;

  while((n = reachable_method_names_next(&from->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reachable_method_t* m;

    while((m = reachable_methods_next(&n->r_methods, &j)) != NULL)
      add_method(s, to, n->name, m->typeargs);
  }
}

static void add_types_to_trait(reachable_method_stack_t** s,
  reachable_types_t* r, reachable_type_t* t)
{
  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t2;

  while((t2 = reachable_types_next(r, &i)) != NULL)
  {
    if(ast_id(t2->type) == TK_TUPLETYPE)
      continue;

    ast_t* def = (ast_t*)ast_data(t2->type);

    switch(ast_id(def))
    {
      case TK_PRIMITIVE:
      case TK_CLASS:
      case TK_ACTOR:
        if(is_subtype(t2->type, t->type))
        {
          reachable_type_cache_put(&t->subtypes, t2);
          reachable_type_cache_put(&t2->subtypes, t);
          add_methods_to_type(s, t, t2);
        }
        break;

      default: {}
    }
  }
}

static void add_traits_to_type(reachable_method_stack_t** s,
  reachable_types_t* r, reachable_type_t* t)
{
  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t2;

  while((t2 = reachable_types_next(r, &i)) != NULL)
  {
    if(ast_id(t2->type) == TK_TUPLETYPE)
      continue;

    ast_t* def = (ast_t*)ast_data(t2->type);

    switch(ast_id(def))
    {
      case TK_INTERFACE:
      case TK_TRAIT:
        if(is_subtype(t->type, t2->type))
        {
          reachable_type_cache_put(&t->subtypes, t2);
          reachable_type_cache_put(&t2->subtypes, t);
          add_methods_to_type(s, t2, t);
        }
        break;

      default: {}
    }
  }
}

static void add_special(reachable_method_stack_t** s, reachable_type_t* t,
  ast_t* type, const char* special)
{
  special = stringtab(special);
  ast_t* find = lookup_try(NULL, NULL, type, special);

  if(find != NULL)
  {
    add_method(s, t, special, NULL);
    ast_free_unattached(find);
  }
}

static reachable_type_t* add_reachable_type(reachable_types_t* r, ast_t* type,
  const char* type_name)
{
  reachable_type_t* t = POOL_ALLOC(reachable_type_t);
  t->name = type_name;

  t->type = set_cap_and_ephemeral(type, TK_REF, TK_NONE);
  reachable_method_names_init(&t->methods, 0);
  reachable_type_cache_init(&t->subtypes, 0);
  t->vtable_size = 0;
  reachable_types_put(r, t);

  return t;
}

static reachable_type_t* add_isect_or_union(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* type)
{
  ast_t* child = ast_child(type);

  while(child != NULL)
  {
    add_type(s, r, child);
    child = ast_sibling(child);
  }

  return NULL;
}

static reachable_type_t* add_tuple(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* type)
{
  const char* type_name = genname_type(type);
  reachable_type_t* t = reach_type(r, type_name);

  if(t != NULL)
    return t;

  t = add_reachable_type(r, type, type_name);

  ast_t* child = ast_child(type);

  while(child != NULL)
  {
    add_type(s, r, child);
    child = ast_sibling(child);
  }

  return t;
}

static reachable_type_t* add_nominal(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* type)
{
  const char* type_name = genname_type(type);
  reachable_type_t* t = reach_type(r, type_name);

  if(t != NULL)
    return t;

  t = add_reachable_type(r, type, type_name);

  AST_GET_CHILDREN(type, pkg, id, typeparams);
  ast_t* typeparam = ast_child(typeparams);

  while(typeparam != NULL)
  {
    add_type(s, r, typeparam);
    typeparam = ast_sibling(typeparam);
  }

  ast_t* def = (ast_t*)ast_data(type);

  switch(ast_id(def))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      add_types_to_trait(s, r, t);
      break;

    case TK_PRIMITIVE:
      add_traits_to_type(s, r, t);
      add_special(s, t, type, "_init");
      add_special(s, t, type, "_final");
      break;

    case TK_STRUCT:
    case TK_CLASS:
      add_traits_to_type(s, r, t);
      add_special(s, t, type, "_final");
      break;

    case TK_ACTOR:
      add_traits_to_type(s, r, t);
      add_special(s, t, type, "_event_notify");
      add_special(s, t, type, "_final");
      break;

    default: {}
  }

  return t;
}

static reachable_type_t* add_type(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      return add_isect_or_union(s, r, type);

    case TK_TUPLETYPE:
      return add_tuple(s, r, type);

    case TK_NOMINAL:
      return add_nominal(s, r, type);

    default:
      assert(0);
  }

  return NULL;
}

static void reachable_pattern(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_DONTCARE:
    case TK_NONE:
      break;

    case TK_VAR:
    case TK_LET:
    {
      AST_GET_CHILDREN(ast, idseq, type);
      add_type(s, r, type);
      break;
    }

    case TK_TUPLE:
    case TK_SEQ:
    {
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        reachable_pattern(s, r, child);
        child = ast_sibling(child);
      }
      break;
    }

    default:
    {
      reachable_method(s, r, ast_type(ast), stringtab("eq"), NULL);
      reachable_expr(s, r, ast);
      break;
    }
  }
}

static void reachable_fun(reachable_method_stack_t** s, reachable_types_t* r,
  ast_t* ast)
{
  AST_GET_CHILDREN(ast, receiver, method);
  ast_t* typeargs = NULL;

  // Dig through function qualification.
  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
      typeargs = method;
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  ast_t* type = ast_type(receiver);
  const char* method_name = ast_name(method);

  reachable_method(s, r, type, method_name, typeargs);
}

static void reachable_addressof(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* ast)
{
  ast_t* expr = ast_child(ast);

  switch(ast_id(expr))
  {
    case TK_FUNREF:
    case TK_BEREF:
      reachable_fun(s, r, expr);
      break;

    default: {}
  }
}

static void reachable_call(reachable_method_stack_t** s, reachable_types_t* r,
  ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);
  reachable_fun(s, r, postfix);
}

static void reachable_ffi(reachable_method_stack_t** s, reachable_types_t* r,
  ast_t* ast)
{
  AST_GET_CHILDREN(ast, name, return_typeargs, args, namedargs, question);
  ast_t* decl = ast_get(ast, ast_name(name), NULL);

  if(decl != NULL)
  {
    AST_GET_CHILDREN(decl, decl_name, decl_ret_typeargs, params, named_params,
      decl_error);

    return_typeargs = decl_ret_typeargs;
  }

  ast_t* return_type = ast_child(return_typeargs);
  add_type(s, r, return_type);
}

static void reachable_expr(reachable_method_stack_t** s, reachable_types_t* r,
  ast_t* ast)
{
  // If this is a method call, mark the method as reachable.
  switch(ast_id(ast))
  {
    case TK_TRUE:
    case TK_FALSE:
    case TK_INT:
    case TK_FLOAT:
    case TK_STRING:
    {
      ast_t* type = ast_type(ast);

      if(type != NULL)
        reachable_method(s, r, type, stringtab("create"), NULL);
      break;
    }

    case TK_CASE:
    {
      AST_GET_CHILDREN(ast, pattern, guard, body);
      reachable_pattern(s, r, pattern);
      reachable_expr(s, r, guard);
      reachable_expr(s, r, body);
      break;
    }

    case TK_CALL:
      reachable_call(s, r, ast);
      break;

    case TK_FFICALL:
      reachable_ffi(s, r, ast);
      break;

    case TK_ADDRESS:
      reachable_addressof(s, r, ast);
      break;

    default: {}
  }

  // Traverse all child expressions looking for calls.
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    reachable_expr(s, r, child);
    child = ast_sibling(child);
  }
}

static void reachable_body(reachable_method_stack_t** s, reachable_types_t* r,
  ast_t* fun)
{
  AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error, body);
  reachable_expr(s, r, body);
}

static void reachable_method(reachable_method_stack_t** s,
  reachable_types_t* r, ast_t* type, const char* name, ast_t* typeargs)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
    {
      reachable_type_t* t = add_type(s, r, type);
      add_method(s, t, name, typeargs);
      break;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_t* find = lookup_try(NULL, NULL, child, name);

        if(find != NULL)
          reachable_method(s, r, child, name, typeargs);

        child = ast_sibling(child);
      }

      break;
    }

    default:
      assert(0);
  }
}

static void handle_stack(reachable_method_stack_t* s, reachable_types_t* r)
{
  while(s != NULL)
  {
    reachable_method_t* m;
    s = reachable_method_stack_pop(s, &m);
    reachable_body(&s, r, m->r_fun);
  }
}

reachable_types_t* reach_new()
{
  reachable_types_t* r = POOL_ALLOC(reachable_types_t);
  reachable_types_init(r, 64);
  return r;
}

void reach_primitives(reachable_types_t* r, pass_opt_t* opt, ast_t* from)
{
  reachable_method_stack_t* s = NULL;

  add_type(&s, r, type_builtin(opt, from, "Bool"));
  add_type(&s, r, type_builtin(opt, from, "I8"));
  add_type(&s, r, type_builtin(opt, from, "I16"));
  add_type(&s, r, type_builtin(opt, from, "I32"));
  add_type(&s, r, type_builtin(opt, from, "I64"));
  add_type(&s, r, type_builtin(opt, from, "I128"));
  add_type(&s, r, type_builtin(opt, from, "U8"));
  add_type(&s, r, type_builtin(opt, from, "U16"));
  add_type(&s, r, type_builtin(opt, from, "U32"));
  add_type(&s, r, type_builtin(opt, from, "U64"));
  add_type(&s, r, type_builtin(opt, from, "U128"));
  add_type(&s, r, type_builtin(opt, from, "F32"));
  add_type(&s, r, type_builtin(opt, from, "F64"));

  handle_stack(s, r);
}

void reach_free(reachable_types_t* r)
{
  if(r == NULL)
    return;

  reachable_types_destroy(r);
  POOL_FREE(reachable_types_t, r);
}

void reach(reachable_types_t* r, ast_t* type, const char* name,
  ast_t* typeargs)
{
  reachable_method_stack_t* s = NULL;
  reachable_method(&s, r, type, name, typeargs);
  handle_stack(s, r);
}

reachable_type_t* reach_type(reachable_types_t* r, const char* name)
{
  reachable_type_t k;
  k.name = name;
  return reachable_types_get(r, &k);
}

reachable_method_name_t* reach_method_name(reachable_type_t* t,
  const char* name)
{
  reachable_method_name_t k;
  k.name = name;
  return reachable_method_names_get(&t->methods, &k);
}

reachable_method_t* reach_method(reachable_method_name_t* n,
  const char* name)
{
  reachable_method_t k;
  k.name = name;
  return reachable_methods_get(&n->r_methods, &k);
}

size_t reach_method_count(reachable_type_t* t)
{
  size_t i = HASHMAP_BEGIN;
  reachable_method_name_t* n;
  size_t count = 0;

  while((n = reachable_method_names_next(&t->methods, &i)) != NULL)
    count += reachable_methods_size(&n->r_methods);

  return count;
}

void reach_dump(reachable_types_t* r)
{
  printf("REACH\n");

  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t;

  while((t = reachable_types_next(r, &i)) != NULL)
  {
    printf("  %s vtable size %d\n", t->name, t->vtable_size);
    size_t j = HASHMAP_BEGIN;
    reachable_method_name_t* m;

    while((m = reachable_method_names_next(&t->methods, &j)) != NULL)
    {
      size_t k = HASHMAP_BEGIN;
      reachable_method_t* p;

      while((p = reachable_methods_next(&m->r_methods, &k)) != NULL)
      {
        printf("    %s vtable index %d (%p)\n", p->name, p->vtable_index, p);
      }
    }
  }
}
