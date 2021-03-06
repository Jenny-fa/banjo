// Copyright (c) 2015-2016 Andrew Sutton
// All rights reserved

#include "builder.hpp"
#include "ast.hpp"
#include "equivalence.hpp"
#include "hash.hpp"

#include <unordered_set>


namespace banjo
{

// FIXME: Move this into lingo.
//
// A unique factory will only allocate new objects if they have not been
// previously created.
template<typename T, typename Hash, typename Eq>
struct Hashed_unique_factory : std::unordered_set<T, Hash, Eq>
{
  template<typename... Args>
  T& make(Args&&... args)
  {
    auto ins = this->emplace(std::forward<Args>(args)...);
    return *const_cast<T*>(&*ins.first); // Yuck.
  }
};


template<typename T>
struct Hash
{
  std::size_t operator()(T const& t) const
  {
    return hash_value(t);
  }

  std::size_t operator()(List<T> const& t) const
  {
    return hash_value(t);
  }
};


template<typename T>
struct Eq
{
  bool operator()(T const& a, T const& b) const
  {
    return is_equivalent(a, b);
  }

  bool operator()(List<T> const& a, List<T> const& b) const
  {
    return is_equivalent(a, b);
  }
};


template<typename T>
using Factory = Hashed_unique_factory<T, Hash<T>, Eq<T>>;


// -------------------------------------------------------------------------- //
// Names

// Returns a simple identifier with the given spelling.
//
// TODO: Unique this?
Simple_id&
Builder::get_id(char const* s)
{
  Symbol const* sym = symbols().put_identifier(identifier_tok, s);
  return make<Simple_id>(*sym);
}


// Returns a simple identifier with the given spelling.
Simple_id&
Builder::get_id(std::string const& s)
{
  Symbol const* sym = symbols().put_identifier(identifier_tok, s);
  return make<Simple_id>(*sym);
}


// Returns a simple identifier for the given symbol.
Simple_id&
Builder::get_id(Symbol const& sym)
{
  lingo_assert(is<Identifier_sym>(&sym));
  return make<Simple_id>(sym);
}


// Returns a simple identifier for the symbol.
Simple_id&
Builder::get_id(Symbol const* sym)
{
  return get_id(*sym);
}


// Returns a simple id for the given token.
Simple_id&
Builder::get_id(Token tok)
{
  return get_id(tok.symbol());
}


// Returns a placeholder for a name.
//
// TODO: Make placeholders unique. Globally?
Placeholder_id&
Builder::get_id()
{
  return make<Placeholder_id>();
}


// Returns a destructor-id for the given type.
Destructor_id&
Builder::get_destructor_id(Type const& t)
{
  lingo_unimplemented();
}


Template_id&
Builder::get_template_id(Template_decl& d, Term_list const& t)
{
  return make<Template_id>(d, t);
}


Concept_id&
Builder::get_concept_id(Concept_decl& d, Term_list const& t)
{
  return make<Concept_id>(d, t);
}


// Returns a qualified-id.
Qualified_id&
Builder::get_qualified_id(Decl& d, Name& n)
{
  return make<Qualified_id>(d, n);
}


// Return the global identifier.
Global_id&
Builder::get_global_id()
{
  // TODO: Global or no?
  static Global_id n;
  return n;
}


// -------------------------------------------------------------------------- //
// Types

Void_type&
Builder::get_void_type()
{
  return make<Void_type>();
}


Boolean_type&
Builder::get_bool_type()
{
  return make<Boolean_type>();
}


Integer_type&
Builder::get_integer_type(bool s, int p)
{
  return make<Integer_type>(s, p);
}

Byte_type&
Builder::get_byte_type()
{
  return make<Byte_type>();
}


// TODO: Default precision depends on configuration.
Integer_type&
Builder::get_int_type()
{
  return get_integer_type(true, 32);
}


// TODO: Default precision depends on configuration.
Integer_type&
Builder::get_uint_type()
{
  return get_integer_type(false, 32);
}


Float_type&
Builder::get_float_type()
{
  return make<Float_type>();
}


Auto_type&
Builder::get_auto_type()
{
  return make<Auto_type>();
}


Decltype_type&
Builder::get_decltype_type(Expr&)
{
  lingo_unimplemented();
}


Declauto_type&
Builder::get_declauto_type()
{
  return make<Declauto_type>();
}


Function_type&
Builder::get_function_type(Decl_list const& ps, Type& r)
{
  Type_list ts;
  for (Decl& d : *modify(&ps)) {
    Object_parm& p = cast<Object_parm>(d);
    ts.push_back(p.type());
  }
  return get_function_type(ts, r);
}


Function_type&
Builder::get_function_type(Type_list const& ts, Type& r)
{
  return make<Function_type>(ts, r);
}


// TODO: Do not build qualified types for functions or arrays.
// Is that a hard error, or do we simply fold the const into
// the return type and/or element type?
Qualified_type&
Builder::get_qualified_type(Type& t, Qualifier_set qual)
{
  if (Qualified_type* q = as<Qualified_type>(&t)) {
    q->qual |= qual;
    return *q;
  }
  return make<Qualified_type>(t, qual);
}


Qualified_type&
Builder::get_const_type(Type& t)
{
  return get_qualified_type(t, const_qual);
}


Qualified_type&
Builder::get_volatile_type(Type& t)
{
  return get_qualified_type(t, volatile_qual);
}


Pointer_type&
Builder::get_pointer_type(Type& t)
{
  return make<Pointer_type>(t);
}


Reference_type&
Builder::get_reference_type(Type& t)
{
  return make<Reference_type>(t);
}


Array_type&
Builder::get_array_type(Type&, Expr&)
{
  lingo_unimplemented();
}


Sequence_type&
Builder::get_sequence_type(Type& t)
{
  return make<Sequence_type>(t);
}


// FIXME: Canonicalize class types?
Class_type&
Builder::get_class_type(Decl& d)
{
  return make<Class_type>(d);
}


Union_type&
Builder::get_union_type(Decl& d)
{
  lingo_unimplemented();
}


Enum_type&
Builder::get_enum_type(Decl& d)
{
  lingo_unimplemented();
}


Typename_type&
Builder::get_typename_type(Decl& d)
{
  return make<Typename_type>(d);
}


Synthetic_type&
Builder::synthesize_type(Decl& d)
{
  return make<Synthetic_type>(d);
}


// -------------------------------------------------------------------------- //
// Expressions

Boolean_expr&
Builder::get_bool(bool b)
{
  return make<Boolean_expr>(get_bool_type(), b);
}


Boolean_expr&
Builder::get_true()
{
  return get_bool(true);
}


Boolean_expr&
Builder::get_false()
{
  return get_bool(false);
}


// TODO: Verify that T can have an integer value?
// I think that all scalars can have integer values.
Integer_expr&
Builder::get_integer(Type& t, Integer const& n)
{
  return make<Integer_expr>(t, n);
}


// Returns the 0 constant, with scalar type `t`.
//
// TODO: Verify that t is scalar.
//
// TODO: Produce zero interpratations for any T?
Integer_expr&
Builder::get_zero(Type& t)
{
  return get_integer(t, 0);
}


Integer_expr&
Builder::get_int(Integer const& n)
{
  return get_integer(get_int_type(), n);
}


Integer_expr&
Builder::get_uint(Integer const& n)
{
  // lingo_assert(n.is_nonnegative(n));
  return get_integer(get_uint_type(), n);
}


// Get an expression that refers to a variable. The type
// is a reference to the declared type of the variable.
Reference_expr&
Builder::make_reference(Variable_decl& d)
{
  return make<Reference_expr>(get_reference_type(d.type()), d);
}


Reference_expr&
Builder::make_reference(Function_decl& d)
{
  return make<Reference_expr>(get_reference_type(d.type()), d);
}


Reference_expr&
Builder::make_reference(Object_parm& d)
{
  return make<Reference_expr>(get_reference_type(d.type()), d);
}


// Make a concept check. The type is bool.
Check_expr&
Builder::make_check(Concept_decl& d, Term_list const& as)
{
  return make<Check_expr>(get_bool_type(), d, as);
}


And_expr&
Builder::make_and(Type& t, Expr& e1, Expr& e2)
{
  return make<And_expr>(t, e1, e2);
}


Or_expr&
Builder::make_or(Type& t, Expr& e1, Expr& e2)
{
  return make<Or_expr>(t, e1, e2);
}


Not_expr&
Builder::make_not(Type& t, Expr& e)
{
  return make<Not_expr>(t, e);
}


Eq_expr&
Builder::make_eq(Type& t, Expr& e1, Expr& e2)
{
  return make<Eq_expr>(t, e1, e2);
}


Ne_expr&
Builder::make_ne(Type& t, Expr& e1, Expr& e2)
{
  return make<Ne_expr>(t, e1, e2);
}


Lt_expr&
Builder::make_lt(Type& t, Expr& e1, Expr& e2)
{
  return make<Lt_expr>(t, e1, e2);
}


Gt_expr&
Builder::make_gt(Type& t, Expr& e1, Expr& e2)
{
  return make<Gt_expr>(t, e1, e2);
}


Le_expr&
Builder::make_le(Type& t, Expr& e1, Expr& e2)
{
  return make<Le_expr>(t, e1, e2);
}


Ge_expr&
Builder::make_ge(Type& t, Expr& e1, Expr& e2)
{
  return make<Ge_expr>(t, e1, e2);
}


Call_expr&
Builder::make_call(Type& t, Expr& f, Expr_list const& a)
{
  return make<Call_expr>(t, f, a);
}


Call_expr&
Builder::make_call(Type& t, Function_decl& f, Expr_list const& a)
{
  return make_call(t, make_reference(f), a);
}


Requires_expr&
Builder::make_requires(Decl_list const& tps, Decl_list const& ps, Req_list const& rs)
{
  return make<Requires_expr>(get_bool_type(), tps, ps, rs);
}


Synthetic_expr&
Builder::synthesize_expression(Decl& d)
{
  return make<Synthetic_expr>(declared_type(d), d);
}


// -------------------------------------------------------------------------- //
// Statements

Compound_stmt&
Builder::make_compound_statement(Stmt_list const& ss)
{
  return make<Compound_stmt>(ss);
}


Return_stmt&
Builder::make_return_statement(Expr& e)
{
  return make<Return_stmt>(e);
}


Expression_stmt&
Builder::make_expression_statement(Expr& e)
{
  return make<Expression_stmt>(e);
}


Declaration_stmt&
Builder::make_declaration_statement(Decl& d)
{
  return make<Declaration_stmt>(d);
}


// -------------------------------------------------------------------------- //
// Initializers

Trivial_init&
Builder::make_trivial_init(Type& t)
{
  return make<Trivial_init>(t);
}


Copy_init&
Builder::make_copy_init(Type& t, Expr& e)
{
  return make<Copy_init>(t, e);
}


Bind_init&
Builder::make_bind_init(Type& t, Expr& e)
{
  return make<Bind_init>(t, e);
}


Direct_init&
Builder::make_direct_init(Type& t, Decl& d, Expr_list const& es)
{
  return make<Direct_init>(t, d, es);
}


Aggregate_init&
Builder::make_aggregate_init(Type& t, Expr_list const& es)
{
  return make<Aggregate_init>(t, es);
}


// -------------------------------------------------------------------------- //
// Definitions



Deleted_def&
Builder::make_deleted_definition()
{
  return make<Deleted_def>();
}


Defaulted_def&
Builder::make_defaulted_definition()
{
  return make<Defaulted_def>();
}


Expression_def&
Builder::make_expression_definition(Expr& e)
{
  return make<Expression_def>(e);
}


Function_def&
Builder::make_function_definition(Stmt& s)
{
  return make<Function_def>(s);
}


Class_def&
Builder::make_class_definition(Decl_list const& ds)
{
  return make<Class_def>(ds);
}


Concept_def&
Builder::make_concept_definition(Req_list const& ss)
{
  return make<Concept_def>(ss);
}


// -------------------------------------------------------------------------- //
// Declarations

Variable_decl&
Builder::make_variable(Name& n, Type& t)
{
  return make<Variable_decl>(n, t);
}


Variable_decl&
Builder::make_variable(char const* s, Type& t)
{
  return make_variable(get_id(s), t);
}


Variable_decl&
Builder::make_variable(Name& n, Type& t, Expr& i)
{
  lingo_assert(is<Init>(&i));
  return make<Variable_decl>(n, t, i);
}


Variable_decl&
Builder::make_variable(char const* s, Type& t, Expr& i)
{
  return make_variable(get_id(s), t, i);
}


// Creates an undefined function with parameters ps and return
// type r.
Function_decl&
Builder::make_function(Name& n, Decl_list const& ps, Type& r)
{
  Type& t = get_function_type(ps, r);
  return make<Function_decl>(n, t, ps);
}


Function_decl&
Builder::make_function(char const* s, Decl_list const& ps, Type& r)
{
  return make_function(get_id(s), ps, r);
}


Class_decl&
Builder::make_class(Name& n)
{
  return make<Class_decl>(n);
}


Class_decl&
Builder::make_class(char const* s)
{
  return make<Class_decl>(get_id(s));
}


Namespace_decl&
Builder::make_namespace(Name& n)
{
  return make<Namespace_decl>(n);
}


Namespace_decl&
Builder::make_namespace(char const* s)
{
  return make_namespace(get_id(s));
}


// FIXME: This should probably be installed on the context.
Namespace_decl&
Builder::get_global_namespace()
{
  static Namespace_decl ns(get_global_id());
  return ns;
}


Template_decl&
Builder::make_template(Decl_list const& p, Decl& d)
{
  return make<Template_decl>(p, d);
}


Concept_decl&
Builder::make_concept(Name& n, Decl_list const& ps)
{
  return make<Concept_decl>(n, ps);
}


Concept_decl&
Builder::make_concept(Name& n, Decl_list const& ps, Def& d)
{
  return make<Concept_decl>(n, ps, d);
}


Concept_decl&
Builder::make_concept(Name& n, Decl_list const& ps, Expr& e)
{
  return make<Concept_decl>(n, ps, make_expression_definition(e));
}


Concept_decl&
Builder::make_concept(char const* s, Decl_list const& ps, Def& d)
{
  return make_concept(get_id(s), ps, d);
}


Concept_decl&
Builder::make_concept(char const* s, Decl_list const& ps, Expr& e)
{
  return make_concept(get_id(s), ps, make_expression_definition(e));
}


// TODO: Parameters can't be functions or void. Check this
// property or assert it.
Object_parm&
Builder::make_object_parm(Name& n, Type& t)
{
  return make<Object_parm>(n, t);
}


Object_parm&
Builder::make_object_parm(char const* s, Type& t)
{
  return make_object_parm(get_id(s), t);
}


Type_parm&
Builder::make_type_parameter(Name& n)
{
  return make<Type_parm>(n);
}


Type_parm&
Builder::make_type_parameter(char const* n)
{
  return make_type_parameter(get_id(n));
}


// Make a type parameter with a default type.
Type_parm&
Builder::make_type_parameter(Name& n, Type& t)
{
  return make<Type_parm>(n, t);
}


// Make a type parameter with a default type.
Type_parm&
Builder::make_type_parameter(char const* n, Type& t)
{
  return make_type_parameter(get_id(n), t);
}


Value_parm&
Builder::make_value_parm(Name& n, Type& t)
{
  return make<Value_parm>(n, t);
}


Value_parm&
Builder::make_value_parm(char const* s, Type& t)
{
  return make_value_parm(get_id(s), t);
}


// -------------------------------------------------------------------------- //
// Constraints

// FIXME: Save all uniqued terms in the context, not as global variables.

Concept_cons&
Builder::get_concept_constraint(Decl& d, Term_list& ts)
{
  static Factory<Concept_cons> f;
  return f.make(d, ts);
}


Predicate_cons&
Builder::get_predicate_constraint(Expr& e)
{
  static Factory<Predicate_cons> f;
  return f.make(e);
}


Conjunction_cons&
Builder::get_conjunction_constraint(Cons& c1, Cons& c2)
{
  static Factory<Conjunction_cons> f;
  return f.make(c1, c2);
}


Disjunction_cons&
Builder::get_disjunction_constraint(Cons& c1, Cons& c2)
{
  static Factory<Disjunction_cons> f;
  return f.make(c1, c2);
}


}
