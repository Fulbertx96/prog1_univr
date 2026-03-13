#include <stdio.h>


int square(int number);

int main(){


	int x ;
	for(x = 1 ;x <= 10 ; x++){
		printf(" %d",  square(x) );
	}

	puts("");

}


int square(int number){

	return number*number;
}