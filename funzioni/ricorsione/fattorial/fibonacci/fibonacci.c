#include <stdio.h>

unsigned long long int fibo(int n);


int main(){
	for (int num = 0 ; num <= 10 ; num++){

	printf("Fibonacci (%d) is %llu \n", num, fibo(num) );
	}

	printf("Fibonacci (20) is %llu \n", fibo(20) );
	printf("Fibonacci (30) is %llu \n", fibo(30) );
	printf("Fibonacci (400) is %llu \n", fibo(40) );


}



unsigned long long int fibo(int n){
	if( n == 0 || n ==1 ){
		return n;
	}
	else{
		return fibo(n-1) + fibo(n-2);
	}
}