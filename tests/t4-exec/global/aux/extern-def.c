/* support file for extern-in-other-unit.c */
typedef short i16;
i16 shared_value = 42;
i16 shared_array[3] = { -1, -2, -3 };
i16 get_twice(void) { return (i16)(shared_value * 2); }
