# 243-Final-Project

The game we implemented on CPUlator is Tic-Tac-Toe. It uses the PS/2 Keyboard to get input from the user. 

Compile and load the code provided on CPUlator. 
- Upon loading the code you will see a welcome screen. Press [X] to start the game. 

- You will now see the game board. This is a 2-player game. At the bottom of the screen is who’s turn it is. Use the number keys to decide which box to place your piece in. For example, if you would like to place X in box 5, press the 5 number key. 
- You can also use A, W, S, and D to select boxes (See Note).

- Once you have selected your box, press [Enter] to draw. You should now see either an X or O drawn in the box depending on whose play it is.

- Keep playing until one person gets 3 consecutive boxes. The game will indicate a winner by drawing a red line over the winning boxes and the status at the bottom will also show there is a winner. 

- To start a new game, press [Spacebar]. 

- While you are playing the game, you can press [H] to open the help screen. This gives a list of all the keyboard controls the game uses. Press [Escape] to close the help screen and resume your game.

Note: The keys for A, W, S, and D invoke 2 keyboard interrupts when typed and we think that is something to do with CPUlator itself. When you type either of those keys, the selection box will move quite fast making it difficult to select. We recommend instead of typing these keys, you send a Make signal instead (see the image below). Typing any of the other keys (other than A, W, S, and D) in the game work fine.
