/*
- chip8 contains 35 instructions total. It seems like 31 are actually documented
while the other 4 are reserved and were used for various things.
- Each chip8 instruction is 2 bytes long.
- chip8 programs should be loaded at 0x200 as the byte range [0x000, 0x1FF] was
used to store the chip8 interpreter. Additionally, the last 352 bytes of memory
are reserved for variables and display refresh. Back when chip8 was actually
being used, it seems like RAM sizes were either 2048 bytes or 4096 bytes. In the
case of 2048 bytes, the address range [0x200, 0x69f] can be used for the chip8
program (1696 - 512 = 1184). That leaves address range [0x6a0, 0x800] to the
variables and display refresh.
- chip8 has 16 general purpose registers (V0 to VF). Each register is 8 bits in
length and is capable of storing unsigned integer values from 0x00 to 0xFF.
- there is a 16-bit address register that is used for reading and writing to
memory. Note that it's only possible to use the 12 least significant bits as
2^12=4096, the max RAM size available at that time.
- there must be enough space of the stack for 12 successive subroutine calls.
- there are two timers: the delay timer and the sound timer. The delay timer can
be set with the instruction FX15, and the sound timer can be set with the
instruction FX18. When a timer is set to a non-zero value, it will count down at
the rate of 60Hz until it reaches 0. Note that the min value the sound timer
will react to is 0x02. A value of 0x01 will have no audible effect.
- the chip8 interpreter will accept inputs from a 16-key keypad. Each key
corresponds with a different hex value, and it may be important to map keys from
a qwerty keyboard to the 16-key keypad.
- chip8 allows output to a monochrome screen of size 64 x 32 pixels (2048 pixels
in total). The top left corner is assigned to (0, 0) while the bottom right is
assigned to (0x3F, 0x1F). A pixel can either be set to 0x0 or 0x1. 0x0 -->
black. 0x1 --> white.
- sprites are drawn with the DXYN instruction. The input is XORed with the
current state of the screen. If the program attempts to draw a sprite at an x
coordinate greater than 0x3F, the pixel will be drawn at `<input> mod 0x3F`.
Similarly, if the program attempts to draw a sprite at a y coordinate greater
than 0x1F, the pixel will be drawn at `<input> mod 0x1F`. Note that sprites
drawn partially offscreen will be clipped. Sprites are always 8 pixels wide with
a height ranging from 1 to 15 pixels.
- there are also hexadecimal sprites that are 4 bits wide and 5 bits tall that
must be stored within the range 0x000 to 0x200. Each value needs 40 bytes in
total. 40*16=640, which is more than 512 (0x200), so I'm not really sure what's
supposed to happen there.
- to draw a font, the games will set the index register I to a character's
address, after which the character will be drawn.
*/

#include "/opt/homebrew/Cellar/sdl2/2.28.5/include/SDL2/SDL.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void add_fonts_to_mem(uint8_t *mem) {
  // Font data for 0 to F
  uint8_t fontData[16][5] = {
      {0xF0, 0x90, 0x90, 0x90, 0xF0}, // 0
      {0x20, 0x60, 0x20, 0x20, 0x70}, // 1
      {0xF0, 0x10, 0xF0, 0x80, 0xF0}, // 2
      {0xF0, 0x10, 0xF0, 0x10, 0xF0}, // 3
      {0x90, 0x90, 0xF0, 0x10, 0x10}, // 4
      {0xF0, 0x80, 0xF0, 0x10, 0xF0}, // 5
      {0xF0, 0x80, 0xF0, 0x90, 0xF0}, // 6
      {0xF0, 0x10, 0x20, 0x40, 0x40}, // 7
      {0xF0, 0x90, 0xF0, 0x90, 0xF0}, // 8
      {0xF0, 0x90, 0xF0, 0x10, 0xF0}, // 9
      {0xF0, 0x90, 0xF0, 0x90, 0x90}, // A
      {0xE0, 0x90, 0xE0, 0x90, 0xE0}, // B
      {0xF0, 0x80, 0x80, 0x80, 0xF0}, // C
      {0xE0, 0x90, 0x90, 0x90, 0xE0}, // D
      {0xF0, 0x80, 0xF0, 0x80, 0xF0}, // E
      {0xF0, 0x80, 0xF0, 0x80, 0x80}  // F
  };

  // Copy font data to mem
  for (int ii = 0; ii < 16; ii++) {
    for (int jj = 0; jj < 5; jj++) {
      mem[80 + ii * 5 + jj] = fontData[ii][jj];
    }
  }
}

struct stack {
  uint16_t m[4096];
  uint16_t needle;
};

void push_stack(struct stack *s, uint16_t val) {
  if (s->needle < UINT16_MAX) {
    s->m[s->needle] = val;
    s->needle++;
  } else {
    printf("DEBUG: no more space on the stack: %d\n", s->needle);
  }
}

// TODO (ddritzenhoff) figure out how to add debug messages.
uint16_t pop_stack(struct stack *s) {
  if (s->needle > 0) {
    uint16_t popped = s->m[s->needle];
    s->needle -= 1;
    return popped;
  }
  printf("DEBUG: nothing left to pop on the stack: %d\n", s->needle);
  return 0;
}

uint8_t char_to_val(int userInput) {
  switch (userInput) {
  case '1':
    return 0;
  case '2':
    return 1;
  case '3':
    return 2;
  case '4':
    return 3;
  case 'q':
    return 4;
  case 'w':
    return 5;
  case 'e':
    return 6;
  case 'r':
    return 7;
  case 'a':
    return 8;
  case 's':
    return 9;
  case 'd':
    return 10;
  case 'f':
    return 11;
  case 'z':
    return 12;
  case 'x':
    return 13;
  case 'c':
    return 14;
  case 'v':
    return 15;
  default:
    printf("DEBUG: unsupported key pressed");
    return 16;
  }
}

int main() {
  // intialize memory
  uint32_t mem_len = 4096;
  uint8_t mem[4096] = {0};
  add_fonts_to_mem(mem);

  // initialize display
  // TODO (ddritzenhoff) finish this.
  uint32_t display_rows = 32;
  uint32_t display_columns = 64;
  uint8_t display[32][64] = {0};

  // initialize stack
  // TODO (ddritzenhoff) change size of stack? Making the size arbitrary.
  uint16_t stack[4096] = {0};

  // initialize timers
  // TODO (ddritzenhoff) decrement timers at 60hz in another thread?
  uint8_t delay_timer = 0;
  uint8_t sound_timer = 0;

  uint16_t reg_index = 0;
  // TODO (ddritzenhoff) pretty sure this is where instructions start within
  // chip8
  uint16_t pc = 512;
  uint8_t interpret = 1;

  // create screen
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window =
      SDL_CreateWindow("Hello, SDL!", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 640, 320, SDL_WINDOW_SHOWN);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Event e;
  bool quit = false;
  while (!quit) {
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        quit = true;
      }
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();

  while (interpret) {
    // fetch
    uint16_t instr = mem[pc] << 8 | mem[pc + 1];
    pc += 2;

    // decode
    // TODO (ddritzenhoff) maybe replace these with macros??
    uint16_t op = instr & 0xF000;
    uint16_t X = instr & 0x0F00;
    uint16_t Y = instr & 0x00F0;
    uint16_t N = instr & 0x000F;
    uint16_t NN = instr & 0x00FF;
    uint16_t NNN = instr & 0x0FFF;

    // execute
    switch (instr) {
    case 0x00E0:
      // clear screen
      break;
    }
  }

  mem[80] = 0xF0;
  return 0;
}