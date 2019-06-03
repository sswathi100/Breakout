// CS 349 Fall 2018
// A1: Breakout code
// Author: Swathi Sendhil
// See makefile for compiling instructions

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/time.h>
#include <vector>
#include <string>
#include <utility>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sstream>
#include <time.h>
using namespace std;

// X11 structures
struct XInfo {
    Display* display;
    int screen;
    GC gc[5];
    Window window;
    Pixmap pixmap;
    int speed;
    int fps;
    long unsigned colours[8];
    int maxWidth;
    int maxHeight;
    int num_blocks; // num of blocks in screen
    bool gameOver;
    bool pause;
};

// gives error message and exits
void error (string str) {
    cerr << str << endl;
    exit(0);
}

// get current time
unsigned long now() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

// sets a colour to gc[2]
void setColour(XInfo &xinfo, int colour) {
    XSetForeground(xinfo.display, xinfo.gc[2], xinfo.colours[colour]); // may not be plus 1
}

// base class
class Displayable{
public:
    virtual void paint(XInfo &xinfo) = 0; // paints the object onto the window or buffer
    virtual void reset() = 0; // resets all the fields to their initialized values
};

// Derived class that keeps track of score
class Score: public Displayable {
public:
    Score():score(0),hScore(0) {}
    // updates score for every hit of brick
    void update() {
        score += 10; // for each block hit, the score increases by 10
        if (hScore < score) hScore = score;
    }
    virtual void reset(){
        score = 0;
    }
    virtual void paint (XInfo &xinfo) {
        stringstream in;
        string s;
        in << "Score: " << score;
        s = in.str();
        XFontStruct * font;
        font = XLoadQueryFont (xinfo.display, "12x24");
        XSetFont (xinfo.display, xinfo.gc[0], font->fid);
        XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[0], xinfo.maxWidth - 200, xinfo.maxHeight - 40, s.c_str(), s.length());
        in.str("");
        in << "High Score: " << hScore;
        s = in.str();
        XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[0], 100 , xinfo.maxHeight - 40, s.c_str(), s.length());
    }
private:
    int score;
    int hScore; // keeps track of high score across multiple games in a session
};

// Derived class that keeps track of number of chances to try before game ends
class Chance: public Displayable {
    public:
        Chance(int num): numChanceLeft(num), totalChance(num) {}
        virtual void reset() {
            numChanceLeft = totalChance;
        }
        // updates the number of chances by reduces each time the player loses
        void update(XInfo &xinfo) {
            numChanceLeft -= 1;
            if (numChanceLeft == 0) xinfo.gameOver = true;
        }
        virtual void paint(XInfo &xinfo) {
                setColour(xinfo, 7);
                for (int i = 0; i < numChanceLeft; ++i) {
                    // draw circles for number of chances left
                    XFillArc(xinfo.display, xinfo.pixmap, xinfo.gc[2], xinfo.maxWidth/2 + (i*30), xinfo.maxHeight - 50, 10, 10, 0, 360*64);
                }
        }
    private:
       int numChanceLeft;
       int totalChance;
};

// Derived class that keeps track of all the blocks in the game
class Block: public Displayable {
public:
    Block(int x, int y, int width, int height):
        x(x), y(y), width(width), height(height), status(true) {
            colour = rand()%6; // assign a random colour to the block
    }
    // get and set accessors
    int getX() const {
        return x;
    }
    int getY() const {
        return y;
    }
    int getWidth() const {
        return width;
    }
    int getHeight() const {
        return height;
    }
    bool getStatus() {
        return status;
    }
    bool setStatus(bool alive) {
        status = alive;
    }
    void setBColour(int c){
        colour = c;
    }
    int getBColour(){
        return colour;
    }
    virtual void reset(){
        setStatus(true);
        int c = rand()%6;
        colour = c;
    }
    virtual void paint(XInfo &xinfo) {
        if (status){ // check if block has been hit or not
        setColour(xinfo, colour); // give colour
        XFillRectangle(xinfo.display, xinfo.pixmap, xinfo.gc[2], x, y, width, height); // fill rectange
        }
    }
private:
    int x;
    int y;
    int width;
    int height;
    bool status;
    int colour;
};

// Derived class that handle the paddle
class Paddle: public Displayable {
public:
    Paddle(int x, int y, int w, int h): x(x), y(y), width(w), height(h), inX(x), inY(y) {}
    // get accesors
    int getX() const {
        return x;
    }
    int getY() const {
        return y;
    }
    int getWidth() const {
        return width;
    }
    int getHeight() const {
        return height;
    }
    virtual void reset(){
        x = inX;
        y = inY;
    }
    virtual void paint(XInfo& xinfo) {
            // draw rectangle after setting colour
            setColour(xinfo, 7);
            XFillRectangle(xinfo.display, xinfo.pixmap, xinfo.gc[2], x, y, width, height);
    }
    // shift position of paddle (only left or right -> only x value changes)
    void shift(int shift_amt, XInfo &xinfo) {
        if (!(x + shift_amt <= 0 || x + shift_amt + width >= xinfo.maxWidth)) x += shift_amt;
    }
private:
    int x;
    int y;
    int width;
    int height;
    int inX;
    int inY;
};

// helper function for only setting the Block class objects to the initial states
void reset_blocks(vector<Block*> blocks){
    vector<Block*>::const_iterator begin = blocks.begin();
    vector<Block*>::const_iterator end = blocks.end();
    while (begin != end){
        Block *block = *begin;
        block->reset();
        begin++;
    }
}

// Derived class that handles the ball
class Ball: public Displayable {
public:
    Ball(double x, double y, int xDir, int yDir, int ballSize, double speed): x(x), y(y), xDir(xDir), yDir(yDir),
    ballSize(ballSize), speed(speed),  num_destroyed(0), inX(x), inY(y), inXDir(xDir), inYDir(yDir) {}
    // get accesors
    double getX() const {
        return x;
    }
    double getY() const {
        return y;
    }
    int getXDir() const {
        return xDir;
    }
    int getYDir() const {
        return yDir;
    }
    int getBallSize() const {
        return ballSize;
    }
    virtual void reset(){
        x = inX;
        y = inY;
        xDir = inXDir;
        yDir = inYDir;
        num_destroyed = 0;
   }
    virtual void paint(XInfo& xinfo) {
        int xInt = (int) (round(x));
        int yInt = (int) (round(y));
        // draw ball from centre
        XFillArc(xinfo.display, xinfo.pixmap, xinfo.gc[0],
                x - ballSize/2,
                y - ballSize/2,
                ballSize, ballSize,
                0, 360*64);
    }
    // move the ball with the help of speed and direction given
    void shift() {
        x += (xDir*speed);
        y += (yDir*speed);
    }
    // check if ball hits the wall or paddle
    void paddle_Wall_collision(XInfo &xinfo, Paddle &paddle, vector<Block*> &blocks, Chance &chances) {
        if (x + ballSize/2 > (xinfo.maxWidth - 20) || // adding safety measure of 20 because paddle cannot shift to right end
                x - ballSize/2 < 0)
                xDir = -xDir;
        if (y + ballSize/2 > xinfo.maxHeight){ // ball touches the bottom wall => player loses a chance or lose the game
            chances.update(xinfo);
            paddle.reset();
            // reset ball position without affecting the number of blocks hit
            x = inX;
            y = inY;
            xDir = inXDir;
            yDir = inYDir;
        }
        if (y - ballSize/2 < 0) yDir = -yDir; // check if ball touches top wall and change direction
        // check if ball touches paddle
        if (((x + ballSize/2 >= paddle.getX()) && (x + ballSize/2 <= paddle.getX() + paddle.getWidth())
            || ((x - ballSize/2 >= paddle.getX()) && (x - ballSize/2 <= paddle.getX() + paddle.getWidth())))
            && (y + ballSize/2 >= paddle.getY()) && (y + ballSize/2 <= paddle.getY() + paddle.getHeight())){
                yDir = -yDir;
                if (num_destroyed == xinfo.num_blocks){ // if there are no blocks in the game, create a new set
                        num_destroyed = 0; // reset the number of blocks hit
                        reset_blocks(blocks); // reset block values
                }
        }
    }
    // check if ball touches the blocks
    void block_collision(XInfo &xinfo, Score &score, vector<Block*> &blocks){
        vector<Block*>::const_iterator begin = blocks.begin();
        vector<Block*>::const_iterator end = blocks.end();
        int blockX, blockY, blockH, blockW;
        while (begin != end){
            Block *block = *begin;
            blockX = block->getX();
            blockY = block->getY();
            blockW = block->getWidth();
            blockH = block->getHeight();
            if (block->getStatus()){ // check if ball is already destroyed or not
                if ((y - (ballSize/2) <= blockY + blockH)
                         && (y - (ballSize/2) >= blockY)
                         && (x >= blockX) && (x <= blockX + blockW)){ // block bottom side
                        if (block->getBColour() == 5) { // if block colour is cyan, then it changes colour instead of being destroyed
                            block->setBColour(rand()%5); // set the block to another colour
                        }
                        else {
                             // if block is not cyan, destoy it and  update the number of blocks destroyed
                            num_destroyed++;
                            block->setStatus(false);
                        }
                        // increase score and change direction
                        score.update();
                        yDir = -yDir;
                }
                else if ((y + (ballSize/2) >= blockY)
                    &&(y - (ballSize/2) <= blockY + blockH)
                    && (x >= blockX) && (x <= blockX + blockW)){ // block top side
                        if (block->getBColour() == 5) {// if block colour is cyan, then it changes colour instead of being destroyed
                            block->setBColour(rand()%5); // set the block to another colour
                        }
                        else {
                            // if block is not cyan, destoy it and  update the number of blocks destroyed
                            num_destroyed++;
                            block->setStatus(false);
                        }
                        // increase score and change direction
                        score.update();
                        yDir = -yDir;
                }
                else if ((x - (ballSize/2) >= blockX)
                         && (x - (ballSize/2) <= blockX + blockW)
                         && (y >= blockY) && (y <= blockY + blockH)){ //block right side
                        if (block->getBColour() == 5) {// if block colour is cyan, then it changes colour instead of being destroyed
                            block->setBColour(rand()%5);// set the block to another colour
                        }
                        else {
                            // if block is not cyan, destoy it and  update the number of blocks destroyed
                            num_destroyed++;
                            block->setStatus(false);
                        }
                        // increase score and change direction
                        score.update();
                        xDir = -xDir;
                }
                else if ((x + (ballSize/2) >= blockX)
                         && (x - (ballSize/2) <= blockX + blockW)
                         && (y >= blockY) && (y <= blockY + blockH)){ // block left side
                        if (block->getBColour() == 5) {// if block colour is cyan, then it changes colour instead of being destroyed
                            block->setBColour(rand()%5);// set the block to another colour
                        }
                        else {
                            // if block is not cyan, destoy it and  update the number of blocks destroyed
                            num_destroyed++;
                            block->setStatus(false);
                        }
                        // increase score and change direction
                        score.update();
                        xDir = -xDir;
                } // else block does not get hit
            }
            begin++;
        }
    }
    // check if ball collides with another object
    void checkHit(XInfo &xinfo, Score &score, vector<Block*> &blocks, Paddle &paddle, Chance &chances) {
        // check if ball collides with paddle or wall
        paddle_Wall_collision(xinfo, paddle, blocks, chances);
        // check if paddle collides with blocks
        block_collision(xinfo, score, blocks);
    }
private:
    double x;
    double y;
    int xDir;
    int yDir;
    int ballSize;
    double speed;
    int num_destroyed;
    // initialized values remembered for resetting
    double inX;
    double inY;
    int inXDir;
    int inYDir;
};

// Derived class that handles the main menu at start of game, when paused and at the end of the game
class mainMenu: public Displayable {
    public:
    mainMenu(bool mainMenu): seeMenu(mainMenu) {}
    virtual void reset() {} // nothing to reset here
    virtual void paint(XInfo &xinfo){
        if (seeMenu || xinfo.gameOver || xinfo.pause){
                // background for the menu screen
                setColour(xinfo, 6);
                XFillRectangle(xinfo.display, xinfo.pixmap, xinfo.gc[2], 300, 100, 600, 600);
                // Words on the menu
                stringstream in;
                string s;
                in << "Instructions:";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 325, 400, s.c_str(), s.length()); // gc[3]  - big font
                in.str("");
                in << "Aim to destroy the blocks by using paddle to direct the ball.";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[4], 450, 425, s.c_str(), s.length()); // gc[4] - small font
                in.str("");
                in << "The game is over when the ball goes below the paddle.";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[4], 450, 445, s.c_str(), s.length());
                in.str("");
                in << "Five chances (lives) are given before game ends.";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[4], 450, 465, s.c_str(), s.length());
                in.str("");
                in << "d       ->       Move Paddle Right";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 400, 525, s.c_str(), s.length());
                in.str("");
                in << "a       ->       Move Paddle Left";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 400, 555, s.c_str(), s.length());
                in.str("");
                in << "p       ->       Pause Game";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 400, 585, s.c_str(), s.length());
                in.str("");
                in << "q       ->       Quit";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 400, 615, s.c_str(), s.length());
                in.str("");
                if (xinfo.gameOver){
                    in << " GAME OVER ";
                    s = in.str();
                    XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[0], 500, 300, s.c_str(), s.length()); // gc[0] - font with different background
                    in.str("");
                    in << "Click Anywhere on Screen to Restart Game";
                    s = in.str();
                    XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 370, 330, s.c_str(), s.length());
                }
                else if (xinfo.pause){
                    in << "GAME PAUSED";
                        s = in.str();
                        XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 500, 300, s.c_str(), s.length());
                    in.str("");
                    in << "Click Anywhere on Screen to Resume Game";
                        s = in.str();
                        XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 370, 330, s.c_str(), s.length());
                }
                else {
                        in << "Click Anywhere on Screen to Start Game";
                        s = in.str();
                        XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 370, 300, s.c_str(), s.length());
                }
                in.str("");
                XFontStruct * font;
                font = XLoadQueryFont (xinfo.display, "12x24");
                XSetFont (xinfo.display, xinfo.gc[3], font->fid);
                in << "BreakOut Game";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 500, 150, s.c_str(), s.length());
                in.str("");
                in << "By Swathi Sendhil";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 650, 180, s.c_str(), s.length());
                in.str("");
                in << "ssendhil";
                s = in.str();
                XDrawImageString(xinfo.display, xinfo.pixmap, xinfo.gc[3], 685, 210, s.c_str(), s.length());
        }
    }
    // get and set accessors
    bool getMenu() {
        return seeMenu;
    }
    void setMenu(bool status) {
        seeMenu = status;
    }
    private:
    bool seeMenu;
};

// initialize XInfo structure with additional constraints added
void initX(int argc, char* argv[], XInfo& xinfo) {
    if (argc >= 2) {
        xinfo.fps = 30; // frames per second
        if (argc == 3)
        xinfo.speed = atoi(argv[1]); // speed of ball
    }
    if (xinfo.speed > 10 || xinfo.speed < 0){
        error("Please enter valid arguments for speed (1-10).");
    }
    XSizeHints hints;
    hints.x=10;
    hints.y=10;
    hints.height = hints.min_height = hints.max_height = 800;
    hints.width = hints.min_width = hints.max_width = 1280;
    hints.flags = PMinSize | PMaxSize | PSize | USPosition;
    // create window
    xinfo.display = XOpenDisplay("");
    if (xinfo.display == NULL) exit (-1);
    xinfo.screen = DefaultScreen(xinfo.display);
    long foreground = XWhitePixel(xinfo.display, xinfo.screen);
    long background = XBlackPixel(xinfo.display, xinfo.screen);
    xinfo.window = XCreateSimpleWindow(xinfo.display, DefaultRootWindow(xinfo.display),
                            hints.x, hints.y, hints.width, hints.height, 2, foreground, background);
    XSetStandardProperties(xinfo.display, xinfo.window, "BreakOut Game - Swathi Sendhil",
        "BO", None, argv, argc, &hints);
    // bools that check if game is over or is paused
    xinfo.gameOver = false;
    xinfo.pause = false;
    XSetWMNormalHints(xinfo.display, xinfo.window, &hints);// prevent resizing of screen
    // set events to monitor and display window
    XSelectInput(xinfo.display, xinfo.window, ButtonPressMask | KeyPressMask);
    XMapRaised(xinfo.display, xinfo.window);
    XFlush(xinfo.display);
    //might need sleep after this
}

// helper function to make new Block objects and push it to the Block and Displayable vectors
void makeBlocks(XInfo &xinfo, vector<Block*> &blocks, vector<Displayable*> &dlist) {
    // arbitrary values chosen to get the best appearance of each block
    int x = 0;
    int y = 0;
    const int space = 5;
    const int b_width = 95;
    const int b_height = 40;
    const int num_r = 5;
    const int num_c = 12;
    xinfo.num_blocks = num_c*num_r;
    srand(time(NULL)); // seed to randomize creation of blocks
    for (int r = 0; r < num_r; ++r){ // rows
        for (int c = 0; c < num_c; ++c) { // colums
            x = 50 + c*(b_width + space);
            y = 50 + r*(b_height + space);
            Block *myBlock = new Block(x,y,b_width,b_height);
            blocks.push_back(myBlock);
            dlist.push_back(myBlock);
        }
    }
}

// helper function that determines the gc features
void assignGC(XInfo &xinfo) {
    // colour setting taken from cs246
    XColor xc;
    Colormap cm;
    char vals[8][8] = {"red", "green", "blue", "yellow", "orange", "cyan", "magenta", "brown"};
    cm = DefaultColormap(xinfo.display, DefaultScreen(xinfo.display));
    for (int i = 0; i < 8; ++i) {
        if (!XParseColor(xinfo.display, cm, vals[i], &xc)) {
            cerr << "Bad colour " << vals[i] << endl;
        }
        if (!XAllocColor(xinfo.display, cm, &xc)) {
            cerr << "Bad colour " << vals[i] << endl;
        }
        xinfo.colours[i] = xc.pixel;
    }
    int i = 0;
        xinfo.gc[i] = XCreateGC(xinfo.display, xinfo.window, 0, 0); // for drawing white objects on top of black background
    XSetForeground(xinfo.display, xinfo.gc[i], WhitePixel(xinfo.display,xinfo.screen));
    XSetBackground(xinfo.display, xinfo.gc[i], BlackPixel(xinfo.display,xinfo.screen));
    i = 1;
    xinfo.gc[i] = XCreateGC(xinfo.display, xinfo.window, 0, 0); // for black objects like background
    XSetForeground(xinfo.display, xinfo.gc[i], BlackPixel(xinfo.display,xinfo.screen));
    XSetBackground(xinfo.display, xinfo.gc[i], WhitePixel(xinfo.display,xinfo.screen));
        i = 2;
    xinfo.gc[i] = XCreateGC(xinfo.display, xinfo.window, 0, 0); // for colours
        i = 3;
    xinfo.gc[i] = XCreateGC(xinfo.display, xinfo.window, 0, 0); // for font with menu background
    XSetForeground(xinfo.display, xinfo.gc[i], BlackPixel(xinfo.display,xinfo.screen));
    XSetBackground(xinfo.display, xinfo.gc[i], xinfo.colours[6]);
        i = 4;
    xinfo.gc[i] = XCreateGC(xinfo.display, xinfo.window, 0, 0); // for different font with menu background
    XSetForeground(xinfo.display, xinfo.gc[i], BlackPixel(xinfo.display,xinfo.screen));
    XSetBackground(xinfo.display, xinfo.gc[i], xinfo.colours[6]);
}

// helper function that paints all the objects in the vector
void repaint (vector<Displayable*> dlist, XInfo &xinfo) {
    //double buffer
    XFillRectangle(xinfo.display, xinfo.pixmap, xinfo.gc[1], 0, 0, xinfo.maxWidth + 1, xinfo.maxHeight + 1);
    vector<Displayable*>::const_iterator begin = dlist.begin();
    vector<Displayable*>::const_iterator end = dlist.end();
    while (begin != end) {
        Displayable *p = *begin;
        p->paint(xinfo);
        begin++;
    }
    // double buffer
    XCopyArea(xinfo.display, xinfo.pixmap, xinfo.window, xinfo.gc[1], 0,0, xinfo.maxWidth + 1, xinfo.maxHeight + 1, 0, 0);
    XFlush(xinfo.display);
}

// delete all the pointers in the vector
void freedpointers (vector<Displayable*> v){
    vector<Displayable*>::const_iterator begin = v.begin();
    vector<Displayable*>::const_iterator end = v.end();
    while (begin != end) {
        Displayable *p = *begin;
        delete p;
        begin++;
    }
}

// helper function to reset the game if player wants to play again afer game over
void reset_game (vector<Displayable*> v) {
    vector<Displayable*>::const_iterator begin = v.begin();
    vector<Displayable*>::const_iterator end = v.end();
    while (begin != end) {
        Displayable *p = *begin;
        p->reset();
        begin++;
    }
}

// event loop function
void eventloop(XInfo &xinfo) {
    vector<Displayable*> dlist; // vector of all he objects to paint
    // get window information (height and width)
    XWindowAttributes w;
    XGetWindowAttributes(xinfo.display, xinfo.window, &w);
    // double buffer
    int depth = DefaultDepth(xinfo.display, DefaultScreen(xinfo.display));
    xinfo.pixmap = XCreatePixmap(xinfo.display, xinfo.window, w.width, w.height, depth);
    // max coordinates
    xinfo.maxHeight = w.height - 1;
    xinfo.maxWidth = w.width - 1;
    // initialization of objects and pushing to vector
    Paddle *paddle = new Paddle(xinfo.maxWidth/2,xinfo.maxHeight - 100, 250,25);
    double speed = (xinfo.speed * 10)/ xinfo.fps;
    Ball *ball = new Ball(xinfo.maxWidth/2 + 125, xinfo.maxHeight - 115, 1, -1, 20, speed);
    Chance *chances = new Chance(5);
    vector<Block*> blocks; // separate vector created for blocks
    makeBlocks(xinfo, blocks, dlist); // initializes blocks and puts pointers to them in vector above
    Score *score = new Score();
    mainMenu *menu = new mainMenu(true);
    dlist.push_back(ball);
    dlist.push_back(paddle);
    dlist.push_back(score);
    dlist.push_back(chances);
    dlist.push_back(menu);
    // create gc for drawing
    assignGC(xinfo);
    // save time of last window paint
    unsigned long lastRepaint = 0;
    // event handle for current event
    XEvent event;
    // event loop
    while ( true ) {
        // process if we have any events
        if (XPending(xinfo.display) > 0) {
            XNextEvent(xinfo.display, &event );
            switch ( event.type ) {
                // mouse button press
                case ButtonPress:
                    if (xinfo.gameOver){
                        reset_game(dlist);
                        xinfo.gameOver = false;
                    }
                    else if (menu->getMenu()){
                        menu->setMenu(false);
                    }
                    else if (xinfo.pause){
                        xinfo.pause = false;
                    }
                    break;
                case KeyPress: // any keypress
                    KeySym key;
                    char text[10];
                    int i = XLookupString( (XKeyEvent*)&event, text, 10, &key, 0 );
                    // move right
                    if ( i == 1 && text[0] == 'd' ) {
                        paddle->shift(30, xinfo);
                    }
                    // move left
                    if ( i == 1 && text[0] == 'a' ) {
                        paddle->shift(-30, xinfo);
                    }
                    // quit game
                    if ( i == 1 && text[0] == 'q' ) {
                        XCloseDisplay(xinfo.display);
                        exit(0);
                                        }
                    if ( i == 1 && text[0] == 'p' ) {
                            xinfo.pause = true;
                    }
                    break;
                }
        }
        unsigned long end = now();	// get current time in microsecond
        if (end - lastRepaint > 1000000 / xinfo.fps) {
            if (!(menu->getMenu() || xinfo.gameOver || xinfo.pause)) {//move ball only if menu screen is not open
                // bounce ball
                ball->shift();
                ball->checkHit(xinfo, *score, blocks, *paddle, *chances);
            }
            //paint
            repaint(dlist, xinfo);
            lastRepaint = now(); // remember when the paint happened
        }
        // IMPORTANT: sleep for a bit to let other processes work
        if (XPending(xinfo.display) == 0) {
            usleep(1000000 / xinfo.fps - (now() - lastRepaint));
        }
    }
    freedpointers(dlist);
}

int main(int argc, char* argv[]) {
    XInfo xinfo;
    initX(argc, argv, xinfo);
    eventloop(xinfo);
    // may not be needed
    XCloseDisplay(xinfo.display);
}
