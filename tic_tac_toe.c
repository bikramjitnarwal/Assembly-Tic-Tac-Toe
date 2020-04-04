#include <stdbool.h>
#include <stdio.h>

void clear_screen ();
void disable_A9_interrupts(void);
void set_A9_IRQ_stack(void);
void config_GIC(void);
void config_KEYs(void);
void enable_A9_interrupts(void);
void keyboard_ISR(void);
void config_interrupt(int, int);
void plot_pixel(int x, int y, short int line_color);
void draw_line(int x0, int y0, int x1, int y1, short int line_color);
void draw_selection_box(int x, int y, short int selection_colour);
void swap(int *first, int *second);
void draw_board(void);
void write_text(int x, int y, char * text_ptr);
void draw_player(int boardIndex, char Turn);
void draw_player_X(int boardIndex);
void draw_player_O(int boardIndex);

// Global variables
int selection_x;
int selection_y;
short int colour = 0x0000;
volatile int pixel_buffer_start; // global variable, to draw 


int main(void) {

	// This is the top left corner of the first box
	// Red selection box starts here initially
	selection_x = 25;
	selection_y = 25;
	
	/* create a message to be displayed on the video and LCD displays */

	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    
	/* Read location of the pixel buffer from the pixel buffer controller */
    pixel_buffer_start = *pixel_ctrl_ptr;
	
	clear_screen();
	draw_board();
	draw_selection_box(selection_x, selection_y, 0xF800);
	
	disable_A9_interrupts(); // disable interrupts in the A9 processor
	set_A9_IRQ_stack(); // initialize the stack pointer for IRQ mode
	config_GIC(); // configure the general interrupt controller
	config_KEYs(); // configure pushbutton KEYs to generate interrupts
	enable_A9_interrupts(); // enable interrupts in the A9 processor
	
	while (1); // wait for an interrupt

}


/* setup the KEY interrupts in the FPGA */
void config_KEYs() {
	volatile int * PS2_ptr = (int *) 0xFF200100; // pushbutton KEY base address
	*(PS2_ptr + 1) = 0x00000001; // enable interrupts for the two KEYs
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

// Turn off interrupts in the ARM processor
void disable_A9_interrupts(void) {
	int status = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r"(status));
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

void clear_screen (){
	int y,x;
	for(x=0;x<320;x++){
		for(y=0;y<240;y++){
			plot_pixel(x,y,0x0000);
		}
	}
}

void keyboard_ISR(void) {

	volatile int * PS2_ptr = (int *)0xFF200100; // Points to PS2 Base
	volatile int * LED_ptr = (int *)0xFF200000;	// Points to LED Address 
	unsigned char byte0 = 0;
    
	int PS2_data = *(PS2_ptr);
	int RVALID = PS2_data & 0x8000;
	
	unsigned char key [4] = {0,0,0,0};
	int readInterruptReg;
    
	//Read Interrupt Register
	 readInterruptReg = *(PS2_ptr + 1 ); 
	  
	//Clear Interrupt 
	*(PS2_ptr+1) = readInterruptReg; 

	*(LED_ptr) = 0x40;
	
	// when RVALID is 1, there is data 
	if (RVALID != 0){
               
		byte0 = (PS2_data & 0xFF); //data in LSB	
		*(LED_ptr) = 0x20;
	
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

		if(byte0 == 0x29){  //Clears Screen, SpaceBar 
			clear_screen(0,0,0x00); 
			draw_board();
						
			// Reinitialize selection box to the top left box
			selection_x = 25;
			selection_y = 25;
			draw_selection_box(selection_x, selection_y, 0xF800);
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
		
		if(byte0 == 0x5A){ //User hit enter
			// do something (draw x/o, check winner, etc..)
			// Use selection coords to check which box user is in and draw there
		}
		
		int i = 0;
		
		if(byte0 == 0xF0) {
			// Check for break
			*(LED_ptr) = 0x00;
			for(i = 0; i < 32; ++i)
				*(LED_ptr) ^= 0x80;
			switch (PS2_data & 0xFF) {
				case 0x1D:
					key[0] = 0;
					break;
				case 0x1B:
					key[1] = 0;
					break;
				case 0x1C:
					key[2]= 0;
					break;
				case 0x23:
					key[3]= 0;
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

void draw_player(int boardIndex, char Turn){
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
