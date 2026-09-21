#include<stdio.h>
//Test case - Copy propagation

int main(){
	int b=4, c,d,e;	
	c=d;//copy propagation	
	e = c+b; 
	return e;
}