#include <stdbool.h>
#include <stdio.h>

// Functions related to keyboard interrupts set-up
void disable_A9_interrupts(void);
void set_A9_IRQ_stack(void);
void config_GIC(void);
void config_KEYs(void);
void enable_A9_interrupts(void);
void keyboard_ISR(void);
void config_interrupt(int, int);

// Functions for drawing objects onto the screen
void draw_player(int boardIndex);
void draw_player_X(int boardIndex);
void draw_player_O(int boardIndex);
void initial_screen();
void plot_pixel(int x, int y, short int line_color);
void draw_line(int x0, int y0, int x1, int y1, short int line_color);
void draw_selection_box(int x, int y, short int selection_colour);
void swap(int *first, int *second);
void draw_board(void);
void write_text(int x, int y, char * text_ptr);
void clear_screen();

// Functions which handle the tic-tac-toe logic
int check_winner();
void clear_text ();
void checkforStalemate();
void AI_move();

// Global variables
int selection_x;
int selection_y;
bool isStalemate = false;
char Turn;
int board[9]; 
volatile int pixel_buffer_start; // global variable, to draw 

int main(void) {
	clear_text();
	
	// First turn goes to X
	Turn = 'X';
	
	// This is the top left corner of the first box
	// Red selection box starts here initially
	selection_x = 25;
	selection_y = 25;
	
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    
	/* Read location of the pixel buffer from the pixel buffer controller */
    pixel_buffer_start = *pixel_ctrl_ptr;
	
	clear_screen();
	initial_screen();
	
	disable_A9_interrupts(); // disable interrupts in the A9 processor
	set_A9_IRQ_stack(); // initialize the stack pointer for IRQ mode
	config_GIC(); // configure the general interrupt controller
	config_KEYs(); // configure pushbutton KEYs to generate interrupts
	enable_A9_interrupts(); // enable interrupts in the A9 processor
	
	while (1); // wait for an interrupt
}


/* setup the PS/2 interrupts in the FPGA */
void config_KEYs() {
	volatile int * PS2_ptr = (int *) 0xFF200100; // PS/2 base address
	*(PS2_ptr + 1) = 0x00000001; // set RE to 1 to enable interrupts
}

// Define the IRQ exception handler
void __attribute__((interrupt)) __cs3_isr_irq(void) {
	// Read the ICCIAR from the CPU Interface in the GIC
	int interrupt_ID = *((int *)0xFFFEC10C);
	if (interrupt_ID == 79) // check if interrupt is from the KEYs
	keyboard_ISR();
	else
	while (1); // if unexpected, then stay here
	// Write to the End of Interrupt Register (ICCEOIR)
	*((int *)0xFFFEC110) = interrupt_ID;
}

// Define the remaining exception handlers
void __attribute__((interrupt)) __cs3_reset(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_undef(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_swi(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_pabort(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_dabort(void) {
	while (1);
}

void __attribute__((interrupt)) __cs3_isr_fiq(void) {
	while (1);
}

//Initialize the banked stack pointer register for IRQ mode
void set_A9_IRQ_stack(void) {
	int stack, mode;
	stack = 0xFFFFFFFF - 7; // top of A9 onchip memory, aligned to 8 bytes
	/* change processor to IRQ mode with interrupts disabled */
	mode = 0b11010010;
	asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
	/* set banked stack pointer */
	asm("mov sp, %[ps]" : : [ps] "r"(stack));
	/* go back to SVC mode before executing subroutine return! */
	mode = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
}

/*
* Turn on interrupts in the ARM processor
*/
void enable_A9_interrupts(void) {
	int status = 0b01010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}

// Turn off interrupts in the ARM processor
void disable_A9_interrupts(void) {
	int status = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}

/*
* Configure the Generic Interrupt Controller (GIC)
*/
void config_GIC(void) {
	config_interrupt (79, 1); // configure the FPGA KEYs interrupt (73)
	// Set Interrupt Priority Mask Register (ICCPMR). Enable interrupts of all
	// priorities
	*((int *) 0xFFFEC104) = 0xFFFF;
	// Set CPU Interface Control Register (ICCICR). Enable signaling of
	// interrupts
	*((int *) 0xFFFEC100) = 1;
	// Configure the Distributor Control Register (ICDDCR) to send pending
	// interrupts to CPUs
	*((int *) 0xFFFED000) = 1;
}

/*
* Configure Set Enable Registers (ICDISERn) and Interrupt Processor Target
* Registers (ICDIPTRn). The default (reset) values are used for other registers
* in the GIC.
*/
void config_interrupt(int N, int CPU_target) {
	int reg_offset, index, value, address;
	/* Configure the Interrupt Set-Enable Registers (ICDISERn).
	* reg_offset = (integer_div(N / 32) * 4
	* value = 1 << (N mod 32) */
	reg_offset = (N >> 3) & 0xFFFFFFFC;
	index = N & 0x1F;
	value = 0x1 << index;
	address = 0xFFFED100 + reg_offset;
	/* Now that we know the register address and value, set the appropriate bit */
	*(int *)address |= value;

	/* Configure the Interrupt Processor Targets Register (ICDIPTRn)
	* reg_offset = integer_div(N / 4) * 4
	* index = N mod 4 */
	reg_offset = (N & 0xFFFFFFFC);
	index = N & 0x3;
	address = 0xFFFED800 + reg_offset + index;
	/* Now that we know the register address and value, write to (only) the
	* appropriate byte */
	*(char *)address = (char)CPU_target;
}

void plot_pixel(int x, int y, short int line_color)
{
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

// Clear screen by writing black into the address
void clear_screen (){
	int y,x;
	for(x=0;x<320;x++){
		for(y=0;y<240;y++){
			plot_pixel(x,y,0x0000);
		}
	}
}

// Clear any text on the screen by writing " " into the address
void clear_text (){
	int y,x;
	for(x=0;x<80;x++){
		for(y=0;y<60;y++){
			char clear[1] = " \0";
			write_text(x, y, clear);
		}
	}
}

// Function which handles what to do once a keyboard interrupt is given
void keyboard_ISR(void) {

	volatile int * PS2_ptr = (int *)0xFF200100; // Points to PS2 Base
	unsigned char byte0 = 0;
    
	int PS2_data = *(PS2_ptr);
	int RVALID = PS2_data & 0x8000;
	
	//Read Interrupt Register
	int readInterruptReg;
	readInterruptReg = *(PS2_ptr + 1 ); 
	  
	//Clear Interrupt 
	*(PS2_ptr+1) = readInterruptReg; 

	// when RVALID is 1, there is data 
	if (RVALID != 0){
               
		byte0 = (PS2_data & 0xFF); //data in LSB	
	
		if(byte0 == 0x22){  //X, start game
			clear_screen();
			clear_text();
			draw_board();
			draw_selection_box(selection_x, selection_y, 0xF800);
			isStalemate = false;
			char player_status[150] = "                    Player X's Turn!                      \0";
			write_text(14, 55, player_status);
		}
		
		if(byte0 == 0x1D){  //UP, W
			// Erase currently drawn selection box by drawing it black
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			selection_y = selection_y - 63;
			
			// Loop back to the first box
			if (selection_y == -38){
				selection_y = 151;
			}
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}

		if(byte0 == 0x1B){ //DOWN, S
			// Erase currently drawn selection box by drawing it black
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			selection_y = selection_y + 63;
			
			// Loop back to the first box
			if (selection_y == 214){
				selection_y = 25;
			}
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
	
		if(byte0 == 0x1C){ //LEFT, A
			// Erase currently drawn selection box by drawing it black
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			selection_x = selection_x - 90;
			
			// Loop back to the first box
			if (selection_x == -65){
				selection_x = 205;
			}
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}

		if(byte0 == 0x23){ //RIGHT, D
			// Erase currently drawn selection box by drawing it black
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			selection_x = selection_x + 90;
			
			// Loop back to the first box
			if (selection_x == 295){
				selection_x = 25;
			}
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}

		if(byte0 == 0x29){  //SpaceBar , Restart Game
			clear_screen(0,0,0x0000); 
			clear_text();
			draw_board();
			
			Turn = 'X';
			memset(board, 0, sizeof(board));
			
			char clear_winner_status[150] = "                                                     \0";                             
			write_text(14, 55, clear_winner_status);
			
			// Reinitialize selection box to the top left box
			selection_x = 25;
			selection_y = 25;
			draw_selection_box(selection_x, selection_y, 0xF800);
			
			char player_status[150] = "                    Player X's Turn!                      \0";
			write_text(14, 55, player_status);
			isStalemate = false;

		}  
		
		if(byte0 == 0x16){ //Select Box 1 
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 25;
			selection_y = 25;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if(byte0 == 0x1E){ //Select Box 2 
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 115;
			selection_y = 25;
			
			draw_selection_box(selection_x, selection_y, 0xF800);

		}
		
		if(byte0 == 0x26){ //Select Box 3 
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 205;
			selection_y = 25;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if(byte0 == 0x25){//Select Box 4
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 25;
			selection_y = 88;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if(byte0 == 0x2E){//Select Box 5
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 115;
			selection_y = 88;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if(byte0 == 0x36){//Select Box 6
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 205;
			selection_y = 88;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if(byte0 == 0x3D){//Select Box 7
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 25;
			selection_y = 151;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if(byte0 == 0x3E){//Select Box 8
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 115;
			selection_y = 151;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if(byte0 == 0x46){//Select Box 9
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 205;
			selection_y = 151;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if(byte0 == 0x33){//H-Help Screen
			clear_screen();			
			clear_text();
			
			char title[100] = "Tic-Tac-Toe Help Screen\0";
			write_text(28, 3, title);
			
			char instructions[100] = "Try to get 3 consecutive boxes to win the game!\0";
			write_text(8, 7, instructions);
			
			char controls[20] = "Game Controls: \0";
			write_text(8, 10, controls);
			
			char number_keys[70] = "[1]-[9]: Select board index\0";
			write_text(8, 13, number_keys);
			
			char selection_key_a[70] = "[A]: Move red selection box left\0";
			write_text(8, 15, selection_key_a);
			
			char selection_key_d[70] = "[D]: Move red selection box right\0";
			write_text(8, 17, selection_key_d);
			
			char selection_key_w[70] = "[W]: Move red selection box up\0";
			write_text(8, 19, selection_key_w);
			
			char selection_key_s[70] = "[S]: Move red selection box down\0";
			write_text(8, 21, selection_key_s);	
			
			char enter[70] = "[enter]: Place piece/ Make a move\0";
			write_text(8, 23, enter);
			
			char help[70] = "[H]: Help screen\0";
			write_text(8, 25, help);
			
			char spacebar[70] = "[spacebar]: Restart game\0";
			write_text(8, 27, spacebar);	
			
			char resume[70] = "Press [ESC] to resume the game\0";
			write_text(8, 30, resume);	
		}
		
		if(byte0 == 0x76){ //Escape - Resume game
			clear_screen();
			clear_text();
			draw_board();
			draw_selection_box(selection_x, selection_y, 0xF800);
			
			for (int i = 0; i < 9; i++){
				if (board[i] == 1){
					draw_player_X(i+1);
				} else if (board[i] == 2){
					draw_player_O(i+1);
				}
			}
			
			if (Turn == 'X'){
				Turn = 'O';
				char player_status[150] = "                    Player O's Turn!                      \0";
				write_text(14, 55, player_status);
			} else {
				Turn = 'X';
				char player_status[150] = "                    Player X's Turn!                      \0";
				write_text(14, 55, player_status);
			}
		}

		if (byte0 == 0x21) { //C - AI makes a move if this is clicked
			//clear_text();
			AI_move();

			// check winner
			int winner = check_winner();
			
			// No winner
			if (winner == 0){
				// Switch turn 
				if (Turn == 'X'){
					Turn = 'O';
					char player_status[150] = "                    Player O's Turn!                      \0";
					write_text(14, 55, player_status);
				} else {
					Turn = 'X';
					char player_status[150] = "                    Player X's Turn!                      \0";
					write_text(14, 55, player_status);
				}
				
			// X wins
			} else if (winner == 1){
				// hide selection box
				draw_selection_box(selection_x, selection_y, 0x0000);
				draw_board();
				
				// show winner status & prompt new game
				char winner_status[150] = "Player X Wins! Press [spacebar] to start a new game.\0";
				write_text(14, 55, winner_status);
				
			// O wins
			} else if (winner == 2){
				// hide selection box
				draw_selection_box(selection_x, selection_y, 0x0000);
				draw_board();
				
				// show winner status & prompt new game
				char winner_status[150] = "Player O Wins! Press [spacebar] to start a new game.\0";
				write_text(14, 55, winner_status);
			
			// Stalemate
			} else if (winner == 3){
				// hide selection box
				draw_selection_box(selection_x, selection_y, 0x0000);
				draw_board();
				
				// show tie status & prompt new game
				char winner_status[150] = "It's a tie! Press [spacebar] to start a new game.\0";
				write_text(14, 55, winner_status);
			}

		}
		
		if(byte0 == 0x5A){ //Enter - place piece on board
			// check which board index 
			int boardIndex = 0; 
			
			if (selection_x == 25 & selection_y == 25){
				boardIndex = 1; 
			} else if (selection_x == 115 & selection_y == 25){
				boardIndex = 2; 
			} else if (selection_x == 205 & selection_y == 25){
				boardIndex = 3; 
			} else if (selection_x == 25 & selection_y == 88){
				boardIndex = 4; 
			} else if (selection_x == 115 & selection_y == 88){
				boardIndex = 5; 
			} else if (selection_x == 205 & selection_y == 88){
				boardIndex = 6; 
			} else if (selection_x == 25 & selection_y == 151){
				boardIndex = 7; 
			} else if (selection_x == 115 & selection_y == 151){
				boardIndex = 8; 
			} else if (selection_x == 205 & selection_y == 151){
				boardIndex = 9; 
			}
			
			// Only draw if box is empty
			if (board[boardIndex - 1] == 0){
				
				// update array	
				// 0 means empty, 1 means there is an X, 2 means there is an O
				if (Turn == 'X'){
					board[boardIndex - 1] = 1;
				} else {
					board[boardIndex - 1] = 2;
				}
				
				// draw player
				draw_player(boardIndex);
				
				// check winner
				int winner = check_winner();
				
				// No winner
				if (winner == 0){
					// Switch turn 
					if (Turn == 'X'){
						Turn = 'O';
						char player_status[150] = "                    Player O's Turn!                      \0";
						write_text(14, 55, player_status);
					} else {
						Turn = 'X';
						char player_status[150] = "                    Player X's Turn!                      \0";
						write_text(14, 55, player_status);
					}
					
				// X wins
				} else if (winner == 1){
					// hide selection box
					draw_selection_box(selection_x, selection_y, 0x0000);
					draw_board();
					
					// show winner status & prompt new game
					char winner_status[150] = "Player X Wins! Press [spacebar] to start a new game.\0";
					write_text(14, 55, winner_status);
					
				// O wins
				} else if (winner == 2){
					// hide selection box
					draw_selection_box(selection_x, selection_y, 0x0000);
					draw_board();
					
					// show winner status & prompt new game
					char winner_status[150] = "Player O Wins! Press [spacebar] to start a new game.\0";
					write_text(14, 55, winner_status);
				
				// Stalemate
				} else if (winner == 3){
					// hide selection box
					draw_selection_box(selection_x, selection_y, 0x0000);
					draw_board();
					
					// show tie status & prompt new game
					char winner_status[150] = "It's a tie! Press [spacebar] to start a new game.\0";
					write_text(14, 55, winner_status);
				}
			}
		}
				
		if(byte0 == 0xF0) {
			// Check for break
			switch (PS2_data & 0xFF) {
				case 0x1D:
					break;
				case 0x1B:
					break;
				case 0x1C:
					break;
				case 0x23:
					break;
				default:
					break;
			}	
		}
					
	} 
	return;
}

void draw_line(int x0, int y0, int x1, int y1, short int line_color) {
    bool is_steep = ( abs(y1 - y0) > abs(x1 - x0) );
	
    if (is_steep) {
        swap(&x0, &y0);
        swap(&x1, &y1);
    }
   
    if (x0 > x1) {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }
    
    int delta_x = x1 - x0;
    int delta_y = abs(y1 - y0);
    int error = -(delta_x / 2);
    
    int y = y0;
    int y_step;
    if (y0 < y1) 
        y_step =1;
    else 
        y_step = -1;
    
    for(int x = x0; x <= x1; x++) {
        if (is_steep) 
            plot_pixel(y, x, line_color);
        else 
            plot_pixel(x, y, line_color);
        
        error += delta_y;
        
        if (error >= 0) {
            y +=y_step;
            error -= delta_x;
        }
    } 
}

void swap(int *first, int *second){
	int temp = *first;
    *first = *second;
    *second = temp;   
}

void draw_board(void){
	draw_line(114, 25, 114, 213, 0XFFFF);
	draw_line(115, 25, 115, 213, 0XFFFF);
	draw_line(116, 25, 116, 213, 0XFFFF);
	
	draw_line(204, 25, 204, 213, 0XFFFF);
	draw_line(205, 25, 205, 213, 0XFFFF);
	draw_line(206, 25, 206, 213, 0XFFFF);

	draw_line(25, 87, 295, 87, 0XFFFF);
	draw_line(25, 88, 295, 88, 0XFFFF);
	draw_line(25, 89, 295, 89, 0XFFFF);
	
	draw_line(25, 150, 295, 150, 0XFFFF);
	draw_line(25, 151, 295, 151, 0XFFFF);
	draw_line(25, 152, 295, 152, 0XFFFF);
	
	char text_top_row[100] = "Welcome to Tic-Tac-Toe!\0";
	write_text(28, 3, text_top_row);
	
	// Top left box 
	char top_left_box_number[10] = "1\0";
	write_text(8, 7, top_left_box_number);
	
	// Top middle box 
	char top_middle_box_number[10] = "2\0";
	write_text(30, 7, top_middle_box_number);
	
	// Top right box 
	char top_right_box_number[10] = "3\0";
	write_text(53, 7, top_right_box_number);
	
	// Middle left box 
	char middle_left_box_number[10] = "4\0";
	write_text(8, 24, middle_left_box_number);
	
	// Middle middle box 
	char middle_middle_box_number[10] = "5\0";
	write_text(30, 24, middle_middle_box_number);
	
	// Middle right box 
	char middle_right_box_number[10] = "6\0";
	write_text(53, 24, middle_right_box_number);
	
	// Bottom left box 
	char bottom_left_box_number[10] = "7\0";
	write_text(8, 39, bottom_left_box_number);
	
	// Bottom middle box 
	char bottom_middle_box_number[10] = "8\0";
	write_text(30, 39, bottom_middle_box_number);
	
	// Bottom right box 
	char bottom_right_box_number[10] = "9\0";
	write_text(53, 39, bottom_right_box_number);
	
	char winner_status[50] = "Press [H] for help screen.";
	write_text(5, 57, winner_status);
}

void write_text(int x, int y, char * text_ptr) {
	int offset;
	volatile char * character_buffer = (char *)0xC9000000; // video character buffer
	
	/* assume that the text string fits on one line */
	offset = (y << 7) + x;
	
	while (*(text_ptr)) {
		*(character_buffer + offset) = *(text_ptr); // write to the character buffer
		++text_ptr;
		++offset;
	}
}


void draw_selection_box(int x, int y, short int selection_colour) {
	draw_line(x, y, x + 90, y, selection_colour);
	draw_line(x + 90, y, x + 90, y + 63, selection_colour);
	draw_line(x + 90, y + 63, x, y + 63, selection_colour);
	draw_line(x, y + 63, x, y, selection_colour);
}

void draw_player(int boardIndex){
	if(Turn == 'X'){
		draw_player_X(boardIndex);
	} else {
		draw_player_O(boardIndex);
	}
}

void draw_player_X(int boardIndex){
	// left diagonal coordinates
	int initial_left_X0 = 29, initial_left_Y0 = 29, initial_left_X1 = 111, initial_left_Y1 = 84;
	// right diagonal coordinates
	int initial_right_X0 = 111, initial_right_Y0 = 29, initial_right_X1 = 29, initial_right_Y1 = 84;
	
	if(boardIndex == 1){
		draw_line(initial_left_X0, initial_left_Y0, initial_left_X1, initial_left_Y1, 0xFFFF);
		draw_line(initial_right_X0, initial_right_Y0, initial_right_X1, initial_right_Y1, 0xFFFF);
		
	} else if(boardIndex == 2){
		draw_line(initial_left_X0 + 90, initial_left_Y0, initial_left_X1 + 90, initial_left_Y1, 0xFFFF);
		draw_line(initial_right_X0 + 90, initial_right_Y0, initial_right_X1 + 90, initial_right_Y1, 0xFFFF);	
		
	} else if(boardIndex == 3){
		draw_line(initial_left_X0 + 180, initial_left_Y0, initial_left_X1 + 180, initial_left_Y1, 0xFFFF);
		draw_line(initial_right_X0 + 180, initial_right_Y0, initial_right_X1 + 180, initial_right_Y1, 0xFFFF);	
		
	} else if(boardIndex == 4){
		draw_line(initial_left_X0, initial_left_Y0 + 63, initial_left_X1, initial_left_Y1 + 63, 0xFFFF);
		draw_line(initial_right_X0, initial_right_Y0 + 63, initial_right_X1, initial_right_Y1 + 63, 0xFFFF);
	
	} else if(boardIndex == 5){
		draw_line(initial_left_X0 + 90, initial_left_Y0 + 63, initial_left_X1 + 90, initial_left_Y1 + 63, 0xFFFF);
		draw_line(initial_right_X0 + 90, initial_right_Y0 + 63, initial_right_X1 + 90, initial_right_Y1 + 65, 0xFFFF);
		
	} else if(boardIndex == 6){
		draw_line(initial_left_X0 + 180, initial_left_Y0 + 63, initial_left_X1 + 180, initial_left_Y1 + 63, 0xFFFF);
		draw_line(initial_right_X0 + 180, initial_right_Y0 + 63, initial_right_X1 + 180, initial_right_Y1 + 63, 0xFFFF);
		
	} else if(boardIndex == 7){
		draw_line(initial_left_X0, initial_left_Y0 + 126, initial_left_X1, initial_left_Y1 + 126, 0xFFFF);
		draw_line(initial_right_X0, initial_right_Y0 + 126, initial_right_X1, initial_right_Y1 + 126, 0xFFFF);
		
	} else if(boardIndex == 8){
		draw_line(initial_left_X0 + 90, initial_left_Y0 + 126, initial_left_X1 + 90, initial_left_Y1 + 126, 0xFFFF);
		draw_line(initial_right_X0 + 90, initial_right_Y0 + 126, initial_right_X1 + 90, initial_right_Y1 + 126, 0xFFFF);
		
	} else if(boardIndex == 9){
		draw_line(initial_left_X0 + 180, initial_left_Y0 + 126, initial_left_X1 + 180, initial_left_Y1 + 126, 0xFFFF);
		draw_line(initial_right_X0 + 180, initial_right_Y0 + 126, initial_right_X1 + 180, initial_right_Y1 + 126, 0xFFFF);
	}
}
	
void draw_player_O(int boardIndex){
	int UpLine_R_X = 98, UpLine_R_Y = 27, UpLine_L_X = 42, UpLine_L_Y = 27;
	int BottomLine_R_X = 98, BottomLine_R_Y = 86, BottomLine_L_X = 42, BottomLine_L_Y = 86;
	
	int LeftLine_U_X = 30, LeftLine_U_Y = 31, LeftLine_B_X = 30, LeftLine_B_Y = 82; 
	int RightLine_U_X = 110, RightLine_U_Y = 31, RightLine_B_X = 110, RightLine_B_Y = 82; 
	
	if(boardIndex == 1){
		draw_line(UpLine_R_X, UpLine_R_Y, UpLine_L_X, UpLine_L_Y, 0xFFFF);
		draw_line(UpLine_L_X, UpLine_L_Y, LeftLine_U_X, LeftLine_U_Y, 0xFFFF);
		draw_line(LeftLine_U_X, LeftLine_U_Y, LeftLine_B_X, LeftLine_B_Y, 0xFFFF);
		draw_line(LeftLine_B_X, LeftLine_B_Y, BottomLine_L_X, BottomLine_L_Y, 0xFFFF);
		draw_line(BottomLine_L_X, BottomLine_L_Y, BottomLine_R_X, BottomLine_R_Y, 0xFFFF);
		draw_line(BottomLine_R_X, BottomLine_R_Y, RightLine_B_X, RightLine_B_Y, 0xFFFF);
		draw_line(RightLine_B_X, RightLine_B_Y, RightLine_U_X, RightLine_U_Y, 0xFFFF);
		draw_line(RightLine_U_X, RightLine_U_Y, UpLine_R_X, UpLine_R_Y, 0xFFFF);
		
	} else if(boardIndex == 2){
		draw_line(UpLine_R_X + 90, UpLine_R_Y, UpLine_L_X + 90, UpLine_L_Y, 0xFFFF);
		draw_line(UpLine_L_X + 90, UpLine_L_Y, LeftLine_U_X + 90, LeftLine_U_Y, 0xFFFF);
		draw_line(LeftLine_U_X + 90, LeftLine_U_Y, LeftLine_B_X + 90, LeftLine_B_Y, 0xFFFF);
		draw_line(LeftLine_B_X + 90, LeftLine_B_Y, BottomLine_L_X + 90, BottomLine_L_Y, 0xFFFF);
		draw_line(BottomLine_L_X + 90, BottomLine_L_Y, BottomLine_R_X + 90, BottomLine_R_Y, 0xFFFF);
		draw_line(BottomLine_R_X + 90, BottomLine_R_Y, RightLine_B_X + 90, RightLine_B_Y, 0xFFFF);
		draw_line(RightLine_B_X + 90, RightLine_B_Y, RightLine_U_X + 90, RightLine_U_Y, 0xFFFF);
		draw_line(RightLine_U_X + 90, RightLine_U_Y, UpLine_R_X + 90, UpLine_R_Y, 0xFFFF);
		
	} else if(boardIndex == 3){
		draw_line(UpLine_R_X + 180, UpLine_R_Y, UpLine_L_X + 180, UpLine_L_Y, 0xFFFF);
		draw_line(UpLine_L_X + 180, UpLine_L_Y, LeftLine_U_X + 180, LeftLine_U_Y, 0xFFFF);
		draw_line(LeftLine_U_X + 180, LeftLine_U_Y, LeftLine_B_X + 180, LeftLine_B_Y, 0xFFFF);
		draw_line(LeftLine_B_X + 180, LeftLine_B_Y, BottomLine_L_X + 180, BottomLine_L_Y, 0xFFFF);
		draw_line(BottomLine_L_X + 180, BottomLine_L_Y, BottomLine_R_X + 180, BottomLine_R_Y, 0xFFFF);
		draw_line(BottomLine_R_X + 180, BottomLine_R_Y, RightLine_B_X + 180, RightLine_B_Y, 0xFFFF);
		draw_line(RightLine_B_X + 180, RightLine_B_Y, RightLine_U_X + 180, RightLine_U_Y, 0xFFFF);
		draw_line(RightLine_U_X + 180, RightLine_U_Y, UpLine_R_X + 180, UpLine_R_Y, 0xFFFF);
				
	} else if(boardIndex == 4){
		draw_line(UpLine_R_X, UpLine_R_Y + 63, UpLine_L_X, UpLine_L_Y + 63, 0xFFFF);
		draw_line(UpLine_L_X, UpLine_L_Y + 63, LeftLine_U_X, LeftLine_U_Y + 63, 0xFFFF);
		draw_line(LeftLine_U_X, LeftLine_U_Y + 63, LeftLine_B_X, LeftLine_B_Y + 63, 0xFFFF);
		draw_line(LeftLine_B_X, LeftLine_B_Y + 63, BottomLine_L_X, BottomLine_L_Y + 63, 0xFFFF);
		draw_line(BottomLine_L_X, BottomLine_L_Y + 63, BottomLine_R_X, BottomLine_R_Y + 63, 0xFFFF);
		draw_line(BottomLine_R_X, BottomLine_R_Y + 63, RightLine_B_X, RightLine_B_Y + 63, 0xFFFF);
		draw_line(RightLine_B_X, RightLine_B_Y + 63, RightLine_U_X, RightLine_U_Y + 63, 0xFFFF);
		draw_line(RightLine_U_X, RightLine_U_Y + 63, UpLine_R_X, UpLine_R_Y + 63, 0xFFFF);

	} else if(boardIndex == 5){
		draw_line(UpLine_R_X + 90, UpLine_R_Y + 63, UpLine_L_X + 90, UpLine_L_Y + 63, 0xFFFF);
		draw_line(UpLine_L_X + 90, UpLine_L_Y + 63, LeftLine_U_X + 90, LeftLine_U_Y + 63, 0xFFFF);
		draw_line(LeftLine_U_X + 90, LeftLine_U_Y + 63, LeftLine_B_X + 90, LeftLine_B_Y + 63, 0xFFFF);
		draw_line(LeftLine_B_X + 90, LeftLine_B_Y + 63, BottomLine_L_X + 90, BottomLine_L_Y + 63, 0xFFFF);
		draw_line(BottomLine_L_X + 90, BottomLine_L_Y + 63, BottomLine_R_X + 90, BottomLine_R_Y + 63, 0xFFFF);
		draw_line(BottomLine_R_X + 90, BottomLine_R_Y + 63, RightLine_B_X + 90, RightLine_B_Y + 63, 0xFFFF);
		draw_line(RightLine_B_X + 90, RightLine_B_Y + 63, RightLine_U_X + 90, RightLine_U_Y + 63, 0xFFFF);
		draw_line(RightLine_U_X + 90, RightLine_U_Y + 63, UpLine_R_X + 90, UpLine_R_Y + 63, 0xFFFF);
	
	} else if(boardIndex == 6){
		draw_line(UpLine_R_X + 180, UpLine_R_Y + 63, UpLine_L_X + 180, UpLine_L_Y + 63, 0xFFFF);
		draw_line(UpLine_L_X + 180, UpLine_L_Y + 63, LeftLine_U_X + 180, LeftLine_U_Y + 63, 0xFFFF);
		draw_line(LeftLine_U_X + 180, LeftLine_U_Y + 63, LeftLine_B_X + 180, LeftLine_B_Y + 63, 0xFFFF);
		draw_line(LeftLine_B_X + 180, LeftLine_B_Y + 63, BottomLine_L_X + 180, BottomLine_L_Y + 63, 0xFFFF);
		draw_line(BottomLine_L_X + 180, BottomLine_L_Y + 63, BottomLine_R_X + 180, BottomLine_R_Y + 63, 0xFFFF);
		draw_line(BottomLine_R_X + 180, BottomLine_R_Y + 63, RightLine_B_X + 180, RightLine_B_Y + 63, 0xFFFF);
		draw_line(RightLine_B_X + 180, RightLine_B_Y + 63, RightLine_U_X + 180, RightLine_U_Y + 63, 0xFFFF);
		draw_line(RightLine_U_X + 180, RightLine_U_Y + 63, UpLine_R_X + 180, UpLine_R_Y + 63, 0xFFFF);
		
	} else if(boardIndex == 7){
		draw_line(UpLine_R_X, UpLine_R_Y + 126, UpLine_L_X, UpLine_L_Y + 126, 0xFFFF);
		draw_line(UpLine_L_X, UpLine_L_Y + 126, LeftLine_U_X, LeftLine_U_Y + 126, 0xFFFF);
		draw_line(LeftLine_U_X, LeftLine_U_Y + 126, LeftLine_B_X, LeftLine_B_Y + 126, 0xFFFF);
		draw_line(LeftLine_B_X, LeftLine_B_Y + 126, BottomLine_L_X, BottomLine_L_Y + 126, 0xFFFF);
		draw_line(BottomLine_L_X, BottomLine_L_Y + 126, BottomLine_R_X, BottomLine_R_Y + 126, 0xFFFF);
		draw_line(BottomLine_R_X, BottomLine_R_Y + 126, RightLine_B_X, RightLine_B_Y + 126, 0xFFFF);
		draw_line(RightLine_B_X, RightLine_B_Y + 126, RightLine_U_X, RightLine_U_Y + 126, 0xFFFF);
		draw_line(RightLine_U_X, RightLine_U_Y + 126, UpLine_R_X, UpLine_R_Y + 126, 0xFFFF);
		
	} else if(boardIndex == 8){
		draw_line(UpLine_R_X + 90, UpLine_R_Y + 126, UpLine_L_X + 90, UpLine_L_Y + 126, 0xFFFF);
		draw_line(UpLine_L_X + 90, UpLine_L_Y + 126, LeftLine_U_X + 90, LeftLine_U_Y + 126, 0xFFFF);
		draw_line(LeftLine_U_X + 90, LeftLine_U_Y + 126, LeftLine_B_X + 90, LeftLine_B_Y + 126, 0xFFFF);
		draw_line(LeftLine_B_X + 90, LeftLine_B_Y + 126, BottomLine_L_X + 90, BottomLine_L_Y + 126, 0xFFFF);
		draw_line(BottomLine_L_X + 90, BottomLine_L_Y + 126, BottomLine_R_X + 90, BottomLine_R_Y + 126, 0xFFFF);
		draw_line(BottomLine_R_X + 90, BottomLine_R_Y + 126, RightLine_B_X + 90, RightLine_B_Y + 126, 0xFFFF);
		draw_line(RightLine_B_X + 90, RightLine_B_Y + 126, RightLine_U_X + 90, RightLine_U_Y + 126, 0xFFFF);
		draw_line(RightLine_U_X + 90, RightLine_U_Y + 126, UpLine_R_X + 90, UpLine_R_Y + 126, 0xFFFF);
		
	} else if(boardIndex == 9){
		draw_line(UpLine_R_X + 180, UpLine_R_Y + 126, UpLine_L_X + 180, UpLine_L_Y + 126, 0xFFFF);
		draw_line(UpLine_L_X + 180, UpLine_L_Y + 126, LeftLine_U_X + 180, LeftLine_U_Y + 126, 0xFFFF);
		draw_line(LeftLine_U_X + 180, LeftLine_U_Y + 126, LeftLine_B_X + 180, LeftLine_B_Y + 126, 0xFFFF);
		draw_line(LeftLine_B_X + 180, LeftLine_B_Y + 126, BottomLine_L_X + 180, BottomLine_L_Y + 126, 0xFFFF);
		draw_line(BottomLine_L_X + 180, BottomLine_L_Y + 126, BottomLine_R_X + 180, BottomLine_R_Y + 126, 0xFFFF);
		draw_line(BottomLine_R_X + 180, BottomLine_R_Y + 126, RightLine_B_X + 180, RightLine_B_Y + 126, 0xFFFF);
		draw_line(RightLine_B_X + 180, RightLine_B_Y + 126, RightLine_U_X + 180, RightLine_U_Y + 126, 0xFFFF);
		draw_line(RightLine_U_X + 180, RightLine_U_Y + 126, UpLine_R_X + 180, UpLine_R_Y + 126, 0xFFFF);
	}
}

void initial_screen(){
	int offset = 20, offset2 = 15;
	
	for (int x = 0; x < 320; x++){
		for (int y = 0; y < 240; y++){
			plot_pixel(x, y, 0x00FF);
		}
	}

	// W
	draw_line(80, 40, 85, 70, 0xFFFF);
	draw_line(85, 70, 90, 55, 0xFFFF);
	draw_line(90, 55, 95, 70, 0xFFFF);
	draw_line(95, 70, 100, 40, 0xFFFF);
	
	// E
	draw_line(103, 40, 103, 70, 0xFFFF);
	draw_line(103, 40, 115, 40, 0xFFFF);
	draw_line(103, 70, 115, 70, 0xFFFF);
	draw_line(103, 55, 110, 55, 0xFFFF);
	
	// L 
	draw_line(118, 40, 118, 70, 0xFFFF);
	draw_line(118, 70, 130, 70, 0xFFFF);

	// C
	draw_line(133, 40, 133, 70, 0xFFFF);
	draw_line(133, 40, 145, 40, 0xFFFF);
	draw_line(133, 70, 145, 70, 0xFFFF);

	// O
	draw_line(148, 40, 160, 40, 0xFFFF);
	draw_line(148, 70, 160, 70, 0xFFFF);
	draw_line(148, 40, 148, 70, 0xFFFF);
	draw_line(160, 40, 160, 70, 0xFFFF);
	
	// M
	draw_line(163, 40, 163, 70, 0xFFFF);
	draw_line(163, 40, 170, 50, 0xFFFF);
	draw_line(170, 50, 177, 40, 0xFFFF);
	draw_line(177, 40, 177, 70, 0xFFFF);

	// E
	draw_line(180, 40, 180, 70, 0xFFFF);
	draw_line(180, 40, 192, 40, 0xFFFF);
	draw_line(180, 70, 192, 70, 0xFFFF);
	draw_line(180, 55, 187, 55, 0xFFFF);
	
	// T
	draw_line(204, 40, 220, 40, 0xFFFF);
	draw_line(212, 40, 212, 70, 0xFFFF);
	
	// O
	draw_line(224, 40, 236, 40, 0xFFFF);
	draw_line(224, 70, 236, 70, 0xFFFF);
	draw_line(224, 40, 224, 70, 0xFFFF);
	draw_line(236, 40, 236, 70, 0xFFFF);
	
	// T
	draw_line(42 + offset, 90, 57 + offset, 90, 0xFFFF);
	draw_line(50 + offset, 90, 50 + offset, 120, 0xFFFF);
	
	// I 
	draw_line(60 + offset, 90, 74 + offset, 90, 0xFFFF);
	draw_line(60 + offset, 120, 74 + offset, 120, 0xFFFF);
	draw_line(67 + offset, 90, 67 + offset, 120, 0xFFFF);

	// C
	draw_line(77 + offset, 90, 77 + offset, 120, 0xFFFF);
	draw_line(77 + offset, 90, 89 + offset, 90, 0xFFFF);
	draw_line(77 + offset, 120, 89 + offset, 120, 0xFFFF);

	// - 
	draw_line(100 + offset, 105, 111 + offset, 105, 0xFFFF);
	
	// T
	draw_line(117 + offset, 90, 131 + offset, 90, 0xFFFF);
	draw_line(124 + offset, 90, 124 + offset, 120, 0xFFFF);
	
	// A
	draw_line(134 + offset, 90, 146 + offset, 90, 0xFFFF);
	draw_line(134 + offset, 105, 146 + offset, 105, 0xFFFF);
	draw_line(134 + offset, 90, 134 + offset, 120, 0xFFFF);
	draw_line(146 + offset, 90, 146 + offset, 120, 0xFFFF);
	
	// C
	draw_line(149 + offset, 90, 149 + offset, 120, 0xFFFF);
	draw_line(149 + offset, 90, 161 + offset, 90, 0xFFFF);
	draw_line(149 + offset, 120, 161 + offset, 120, 0xFFFF);
	
	// - 
	draw_line(166 + offset, 105, 177 + offset, 105, 0xFFFF);
		
	// T
	draw_line(183 + offset, 90, 197 + offset, 90, 0xFFFF);
	draw_line(190 + offset, 90, 190 + offset, 120, 0xFFFF);
		
	// O
	draw_line(200 + offset, 90, 212 + offset, 90, 0xFFFF);
	draw_line(200 + offset, 120, 212 + offset, 120, 0xFFFF);
	draw_line(200 + offset, 90, 200 + offset, 120, 0xFFFF);
	draw_line(212 + offset, 90, 212 + offset, 120, 0xFFFF);
		
	// E
	draw_line(215 + offset, 90, 215 + offset, 120, 0xFFFF);
	draw_line(215 + offset, 90, 227 + offset, 90, 0xFFFF);
	draw_line(215 + offset, 120, 227 + offset, 120, 0xFFFF);
	draw_line(215 + offset, 105, 221 + offset, 105, 0xFFFF);
	
	// P
	draw_line(20 + offset2, 160, 35 + offset2, 160, 0xFFFF); 
	draw_line(20 + offset2, 160, 20 + offset2, 190, 0xFFFF); 
	draw_line(20 + offset2, 173, 35 + offset2, 173, 0xFFFF); 
	draw_line(35 + offset2, 160, 35 + offset2, 173, 0xFFFF); 
	
	// R
	draw_line(38 + offset2, 160, 53 + offset2, 160, 0xFFFF); 
	draw_line(38 + offset2, 160, 38 + offset2, 190, 0xFFFF); 
	draw_line(38 + offset2, 173, 53 + offset2, 173, 0xFFFF); 
	draw_line(53 + offset2, 160, 53 + offset2, 173, 0xFFFF); 
	draw_line(38 + offset2, 173, 53 + offset2, 190, 0xFFFF); 
	
	// E
	draw_line(56 + offset2, 160, 56 + offset2, 190, 0xFFFF);
	draw_line(56 + offset2, 160, 68 + offset2, 160, 0xFFFF);
	draw_line(56 + offset2, 190, 68 + offset2, 190, 0xFFFF);
	draw_line(56 + offset2, 175, 62 + offset2, 175, 0xFFFF);
	
	// S
	draw_line(71 + offset2, 160, 83 + offset2, 160, 0xFFFF);
	draw_line(71 + offset2, 190, 83 + offset2, 190, 0xFFFF);
	draw_line(71 + offset2, 160, 71 + offset2, 175, 0xFFFF);
	draw_line(71 + offset2, 175, 83 + offset2, 175, 0xFFFF);
	draw_line(83 + offset2, 175, 83 + offset2, 190, 0xFFFF);
	
	// S
	draw_line(86 + offset2, 160, 98 + offset2, 160, 0xFFFF);
	draw_line(86 + offset2, 190, 98 + offset2, 190, 0xFFFF);
	draw_line(86 + offset2, 160, 86 + offset2, 175, 0xFFFF);
	draw_line(86 + offset2, 175, 98 + offset2, 175, 0xFFFF);
	draw_line(98 + offset2, 175, 98 + offset2, 190, 0xFFFF);
	
	// [X]
	draw_line(111 + offset2, 158, 120 + offset2, 158, 0xFFFF); 
	draw_line(111 + offset2, 158, 111 + offset2, 192, 0xFFFF);
	draw_line(111 + offset2, 192, 120 + offset2, 192, 0xFFFF); 
	
	draw_line(123 + offset2, 163, 135 + offset2, 187, 0xFFFF);
	draw_line(135 + offset2, 163, 123 + offset2, 187, 0xFFFF);
	
	draw_line(138 + offset2, 158, 147 + offset2, 158, 0xFFFF);
	draw_line(138 + offset2, 192, 147 + offset2, 192, 0xFFFF);
	draw_line(147 + offset2, 158, 147 + offset2, 192, 0xFFFF);

	// T
	draw_line(157 + offset2, 160, 172 + offset2, 160, 0xFFFF);
	draw_line(164 + offset2, 160, 164 + offset2, 190, 0xFFFF);
	
	// O 
	draw_line(175 + offset2, 160, 187 + offset2, 160, 0xFFFF);
	draw_line(175 + offset2, 190, 187 + offset2, 190, 0xFFFF);
	draw_line(175 + offset2, 160, 175 + offset2, 190, 0xFFFF);
	draw_line(187 + offset2, 160, 187 + offset2, 190, 0xFFFF);
	
	// P
	draw_line(200 + offset2, 160, 215 + offset2, 160, 0xFFFF); 
	draw_line(200 + offset2, 160, 200 + offset2, 190, 0xFFFF); 
	draw_line(200 + offset2, 173, 215 + offset2, 173, 0xFFFF); 
	draw_line(215 + offset2, 160, 215 + offset2, 173, 0xFFFF); 
	
	// L
	draw_line(218 + offset2, 160, 218 + offset2, 190, 0xFFFF);
	draw_line(218 + offset2, 190, 230 + offset2, 190, 0xFFFF);
	
	// A
	draw_line(233 + offset2, 160, 245 + offset2, 160, 0xFFFF);
	draw_line(233 + offset2, 175, 245 + offset2, 175, 0xFFFF);
	draw_line(233 + offset2, 160, 233 + offset2, 190, 0xFFFF);
	draw_line(245 + offset2, 160, 245 + offset2, 190, 0xFFFF);
	
	// Y 
	draw_line(248 + offset2, 160, 255 + offset2, 175, 0xFFFF);
	draw_line(261 + offset2, 160, 255 + offset2, 175, 0xFFFF);
	draw_line(255 + offset2, 175, 255 + offset2, 190, 0xFFFF);
	
	char developers[150] = "By Bikramjit Narwal & Nurin Fazil\0";
	write_text(25, 53, developers);
}

// Functions checks every possibly win (3 in a row) for either player and returns the winner
int check_winner(){
	// First Column Win
    if(1 == board[0] && 1 == board[3] && 1 == board[6]){
		draw_line(69, 25, 69, 214, 0xF800);
		draw_line(70, 25, 70, 214, 0xF800);
		draw_line(71, 25, 71, 214, 0xF800);
        return 1;
    }

    if(2 == board[0] && 2 == board[3] && 2 == board[6]){
		draw_line(69, 25, 69, 214, 0xF800);
		draw_line(70, 25, 70, 214, 0xF800);
		draw_line(71, 25, 71, 214, 0xF800);
        return 2;
    }
	
	// Second Column Win 
    if(1 == board[1] && 1 == board[4] && 1 == board[7]){
		draw_line(159, 25, 159, 214, 0xF800);
		draw_line(160, 25, 160, 214, 0xF800);
		draw_line(161, 25, 161, 214, 0xF800);
        return 1;
    }

    if(2 == board[1] && 2 == board[4] && 2 == board[7]){
		draw_line(159, 25, 159, 214, 0xF800);
		draw_line(160, 25, 160, 214, 0xF800);
		draw_line(161, 25, 161, 214, 0xF800);
        return 2;
    }
	
	// Third column win 
	if(1 == board[2] && 1 == board[5] && 1 == board[8]){
		draw_line(249, 25, 249, 214, 0xF800);
		draw_line(250, 25, 250, 214, 0xF800);
		draw_line(251, 25, 251, 214, 0xF800);
        return 1;
    }

    if(2 == board[2] && 2 == board[5] && 2 == board[8]){
		draw_line(249, 25, 249, 214, 0xF800);
		draw_line(250, 25, 250, 214, 0xF800);
		draw_line(251, 25, 251, 214, 0xF800);        
		return 2;
    }
	
	// First row Win
    if(1 == board[0] && 1 == board[1] && 1 == board[2]){
		draw_line(25, 55, 295, 55, 0xF800);
		draw_line(25, 56, 295, 56, 0xF800);
		draw_line(25, 57, 295, 57, 0xF800);
        return 1;
    }

    if(2 == board[0] && 2 == board[1] && 2 == board[2]){
		draw_line(25, 55, 295, 55, 0xF800);
		draw_line(25, 56, 295, 56, 0xF800);
		draw_line(25, 57, 295, 57, 0xF800);
        return 2;
    }
	
	// Second row Win 
    if(1 == board[3] && 1 == board[4] && 1 == board[5]){
		draw_line(25, 118, 295, 118, 0xF800);
		draw_line(25, 119, 295, 119, 0xF800);
		draw_line(25, 120, 295, 120, 0xF800);
        return 1;
    }

    if(2 == board[3] && 2 == board[4] && 2 == board[5]){
		draw_line(25, 118, 295, 118, 0xF800);
		draw_line(25, 119, 295, 119, 0xF800);
		draw_line(25, 120, 295, 120, 0xF800);
        return 2;
    }
	
	// Third row win 
	if(1 == board[6] && 1 == board[7] && 1 == board[8]){
		draw_line(25, 181, 295, 181, 0xF800);
		draw_line(25, 182, 295, 182, 0xF800);
		draw_line(25, 183, 295, 183, 0xF800);
        return 1;
    }

    if(2 == board[6] && 2 == board[7] && 2 == board[8]){
		draw_line(25, 181, 295, 181, 0xF800);
		draw_line(25, 182, 295, 182, 0xF800);
		draw_line(25, 183, 295, 183, 0xF800);
        return 2;
    }
	
	// Left diagonal win
    if(1 == board[0] && 1 == board[4] && 1 == board[8]){
		draw_line(24, 24, 294, 213, 0xF800);
		draw_line(25, 25, 295, 214, 0xF800);
		draw_line(26, 26, 296, 215, 0xF800);
        return 1;
    }

    if(2 == board[0] && 2 == board[4] && 2 == board[8]){
		draw_line(24, 24, 294, 213, 0xF800);
		draw_line(25, 25, 295, 214, 0xF800);
		draw_line(26, 26, 296, 215, 0xF800);
        return 2;
    }
    
	// Right diagonal win
    if(1 == board[2] && 1 == board[4] && 1 == board[6]){
		draw_line(294, 24, 24, 213, 0xF800);
		draw_line(295, 25, 25, 214, 0xF800);
		draw_line(296, 26, 26, 215, 0xF800);
        return 1;
    }
    
    if (2 == board[2] && 2 == board[4] && 2 == board[6]){
		draw_line(294, 24, 24, 213, 0xF800);
		draw_line(295, 25, 25, 214, 0xF800);
		draw_line(296, 26, 26, 215, 0xF800);
        return 2;
    }
	
	checkforStalemate();
	if (isStalemate){
		return 3;
	}
	
	return 0;
}

// Checks if every position has been filled
void checkforStalemate(){
    for(int index = 0; index < 9; index++){
        // 0 means no one has claimed that position
        if(board[index] == 0){
            isStalemate = false;
			return;
        }
    }
	isStalemate = true;
}

void AI_move(){	
	// AI can only move if there is a possible spot on the board to move 
	if(isStalemate == false){
		srand(time(0));
		int AI_Index = rand() % 9; // generate number from 0 to 8
		
		while(board[AI_Index] != 0){ // If box is full, find another box that is empty
			AI_Index = rand() % 9;
		}
		
		if(AI_Index == 0){
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 25;
			selection_y = 25;
			
			draw_selection_box(selection_x, selection_y, 0xF800);

		} else if(AI_Index == 1){
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 115;
			selection_y = 25;
			
			draw_selection_box(selection_x, selection_y, 0xF800);

		} else if(AI_Index == 2){
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 205;
			selection_y = 25;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
			
		} else if(AI_Index == 3){
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 25;
			selection_y = 88;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
			
		} else if(AI_Index == 4){
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 115;
			selection_y = 88;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
			
		} else if(AI_Index == 5){
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 205;
			selection_y = 88;
			
			draw_selection_box(selection_x, selection_y, 0xF800);

		} else if(AI_Index == 6){
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 25;
			selection_y = 151;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
			
		} else if(AI_Index == 7){
			draw_selection_box(selection_x, selection_y, 0x0000);
			draw_board();
			
			selection_x = 115;
			selection_y = 151;
			
			draw_selection_box(selection_x, selection_y, 0xF800);
		}
		
		if (Turn == 'X'){
			board[AI_Index] = 1;
			draw_player_X(AI_Index + 1);
		} else {
			board[AI_Index] = 2;
			draw_player_O(AI_Index + 1);
		}
	}
}
