#include<stdio.h>
//Test case - redundant assignment elimination
int main(){
	int a=3,b=4,c;
	a=3; //redundant assignment elimination
	b=4; // 
	c = a+b;
	return a;
}