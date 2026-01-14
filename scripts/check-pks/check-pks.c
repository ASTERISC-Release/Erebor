#include <stdio.h>
#define BIT_MASK(x) (1ULL << x)

int main(){
    int a = 7, b, c = 0, d;
    __asm__(
        "cpuid \n\t"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(a), "2"(c)
    );

    if((c & BIT_MASK(31)) == 0){
        printf("PKS not supported.\n");
    }
    else{
        printf("PKS supported.\n");
    }
    return 0;
}
