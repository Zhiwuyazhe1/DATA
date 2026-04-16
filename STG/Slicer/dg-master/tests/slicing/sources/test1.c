/* basic test */

int main(void) {
    int a, b, c;
    a = 0;
    b = 1;
    c = 3;

    a = b + c;
    b = 3;
    c = 5;

    test_assert(a == 4);
    return 0;
}
// ./llvm-slicer -sc '##9#a;' -annotate=slice /home/nishikino/new_version_benchmarks/dg-master/tests/slicing/test1.bc
//  clang -c -g -emit-llvm -include test_assert.h ./sources/test1.c 