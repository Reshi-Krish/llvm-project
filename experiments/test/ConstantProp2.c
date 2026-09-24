#include<stdio.h>
//Test case - Copy propagation

int main(){
	int b=4, c,d,e;	
	c=b;//copy propagation	
	e = c+b; 
	return e;
}