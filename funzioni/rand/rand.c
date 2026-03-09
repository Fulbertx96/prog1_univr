//gioco cosino basato sulla generazione di numeri casuali

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


enum Status{
				CONTINUE,WON,LOST
			};

int roll();



int main(){

	srand(time(NULL));

	int myPoint =0;
	enum Status gameStatus = CONTINUE;
	int sum = roll(); //primo lancio di dadi

	//operatore di switch per capire come è andato il primo lancio
	//in base al risultato , la partita potrbbe ;
	//continuare, o finire a favore della casa o del giocatore

	switch(sum){
			//vincita al primo lancio
		case 7:
		case 11:
			gameStatus = WON;
			break;


			//perdita al primo lancio
		case 2:
		case 3:
		case 12:
			gameStatus = LOST;

			break;
		default:
			gameStatus = CONTINUE;

			myPoint = sum;	
			printf("%s %d \n","Point is ",myPoint );

			break;

	}




	while(CONTINUE == gameStatus){
		sum = roll();
		if(sum == myPoint){
			gameStatus = WON;
		}
		else if(7 == sum){
			gameStatus = LOST;
		}

	}

	if(WON == gameStatus){
		puts("Player wins");
	}
	else{
		puts("player loses");
	}


}



//funzione per il lancio del dado

int roll(){
	int die1 = 1 + rand() % 6 ;
	int die2 = 1 + rand() % 6 ;

	printf("Player rolled %d + %d \n",die1 , die2);
	return die1 + die2; 
}