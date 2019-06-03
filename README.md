# breakout
Breakout game made in c++ with X11 graphics

Features of Game:
1. Keep hitting blocks till you lose - if all the blocks in the screen have been cleared,
   a new set of blocks appear to ensure the player can keep playing.
2. Lives system - player gets 5 lives before dying and game ending.
3. Special blocks - cyan coloured blocks are special blocks that change into another coloured block when hit, instead of disappearing.
4. Score system - Score of 10 points is awarded when a block is hit, regardless of the colour.
5. High score - Player can keep track of all their high score along their numerous games in one session.
6. Pause - player can pause at any time during the game to read the instructions again.
7. Bonus - if lucky, player can align the paddle so that ball travels along the paddle like inside a tunnel
   and bounces up through the other side.

How To Play:
d -> move paddle to right
a -> move paddle to left
p -> pause
q -> quit

To compile and run:
Within the directory, type "make run" to compile and run the game
