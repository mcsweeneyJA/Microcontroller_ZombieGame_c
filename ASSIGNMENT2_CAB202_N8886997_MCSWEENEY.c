
////includes
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h> 
#include <stdbool.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include "lcd_model.h"
#include <math.h>
#include <string.h>
#include <macros.h>
#include <graphics.h>
#include <sprite.h>
#include "cab202_adc.h"
#include "usb_serial.h"


#define MAX_PLATFORMS (50)
#define BIT(x) (1 << (x))
#define OVERFLOW_TOP (1023)
#define ADC_MAX (1023)
#define PLATFORM_W (10)
#define COLUMN_W (PLATFORM_W + 4)
#define COLUMN_H (PLATFORM_H + 15)
#define PLATFORM_H (3)
#define DELAY_MS (20)
#define INIT_LIVES (10)
#define FREQ     (8000000.0)
#define PRESCALE (1024.0)
#define MAX_W (10)
#define MAX_ZOMBIE (5)
#define NONSAFE_PERCENT (30)
#define OCCUPIED_PERCENT (80)  // logic taken from assignment1 lecture solution
#define MAX_FOOD (5)


uint8_t score;
uint8_t lives;
uint8_t alive_zombies;
uint8_t foodsupply;
uint8_t num_platforms;
double start_time;
bool toggle = false;
bool game_over = false;
bool display = true;
static char print_buffer[50];
bool is_safe[MAX_PLATFORMS];  
int mask = 0b00000111;
volatile uint8_t switch_closed_down;
volatile uint8_t bit_counter = 0;


//------------ SPRITE STRUCTS ------------------------------------------
Sprite hero;
Sprite starter;
Sprite treasure;
Sprite food [MAX_FOOD];
Sprite zombie [MAX_ZOMBIE];
Sprite platforms [MAX_PLATFORMS];

// ---------- PREDECLARATIONS ------------------------------------------
void setup(void);
void setup_hero(void);
void setup_starter(void);
void setup_treasure(void);
void setup_zombies(void);
void draw(void);
void food_drop(void);
void setup_food(void);
void setup_platforms(void);
void setup_buttons(void);
void setup_usb_serial(void);
void usb_serial_send(char * message);
void usb_serial_send_int(int value);
void process(void);
void game_over_function(void);
void auto_move(void);
void die_zombie(void);
void zombies_collide(void);
void KeepPlayerInGameArea(void);
void zombie_walk(void);
void send_lives(void);
void send_score(void);
void foodremain(void);
void send_foodsupply(void);
void send_moved(void);
void send_time(void);

//-------------------- MAIN ----------------------------------

int main(void) {
	setup();
	clear_screen();
	draw_string(6, 10, "JACK MCSWEENEY", FG_COLOUR);
	draw_string(12, 20, "  N8886997  ", FG_COLOUR);
	show_screen();
	
	while(1){
		if ( BIT_IS_SET(PINF, 5)){
			for ( ;; ) {
				process();
			}
		}

	
	}	


}
//-------------------------------------------------------------------





//-------------------------------------------------------------------

void setup(void) {
	set_clock_speed(CPU_8MHz);
	lcd_init(LCD_DEFAULT_CONTRAST);
	TCCR0A =0;  // TIMER 0 STARTED
	TCCR0B = 5;
	TCCR1A =0;	// TIMER 1 STARTED
	TCCR1B = 1;
	TIMSK0 = 1; // TIMER0 OVERFLOW ENABLES
	TIMSK1 = 1; // TIMER1 OVERFLOW ENABLES
	sei(); // INTERRUPTS ENABLED
	// LIGHT / BACKLIGHT SETUP
	TC4H = OVERFLOW_TOP >> 0x8;
	OCR4C = OVERFLOW_TOP & 0xff;
	TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
	TCCR4A = 0;
	TCCR4A = BIT(COM4A1) | BIT(PWM4A);
	SET_BIT(DDRC, 7);

	TCCR4D = 0;
	setup_buttons();
	setup_usb_serial();
	setup_hero();
	// INITIALISE VARIABLES
	lives = 10;
	score = 0;
	foodsupply = 5;
	alive_zombies = 5;
	setup_starter();
	setup_food();
	setup_zombies();
	setup_treasure();
	setup_platforms();
	
	
}
// _--------HELPER FUNCTION FOR PLATFORMS---------------------

int get_column( sprite_id s)
{
    return (int) round(s->x) / COLUMN_W;
}




//---------PLATFORM SETUP MODELLED ON LECTURE EXAMPLE-------
void setup_platforms(void)
{
	static uint8_t safe_img[] =
	{0b11111111, 0b11000000,
	0b11100001, 0b11000000,
	0b11111111, 0b11000000,};

	static uint8_t nonsafe_img[] = 
	{0b11111111, 0b11000000,
	0b11111111, 0b11000000,
	0b11111111, 0b11000000};

    int cols = 84/ COLUMN_W;
    int rows = 48 / 2;
    int desired = rows * cols * OCCUPIED_PERCENT / 100;
    int out_of = rows * cols;
    num_platforms = 0;
    int safe_count_column[MAX_PLATFORMS] = {0};
	srand(TCNT0);
    for (int row = 0; row < rows; row++){
        for (int col = 0; col < cols; col++){
            if ( num_platforms >= MAX_PLATFORMS) break;

            double p = (double) rand() / RAND_MAX;
            if (p <= (double) desired / out_of){
                desired --;

                sprite_id plat = &platforms[num_platforms];
                int location_x = col * (LCD_X - COLUMN_W) / (cols -1) + rand() % (COLUMN_W - PLATFORM_W);
                int location_y = 5 + (row+1) * COLUMN_H - PLATFORM_H;

                sprite_init(plat, location_x, location_y, PLATFORM_W, PLATFORM_H, safe_img);

                is_safe[num_platforms] = true;
                safe_count_column[col]++;
                num_platforms++;

            }
            out_of--;
        }
        
    }
    int num_nonsafe = num_platforms * NONSAFE_PERCENT / 100;
    if (num_nonsafe < 2) num_nonsafe =2;

    for (int i=0; i < num_nonsafe; i++){
        #define MAX_TRIALS (1000)
        for (int trial = 0; trial < MAX_TRIALS; i++){
            int plat_index = 1+ rand() % (num_platforms -1);
            sprite_id plat = &platforms[plat_index];
            int col = get_column(plat);

            if (safe_count_column[col] >1){
                is_safe[plat_index] = false;
                safe_count_column[col] --;
                plat->bitmap = nonsafe_img;
                break;
            }
        }
    }
}

// INITIALISE OVERFLOW COUNTER AND FIRST INTERRUPT SERVICE 
volatile int overflow_counter = 0;
ISR(TIMER0_OVF_vect) {

	overflow_counter ++;
}

//------------------ DRAW ARRAYS -------------------------
void draw(void)
{	
	
	double time = ( overflow_counter * 256.0 + TCNT0 ) * PRESCALE  / FREQ;
    for (int i=0; i< num_platforms; i++){
        sprite_draw( &platforms[i]);
		
    }
	for (int i=0; i< (MAX_FOOD); i++){
        sprite_draw( &food[i]);
	}
	if (time > 4){
		for (int i=0; i< (MAX_ZOMBIE); i++){
			sprite_draw( &zombie[i]);
			
		}

      	SET_BIT(PINB, 2);
    	SET_BIT(PINB, 3);

	}

}

//------------------HERO SETUP-----------------------------


void setup_hero(void) {
	static uint8_t bitmap[] = {
		0b00011000,
		0b00100100,
		0b00100100,
		0b01111110,
		0b00011000,
		0b00011000,
		0b00011000,
		0b01100110,
		0b01100110,
	};
	int W = 6, H = 9;
	sprite_init(&hero, (LCD_X - LCD_X + W), (LCD_Y - LCD_Y + 0.1*H), W, H, bitmap);

}
//---------------- STARTER SETUP ---------------------------------
void setup_starter(void) {
	static uint8_t starter_img[] = 
	{0b11111111, 0b11000000,
	0b11111111, 0b11000000,
	0b11111111, 0b11000000};
	int W = 10, H = 3;
	sprite_init(&starter, (LCD_X - LCD_X + (0.7*W)), (LCD_Y - LCD_Y + 2.9*H), W, H, starter_img);
	starter.dy = 0.0;

}

//---------------- SETUP TREASURE----------------------------------
void setup_treasure(void) {
	static uint8_t treasure_img[] = {
		0b00111000,
		0b01111100,
		0b11101110,
		0b11010110,
		0b11101110,
		0b01111100,
		0b00111000};
	int W = 7, H = 7;
	sprite_init(&treasure, (LCD_X - (0.5*LCD_X)), (LCD_Y - 3*H), W, H, treasure_img);
	treasure.dx = 0.5;

}

//---------------SETUP FOOD------------------------------------
void setup_food(void) {
	static uint8_t food_img[] = {
	
		0b00111100,
		0b01000010,
		0b01111110,
		0b11111111,
		0b01000010,
		0b01111110};
	int W = 8, H = 6;
	int i;
	for (i = 0; i < 5; i++){
		sprite_id foodP = &food[i];
		sprite_init(foodP, (LCD_X - 35 ), (LCD_Y - LCD_Y + 1), W, H, food_img);
	}
}
//---------------SETUP ZOMBIES----------------------------------
void setup_zombies(void) {
	static uint8_t zombies_img[] = {
		0b00111100,
		0b00111100,
		0b00011000,
		0b01111110,
		0b01011010,
		0b00011000,
		0b01100110,
		0b01100110,};
	int W = 8, H = 8;
	int i;
	int cols = 84/ COLUMN_W;

	for (i = 0; i < 5; i++){
		int location_x = 30 + (i * (LCD_X - COLUMN_W) / (cols -1) + rand() % (COLUMN_W - PLATFORM_W));
		sprite_id zombieP = &zombie[i];
		sprite_init(zombieP, (location_x), (LCD_Y - LCD_Y + 5), W, H, zombies_img);

	}
	send_lives();
	send_score();
	usb_serial_send("ZOMBIES SPAWNING");

}
//-------------SETUP BUTTONS------------------------------------

void setup_buttons(void) {
	CLEAR_BIT(DDRD, 1);
	CLEAR_BIT(DDRB, 7);
	CLEAR_BIT(DDRB, 1);
	CLEAR_BIT(DDRB, 0);
	CLEAR_BIT(DDRD, 0);
	CLEAR_BIT(DDRF,5);
	CLEAR_BIT(DDRF, 6);
	CLEAR_BIT(DDRB, 2);
    CLEAR_BIT(DDRB, 3);
	CLEAR_BIT(DDRF,6);
	SET_BIT(DDRD, 6);
	PORTB = 0x00;
	

}
//---------------------FUNCTION TO STEP SPRITE-----------------------


bool auto_step( sprite_id sprite ) {

	int x0 = round( sprite->x );
	int y0 = round( sprite->y );
	sprite->x += sprite->dx;
	sprite->y += sprite->dy;
	int x1 = round( sprite->x );
	int y1 = round( sprite->y );
	return ( x1 != x0 ) || ( y1 != y0 );
}

//---------------------------------------------------------

void auto_move(void)
{	
	double dx = 0;

	
	for (int i =0; i < MAX_PLATFORMS; i++){
		if (platforms[i].y >20){
			dx = -0.5;
		}
		else {
			dx = 0.5;
		}
		platforms[i].x += dx;
		if (platforms[i].x == 84){
			platforms[i].x = 0 - (0.5 * PLATFORM_W);
		}
		if (platforms[i].x == 0 - PLATFORM_W && platforms[i].y >20 ){
			platforms[i].x = 80;
		}

	}	
}
// ---------------------------------------------------------
bool sprites_collide(sprite_id s1, sprite_id s2)
{
    if ((!s1->is_visible) || (!s2->is_visible))
    {
        return false;
    }

    int l1 = round(s1->x);
    int l2 = round(s2->x);
    int t1 = round(s1->y);
    int t2 = round(s2->y);
    int r1 = l1 + s1->width - 1;
    int r2 = l2 + s2->width - 1;
    int b1 = t1 + s1->height - 1;
    int b2 = t2 + s2->height - 1;

    if (l1 > r2)
        return false;
    if (l2 > r1)
        return false;
    if (t1 > b2)
        return false;
    if (t2 > b1)
        return false;

    return true;
}
// ---------------------------------------------------------
// ---------------------------------------------------------
int get_current_platform( sprite_id s)
{
	int sl = (int) round(s->x);
	int sr = sl + s->width-1;
	int sy = (int) round(s->y);

	for (int plat =0; plat < num_platforms; plat++){
		sprite_id p = &platforms[plat];
		int pl  = (int) round(p->x);
		int pr = pl + p-> width-1;
		int py = (int) round(p->y);
	

		if (sr >= pl && sl <= pr && sy == py - s->height){
			return plat;
		}
		
	}
	

	return -1;
}
//----------------------------------------------------------
// ---------------------auxillary draw----------------------
char buffer[20];
void draw_double(uint8_t x, uint8_t y, double value, colour_t colour) {
	snprintf(buffer, sizeof(buffer), "%f", value);
	draw_string(x, y, buffer, colour);
}
// ---------------------------------------------------------
//	USB serial business.
// ---------------------------------------------------------

void setup_usb_serial(void) {
	// Set up LCD and display message
	lcd_init(LCD_DEFAULT_CONTRAST);
	draw_string(10, 10, "Connect USB", FG_COLOUR);
	show_screen();
	usb_init();
	while ( !usb_configured() ) {
		// Block until USB is ready.
	}
	adc_init();
	clear_screen();
	usb_serial_send("Please press W A S D to control the sprite!\r\n");
}

/*
**	Transmits a string via usb_serial.
*/
void usb_serial_send(char * message) {
	// Cast to avoid "error: pointer targets in passing argument 1 
	//	of 'usb_serial_write' differ in signedness"
	usb_serial_write((uint8_t *) message, strlen(message));
}

///-------- VERY UGLY FUNCTION THAT IS JUST A DELAY ----------------
int wait_loop0 = 400;
int wait_loop1 = 100;
void wait( int seconds )
{   // this function needs to be finetuned for the specific microprocessor
    int i, j, k;
    for(i = 0; i < seconds; i++)
    {
        for(j = 0; j < wait_loop0; j++)
        {
            for(k = 0; k < wait_loop1; k++)
            {   // waste function, volatile makes sure it is not being optimized out by compiler
                int volatile t = 120 * j * i + k;
                t = t + 5;
            }
        }
    }
}

/// auxilary to initialise duty cycle for backlight
void set_duty_cycle(int duty_cycle) {
	// Set bits 8 and 9 of Output Compare Register 4D.
	TC4H = duty_cycle >> 8;

	// Set bits 0..7 of Output Compare Register 4D.
	OCR4A = duty_cycle & 0xff;
}

/// ---- hero death function controlling backlight
void hero_die(void){
	int i;
	for (i = 0; i< 12; i++){
		int x = 100;
		set_duty_cycle(ADC_MAX - (x*i));
		wait(1);
	}
	send_moved();
	send_lives();
	send_score();
}

// function to keep player movement restricted and take life when exitting.
void KeepPlayerInGameArea(void) {
	if (hero.x >= LCD_X -5 ) {
        hero.y = starter.y -8;
        hero.x = starter.x;
        lives = lives -1;
		hero_die();
		usb_serial_send("You died! from exiting the screen. ");
		send_lives();
		game_over = true;
		game_over_function();		
        
	}
	if (hero.x < 0) {
        hero.y = starter.y -8;
        hero.x = starter.x;
        lives = lives -1;
		hero_die();
		usb_serial_send("You died! from exiting the screen. ");
		send_lives();
		game_over = true;
		game_over_function();
        
	}
    if (hero.y >= LCD_Y -1) {
        hero.y = starter.y -8;
        hero.x = starter.x;
        lives = lives -1;
		hero_die();
		usb_serial_send("You died! from exiting the screen. ");
		send_lives();
		game_over = true;
		game_over_function();	
    }
}

//------------------PAUSE FUNCTION-----------------------------
void pause(void){
	
	toggle = !toggle;
	while( !toggle){  
		double time = ( overflow_counter * 256.0 + TCNT0 ) * PRESCALE  / FREQ;
			
		clear_screen();
		draw_string(0, 0, "Game time:", FG_COLOUR);
		draw_double(55, 0, time, FG_COLOUR);
		draw_string(0, 10, "Lives:", FG_COLOUR);
		draw_double(55, 10, lives, FG_COLOUR);
		draw_string(0, 20, "Score:", FG_COLOUR);
		draw_double(55, 20, score, FG_COLOUR);
		draw_string(0, 30, "Food:", FG_COLOUR);
		draw_double(55, 30, foodsupply, FG_COLOUR);
		draw_string(0, 40, "Zombies:", FG_COLOUR);
		draw_double(55, 40, alive_zombies, FG_COLOUR);
		show_screen();
		if ( BIT_IS_SET(PINB, 0) ) toggle = !toggle;
		if ( usb_serial_available() ) {
		int c = usb_serial_getchar();
			if ( c == 'p') toggle = !toggle;
		}
		usb_serial_send("Game Paused ");
		send_lives();
		send_score();
	}
}
//// ----- FUNCTION to end game ---------------------------
void game_over_function(void){
	if (game_over == true){

		double time = ( overflow_counter * 256.0 + TCNT0 ) * PRESCALE  / FREQ;
		clear_screen();
		draw_string(0, 0, "Game time:", FG_COLOUR);
		draw_double(55, 0, time, FG_COLOUR);
		draw_string(0, 10, "GAME OVER !!", FG_COLOUR);

		draw_string(0, 28, "Score:", FG_COLOUR);
		draw_double(55, 28, score, FG_COLOUR);
		show_screen();
		usb_serial_send("GAME OVER !!");
		send_lives();
		send_score();
		usb_serial_send("Zombies fed: 2");
		
		while ( display == true){

			if ( usb_serial_available() ) {
				int c = usb_serial_getchar();
				if ( c == 'r' ) {
					setup_hero();
					overflow_counter = 0;
					lives = 10;
					display = false;
					show_screen();					
				}
				if ( c == 'q' ) {
					clear_screen();
					draw_string(6, 10, "JACK MCSWEENEY", FG_COLOUR);
					draw_string(12, 20, "  N8886997  ", FG_COLOUR);
					show_screen();		
				}							
			}

			if ( BIT_IS_SET(PINF, 5)){
				clear_screen();
				setup_hero();
				overflow_counter = 0;
				lives = 10;
				display = false;
				show_screen();
			}		
			if (BIT_IS_SET(PINF, 6)){
				clear_screen();
				draw_string(6, 10, "JACK MCSWEENEY", FG_COLOUR);
				draw_string(12, 20, "  N8886997  ", FG_COLOUR);
				show_screen();
			}
		}
		
	}
}
// -------------- function to drop ------------------

void food_drop(void){
	int i;

	for (i =0; i<1; i++){
			
		food[i].x = hero.x;
		food[i].y = hero.y;
		foodsupply = foodsupply -1;
		if (foodsupply == -1){
			foodsupply = foodsupply +1;
			food[i].x = 200;
		}
	}	
}

/// --------------- Serial send ---------------------
void usb_serial_send_int(int value) {
	static char buffer[8];
	snprintf(buffer, sizeof(buffer), "%d", value);
	usb_serial_send( buffer );
}
////----- Function using PSTR to save ram memory and allocate string to flash----
void send_lives(void){
	
	snprintf_P(print_buffer, sizeof(print_buffer), PSTR("Lives: %d \r\n"), lives);
    usb_serial_write((uint8_t*)(print_buffer), strlen(print_buffer));

}
////----- Function using PSTR to save ram memory and allocate string to flash----
void send_score(void){
	
	snprintf_P(print_buffer, sizeof(print_buffer), PSTR("Score increased to: %d \r\n"), score);
    usb_serial_write((uint8_t*)(print_buffer), strlen(print_buffer));

}
////----- Function using PSTR to save ram memory and allocate string to flash----
void send_foodremain(void){
	
	snprintf_P(print_buffer, sizeof(print_buffer), PSTR("Food remaining: %d \r\n"), foodsupply);
    usb_serial_write((uint8_t*)(print_buffer), strlen(print_buffer));

}
////----- Function using PSTR to save ram memory and allocate string to flash----
void send_moved(void){
	
	snprintf_P(print_buffer, sizeof(print_buffer), PSTR("Respawned at: (%d, %d) \r\n"), hero.x, hero.y);
    usb_serial_write((uint8_t*)(print_buffer), strlen(print_buffer));

}
////----- Function using PSTR to save ram memory and allocate string to flash----
void send_zombies(void){
	
	snprintf_P(print_buffer, sizeof(print_buffer), PSTR("Remaining Zombies: %d \r\n"), alive_zombies);
    usb_serial_write((uint8_t*)(print_buffer), strlen(print_buffer));

}

//-------------------------------------------------------------------
// sprite collision between sprite and array of sprites

int sprite_collide_2(sprite_id i, sprite_id j)
{
    uint8_t l1 = (i->x) + 1;
    uint8_t l2 = (j->x);
    uint8_t t1 = (i->y) + 1;
    uint8_t t2 = (j->y);
    uint8_t r1 = (i->x + i->width) - 1;
    uint8_t r2 = (j->x + j->width);
    uint8_t b1 = i->y + i->height + 1;
    uint8_t b2 = j->y + j->height;

    if (l1 > r2)
        return 0;
    if (l2 > r1)
        return 0;
    if (t1 > b2)
        return 0;
    if (t2 > b1)
        return 0;
    return 1;
}

sprite_id sprites_collide_any(sprite_id s, Sprite sprites[], int n)
{
    sprite_id result = 0;
    for (int i = 0; i < n; i++)
    {
        if (sprite_collide_2(s, &sprites[i]))
        {
            result = &sprites[i];
            break;
        }
    }
    return result;
}
// -------------- ZOMBIE DEATH FUNCTION ---------------------
void die_zombie(void){

	for (int j = 0; j < MAX_ZOMBIE; j++){
		if (sprites_collide_any(&zombie[j], food, MAX_ZOMBIE) && food[j].is_visible == 1 && zombie[j].is_visible == 1){
			zombie[j].is_visible = 0;
			food[j].is_visible = 0;
			score = score + 10;
			alive_zombies = alive_zombies -1;
			usb_serial_send("Zombie ate food");
			send_zombies();
			send_foodremain();
		}
	}

	
}

// ---------------- ZOMBIE MOVEMENT --------------------------
void zombie_walk(void)
{	srand(TCNT0);
    for (int i = 0; i < MAX_ZOMBIE; i++)
    {
        if (zombie[i].dy == 0)
        {
            zombie[i].x += (rand() % 3 + (-1));
            if (zombie[i].x >= 0 || zombie[i].x < LCD_X )
            {
                zombie[i].dx = -zombie[i].dx;
            }
            if (zombie[i].dx != zombie[i].dx)
            {
                zombie[i].x -= zombie[i].dx;
                zombie[i].dx = zombie[i].dx;
            }
        }
		if (sprites_collide_any(&zombie[i], platforms, MAX_PLATFORMS)){
            zombie[i].dy = 0; 
        }
		else if (sprites_collide_any(&zombie[i], platforms, MAX_PLATFORMS) == 0){
			
		
            zombie[i].dy = 3;
            zombie[i].y += zombie[i].dy;
        }
	}
}
// debounce funtion with bool result to be fed into interrupt service
bool debounce(void) {
	static uint8_t state = 0;
	state = ((state << 1) & mask) | BIT_IS_SET(PINB, 7);
	if (state == 0) return  false;
	return true;
}
//--------------------TIMER1 interrupt--------------------------------
ISR(TIMER1_OVF_vect) {

	debounce();

}
//------------------ MAIN PROCESS-------------------------------
void process(void) {


	set_duty_cycle(ADC_MAX );

	int dx = 0;
	int dy = 0.3;
	KeepPlayerInGameArea();
	if ( BIT_IS_SET(PIND, 1) ) dy = -4;
	if ( debounce()) food_drop();
	if ( BIT_IS_SET(PINB, 1) ) dx = -2;
	if ( BIT_IS_SET(PIND, 0) ) dx = 2;
	if ( BIT_IS_SET(PINB, 0) ) pause();
	
	static bool treasure_paused = false;
	if ( BIT_IS_SET(PINF, 5)){
		treasure_paused = !treasure_paused;
	}

	if( !treasure_paused){
		treasure.x += treasure.dx;
		int tl = (int) round(treasure.x);
		int tr = tl + treasure.width -1;
		if( tl < 0 || tr >= 80){
			treasure.x -= treasure.dx;
			treasure.dx = -treasure.dx;
		}
	}
	
	if(sprites_collide(&hero, &treasure)){
		lives = lives +1;
		treasure.y = -1000;
	}
	
	if ( usb_serial_available() ) {
		int c = usb_serial_getchar();
		if ( c == 'a' ) dx = -1;
		if ( c == 'd' ) dx = +1;
		if (c == 't' ) treasure_paused = !treasure_paused;
		if ( c == 's' ) food_drop();
		if ( c == 'p' ) pause();
	}
	clear_screen();
	sprite_draw(&hero);
	sprite_draw(&starter);
	sprite_draw(&treasure);
	draw_string((LCD_X - 26), (LCD_Y - LCD_Y + 1), "x", FG_COLOUR);
	draw_double((LCD_X - 20), (LCD_Y - LCD_Y + 1), foodsupply, FG_COLOUR);
	draw();
	zombie_walk();
	int plat = get_current_platform(&hero);	
	static bool falling = false;
	bool died = false;

	if (plat >= 0){
		if (is_safe[plat]){
			if (falling){
				score ++;		
				send_score();	
			}
			falling = false;
			if (hero.y > 20){
				hero.x -= 0.5;
			}
			else{ 
				hero.x += 0.5;
			}
		}
		else{
			died = true;
		}
	}
	else if( hero.y <= (starter.y + hero.height) && hero.x >= (starter.x -5) && hero.x <= (starter.x + starter.width)){
		hero.dy = 0;
	}

	else{
		falling = true;
		hero.y ++;

	}


	if (died){
		falling = false;
		if (lives > 0){
			lives--;
			usb_serial_send("Hit unsafe block! lives decreased to: ");
			usb_serial_send_int((int)lives);
			usb_serial_send("\r\n");
			hero_die();
			setup_hero();
		}

		if (lives < 1) {
			game_over = true;
			game_over_function();
		}

	}
	die_zombie();
	
	show_screen();
	auto_move();
	if ( dx || dy ) {
		hero.x += dx;
		hero.y += dy;
		clear_screen();
		show_screen();
		usb_serial_send("Moved hero to (");
		usb_serial_send_int((int)hero.x);
		usb_serial_putchar(',');
		usb_serial_send_int((int)hero.y);
		usb_serial_send(")\r\n");
	}
}