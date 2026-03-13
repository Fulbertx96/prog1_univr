//progaramma che illustra il campo di azione delle variabili

#include <stdio.h>


//prototipi di funzioni

void userLocal();
void userStaticLocal();
void userGlobal();

int x = 1;

int main(){
	int x = 5;	
	printf("\n%s %d","local x outer scope of main is ",x );
	{
		int x = 7;
		printf("\nlocal x in inner scope of main is %d ", x);
	}

	printf("\n%s %d \n","local x outer scope of main is ",x );

	userLocal();
	userStaticLocal();
	userGlobal();
	userLocal();
	userStaticLocal();
	userGlobal();

	printf("\nlocal x in main is %d \n",x);

}




void userLocal(){

	int x = 25;
	printf("\nlocal x in userLocal is %d after entering userLocal\n",x);
	++x;
	printf("local x in userLocal is %d before exiting userLocal \n",x);


}
void userStaticLocal(){

	static int x = 50;
	printf("\nlocal static x  is %d on entering userStaticLocal\n",x);
	++x;
	printf("local static x  is %d on exiting userStaticLocal \n",x);
}


void userGlobal(){

	printf("\nglobal x  is %d on entering userGlobal\n",x);
	x *=10;
	printf("global x  is %d on exiting userGlobal \n",x);
}


