// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-digraphs] 816-tcc does not know digraphs
// Digraphs behave like the tokens they stand for, and keep their spelling.
%:define X <: :> <% %>
X
%: define Y 1
Y
// loomcc-expect: <: :> <% %>
// loomcc-expect: 1
