#include <stdio.h>

double factorial(int number);

int main(int argc, char const *argv[])
{
	for(int i = 0; i < 21 ; ++i){
		printf("%d! = %lf \n",i,factorial(i)  );
	}
	return 0;
}

double factorial(int number){
	if(number <=1){
		return 1;
	}

	else{
		return(number*factorial(number-1));
	}
}