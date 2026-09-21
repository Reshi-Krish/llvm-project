#include <stdio.h>

int main() {
    int a = 15;
    int b = 20;
    if (a < b) {//this condition is true
        printf("inside if %d %d\n",a,b);//this will execute
    } else {
        printf("inside else\n");//this will not execute
    }
    return 0;
}


