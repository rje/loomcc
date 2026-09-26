// loomcc-do: preprocess
#define cat(a, b) a ## b
cat(%:, %:) cat(<, :) cat(%, >)
// loomcc-expect: %:%: <: %>
