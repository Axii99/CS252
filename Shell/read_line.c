#include "read_line.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

#include "tty_raw_mode.h"


//extern void tty_raw_mode(void);

int g_line_length = 0;
char g_line_buffer[MAX_BUFFER_LINE];

// Simple history array
// This history does not change.
// Yours have to be updated.
int g_line_position = 0;
int g_history_index = 0;
int g_max_hist_num = 100;
char **g_history = NULL;
int g_history_length = 0;

/*
 *  Prints usage for read_line
 */

void read_line_print_usage() {
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
} /* read_line_print_usage() */

/*
 * Input a line with some basic editing.
 */

char *read_line() {
  struct termios tmp_attr = {0};
  tcgetattr(0, &tmp_attr);
  // Set terminal in raw mode
  tty_raw_mode();

  g_line_length = 0;
  g_line_position = 0;
  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch = '\0';
    read(0, &ch, 1);

    if ((ch >= 32) && (ch < 127)) {
        // It is a printable character.
      if (g_line_position == g_line_length) {
        // Do echo
        write(1, &ch, 1);

        // If max number of character reached return.
        if (g_line_length == (MAX_BUFFER_LINE - 2)) {
          break;
        }

        // add char to buffer.
        g_line_buffer[g_line_length] = ch;
        g_line_length++;
        g_line_position++;
      }
      else {
        char temp[MAX_BUFFER_LINE];
        strncpy(temp, g_line_buffer + g_line_position,
                    g_line_length - g_line_position);
        temp[g_line_length - g_line_position] = '\0';
        write(1, &ch, 1);
        g_line_buffer[g_line_position] = ch;
        g_line_length++;
        g_line_position++;
        if (g_line_length == (MAX_BUFFER_LINE - 2)) {
          break;
        }
        for (unsigned int i = 0; i < strlen(temp); i++) {
          write(1, &temp[i], 1);
          g_line_buffer[g_line_position + i] = temp[i];
        }
        for (unsigned int i = 0; i < strlen(temp); i++) {
          char c = 8;
          write(1, &c, 1);
        }
      }
    }
    else if (ch == 10) {
      // <Enter> was typed. Return line
      // Print newline
      write(1, &ch, 1);
      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      g_line_buffer[0] = 0;
      break;
    }
    else if ((ch == 8) || (ch == 127)) {
        // <backspace> was typed. Remove previous character read.
      if (g_line_length > 0) {
        // Go back one character
        ch = 8;
        write(1, &ch, 1);

        // Write a space to erase the last character read
        ch = ' ';
        write(1, &ch, 1);

        // Go back one character
        ch = 8;
        write(1, &ch, 1);

        // Remove one character from buffer
        g_line_length--;
        g_line_position--;
      }
    }
    else if (ch == 18) {
      int index = 0;
      while (1) {
        char c = '\0';
        read(0, &c, 1);
        if ((c >= 32) && (c < 127)) {
          g_line_buffer[g_line_length] = c;
          g_line_length++;
          g_line_position++;
          index = 0;
          while (index < g_history_length) {
            if (strchr(g_history[index], c)) {
              char* result = g_history[index];
              write(1, result, strlen(result));
              break;
            }
            index++;
          }
        }
        else {
          break;
        }
      }
    }
    else if (ch == 27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1 = '\0';
      char ch2 = '\0';
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if ((ch1 == 91) && (ch2 == 65)) {
        // Up arrow. Print next line in history.
        char* temp = strdup(g_line_buffer);
        // Erase old line
        // Print backspaces
        int i = 0;
        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // Print spaces on top
        for (i = 0; i < g_line_length; i++) {
          ch = ' ';
          write(1, &ch, 1);
        }

        // Print backspaces
        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // Copy line from history
        if ((g_history_length > 0) && (g_history_index >= 0)) {
          if (g_history[g_history_index] != NULL) {
            strcpy(g_line_buffer, g_history[g_history_index]);
            g_line_length = strlen(g_line_buffer);
          }
          else {
            strcpy(g_line_buffer, "");
            g_line_length = 0;
          }
          g_history_index = g_history_index - 1;
          if (g_history_index < 0) {
            g_history_index = 0;
          }
          // echo line
          write(1, g_line_buffer, g_line_length);
        }
        else {
          strcpy(g_line_buffer, temp);
          g_line_length = strlen(g_line_buffer);
          write(1, g_line_buffer, g_line_length);
        }
        g_line_position = g_line_length;
      }
      else if ((ch1 == 91) && (ch2 == 66)) {
        //Down arrow
        char *temp = strdup(g_line_buffer);
        int i = 0;
        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        for (i = 0; i < g_line_length; i++) {
          ch = ' ';
          write(1, &ch, 1);
        }

        for (i = 0; i < g_line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        if ((g_history_length > 0)
            && (g_history_index <= g_history_length - 1)) {
          if (g_history[g_history_index] != NULL) {
            strcpy(g_line_buffer, g_history[g_history_index]);
            g_line_length = strlen(g_line_buffer);
          }
          else {
            strcpy(g_line_buffer, "");
            g_line_length = 0;
          }
          g_history_index = g_history_index + 1;
          if (g_history_index == g_history_length) {
            g_history_index = g_history_length - 1;
          }
          write(1, g_line_buffer, g_line_length);
        }
        else {
          strcpy(g_line_buffer, temp);
          write(1, temp, strlen(temp));
          g_line_length = strlen(temp);
        }
        g_line_position = g_line_length;
      }
      else if ((ch1 == 91) && (ch2 == 68)) {
        if (g_line_position > 0) {
          g_line_position--;
          char c = 8;
          write(1, &c, 1);
        }
      }
      else if ((ch1 == 91) && (ch2 == 67)) {
        if (g_line_position < g_line_length) {
          char c = g_line_buffer[g_line_position];
          g_line_position++;
          write(1, &c, 1);
        }
      }
      else if ((ch1 == 91) && (ch2 == 51)) {
        char c = '\0';
        read(0, &c, 1);
        if (g_line_position == g_line_length) {
          if (g_line_length > 0) {
            c = 8;
            write(1, &c, 1);
            c = ' ';
            write(1, &c, 1);
            c = 8;
            write(1, &c, 1);
            g_line_length--;
            g_line_position--;
          }
        }
        else {
          for (int i = 0; i <= g_line_length - g_line_position; i++) {
            c = g_line_buffer[g_line_position + i + 1];
            write(1, &c, 1);
          }
          c = ' ';
          write(1, &c, 1);
          for (int i = 0; i < g_line_length - g_line_position; i++) {
            c = 8;
            write(1, &c, 1);
          }

          for (int i = 0; i <=
                g_line_length - g_line_position; i++) {
            g_line_buffer[g_line_position + i] =
                                g_line_buffer[g_line_position + i + 1];
          }
          g_line_length--;
          g_line_buffer[g_line_length] = '\0';
        }
      }
      else if ((ch1 == 91) && (ch2 == 49)) {
        char c = '\0';
        read(0, &c, 1);
        while (g_line_position > 0) {
          g_line_position--;
          char c = 8;
          write(1, &c, 1);
        }
      }
      else if ((ch1 == 91) && (ch2 == 52)) {
        char c = '\0';
        read(0, &c, 1);
        while (g_line_position < g_line_length) {
          char c = g_line_buffer[g_line_position];
          g_line_position++;
          write(1, &c, 1);
        }
      }
    }
  }

  // Add eol and null char at the end of string
  g_line_buffer[g_line_length] = 10;
  g_line_length++;
  g_line_buffer[g_line_length] = 0;

  if (g_history == NULL) {
    g_history = (char**) malloc(g_max_hist_num * sizeof(char*));
  }
  if (g_history_length == g_max_hist_num) {
    g_max_hist_num *= 2;
    g_history = (char**) realloc(g_history,
                          g_max_hist_num * sizeof(char*));
  }
  g_history[g_history_length] =
      (char*) malloc(strlen(g_line_buffer) * sizeof(char) + 1);
  strcpy(g_history[g_history_length], g_line_buffer);
  g_history[g_history_length][strlen(g_line_buffer) - 1] = '\0';
  g_history_length++;
  g_history_index = g_history_length - 1;

  tcsetattr(0, TCSANOW, &tmp_attr);
  return g_line_buffer;
} /* read_line() */

