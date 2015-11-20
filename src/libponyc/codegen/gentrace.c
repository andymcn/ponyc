#include "gentrace.h"
#include "gencall.h"
#include "gendesc.h"
#include "genname.h"
#include "genprim.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include <assert.h>

static bool trace_as_tag(compile_t* c, LLVMValueRef value, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!trace_as_tag(c, value, child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_TUPLETYPE:
      return false;

    case TK_NOMINAL:
      return cap_single(type) == TK_TAG;

    default: {}
  }

  assert(0);
  return false;
}

static void trace_tag(compile_t* c, LLVMValueRef ctx, LLVMValueRef value)
{
  // Cast the value to a void pointer.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, value, c->void_ptr, "");

  gencall_runtime(c, "pony_trace", args, 2, "");
}

static void trace_tag_or_actor(compile_t* c, LLVMValueRef ctx,
  LLVMValueRef value)
{
  // We're an object.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = value;
  gencall_runtime(c, "pony_trace_tag_or_actor", args, 2, "");
}

static void trace_actor(compile_t* c, LLVMValueRef ctx, LLVMValueRef value)
{
  // Cast the value to an object pointer.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "");

  gencall_runtime(c, "pony_traceactor", args, 2, "");
}

static void trace_maybe(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* type, bool tag)
{
  ast_t* type_args = ast_childidx(type, 2);
  ast_t* elem = ast_child(type_args);

  if(is_machine_word(elem))
    return;

  LLVMValueRef test = genprim_maybe_is_null(c, elem, value);
  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  LLVMPositionBuilderAtEnd(c->builder, is_false);

  if(tag)
    trace_tag(c, ctx, value);
  else
    gentrace(c, ctx, value, elem);

  LLVMBuildBr(c->builder, is_true);
  LLVMPositionBuilderAtEnd(c->builder, is_true);
}

static bool trace_known(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* type)
{
  gentype_t g;

  if(!gentype(c, type, &g))
    return false;

  // Get the trace function statically.
  const char* fun = genname_trace(g.type_name);
  LLVMValueRef trace_fn = LLVMGetNamedFunction(c->module, fun);

  // If this type has no trace function, don't try to recurse in the runtime.
  if(trace_fn != NULL)
  {
    // Cast the value to an object pointer.
    LLVMValueRef args[3];
    args[0] = ctx;
    args[1] = LLVMBuildBitCast(c->builder, value, c->object_ptr, "");
    args[2] = trace_fn;

    gencall_runtime(c, "pony_traceobject", args, 3, "");
  } else {
    // Cast the value to a void pointer.
    LLVMValueRef args[2];
    args[0] = ctx;
    args[1] = LLVMBuildBitCast(c->builder, value, c->void_ptr, "");
    gencall_runtime(c, "pony_trace", args, 2, "");
  }

  return true;
}

static void trace_unknown(compile_t* c, LLVMValueRef ctx, LLVMValueRef value)
{
  // We're an object.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = value;

  gencall_runtime(c, "pony_traceunknown", args, 2, "");
}

static bool trace_tuple(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* type)
{
  // Invoke the trace function directly. Do not trace the address of the tuple.
  const char* type_name = genname_type(type);
  const char* trace_name = genname_tracetuple(type_name);
  LLVMValueRef trace_fn = LLVMGetNamedFunction(c->module, trace_name);

  // There will be no trace function if the tuple doesn't need tracing.
  if(trace_fn == NULL)
    return false;

  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = value;

  LLVMBuildCall(c->builder, trace_fn, args, 2, "");
  return true;
}

bool gentrace(compile_t* c, LLVMValueRef ctx, LLVMValueRef value, ast_t* type)
{
  bool tag = trace_as_tag(c, value, type);

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      if(tag)
        trace_tag_or_actor(c, ctx, value);
      else
        trace_unknown(c, ctx, value);

      return true;
    }

    case TK_TUPLETYPE:
      return trace_tuple(c, ctx, value, type);

    case TK_NOMINAL:
    {
      switch(ast_id((ast_t*)ast_data(type)))
      {
        case TK_INTERFACE:
        case TK_TRAIT:
          if(tag)
            trace_tag_or_actor(c, ctx, value);
          else
            trace_unknown(c, ctx, value);

          return true;

        case TK_PRIMITIVE:
          // Do nothing.
          return false;

        case TK_STRUCT:
        case TK_CLASS:
          if(tag)
          {
            if(is_maybe(type))
              trace_maybe(c, ctx, value, type, true);
            else
              trace_tag(c, ctx, value);

            return true;
          }

          if(is_maybe(type))
          {
            trace_maybe(c, ctx, value, type, false);
            return true;
          }

          return trace_known(c, ctx, value, type);

        case TK_ACTOR:
          trace_actor(c, ctx, value);
          return true;

        default: {}
      }
      break;
    }

    default: {}
  }

  assert(0);
  return false;
}
