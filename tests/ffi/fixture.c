typedef struct { int a; int b; } Pair;
int c_add(int a, int b) { return a + b; }
int c_pair_sum(Pair p) { return p.a + p.b; }
Pair c_pair_make(int a, int b) { Pair p = {a, b}; return p; }
double c_scale(double value, double factor) { return value * factor; }
int c_read_int(const int* value) { return *value; }
void c_increment(int* value) { ++*value; }
