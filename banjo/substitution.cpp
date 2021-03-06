// Copyright (c) 2015-2016 Andrew Sutton
// All rights reserved

#include "substitution.hpp"
#include "builder.hpp"
#include "print.hpp"

#include <iostream>


namespace banjo
{

// -------------------------------------------------------------------------- //
// Substitution class


// Helper debug output.
std::ostream&
operator<<(std::ostream& os, Substitution const& s)
{
  os << "{\n";
  for (auto& x : s) {
    os << "  " << *x.first << " => ";
    if (x.second)
      os << *x.second;
    else
      os << "<nullptr>";
    os << '\n';
  }
  os << "}\n";
  return os;
}


// -------------------------------------------------------------------------- //
// Substitution helpers

template<typename T>
List<T>
substitute(Context& cxt, List<T>& list, Substitution& sub)
{
  List<T> ret;
  ret.reserve(list.size());
  for (T& x : list)
    ret.push_back(substitute(cxt, x, sub));
  return ret;
}


// -------------------------------------------------------------------------- //
// Substitution into terms

Term&
substitute(Context& cxt, Term& x, Substitution& sub)
{
  if (Type* t = as<Type>(&x))
    return substitute(cxt, *t, sub);
  if (Expr* e = as<Expr>(&x))
    return substitute(cxt, *e, sub);
  if (Decl* d = as<Decl>(&x))
    return substitute(cxt, *d, sub);
  lingo_unreachable();
}



// -------------------------------------------------------------------------- //
// Substitution into types

Type& substitute_type(Context&, Function_type&, Substitution&);
Type& substitute_type(Context&, Reference_type&, Substitution&);
Type& substitute_type(Context&, Qualified_type&, Substitution&);
Type& substitute_type(Context&, Pointer_type&, Substitution&);
Type& substitute_type(Context&, Array_type&, Substitution&);
Type& substitute_type(Context&, Sequence_type&, Substitution&);
Type& substitute_type(Context&, Typename_type&, Substitution&);


Type&
substitute(Context& cxt, Type& t, Substitution& sub)
{
  struct fn
  {
    Context&      cxt;
    Substitution& sub;

    // Most types cannot be substituted into.
    Type& operator()(Type& t)           { return t; }

    Type& operator()(Auto_type& t)      { lingo_unimplemented(); }
    Type& operator()(Decltype_type& t)  { lingo_unimplemented(); }
    Type& operator()(Declauto_type& t)  { lingo_unimplemented(); }

    // Recrusively substitute through compound types.
    Type& operator()(Function_type& t)  { return substitute_type(cxt, t, sub); }
    Type& operator()(Reference_type& t) { return substitute_type(cxt, t, sub); }
    Type& operator()(Qualified_type& t) { return substitute_type(cxt, t, sub); }
    Type& operator()(Pointer_type& t)   { return substitute_type(cxt, t, sub); }
    Type& operator()(Array_type& t)     { return substitute_type(cxt, t, sub); }
    Type& operator()(Sequence_type& t)  { return substitute_type(cxt, t, sub); }
    Type& operator()(Typename_type& t)  { return substitute_type(cxt, t, sub); }
  };
  return apply(t, fn{cxt, sub});
}


Type&
substitute_type(Context& cxt, Function_type& t, Substitution& sub)
{
  Builder build(cxt);
  Type_list ps = substitute(cxt, t.parameter_types(), sub);
  Type& r = substitute(cxt, t.return_type(), sub);
  return build.get_function_type(ps, r);
}


Type&
substitute_type(Context& cxt, Reference_type& t, Substitution& sub)
{
  Builder build(cxt);
  Type& s = substitute(cxt, t.type(), sub);
  return build.get_reference_type(s);
}


Type&
substitute_type(Context& cxt, Qualified_type& t, Substitution& sub)
{
  Builder build(cxt);
  Type& s = substitute(cxt, t.type(), sub);
  return build.get_qualified_type(s, t.qualifier());
}


Type&
substitute_type(Context& cxt, Pointer_type& t, Substitution& sub)
{
  Builder build(cxt);
  Type& s = substitute(cxt, t.type(), sub);
  return build.get_pointer_type(s);
}


Type&
substitute_type(Context& cxt, Array_type& t, Substitution& sub)
{
  lingo_unimplemented();
}


Type&
substitute_type(Context& cxt, Sequence_type& t, Substitution& sub)
{
  Builder build(cxt);
  Type& s = substitute(cxt, t.type(), sub);
  return build.get_sequence_type(s);
}


// Substitute into a typename type. If the type's declaration is
// in the mapping, then reteurn the mapped type. Otherwise,
// return the original type.
Type&
substitute_type(Context& cxt, Typename_type& t, Substitution& sub)
{
  Decl& d = t.declaration();
  if (sub.has_mapping(d))
    return cast<Type>(*sub.get_mapping(d));
  else
    return t;
}


// -------------------------------------------------------------------------- //
// Substitution into expressions
//
// FIXME: None of this is correct. We actually need to elaborate the
// result of substitution.


// FIXME: This isn't right... we probably need to re-bind references,
// especially when they refer to parameters or local variables.
Expr&
subst_ref(Context& cxt, Reference_expr& e, Substitution& sub)
{
  return e;
}


Expr&
subst_check(Context& cxt, Check_expr& e, Substitution& sub)
{
  Builder build(cxt);
  Term_list args = substitute(cxt, e.arguments(), sub);
  return build.make_check(e.declaration(), args);
}


Expr&
subst_call(Context& cxt, Call_expr& e, Substitution& sub)
{
  Builder build(cxt);
  Expr& fn = substitute(cxt, e.function(), sub);
  Expr_list args = substitute(cxt, e.arguments(), sub);
  return build.make_call(build.get_void_type(), fn, args);
}


Expr&
subst_expr(Context& cxt, And_expr& e, Substitution& sub)
{
  Builder build(cxt);
  Expr& e1 = substitute(cxt, e.left(), sub);
  Expr& e2 = substitute(cxt, e.right(), sub);
  return build.make_and(e.type(), e1, e2);
}


Expr&
subst_expr(Context& cxt, Or_expr& e, Substitution& sub)
{
  Builder build(cxt);
  Expr& e1 = substitute(cxt, e.left(), sub);
  Expr& e2 = substitute(cxt, e.right(), sub);
  return build.make_or(e.type(), e1, e2);
}


Expr&
subst_expr(Context& cxt, Not_expr& e, Substitution& sub)
{
  Builder build(cxt);
  Expr& e1 = substitute(cxt, e.operand(), sub);
  return build.make_not(e.type(), e1);
}


Expr&
substitute(Context& cxt, Expr& e, Substitution& sub)
{
  struct fn
  {
    Context&      cxt;
    Substitution& sub;

    Expr& operator()(Expr& e)
    {
      std::cout << type_str(e) << '\n';
      lingo_unimplemented();
    }

    Expr& operator()(Boolean_expr& e)   { return e; }
    Expr& operator()(Integer_expr& e)   { return e; }
    Expr& operator()(Reference_expr& e) { return subst_ref(cxt, e, sub); }
    Expr& operator()(Check_expr& e)     { return subst_check(cxt, e, sub); }
    Expr& operator()(Call_expr& e)      { return subst_call(cxt, e, sub); }

    Expr& operator()(And_expr& e) { return subst_expr(cxt, e, sub); }
    Expr& operator()(Or_expr& e)  { return subst_expr(cxt, e, sub); }
    Expr& operator()(Not_expr& e) { return subst_expr(cxt, e, sub); }

  };
  return apply(e, fn{cxt, sub});
}


// -------------------------------------------------------------------------- //
// Substitution into declarations
//
// Note that substitution into a declaration does not change
// the name of the declaration. Specialization, however, is a
// special form of substitution where we generate a newly named
// declaration.
//
// FIXME: Substitution is kind of like parsing. We need to interpret
// the resulting constructs as if they were parsed. That means we
// need to maintain binding environments to support lookup and
// resolution.


// FIXME: Rebuild the variable as if parsed.
Decl&
substitute_decl(Context& cxt, Variable_decl& d, Substitution& sub)
{
  Name& n = d.name();
  Type& t = substitute(cxt, d.type(), sub);

  Builder build(cxt);
  return build.make_variable(n, t);
}


// FIXME: Rebuild the parameter as if parsed.
//
// FIXME: Do we substitute into a default argument? It probably
// depends on context, but I assume that the general answer is yes.
Decl&
substitute_decl(Context& cxt, Object_parm& d, Substitution& sub)
{
  Name& n = d.name();
  Type& t = substitute(cxt, d.type(), sub);

  Builder build(cxt);
  return build.make_object_parm(n, t);
}


Decl&
substitute(Context& cxt, Decl& d, Substitution& sub)
{
  struct fn
  {
    Context&      cxt;
    Substitution& sub;
    Decl& operator()(Decl& d)           { lingo_unimplemented(); }
    Decl& operator()(Variable_decl& d)  { return substitute_decl(cxt, d, sub); }
    Decl& operator()(Object_parm& d)    { return substitute_decl(cxt, d, sub); }
  };
  return apply(d, fn{cxt, sub});
}


// -------------------------------------------------------------------------- //
// Substitution into constraints


Cons&
substitute(Context& cxt, Cons& c, Substitution& sub)
{
  struct fn
  {
    Context&      cxt;
    Substitution& sub;
    Cons& operator()(Cons& c)           { banjo_unhandled_case(c); }
  };
  return apply(c, fn{cxt, sub});
}

} // namespace banjo
