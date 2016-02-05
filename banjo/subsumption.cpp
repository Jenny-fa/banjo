// Copyright (c) 2015-2016 Andrew Sutton
// All rights reserved

#include "subsumption.hpp"
#include "ast.hpp"
#include "builder.hpp"
#include "substitution.hpp"
#include "normalization.hpp"
#include "hash.hpp"
#include "equivalence.hpp"
#include "print.hpp"

#include <list>
#include <unordered_set>
#include <iostream>


namespace banjo
{


// -------------------------------------------------------------------------- //
// Proof structures

// A list of propositions (constraints). These are accumulated on either
// side of a sequent. This is actually a list equipped with a side-table
// to optimize list membership.
//
// FIXME: An unordered map on constraint *values* will be inefficient.
// We really want to work on constraint identities, which means we
// need to canonicalize constraints.
struct Prop_list
{
  using Seq            = std::list<Cons const*>;
  using iterator       = Seq::iterator;
  using const_iterator = Seq::const_iterator;

  using Map = std::unordered_map<Cons const*, iterator, Cons_hash, Cons_eq>;


  // Returns true if the list has a constraint that is identical
  // to c.
  bool contains(Cons const& c) const
  {
    return map.count(&c) != 0;
  }

  // Insert a new constraint. No action is taken if the constraint
  // is already in the set. Returns the index of the added
  // constraint or that of the original constraint.
  std::pair<iterator, bool> insert(Cons const& c)
  {
    auto iter = map.find(&c);
    if (iter != map.end()) {
      return {iter->second, false};
    } else {
      auto pos = seq.insert(seq.end(), &c);
      map.emplace(&c, pos);
      return {pos, true};
    }
  }

  // Positionally insert the constraint before pos. This does nothing if
  // c is in the map, returing the same iterator.
  std::pair<iterator, bool> insert(iterator pos, Cons const& c)
  {
    auto iter = map.find(&c);
    if (iter != map.end()) {
      return {pos, false};
    } else {
      pos = seq.insert(pos, &c);
      map.emplace(&c, pos);
      return {pos, true};
    }
  }

  // Erase the constraint from the list. This nulls the entry in the
  // sequence, so it must be replaced.
  iterator erase(iterator pos)
  {
    map.erase(*pos);
    return seq.erase(pos);
  }

  // Replace the term in the list with c. Note that no replacement
  // may be made if c is already in the list.
  //
  // Returns a pair (i, b) where i is an iterator and b a boolean
  // value indicating successful insertion. If b is true, then i
  // i is the position of the inserted constraint. Otherise, i the
  // iterator past the original replaced element.
  std::pair<iterator, bool> replace(iterator pos, Cons const& c)
  {
    pos = erase(pos);
    auto x = insert(pos, c);
    if (x.second)
      return x;
    return {pos, false};
  }


  // Replace the term in the list with c1 folowed by c2. Note that
  // no replacements may be made if c1 and c2 are already in the list.
  //
  // Returns a pair (i, b) where i is an iterator and b a boolean
  // value indicating successful insertion. If b is true, then i
  // i is the position of the first inserted constraint. Otherwise,
  // i is the iterator past the original replaced element.
  std::pair<iterator, bool> replace(iterator pos, Cons const& c1, Cons const& c2)
  {
    pos = erase(pos);
    auto x1 = insert(pos, c1);
    auto x2 = insert(pos, c2);
    if (x1.second)
      return x1;
    if (x2.second)
      return x2;
    return {pos, false};
  }

  // Iterators
  iterator start()   { return (cur = seq.begin()); }
  iterator advance() { return ++cur; }
  iterator current() { return cur; }

  iterator begin() { return seq.begin(); }
  iterator end()   { return seq.end(); }

  const_iterator begin() const { return seq.begin(); }
  const_iterator end()   const { return seq.end(); }

  Map      map;
  Seq      seq;
  iterator cur;
};


std::ostream&
operator<<(std::ostream& os, Prop_list const& cs)
{
  for (auto iter = cs.begin(); iter != cs.end(); ++iter) {
    os << **iter;
    if (std::next(iter) != cs.end())
      os << ", ";
  }
  return os;
}


// An iterator into a proposition lis.
using Prop_iter       = Prop_list::iterator;
using Prop_const_iter = Prop_list::const_iterator;


// A sequent associates a set of antecedents with a set of
// propositions, indicating a proof thereof (the consequences
// follow from the antecedents).
struct Sequent
{

  // Create a sequent having the antecedent a and the consequent c.
  Sequent(Cons const& a, Cons const& c)
  {
    ants.insert(a);
    cons.insert(c);
  }

  // Returns the list of antecedents.
  Prop_list const& antecedents() const { return ants; }
  Prop_list&       antecedents()       { return ants; }

  // Returns the list of consequents.
  Prop_list const& consequents() const { return cons; }
  Prop_list&       consequents()       { return cons; }

  Prop_list ants;
  Prop_list cons;
};


std::ostream&
operator<<(std::ostream& os, Sequent const& seq)
{
  return os << seq.antecedents() << " |- " << seq.consequents();
}


// The goal list stores the current set of goals in the syntactic
// proof of a sequent. Proof tactics manipulate the goal list.
//
// Proof strategies must attempt to minimize the creation of
// sub-goals in the proof.
struct Goal_list : std::list<Sequent>
{
  using iterator       = std::list<Sequent>::iterator;
  using const_iterator = std::list<Sequent>::const_iterator;

  // Initalize with a single sequent s.
  Goal_list(Sequent&& s)
    : std::list<Sequent>{std::move(s)}
  { }

  // Generate a new proof obligation as a copy of s, returning
  // an iterator referring to the sequent.
  iterator generate(Sequent const& s)
  {
    return insert(end(), s);
  }

  // Discharge the proof obligation referred to by i.
  iterator discharge(iterator i)
  {
    return erase(i);
  }
};


using Goal_iter = Goal_list::iterator;


// The proof class represents the current work on the proof of a single
// sequent. New work proof tasks can be created with the branch function.
struct Proof
{
  Proof(Context& c, Goal_list& g)
    : cxt(c), gs(g), iter(g.begin())
  { }

  Proof(Context& c, Goal_list& g, Goal_iter i)
    : cxt(c), gs(g), iter(i)
  { }

  Context& context() { return cxt; }

  // Returns the list of goals for this proof.
  Goal_list const& goals() const { return gs; }
  Goal_list&       goals()       { return gs; }

  // Return the current goal (sequent).
  Sequent& sequent() { return *iter; }

  // Return the antecedents or consequents of the current sequent.
  Prop_list& antecedents() { return sequent().antecedents(); }
  Prop_list& consequents() { return sequent().consequents(); }

  // Insert a new sequent into the goals et, and return that.
  //
  // NOTE: If we can branch, can we ever join (i.e. discharge) a
  // proof obligation.
  Proof branch()
  {
    // Create a *copy* of the current sequence.
    auto iter = gs.insert(gs.end(), sequent());

    // And yield a new proof object.
    return Proof(cxt, gs, iter);
  }

  Context&   cxt;  // The global context
  Goal_list& gs;   // Global list of goals
  Goal_iter  iter; // The current goal
};


std::ostream&
operator<<(std::ostream& os, Proof const& p)
{
  os << "[\n";
  std::size_t n = 0;
  for (Sequent const& s : p.goals())
    os << "  " << n << ": " << s << '\n';
  os << "]";
  return os;
}


// -------------------------------------------------------------------------- //
// Proof validation
//
// Iterate over the goals in the proof. All goals must be satsified in
// order for the proof to be valid.
//
// TODO: Goal validation is a three-valued logic. Either a goal is
// satisfied (in which case we can discharge it), not satisfied
// (in which case the proof is invalid), or unknown. This latest case
// applies only when sequents have unexpanded propositions.
//
// TODO: Experiment with memoization!

enum Validation
{
  valid_proof,
  invalid_proof,
  incomplete_proof,
};


// Returns true if c is an atomic constraint.
inline bool
is_atomic(Cons const& c)
{
  struct fn
  {
    bool operator()(Cons const& c) const               { return true; }
    bool operator()(Concept_cons const& c) const       { return false; }
    bool operator()(Parameterized_cons const& c) const { return false; }
    bool operator()(Conjunction_cons const& c) const   { return false; }
    bool operator()(Disjunction_cons const& c) const   { return false; }
  };
  return apply(c, fn{});
}


Validation validate_implication(Context&, Cons const&, Cons const&);
Validation validate_implication(Context&, Prop_list&, Cons const&);

Cons const& expand(Context&, Concept_cons const&);


// Validate a sequent of the form A |- C when A and C when
// A and C differ syntactically (e.g., A => C by semantic rules).
//
// Note that if both a and b are atomic and there are no semantic
// rules that validate the proof, then the proof cannot be validated.
//
// TODO: Define and check semantic rules.
Validation
validate_non_matching(Context& cxt, Cons const& a, Cons const& c)
{
  if (is_atomic(a) && is_atomic(c))
    return invalid_proof;
  return incomplete_proof;
}


// Validate a sequent of the form A1, A2, ..., An |- C when each Ai
// differs from C syntactically (e.g. if some Ai => C by semantic
// rules).
//
// TODO: Re-enable this loop.
//
// FIXME: !!!! This is broken. We probably need to recursively
// evalute c against the entirety of constraints in ants. This
// needs to be a recursive function, and it's going to have
// exponential branching.
Validation
validate_non_matching(Context& cxt, Prop_list& ants, Cons const& c)
{
  Validation r = invalid_proof;
  for (Cons const* a : ants) {
    Validation v = validate_non_matching(cxt, *a, c);
    if (v == valid_proof)
      return v;
    if (v == incomplete_proof)
      r = v;
  }
  return r;
}


// Validate a sequent of the form A |- C when it is unknown whether
// A and C are equivalent constraints. If A and C are equivalent,
// the the proof is valid. Otherwise, check semantic implications.
Validation
validate_implication(Context& cxt, Cons const& a, Cons const& c)
{
  if (is_equivalent(a, c))
    return valid_proof;
  else
    return validate_non_matching(cxt, a, c);
}


// Given a list of antecedents and a conclusion, determine if
// A1, A2, ..., An |- C. If C is equivalent to any Ai, then the
// proof is valid. Otherwise, we must check semantic implications.
Validation
validate_implication(Context& cxt, Prop_list& as, Cons const& c)
{
  if (as.contains(c))
    return valid_proof;
  else
    return validate_non_matching(cxt, as, c);
}


// Given a sequent of the form A1, A2, ..., An |- C1, C2, ... Cm,
// returns true if any Ci is proven by all Ai.
Validation
validate_obligation(Context& cxt, Sequent& s)
{
  Prop_list& as = s.antecedents();
  Prop_list& cs = s.consequents();

  Validation r = invalid_proof;
  for (Cons const* c : cs) {
    Validation v = validate_implication(cxt, as, *c);
    if (v == valid_proof)
      return v;
    if (v == incomplete_proof)
      r = v;
  }
  return r;
}


// Verify that all proof goals are satisfied. Whenever a goal
// is known to be satisfied, it is discharged (removed from
// the goal list).
Validation
validate_proof(Proof& p)
{
  Context& cxt = p.cxt;
  Goal_list& goals = p.goals();

  auto iter = goals.begin();
  while (iter != goals.end()) {
    Validation v = validate_obligation(cxt, *iter);
    if (v == valid_proof)
      iter = goals.discharge(iter);
    else if (v == invalid_proof)
      return v;
    ++iter;
  }
  if (goals.empty())
    return valid_proof;
  else
    return incomplete_proof;
}


// -------------------------------------------------------------------------- //
// Flattening
//
// These operations try to move as many propositions as possible into
// the constraint sets on the left and right of a sequent. This will
// never produce sub-goals.

Prop_iter flatten_left(Proof, Cons const&);
Prop_iter flatten_right(Proof, Cons const&);


inline Prop_iter
advance(Prop_list& ps)
{
  return ps.advance();
}


inline Prop_iter
replace(Prop_list& ps, Cons const& c)
{
  auto x = ps.replace(ps.current(), c);
  return x.first;
}


inline Prop_iter
replace(Prop_list& ps, Cons const& c1, Cons const& c2)
{
  auto x = ps.replace(ps.current(), c1, c2);
  return x.first;
}


// Do nothing for atomic constraints.
Prop_iter
flatten_left_atom(Proof p, Cons const& c)
{
  return advance(p.sequent().antecedents());
}


// Replace the current consequent with its operand (maybe).
// Parameterized constraints are essentially transparent, so
// they can be reduced immediately.
Prop_iter
flatten_left_lambda(Proof p, Parameterized_cons const& c)
{
  Cons const& c1 = c.constraint();
  return replace(p.sequent().antecedents(), c1);
}


// Replace the current antecedent with its operands (maybe).
Prop_iter
flatten_left_conjunction(Proof p, Conjunction_cons const& c)
{
  Cons const& c1 = c.left();
  Cons const& c2 = c.right();
  return replace(p.sequent().antecedents(), c1, c2);
}


// Advance to the next term so we don't produce subgoals.
Prop_iter
flatten_left_disjunction(Proof p, Disjunction_cons const& c)
{
  return advance(p.sequent().antecedents());
}


// Select an appropriate action for the current proposition.
Prop_iter
flatten_left(Proof p, Cons const& c)
{
  struct fn
  {
    Proof p;
    Prop_iter operator()(Cons const& c)               { return flatten_left_atom(p, c); }
    Prop_iter operator()(Parameterized_cons const& c) { return flatten_left_lambda(p, c); }
    Prop_iter operator()(Conjunction_cons const& c)   { return flatten_left_conjunction(p, c); }
    Prop_iter operator()(Disjunction_cons const& c)   { return flatten_left_disjunction(p, c); }
  };
  return apply(c, fn{p});
}


// Flatten all propositions in the antecedents.
void
flatten_left(Proof p, Sequent& s)
{
  Prop_list& as = s.antecedents();
  auto ai = as.start();
  while (ai != as.end())
    ai = flatten_left(p, **ai);
}


// Advance to the next goal.
Prop_iter
flatten_right_atom(Proof p, Cons const& c)
{
  return advance(p.sequent().consequents());
}


// Replace the current consequent with its operand (maybe).
// Parameterized constraints are essentially transparent, so
// they can be reduced immediately.
Prop_iter
flatten_right_lambda(Proof p, Parameterized_cons const& c)
{
  Cons const& c1 = c.constraint();
  return replace(p.sequent().consequents(), c1);
}


// Advance to the next term so we don't produce subgoals.
Prop_iter
flatten_right_conjunction(Proof p, Conjunction_cons const& c)
{
  return advance(p.sequent().consequents());
}


// Replace the current antecedent with its operands (maybe).
Prop_iter
flatten_right_disjunction(Proof p, Disjunction_cons const& c)
{
  Cons const& c1 = c.left();
  Cons const& c2 = c.right();
  return replace(p.sequent().consequents(), c1, c2);
}


// Select an appropriate action for the current proposition.
Prop_iter
flatten_right(Proof p, Cons const& c)
{
  struct fn
  {
    Proof p;
    Prop_iter operator()(Cons const& c)               { return flatten_right_atom(p, c); }
    Prop_iter operator()(Parameterized_cons const& c) { return flatten_right_lambda(p, c); }
    Prop_iter operator()(Conjunction_cons const& c)   { return flatten_right_conjunction(p, c); }
    Prop_iter operator()(Disjunction_cons const& c)   { return flatten_right_disjunction(p, c); }
  };
  return apply(c, fn{p});
}


// Flatten all terms in the consequents.
void
flatten_right(Proof p, Sequent& s)
{
  Prop_list& cs = s.consequents();
  auto ci = cs.start();
  while (ci != cs.end())
    ci = flatten_right(p, **ci);
}


// Flatten each sequent in the proof.
//
// FIXME: Cache the "flatness" of each constraint set so that we
// can avoid redundant computations.
//
// Note that we never expand terms on the right hand side after the
// first pass. This is because we expect that most concept are defined
// using conjunctions, and expanding on the right could consume a
// lot of memory (instead of just time).
void
flatten(Proof p)
{
  for (Sequent& s : p.goals()) {
    flatten_left(p, s);
    flatten_right(p, s);
  }
}

// -------------------------------------------------------------------------- //
// Expansion
//
// This tries to select an antecedent to expand. In general, we prefer
// to expand concepts before disjunctions unless the concept containts
// disjunctions.
//
// TODO: We need to cache properties of concepts so that we can quickly
// determine the relative cost of expanding those terms.


// Returns true if a is a better choice for expansion than b. In general,
// we always prefer to expand a concept before a disjunction UNLESS
//
// - the concept contains nested disjunction
// - the disjunction is relatively shallow
//
// Note that a and b can be atomic. We never prefer to constrain
// an atomic expression. In other words, a concept is better
// than anything except a concept. A disjunction is only better
// than atomic constraint.
//
// TODO: Implement ordering heuristics.
inline bool
is_better_expansion(Cons const* a, Cons const* b)
{
  // A concept is better than anything other than another concept.
  if (is<Concept_cons>(a)) {
    if (is<Concept_cons>(b))
      return false;
    return true;
  }

  // A disjunction is better than atomic constraints.
  if (is<Disjunction_cons>(a))
    return is_atomic(*b);

  return false;
}


// Expand the concept by substituting the template arguments
// throughthe concept's definition and normalizing the result.
//
// FIXME: WE should cache the expansion so we don't keep re-running
// the substitution.
Cons const&
expand(Context& cxt, Concept_cons const& c)
{
  // Oops. We need a mutable object for subsitution. *sigh*.
  Concept_cons& x = const_cast<Concept_cons&>(c); // Yuck.

  Concept_decl& decl = x.declaration();
  Decl_list& tparms = decl.parameters();
  Term_list& targs = x.arguments();
  Expr& def = decl.definition();

  // NOTE: Template arguments must have been checked (in kind?)
  // prior to the formation of the constraint. It's should be
  // a semantic requirement of the original check expression.
  Substitution sub(tparms, targs);

  Expr& e = substitute(cxt, def, sub);
  Cons& r = normalize(cxt, e);
  std::cout << "EXPAND: " << c << " ~> " << r << '\n';
  return r;
}


// Select a term in the sequent to expand. Collect all non-atomic
// terms and select the best term to expand.
//
// NOTE: We should never have conjunctions or parameterized constraints
// as non-atomic propositions in the list of antecedents. Those must
// have been flattened during the previous pass on the proof state.
//
// TODO: If we cache the "flattness" state, then we could skip the
// ordering step since no non-atomic constraint would be expanded.
void
expand_left(Proof& p, Sequent& s)
{
  Prop_list& ps = s.antecedents();

  // Select the best candidate to expand. Only expand if
  // the selected element is non-atomic.
  auto best = std::min_element(ps.begin(), ps.end(), is_better_expansion);
  if (Concept_cons const* c = as<Concept_cons>(*best))
    ps.replace(best, expand(p.context(), *c));
  else if (Disjunction_cons const* d = as<Disjunction_cons>(*best))
    ps.replace(best, d->left(), d->right());
}


// Find a concept, and expand it.
//
// FIXME: This only really works if a concept appears in the
// consequents of the goal. Any degree of conunjunctive nesting
// will ensure that this does not happen.
void
expand_right(Proof& p, Sequent& s)
{
  Prop_list& ps = s.consequents();

  // Replace the first concept.
  auto cmp = [](Cons const* c) { return is<Concept_cons>(c); };
  auto iter = std::find_if(ps.begin(), ps.end(), cmp);
  if (iter != ps.end()) {
    std::cout << "RIGHT: " << **iter << '\n';
    Concept_cons const& c = cast<Concept_cons>(**iter);
    ps.replace(iter, expand(p.context(), c));
  }
}



// Select, in each goal, a term to expand (and expand it).
//
// TODO: There are other interesting strategies. For example,
// we might choose to expand all concepts first.
void
expand(Proof p)
{
  for (Sequent& s : p.goals()) {
    expand_left(p, s);
    // expand_right(p, s);
  }
}


// -------------------------------------------------------------------------- //
// Subsumption


// Returns true if a subsumes c.
//
// TODO: How do I know when I've exhuasted all opportunities.
bool
subsumes(Context& cxt, Cons const& a, Cons const& c)
{
  // Try validating the proof by comparing constraitns.
  // This lets us avoid initial setup costs for the proof
  // state.
  Validation v = validate_implication(cxt, a, c);
  if (v == valid_proof)
    return true;
  if (v == invalid_proof)
    return false;

  // Alas... no quick check. We have to prove the implication.
  Goal_list goals(Sequent(a, c));
  Proof p(cxt, goals);

  std::cout << "INIT: " << p.sequent() << '\n';

  // Continue manipulating the proof state until we know that
  // the implication is valid or not.
  int n = 1;
  while (v == incomplete_proof) {
    // Opportunistically flatten sequents in each goal.
    flatten(p);
    std::cout << "STEP " << n << ": " << p.sequent() << '\n';

    // Having done that, determine if the proof is valid (or not).
    // In either case, we can stop.
    Validation v = validate_proof(p);
    std::cout << "RESULT: " << (int)v << '\n';
    if (v == valid_proof)
      return true;
    if (v == invalid_proof)
      return false;

    // Otherwise, select a term in each goal to expand.
    expand(p);
    ++n;

    // FIXME: Implementation limit.
    if (n > 20)
      lingo_unimplemented();
  }
  return false;
}


} // namespace banjo
