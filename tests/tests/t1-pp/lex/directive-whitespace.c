// loomcc-do: preprocess
   #   define   X   3
	#	define	Y	4
/* comment */ # /* comment */ define Z 5
X Y Z
// loomcc-expect: 3 4 5
