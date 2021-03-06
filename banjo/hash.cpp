// Copyright (c) 2015-2016 Andrew Sutton
// All rights reserved

#include "hash.hpp"
#include "ast.hpp"

#include <typeinfo>

namespace banjo
{

// Returns an initial hash value based on the dynamic type of T.
template<typename T>
std::size_t hash_type(T const& t)
{
  return typeid(t).hash_code();
}


// -------------------------------------------------------------------------- //
// Names

inline std::size_t
hash_value(Simple_id const& n)
{
  std::size_t h = hash_type(n);
  boost::hash_combine(h, &n.symbol());
  return h;
}


inline std::size_t
hash_value(Global_id const& n)
{
  return hash_type(n);
}


inline std::size_t
hash_value(Placeholder_id const&)
{
  lingo_unimplemented();
}


inline std::size_t
hash_value(Operator_id const&)
{
  lingo_unimplemented();
}


inline std::size_t
hash_value(Conversion_id const&)
{
  lingo_unimplemented();
}


inline std::size_t
hash_value(Literal_id const&)
{
  lingo_unimplemented();
}


inline std::size_t
hash_value(Destructor_id const&)
{
  lingo_unimplemented();
}


inline std::size_t
hash_value(Template_id const&)
{
  lingo_unimplemented();
}


inline std::size_t
hash_value(Concept_id const&)
{
  lingo_unimplemented();
}


inline std::size_t
hash_value(Qualified_id const&)
{
  lingo_unimplemented();
}


std::size_t
hash_value(Name const& n)
{
  struct fn
  {
    std::size_t operator()(Simple_id const& n)      { return hash_value(n); }
    std::size_t operator()(Global_id const& n)      { return hash_value(n); }
    std::size_t operator()(Placeholder_id const& n) { return hash_value(n); }
    std::size_t operator()(Operator_id const& n)    { return hash_value(n); }
    std::size_t operator()(Conversion_id const& n)  { return hash_value(n); }
    std::size_t operator()(Literal_id const& n)     { return hash_value(n); }
    std::size_t operator()(Destructor_id const& n)  { return hash_value(n); }
    std::size_t operator()(Template_id const& n)    { return hash_value(n); }
    std::size_t operator()(Concept_id const& n)     { return hash_value(n); }
    std::size_t operator()(Qualified_id const& n)   { return hash_value(n); }
  };
  return apply(n, fn{});
}


// -------------------------------------------------------------------------- //
// Types

inline std::size_t
hash_value(Void_type const& t)
{
  return hash_type(t);
}


inline std::size_t
hash_value(Boolean_type const& t)
{
  return hash_type(t);
}


inline std::size_t
hash_value(Integer_type const& t)
{
  std::size_t h = hash_type(t);
  boost::hash_combine(h, t.sign());
  boost::hash_combine(h, t.precision());
  return h;
}

inline std::size_t
hash_value(Float_type const& t)
{
  std::size_t h = hash_type(t);
  boost::hash_combine(h, t.precision());
  return h;
}


inline std::size_t
hash_value(Auto_type const& t)
{
  lingo_unreachable();
}


inline std::size_t
hash_value(Decltype_type const&)
{
  lingo_unreachable();
}


inline std::size_t
hash_value(Declauto_type const& t)
{
  lingo_unreachable();
}


inline std::size_t
hash_value(Function_type const& t)
{
  std::size_t h = hash_type(t);
  boost::hash_combine(h, t.parameter_types());
  boost::hash_combine(h, t.return_type());
  return h;
}


// Compute the hash value of a type.
std::size_t
hash_value(Type const& t)
{
  struct fn
  {
    std::size_t operator()(Void_type const& t) const      { return hash_value(t); }
    std::size_t operator()(Boolean_type const& t) const   { return hash_value(t); }
    std::size_t operator()(Integer_type const& t) const   { return hash_value(t); }
    std::size_t operator()(Float_type const& t) const     { return hash_value(t); }
    std::size_t operator()(Auto_type const& t) const      { return hash_value(t); }
    std::size_t operator()(Decltype_type const& t) const  { return hash_value(t); }
    std::size_t operator()(Declauto_type const& t) const  { return hash_value(t); }
    std::size_t operator()(Function_type const& t) const  { return hash_value(t); }
    std::size_t operator()(Qualified_type const& t) const { lingo_unimplemented(); }
    std::size_t operator()(Pointer_type const& t) const   { lingo_unimplemented(); }
    std::size_t operator()(Reference_type const& t) const { lingo_unimplemented(); }
    std::size_t operator()(Array_type const& t) const     { lingo_unimplemented(); }
    std::size_t operator()(Sequence_type const& t) const  { lingo_unimplemented(); }
    std::size_t operator()(Class_type const& t) const     { lingo_unimplemented(); }
    std::size_t operator()(Union_type const& t) const     { lingo_unimplemented(); }
    std::size_t operator()(Enum_type const& t) const      { lingo_unimplemented(); }
    std::size_t operator()(Typename_type const& t) const  { lingo_unimplemented(); }
    std::size_t operator()(Synthetic_type const& t) const { lingo_unimplemented(); }
  };
  return apply(t, fn{});
}


// -------------------------------------------------------------------------- //
// Expressions

template<typename T>
std::size_t
hash_value(Literal_expr<T> const& e)
{
  std::size_t h = hash_type(e);
  boost::hash_combine(h, e.value());
  return h;
}


std::size_t
hash_value(Reference_expr const& e)
{
  std::size_t h = hash_type(e);
  boost::hash_combine(h, e.declaration());
  return h;
}


std::size_t
hash_value(Unary_expr const& e)
{
  std::size_t h = hash_type(e);
  boost::hash_combine(h, e.operand());
  return h;
}


std::size_t
hash_value(Binary_expr const& e)
{
  std::size_t h = hash_type(e);
  boost::hash_combine(h, e.left());
  boost::hash_combine(h, e.right());
  return h;
}


std::size_t
hash_value(Call_expr const& e)
{
  std::size_t h = hash_type(e);
  boost::hash_combine(h, e.function());
  boost::hash_combine(h, e.arguments());
  return h;
}


std::size_t
hash_value(Expr const& e)
{
  struct fn
  {
    std::size_t operator()(Expr const& e) const           { lingo_unimplemented(); }
    std::size_t operator()(Boolean_expr const& e) const   { return hash_value(e); }
    std::size_t operator()(Integer_expr const& e) const   { return hash_value(e); }
    std::size_t operator()(Reference_expr const& e) const { return hash_value(e); }
    std::size_t operator()(Unary_expr const& e) const     { return hash_value(e); }
    std::size_t operator()(Binary_expr const& e) const    { return hash_value(e); }
    std::size_t operator()(Call_expr const& e) const      { return hash_value(e); }
  };
  return apply(e, fn{});
}


// -------------------------------------------------------------------------- //
// Declartions

// Compute the hash value of a declaration. Because declarations
// are unique, the hash is derived from the identity of the declaration.
std::size_t
hash_value(Decl const& d)
{
  std::hash<Decl const*> h;
  return h(&d);
}

// -------------------------------------------------------------------------- //
// Constraints

std::size_t
hash_value(Concept_cons const& c)
{
  std::size_t h = hash_type(c);
  boost::hash_combine(h, c.declaration());
  boost::hash_combine(h, c.arguments());
  return h;
}


std::size_t
hash_value(Predicate_cons const& c)
{
  std::size_t h = hash_type(c);
  boost::hash_combine(h, c.expression());
  return h;
}


std::size_t
hash_value(Binary_cons const& c)
{
  std::size_t h = hash_type(c);
  boost::hash_combine(h, c.left());
  boost::hash_combine(h, c.right());
  return h;
}



std::size_t
hash_value(Cons const& c)
{
  struct fn
  {
    std::size_t operator()(Cons const& c) const           { lingo_unimplemented(); }
    std::size_t operator()(Concept_cons const& c) const  { return hash_value(c); }
    std::size_t operator()(Predicate_cons const& c) const { return hash_value(c); }
    std::size_t operator()(Binary_cons const& c) const    { return hash_value(c); }
  };
  return apply(c, fn{});
}


} // namespace banjo
