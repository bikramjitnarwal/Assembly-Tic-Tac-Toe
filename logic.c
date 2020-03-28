#include <stdio.h>
#include <stdbool.h>

void printBoard(int board[], int playerA, int playerB);
int requestValidInput(int board[], int playerA, int playerB);
int checkForWinner(int board[], int playerA, int playerB);
bool checkforStalemate(int board[]);

int main(void){
    int board[9], i, validIndex, winner;
    const int playerA = 10;
    const int playerB = 11;
    char turn = 'A';

    bool gameOver = false;
    // Initial entire board to 0
    for(i = 0; i < 9; i++){
        board[i] = 0;
    }
  
    while(gameOver == false){
        printf("The current state of the Tic-tac-toe Board:\n\n");
        printBoard(board, playerA, playerB);

        printf("\nIt is Player %c’s turn.\nPlease enter a valid position to play.\n", turn);
        validIndex = requestValidInput(board, playerA, playerB);
        if(turn == 'A'){
            board[validIndex] = playerA;
            turn = 'B';
        } else {
            board[validIndex] = playerB;
            turn = 'A';
        }
        winner = checkForWinner(board, playerA, playerB);
        if(winner != 0){
            gameOver = true;
        // Checks if every position has been filled, if it has that means someone has won
        } else if(checkforStalemate(board))
            gameOver = true;
    }

    // Checks for winner and displays the final board
    if(playerA == winner){
        printf("Player A wins!\n\n");
        printBoard(board, playerA, playerB);
    } else if(playerB == winner){
        printf("Player B wins!\n\n");
        printBoard(board, playerA, playerB);
    } else {
        printf("It’s a draw!\n\n");
        printBoard(board, playerA, playerB);
    }
    return 0;
}

// Prints the game board
void printBoard(int board[], int playerA, int playerB){
    for(int i = 0; i < 9; i++){
        // If the value at index i is 10, that means it is player A's spot
        if(board[i] == playerA){
            printf(" A");
        } else if (board[i] == playerB){
        // If the value at index i is 11, that means it is player B's spot
            printf(" B");
        } else {
            printf(" %d", i + 1);
        }
        // New line of board
        if(i % 3 == 2){
            printf("\n");
        }
    }
}

// Ensures that each position is not claimed twice
int requestValidInput(int board[], int playerA, int playerB){
    int index;
    scanf("%d", &index);

    while(true){
        if(index > 0 && index < 10){
            // User input is one more than array index so we need to subtract it
            index = index - 1;
            // If index is not 0, that means it is claimed
            if(board[index] != 0){
                printf("That position has already been played, please try again.\n");
                scanf("%d", &index);
            } else {
                return index;
            }
            
        } else {
            // User entered invalid position
            printf("Invalid input, please try again.\n");
            scanf("%d", &index);
        }
    }
}

// Functions checks every possibly win (3 in a row) for either player and returns the winner
int checkForWinner(int board[], int playerA, int playerB){
    for(int i = 0; i < 3; i++){
        if(playerA == board[i + 0] && playerA == board[i + 3] && playerA == board[i + 6]){
            return playerA;
        }

        if(playerB == board[i + 0] && playerB == board[i + 3] && playerB == board[i + 6]){
            return playerB;
        }

        if(playerA == board[i * 3 + 0] && playerA == board[i * 3 + 1] && playerA == board[i * 3 + 2]){
            return playerA;
        }

        if(playerB == board[i * 3 + 0] && playerB == board[i * 3 + 1] && playerB == board[i * 3 + 2]){
            return playerB;
        }
    }

    if(playerA == board[0] && playerA == board[4] && playerA == board[8]){
        return playerA;
    }

    if(playerB == board[0] && playerB == board[4] && playerB == board[8]){
        return playerB;
    }
    
    if(playerA == board[2] && playerA == board[4] && playerA == board[6]){
        return playerA;
    }
    
    if (playerB == board[2] && playerB == board[4] && playerB == board[6]){
        return playerB;
    }
    return 0;
}

// Checks if every position has been filled
bool checkforStalemate(int board[]){
    for(int index = 0; index < 9; index++){
        // 0 means no one has claimed that position
        if(board[index] == 0){
            return false;
        }
    }
    return true;
}
