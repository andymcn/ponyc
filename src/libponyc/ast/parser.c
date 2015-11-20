#include "parserapi.h"
#include "parser.h"
#include <stdio.h>

/* This file contains the definition of  the Pony parser, built from macros
 * defined in parserapi.h.
 *
 * Any normal C functions defined here must be within a preprocessor block:
 * #ifdef PARSER
 * ...
 * #endif
 */


// Forward declarations
DECL(type);
DECL(rawseq);
DECL(seq);
DECL(exprseq);
DECL(nextexprseq);
DECL(assignment);
DECL(term);
DECL(infix);
DECL(idseq);
DECL(members);


/* Precedence
 *
 * We do not support precedence of infix operators, since that leads to many
 * bugs. Instead we require the use of parentheses to disambiguiate operator
 * interactions. This is checked in the parse fix pass, so we treat all infix
 * operators as having the same precedence during parsing.
 *
 * Overall, the precedences built into the below grammar are as follows.
 *
 * Value operators:
 *  postfix (eg . call) - highest precedence, most tightly binding
 *  prefix (eg not consume)
 *  infix (eg + <<)
 *  assignment (=) - right associative
 *  sequence (consecutive expressions)
 *  tuple elements (,) - lowest precedence
 *
 * Type operators:
 *  viewpoint (->) - right associative, highest precedence
 *  infix (& |)
 *  tuple elements (,) - lowest precedence
 */


// Parse rules

// type
DEF(provides);
  PRINT_INLINE();
  AST_NODE(TK_PROVIDES);
  RULE("provided type", type);
  DONE();

// ID COLON type [ASSIGN infix]
DEF(param);
  AST_NODE(TK_PARAM);
  TOKEN("name", TK_ID);
  SKIP(NULL, TK_COLON);
  RULE("parameter type", type);
  IF(TK_ASSIGN, RULE("default value", infix));
  DONE();

// ELLIPSIS
DEF(ellipsis);
  TOKEN(NULL, TK_ELLIPSIS);
  DONE();

// ID [COLON type] [ASSIGN type]
DEF(typeparam);
  AST_NODE(TK_TYPEPARAM);
  TOKEN("name", TK_ID);
  IF(TK_COLON, RULE("type constraint", type));
  IF(TK_ASSIGN, RULE("default type", type));
  DONE();

// param {COMMA param}
DEF(params);
  AST_NODE(TK_PARAMS);
  RULE("parameter", param, ellipsis);
  WHILE(TK_COMMA, RULE("parameter", param, ellipsis));
  DONE();

// LSQUARE typeparam {COMMA typeparam} RSQUARE
DEF(typeparams);
  AST_NODE(TK_TYPEPARAMS);
  SKIP(NULL, TK_LSQUARE, TK_LSQUARE_NEW);
  RULE("type parameter", typeparam);
  WHILE(TK_COMMA, RULE("type parameter", typeparam));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// LSQUARE type {COMMA type} RSQUARE
DEF(typeargs);
  AST_NODE(TK_TYPEARGS);
  SKIP(NULL, TK_LSQUARE);
  RULE("type argument", type);
  WHILE(TK_COMMA, RULE("type argument", type));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// CAP
DEF(cap);
  TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  DONE();

  // GENCAP
DEF(gencap);
  TOKEN("generic capability", TK_CAP_READ, TK_CAP_SEND, TK_CAP_SHARE,
    TK_CAP_ANY);
  DONE();

// ID [DOT ID] [typeargs] [CAP] [EPHEMERAL | BORROWED]
DEF(nominal);
  AST_NODE(TK_NOMINAL);
  TOKEN("name", TK_ID);
  IFELSE(TK_DOT,
    TOKEN("name", TK_ID),
    AST_NODE(TK_NONE);
    REORDER(1, 0);
  );
  OPT RULE("type arguments", typeargs);
  OPT RULE("capability", cap, gencap);
  OPT TOKEN(NULL, TK_EPHEMERAL, TK_BORROWED);
  DONE();

// PIPE type
DEF(uniontype);
  INFIX_BUILD();
  AST_NODE(TK_UNIONTYPE);
  SKIP(NULL, TK_PIPE);
  RULE("type", type);
  DONE();

// AMP type
DEF(isecttype);
  INFIX_BUILD();
  TOKEN(NULL, TK_ISECTTYPE);
  RULE("type", type);
  DONE();

// type {uniontype | isecttype}
DEF(infixtype);
  RULE("type", type);
  SEQ("type", uniontype, isecttype);
  DONE();

// DONTCARE
DEF(dontcare);
  TOKEN(NULL, TK_DONTCARE);
  DONE();

// COMMA (infixtype | dontcare) {COMMA (infixtype | dontcare)}
DEF(tupletype);
  INFIX_BUILD();
  TOKEN(NULL, TK_COMMA);
  MAP_ID(TK_COMMA, TK_TUPLETYPE);
  RULE("type", infixtype, dontcare);
  WHILE(TK_COMMA, RULE("type", infixtype, dontcare));
  DONE();

// (LPAREN | LPAREN_NEW) (infixtype | dontcare) [tupletype] RPAREN
DEF(groupedtype);
  PRINT_INLINE();
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("type", infixtype, dontcare);
  OPT_NO_DFLT RULE("type", tupletype);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(AST_FLAG_IN_PARENS);
  DONE();

// THIS
DEF(thistype);
  PRINT_INLINE();
  AST_NODE(TK_THISTYPE);
  SKIP(NULL, TK_THIS);
  DONE();

// BOX
DEF(boxtype);
  PRINT_INLINE();
  AST_NODE(TK_BOXTYPE);
  SKIP(NULL, TK_BOX);
  DONE();

// (thistype | boxtype | typeexpr | nominal)
DEF(atomtype);
  RULE("type", thistype, boxtype, groupedtype, nominal);
  DONE();

// ARROW type
DEF(viewpoint);
  PRINT_INLINE();
  INFIX_BUILD();
  TOKEN(NULL, TK_ARROW);
  RULE("viewpoint", type);
  DONE();

// atomtype [viewpoint]
DEF(type);
  RULE("type", atomtype);
  OPT_NO_DFLT RULE("viewpoint", viewpoint);
  DONE();

// ID [$updatearg] ASSIGN rawseq
DEF(namedarg);
  AST_NODE(TK_NAMEDARG);
  TOKEN("argument name", TK_ID);
  IFELSE(TK_TEST_UPDATEARG,
    MAP_ID(TK_NAMEDARG, TK_UPDATEARG); SET_FLAG(AST_FLAG_TEST_ONLY),
    {}
  );
  SKIP(NULL, TK_ASSIGN);
  RULE("argument value", rawseq);
  DONE();

// WHERE namedarg {COMMA namedarg}
DEF(named);
  AST_NODE(TK_NAMEDARGS);
  SKIP(NULL, TK_WHERE);
  RULE("named argument", namedarg);
  WHILE(TK_COMMA, RULE("named argument", namedarg));
  DONE();

// rawseq {COMMA rawseq}
DEF(positional);
  AST_NODE(TK_POSITIONALARGS);
  RULE("argument", rawseq);
  WHILE(TK_COMMA, RULE("argument", rawseq));
  DONE();

// OBJECT [CAP] [IS type] members END
DEF(object);
  PRINT_INLINE();
  TOKEN(NULL, TK_OBJECT);
  OPT RULE("capability", cap);
  IF(TK_IS, RULE("provided type", provides));
  RULE("object member", members);
  SKIP(NULL, TK_END);
  DONE();

// ID [COLON type] [ASSIGN infix]
DEF(lambdacapture);
  AST_NODE(TK_LAMBDACAPTURE);
  TOKEN("name", TK_ID);
  IF(TK_COLON, RULE("capture type", type));
  IF(TK_ASSIGN, RULE("capture value", infix));
  DONE();

// (LPAREN | LPAREN_NEW) lambdacapture {COMMA lambdacapture} RPAREN
DEF(lambdacaptures);
  AST_NODE(TK_LAMBDACAPTURES);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("capture", lambdacapture);
  WHILE(TK_COMMA, RULE("capture", lambdacapture));
  SKIP(NULL, TK_RPAREN);
  DONE();

// LAMBDA [CAP] [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN
// [lambdacaptures] [COLON type] [QUESTION] ARROW rawseq END
DEF(lambda);
  PRINT_INLINE();
  TOKEN(NULL, TK_LAMBDA);
  OPT RULE("capability", cap);
  OPT RULE("type parameters", typeparams);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("parameters", params);
  SKIP(NULL, TK_RPAREN);
  OPT RULE("captures", lambdacaptures);
  IF(TK_COLON, RULE("return type", type));
  OPT TOKEN(NULL, TK_QUESTION);
  SKIP(NULL, TK_DBLARROW);
  RULE("lambda body", rawseq);
  SKIP(NULL, TK_END);
  SET_CHILD_FLAG(1, AST_FLAG_PRESERVE); // Type parameters
  SET_CHILD_FLAG(2, AST_FLAG_PRESERVE); // Parameters
  SET_CHILD_FLAG(4, AST_FLAG_PRESERVE); // Return type
  SET_CHILD_FLAG(6, AST_FLAG_PRESERVE); // Body
  DONE();

// AS type ':'
DEF(arraytype);
  PRINT_INLINE();
  SKIP(NULL, TK_AS);
  RULE("type", type);
  SKIP(NULL, TK_COLON);
  DONE();

// (LSQUARE | LSQUARE_NEW) rawseq {COMMA rawseq} RSQUARE
DEF(array);
  PRINT_INLINE();
  AST_NODE(TK_ARRAY);
  SKIP(NULL, TK_LSQUARE, TK_LSQUARE_NEW);
  OPT RULE("element type", arraytype);
  RULE("array element", rawseq);
  WHILE(TK_COMMA, RULE("array element", rawseq));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// LSQUARE_NEW rawseq {COMMA rawseq} RSQUARE
DEF(nextarray);
  PRINT_INLINE();
  AST_NODE(TK_ARRAY);
  SKIP(NULL, TK_LSQUARE_NEW);
  OPT RULE("element type", arraytype);
  RULE("array element", rawseq);
  WHILE(TK_COMMA, RULE("array element", rawseq));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// COMMA (rawseq | dontcare) {COMMA (rawseq | dontcare)}
DEF(tuple);
  INFIX_BUILD();
  TOKEN(NULL, TK_COMMA);
  MAP_ID(TK_COMMA, TK_TUPLE);
  RULE("value", rawseq, dontcare);
  WHILE(TK_COMMA, RULE("value", rawseq, dontcare));
  DONE();

// (LPAREN | LPAREN_NEW) (rawseq | dontcare) [tuple] RPAREN
DEF(groupedexpr);
  PRINT_INLINE();
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("value", rawseq, dontcare);
  OPT_NO_DFLT RULE("value", tuple);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(AST_FLAG_IN_PARENS);
  DONE();

// LPAREN_NEW (rawseq | dontcare) [tuple] RPAREN
DEF(nextgroupedexpr);
  PRINT_INLINE();
  SKIP(NULL, TK_LPAREN_NEW);
  RULE("value", rawseq, dontcare);
  OPT_NO_DFLT RULE("value", tuple);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(AST_FLAG_IN_PARENS);
  DONE();

// THIS | TRUE | FALSE | INT | FLOAT | STRING
DEF(literal);
  TOKEN("literal", TK_THIS, TK_TRUE, TK_FALSE, TK_INT, TK_FLOAT, TK_STRING);
  DONE();

DEF(ref);
  PRINT_INLINE();
  AST_NODE(TK_REFERENCE);
  TOKEN("name", TK_ID);
  DONE();

// AT (ID | STRING) typeargs (LPAREN | LPAREN_NEW) [positional] RPAREN
// [QUESTION]
DEF(ffi);
  PRINT_INLINE();
  TOKEN(NULL, TK_AT);
  MAP_ID(TK_AT, TK_FFICALL);
  TOKEN("ffi name", TK_ID, TK_STRING);
  OPT RULE("return type", typeargs);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("ffi arguments", positional);
  OPT RULE("ffi arguments", named);
  SKIP(NULL, TK_RPAREN);
  OPT TOKEN(NULL, TK_QUESTION);
  DONE();

// ref | literal | tuple | array | object | lambda | ffi
DEF(atom);
  RULE("value", ref, literal, groupedexpr, array, object, lambda, ffi);
  DONE();

// ref | literal | tuple | array | object | lambda | ffi
DEF(nextatom);
  RULE("value", ref, literal, nextgroupedexpr, nextarray, object, lambda, ffi);
  DONE();

// DOT ID
DEF(dot);
  INFIX_BUILD();
  TOKEN(NULL, TK_DOT);
  TOKEN("member name", TK_ID);
  DONE();

// TILDE ID
DEF(tilde);
  INFIX_BUILD();
  TOKEN(NULL, TK_TILDE);
  TOKEN("method name", TK_ID);
  DONE();

// typeargs
DEF(qualify);
  INFIX_BUILD();
  AST_NODE(TK_QUALIFY);
  RULE("type arguments", typeargs);
  DONE();

// LPAREN [positional] [named] RPAREN
DEF(call);
  INFIX_REVERSE();
  AST_NODE(TK_CALL);
  SKIP(NULL, TK_LPAREN);
  OPT RULE("argument", positional);
  OPT RULE("argument", named);
  SKIP(NULL, TK_RPAREN);
  DONE();

// atom {dot | tilde | qualify | call}
DEF(postfix);
  RULE("value", atom);
  SEQ("postfix expression", dot, tilde, qualify, call);
  DONE();

// atom {dot | tilde | qualify | call}
DEF(nextpostfix);
  RULE("value", nextatom);
  SEQ("postfix expression", dot, tilde, qualify, call);
  DONE();

// idseq
DEF(idseq_in_seq);
  AST_NODE(TK_SEQ);
  RULE("variable name", idseq);
  DONE();

// (LPAREN | LPAREN_NEW) idseq {COMMA idseq} RPAREN
DEF(idseqmulti);
  PRINT_INLINE();
  AST_NODE(TK_TUPLE);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("variable name", idseq_in_seq);
  WHILE(TK_COMMA, RULE("variable name", idseq_in_seq));
  SKIP(NULL, TK_RPAREN);
  DONE();

// ID | '_'
DEF(idseqsingle);
  PRINT_INLINE();
  AST_NODE(TK_LET);
  TOKEN("variable name", TK_ID, TK_DONTCARE);
  AST_NODE(TK_NONE);  // Type
  DONE();

// ID | '_' | (LPAREN | LPAREN_NEW) idseq {COMMA idseq} RPAREN
DEF(idseq);
  RULE("variable name", idseqsingle, idseqmulti);
  DONE();

// (VAR | LET | EMBED) ID [COLON type]
DEF(local);
  PRINT_INLINE();
  TOKEN(NULL, TK_VAR, TK_LET, TK_EMBED);
  TOKEN("variable name", TK_ID);
  IF(TK_COLON, RULE("variable type", type));
  DONE();

// ELSE seq END
DEF(elseclause);
  PRINT_INLINE();
  SKIP(NULL, TK_ELSE);
  RULE("else value", seq);
  DONE();

// ELSEIF rawseq THEN seq [elseif | (ELSE seq)]
DEF(elseif);
  AST_NODE(TK_IF);
  SCOPE();
  SKIP(NULL, TK_ELSEIF);
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_THEN);
  RULE("then value", seq);
  OPT RULE("else clause", elseif, elseclause);
  DONE();

// IF rawseq THEN seq [elseif | (ELSE seq)] END
DEF(cond);
  PRINT_INLINE();
  TOKEN(NULL, TK_IF);
  SCOPE();
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_THEN);
  RULE("then value", seq);
  OPT RULE("else clause", elseif, elseclause);
  SKIP(NULL, TK_END);
  DONE();

// ELSEIF rawseq [$EXTRA rawseq] THEN seq [elseifdef | (ELSE seq)]
DEF(elseifdef);
  AST_NODE(TK_IFDEF);
  SCOPE();
  SKIP(NULL, TK_ELSEIF);
  RULE("condition expression", infix);
  IF(TK_TEST_EXTRA,
    RULE("else condition", infix);
    SET_FLAG(AST_FLAG_TEST_ONLY)
  );
  SKIP(NULL, TK_THEN);
  RULE("then value", seq);
  OPT RULE("else clause", elseifdef, elseclause);
  // Order should be:
  // condition then_clause else_clause else_condition
  REORDER(0, 2, 3, 1);
  DONE();

// IF rawseq [$EXTRA rawseq] THEN seq [elseifdef | (ELSE seq)] END
DEF(ifdef);
  PRINT_INLINE();
  TOKEN(NULL, TK_IFDEF);
  SCOPE();
  RULE("condition expression", infix);
  IF(TK_TEST_EXTRA,
    RULE("else condition", infix);
    SET_FLAG(AST_FLAG_TEST_ONLY)
  );
  SKIP(NULL, TK_THEN);
  RULE("then value", seq);
  OPT RULE("else clause", elseifdef, elseclause);
  SKIP(NULL, TK_END);
  // Order should be:
  // condition then_clause else_clause else_condition
  REORDER(0, 2, 3, 1);
  DONE();

// PIPE [infix] [WHERE rawseq] [ARROW rawseq]
DEF(caseexpr);
  AST_NODE(TK_CASE);
  SCOPE();
  SKIP(NULL, TK_PIPE);
  OPT RULE("case pattern", infix);
  IF(TK_WHERE, RULE("guard expression", rawseq));
  IF(TK_DBLARROW, RULE("case body", rawseq));
  DONE();

// {caseexpr}
DEF(cases);
  PRINT_INLINE();
  AST_NODE(TK_CASES);
  SCOPE();  // TODO: Why is cases a scope?
  SEQ("cases", caseexpr);
  DONE();

// MATCH rawseq cases [ELSE seq] END
DEF(match);
  PRINT_INLINE();
  TOKEN(NULL, TK_MATCH);
  SCOPE();
  RULE("match expression", rawseq);
  RULE("cases", cases);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// WHILE rawseq DO seq [ELSE seq] END
DEF(whileloop);
  PRINT_INLINE();
  TOKEN(NULL, TK_WHILE);
  SCOPE();
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_DO);
  RULE("while body", seq);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// REPEAT seq UNTIL seq [ELSE seq] END
DEF(repeat);
  PRINT_INLINE();
  TOKEN(NULL, TK_REPEAT);
  SCOPE();
  RULE("repeat body", seq);
  SKIP(NULL, TK_UNTIL);
  RULE("condition expression", seq);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// FOR idseq IN rawseq DO rawseq [ELSE seq] END
// =>
// (SEQ
//   (ASSIGN (LET $1) iterator)
//   (WHILE $1.has_next()
//     (SEQ (ASSIGN idseq $1.next()) body) else))
DEF(forloop);
  PRINT_INLINE();
  TOKEN(NULL, TK_FOR);
  RULE("iterator name", idseq);
  SKIP(NULL, TK_IN);
  RULE("iterator", rawseq);
  SKIP(NULL, TK_DO);
  RULE("for body", rawseq);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// idseq = rawseq
DEF(withelem);
  AST_NODE(TK_SEQ);
  RULE("with name", idseq);
  SKIP(NULL, TK_ASSIGN);
  RULE("initialiser", rawseq);
  DONE();

// withelem {COMMA withelem}
DEF(withexpr);
  PRINT_INLINE();
  AST_NODE(TK_SEQ);
  RULE("with expression", withelem);
  WHILE(TK_COMMA, RULE("with expression", withelem));
  DONE();

// WITH withexpr DO rawseq [ELSE rawseq] END
// =>
// (SEQ
//   (ASSIGN (LET $1 initialiser))*
//   (TRY_NO_CHECK
//     (SEQ (ASSIGN idseq $1)* body)
//     (SEQ (ASSIGN idseq $1)* else)
//     (SEQ $1.dispose()*)))
DEF(with);
  PRINT_INLINE();
  TOKEN(NULL, TK_WITH);
  RULE("with expression", withexpr);
  SKIP(NULL, TK_DO);
  RULE("with body", rawseq);
  IF(TK_ELSE, RULE("else clause", rawseq));
  SKIP(NULL, TK_END);
  DONE();

// TRY seq [ELSE seq] [THEN seq] END
DEF(try_block);
  PRINT_INLINE();
  TOKEN(NULL, TK_TRY);
  RULE("try body", seq);
  IF(TK_ELSE, RULE("try else body", seq));
  IF(TK_THEN, RULE("try then body", seq));
  SKIP(NULL, TK_END);
  DONE();

// $TRY_NO_CHECK seq [ELSE seq] [THEN seq] END
DEF(test_try_block);
  PRINT_INLINE();
  TOKEN(NULL, TK_TEST_TRY_NO_CHECK);
  MAP_ID(TK_TEST_TRY_NO_CHECK, TK_TRY_NO_CHECK);
  RULE("try body", seq);
  IF(TK_ELSE, RULE("try else body", seq));
  IF(TK_THEN, RULE("try then body", seq));
  SKIP(NULL, TK_END);
  SET_FLAG(AST_FLAG_TEST_ONLY);
  DONE();

// RECOVER [CAP] rawseq END
DEF(recover);
  PRINT_INLINE();
  TOKEN(NULL, TK_RECOVER);
  SCOPE();
  OPT RULE("capability", cap);
  RULE("recover body", rawseq);
  SKIP(NULL, TK_END);
  DONE();

// $BORROWED
DEF(test_borrowed);
  PRINT_INLINE();
  TOKEN(NULL, TK_TEST_BORROWED);
  MAP_ID(TK_TEST_BORROWED, TK_BORROWED);
  SET_FLAG(AST_FLAG_TEST_ONLY);
  DONE();

// CONSUME [cap | test_borrowed] term
DEF(consume);
  PRINT_INLINE();
  TOKEN("consume", TK_CONSUME);
  OPT RULE("capability", cap, test_borrowed);
  RULE("expression", term);
  DONE();

// (NOT | AMP | MINUS | MINUS_NEW | IDENTITY) term
DEF(prefix);
  PRINT_INLINE();
  TOKEN("prefix", TK_NOT, TK_ADDRESS, TK_MINUS, TK_MINUS_NEW, TK_IDENTITY);
  MAP_ID(TK_MINUS, TK_UNARY_MINUS);
  MAP_ID(TK_MINUS_NEW, TK_UNARY_MINUS);
  RULE("expression", term);
  DONE();

// (NOT | AMP | MINUS_NEW | IDENTITY) term
DEF(nextprefix);
  PRINT_INLINE();
  TOKEN("prefix", TK_NOT, TK_ADDRESS, TK_MINUS_NEW, TK_IDENTITY);
  MAP_ID(TK_MINUS_NEW, TK_UNARY_MINUS);
  RULE("expression", term);
  DONE();

// $IFDEFNOT term
DEF(test_prefix);
  PRINT_INLINE();
  TOKEN(NULL, TK_IFDEFNOT);
  RULE("expression", term);
  SET_FLAG(AST_FLAG_TEST_ONLY);
  DONE();

// $SEQ '(' rawseq ')'
// For testing only, thrown out by syntax pass
DEF(test_seq);
  PRINT_INLINE();
  SKIP(NULL, TK_TEST_SEQ);
  SKIP(NULL, TK_LPAREN);
  RULE("sequence", rawseq);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(AST_FLAG_TEST_ONLY);
  DONE();

// $NOSEQ '(' infix ')'
// For testing only, thrown out by syntax pass
DEF(test_noseq);
  PRINT_INLINE();
  SKIP(NULL, TK_TEST_NO_SEQ);
  SKIP(NULL, TK_LPAREN);
  RULE("sequence", infix);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(AST_FLAG_TEST_ONLY);
  DONE();

// $SCOPE '(' rawseq ')'
// For testing only, thrown out by syntax pass
DEF(test_seq_scope);
  PRINT_INLINE();
  SKIP(NULL, TK_TEST_SEQ_SCOPE);
  SKIP(NULL, TK_LPAREN);
  RULE("sequence", rawseq);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(AST_FLAG_TEST_ONLY);
  SCOPE();
  DONE();

// $IFDEFFLAG id
// For testing only, thrown out by syntax pass
DEF(test_ifdef_flag);
  PRINT_INLINE();
  TOKEN(NULL, TK_IFDEFFLAG);
  TOKEN(NULL, TK_ID);
  SET_FLAG(AST_FLAG_TEST_ONLY);
  DONE();

// local | cond | ifdef | match | whileloop | repeat | forloop | with | try |
// recover | consume | prefix | postfix | test_<various>
DEF(term);
  RULE("value", local, cond, ifdef, match, whileloop, repeat, forloop, with,
    try_block, recover, consume, prefix, postfix, test_seq, test_noseq,
    test_seq_scope, test_try_block, test_ifdef_flag, test_prefix);
  DONE();

// local | cond | ifdef | match | whileloop | repeat | forloop | with | try |
// recover | consume | prefix | postfix | test_<various>
DEF(nextterm);
  RULE("value", local, cond, ifdef, match, whileloop, repeat, forloop, with,
    try_block, recover, consume, nextprefix, nextpostfix, test_seq, test_noseq,
    test_seq_scope, test_try_block, test_ifdef_flag, test_prefix);
  DONE();

// AS type
// For tuple types, use multiple matches.
// (AS expr type) =>
// (MATCH expr
//   (CASES
//     (CASE
//       (LET $1 type)
//       NONE
//       (SEQ (CONSUME BORROWED $1))))
//   (SEQ ERROR))
DEF(asop);
  PRINT_INLINE();
  INFIX_BUILD();
  TOKEN("as", TK_AS);
  RULE("type", type);
  DONE();

// BINOP term
DEF(binop);
  INFIX_BUILD();
  TOKEN("binary operator",
    TK_AND, TK_OR, TK_XOR,
    TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD,
    TK_LSHIFT, TK_RSHIFT,
    TK_IS, TK_ISNT, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GE, TK_GT
    );
  RULE("value", term);
  DONE();

// TEST_BINOP term
// For testing only, thrown out by syntax pass
DEF(test_binop);
  INFIX_BUILD();
  TOKEN("binary operator", TK_IFDEFAND, TK_IFDEFOR);
  RULE("value", term);
  SET_FLAG(AST_FLAG_TEST_ONLY);
  DONE();

// term {binop | asop}
DEF(infix);
  RULE("value", term);
  SEQ("value", binop, asop, test_binop);
  DONE();

// term {binop | asop}
DEF(nextinfix);
  RULE("value", nextterm);
  SEQ("value", binop, asop, test_binop);
  DONE();

// ASSIGNOP assignment
DEF(assignop);
  PRINT_INLINE();
  INFIX_REVERSE();
  TOKEN("assign operator", TK_ASSIGN);
  RULE("assign rhs", assignment);
  DONE();

// term [assignop]
DEF(assignment);
  RULE("value", infix);
  OPT_NO_DFLT RULE("value", assignop);
  DONE();

// term [assignop]
DEF(nextassignment);
  RULE("value", nextinfix);
  OPT_NO_DFLT RULE("value", assignop);
  DONE();

// RETURN | BREAK | CONTINUE | ERROR | COMPILE_INTRINSIC | COMPILE_ERROR
DEF(jump);
  TOKEN("statement", TK_RETURN, TK_BREAK, TK_CONTINUE, TK_ERROR,
    TK_COMPILE_INTRINSIC, TK_COMPILE_ERROR);
  OPT RULE("return value", rawseq);
  DONE();

// SEMI
DEF(semi);
  IFELSE(TK_NEWLINE, NEXT_FLAGS(AST_FLAG_BAD_SEMI), NEXT_FLAGS(0));
  TOKEN(NULL, TK_SEMI);
  IF(TK_NEWLINE, SET_FLAG(AST_FLAG_BAD_SEMI));
  DONE();

// semi (exprseq | jump)
DEF(semiexpr);
  AST_NODE(TK_FLATTEN);
  RULE("semi", semi);
  RULE("value", exprseq, jump);
  DONE();

// nextexprseq | jump
DEF(nosemi);
  IFELSE(TK_NEWLINE, NEXT_FLAGS(0), NEXT_FLAGS(AST_FLAG_MISSING_SEMI));
  RULE("value", nextexprseq, jump);
  DONE();

// nextassignment (semiexpr | nosemi)
DEF(nextexprseq);
  AST_NODE(TK_FLATTEN);
  RULE("value", nextassignment);
  OPT_NO_DFLT RULE("value", semiexpr, nosemi);
  NEXT_FLAGS(0);
  DONE();

// assignment (semiexpr | nosemi)
DEF(exprseq);
  AST_NODE(TK_FLATTEN);
  RULE("value", assignment);
  OPT_NO_DFLT RULE("value", semiexpr, nosemi);
  NEXT_FLAGS(0);
  DONE();

// (exprseq | jump)
DEF(rawseq);
  AST_NODE(TK_SEQ);
  RULE("value", exprseq, jump);
  DONE();

// rawseq
DEF(seq);
  RULE("value", rawseq);
  SCOPE();
  DONE();

// (FUN | BE | NEW) [CAP] ID [typeparams] (LPAREN | LPAREN_NEW) [params]
// RPAREN [COLON type] [QUESTION] [ARROW rawseq]
DEF(method);
  TOKEN(NULL, TK_FUN, TK_BE, TK_NEW);
  SCOPE();
  OPT RULE("capability", cap);
  TOKEN("method name", TK_ID);
  OPT RULE("type parameters", typeparams);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("parameters", params);
  SKIP(NULL, TK_RPAREN);
  IF(TK_COLON, RULE("return type", type));
  OPT TOKEN(NULL, TK_QUESTION);
  OPT TOKEN(NULL, TK_STRING);
  IF(TK_DBLARROW, RULE("method body", rawseq));
  // Order should be:
  // cap id type_params params return_type error body docstring
  REORDER(0, 1, 2, 3, 4, 5, 7, 6);
  DONE();

// (VAR | LET | EMBED) ID [COLON type] [ASSIGN infix]
DEF(field);
  TOKEN(NULL, TK_VAR, TK_LET, TK_EMBED);
  MAP_ID(TK_VAR, TK_FVAR);
  MAP_ID(TK_LET, TK_FLET);
  TOKEN("field name", TK_ID);
  SKIP(NULL, TK_COLON);
  RULE("field type", type);
  IF(TK_DELEGATE, RULE("delegated type", provides));
  IF(TK_ASSIGN, RULE("field value", infix));
  // Order should be:
  // id type value delegate_type
  REORDER(0, 1, 3, 2);
  DONE();

// {field} {method}
DEF(members);
  AST_NODE(TK_MEMBERS);
  SEQ("field", field);
  SEQ("method", method);
  DONE();

// (TYPE | INTERFACE | TRAIT | PRIMITIVE | CLASS | ACTOR) [AT] ID [typeparams]
// [CAP] [IS type] [STRING] members
DEF(class_def);
  RESTART(TK_TYPE, TK_INTERFACE, TK_TRAIT, TK_PRIMITIVE, TK_STRUCT, TK_CLASS,
    TK_ACTOR);
  TOKEN("entity", TK_TYPE, TK_INTERFACE, TK_TRAIT, TK_PRIMITIVE, TK_STRUCT,
    TK_CLASS, TK_ACTOR);
  SCOPE();
  OPT TOKEN(NULL, TK_AT);
  OPT RULE("capability", cap);
  TOKEN("name", TK_ID);
  OPT RULE("type parameters", typeparams);
  IF(TK_IS, RULE("provided type", provides));
  OPT TOKEN("docstring", TK_STRING);
  RULE("members", members);
  // Order should be:
  // id type_params cap provides members c_api docstring
  REORDER(2, 3, 1, 4, 6, 0, 5);
  DONE();

// STRING
DEF(use_uri);
  PRINT_INLINE();
  TOKEN(NULL, TK_STRING);
  DONE();

// AT (ID | STRING) typeparams (LPAREN | LPAREN_NEW) [params] RPAREN [QUESTION]
DEF(use_ffi);
  TOKEN(NULL, TK_AT);
  MAP_ID(TK_AT, TK_FFIDECL);
  SCOPE();
  TOKEN("ffi name", TK_ID, TK_STRING);
  RULE("return type", typeargs);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("ffi parameters", params);
  AST_NODE(TK_NONE);  // Named parameters
  SKIP(NULL, TK_RPAREN);
  OPT TOKEN(NULL, TK_QUESTION);
  DONE();

// ID ASSIGN
DEF(use_name);
  PRINT_INLINE();
  TOKEN(NULL, TK_ID);
  SKIP(NULL, TK_ASSIGN);
  DONE();

// USE [ID ASSIGN] (STRING | USE_FFI) [IF infix]
DEF(use);
  RESTART(TK_USE, TK_TYPE, TK_INTERFACE, TK_TRAIT, TK_PRIMITIVE, TK_STRUCT,
    TK_CLASS, TK_ACTOR);
  TOKEN(NULL, TK_USE);
  OPT RULE("name", use_name);
  RULE("specifier", use_uri, use_ffi);
  IF(TK_IF, RULE("use condition", infix));
  DONE();

// {use} {class}
DEF(module);
  AST_NODE(TK_MODULE);
  SCOPE();
  OPT_NO_DFLT TOKEN("package docstring", TK_STRING);
  SEQ("use command", use);
  SEQ("type, interface, trait, primitive, class or actor definition",
    class_def);
  SKIP("type, interface, trait, primitive, class, actor, member or method",
    TK_EOF);
  DONE();


#ifdef PARSER

// external API
bool pass_parse(ast_t* package, source_t* source)
{
  return parse(package, source, module, "class, actor, primitive or trait");
}

#endif
