#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "portmidi.h"
#include "consts.h"

#define LINE_BUFSIZ 8192
#define MIDI_BUFSIZ 128
#define BYTES_BUFSIZ 8192
#define MAX_WORDS 1024
#define PM_EVENT_BUFSIZ 256
#define WAIT_FOR_SYSEX_TIMEOUT_SECS 5
// 10 milliseconds, in nanoseconds
#define SLEEP_NANOSECS 1e7

typedef unsigned char byte;

typedef enum SysexState {
  SYSEX_WAITING,
  SYSEX_PROCESSING,
  SYSEX_DONE
} SysexState;

void list_devices(const char *title, const PmDeviceInfo *infos[], int num_devices) {
  printf("%s:\n", title);
  for (int i = 0; i < num_devices; ++i)
    if (infos[i] != 0) {
      const char *name = infos[i]->name;
      const char *q = (name[0] == ' ' || name[strlen(name)-1] == ' ') ? "\"" : "";
      printf("  %2d: %s%s%s%s\n", i, q, name, q, infos[i]->opened ? " (open)" : "");
    }
}

void list_all_devices() {
  int num_devices = Pm_CountDevices();
  const PmDeviceInfo *inputs[num_devices], *outputs[num_devices];

  for (int i = 0; i < num_devices; ++i) {
    const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
    inputs[i] = info->input ? info : 0;
    outputs[i] = info->output ? info : 0;
  }

  list_devices("Inputs", inputs, num_devices);
  list_devices("Outputs", outputs, num_devices);
}

void cleanup() {
  Pm_Terminate();
}

void initialize() {
  Pm_Initialize();
  atexit(cleanup);
}

byte hex_word_to_byte(char *word) {
  byte val = 0;
  for (char *ch = word; *ch; ++ch) {
    val *= 16;
    switch (*ch) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      val += (byte)(*ch - '0');
      break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      val += (byte)(*ch - 'a' + 10);
      break;
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
      val += (byte)(*ch - 'A' + 10);
      break;
    }
  }
  return val;
}

void split_line_into_words(char *line, char *words[]) {
  char *string, **ap;
  int line_len = strlen(line);

  if (line[line_len - 1] == '\n')
    line[line_len - 1] = 0;
  string = line;
  for (ap = words; (*ap = strsep(&string, " ")) != 0; )
    if (**ap != 0)
      if (++ap >= &words[MAX_WORDS])
        break;
  *ap = 0;
}

void send_file_bytes(PortMidiStream *input, char *fname) {
  char line[BUFSIZ], *words[MAX_WORDS];
  byte bytes[BYTES_BUFSIZ], *bptr = bytes;
  int i, offset = 0;
  FILE *fp = fopen(fname, "r");

  while (fgets(line, BUFSIZ, fp) != 0) {
    split_line_into_words(line, words);
    for (i = 0; words[i]; ++i) {
      if (words[i][0] == '#')
        break;
      *bptr++ = hex_word_to_byte(words[i]);
    }
  }
  Pm_WriteSysEx(input, 0, bytes);
  fclose(fp);
}

void send_hex_bytes(PortMidiStream *input, char **words) {
  byte bytes[BYTES_BUFSIZ];
  int i;

  for (i = 0; words[i]; ++i)
    bytes[i] = hex_word_to_byte(words[i]);
  Pm_WriteSysEx(input, 0, bytes);
}

void send_file_or_bytes(PortMidiStream *input, char **words) {
  if (access(words[0], F_OK) != -1)
    send_file_bytes(input, words[0]);
  else
    send_hex_bytes(input, words);
}

SysexState read_and_process_sysex(PortMidiStream *output, SysexState sysex_state) {
  PmEvent events[PM_EVENT_BUFSIZ];

  int num_read = Pm_Read(output, events, PM_EVENT_BUFSIZ);
  for (int i = 0; i < num_read; ++i) {
    for (int j = 0; j < 4; ++j) {
      PmMessage msg = events[i].message;
      byte b = ((byte *)(void *)&msg)[j];
      if (sysex_state == SYSEX_PROCESSING) {
        printf(" %02x", b);
        if (b == EOX) {
          printf("\n");
          return SYSEX_DONE;
        }
      }
      else if (b == SYSEX) {
        if (sysex_state != SYSEX_WAITING)
          fprintf(stderr, "Hmm, something odd here: state is not WAITING but I just saw my first SYSEX byte\n");
        printf(" %02x", b);
        sysex_state = SYSEX_PROCESSING;
      }
    }
  }
  return sysex_state;
}

void receive_and_print_bytes(PortMidiStream *output) {
  struct timespec rqtp = {0, SLEEP_NANOSECS};
  SysexState sysex_state = SYSEX_WAITING;
  time_t start_time = time(0);

  while (sysex_state != SYSEX_DONE) {
    if (difftime(time(0), start_time) >= WAIT_FOR_SYSEX_TIMEOUT_SECS) {
      fprintf(stderr,
              "it's been %d seconds and I haven't seen a SYSEX message\n",
              WAIT_FOR_SYSEX_TIMEOUT_SECS);
      return;
    }
    if (Pm_Poll(output) == TRUE)
      sysex_state = read_and_process_sysex(output, sysex_state);
    else {
      if (nanosleep(&rqtp, 0) == -1)
        return;                 // TODO handle error
    }
  }
}

void help() {
  printf("list                  List all devices\n");
  printf("open input/output N   Open input or output port\n");
  printf("send file | b [b...]  Send file or bytes to open input; all b must be hex\n");
  printf("receive               Receive and print bytes from open output\n");
  printf("x file | b [b...]     Send file or bytes, then receive and print\n");
  printf("p words...            Print words (good for scripts)\n");
  printf("help                  This help\n");
  printf("quit                  Quit\n");
  printf("\n");
  printf("all commands can be entered using the shortest unique prefix (1 char)\n");
}

void run() {
  char line[LINE_BUFSIZ];
  char *string, **ap, *words[MAX_WORDS];
  int port, err;
  PortMidiStream *input = 0, *output = 0;

  while (1) {
    printf("> ");
    fflush(stdout);
    if (fgets(line, LINE_BUFSIZ, stdin) == 0) {
      if (feof(stdin)) {
        printf("\n");
        return;
      }
      continue;
    }
    line[strlen(line) - 1] = 0;
    string = line;
    for (ap = words; (*ap = strsep(&string, " ")) != 0; )
      if (**ap != 0)
        if (++ap >= &words[MAX_WORDS])
          break;
    *ap = 0;

    switch (words[0][0]) {
    case 'l':
      list_all_devices();
      break;
    case 'o':
      if (words[0][0] == 0 || words[1][0] == 0) {
        fprintf(stderr, "open input/output N\n");
        break;
      }
      port = atoi(words[2]);
      switch (words[1][0]) {
      case 'o':
        if (output != 0)
          Pm_Close(output);
        err = Pm_OpenOutput(&output, port, 0, 128, 0, 0, 0);
        // TODO check error
        break;
      case 'i':
        if (input != 0)
          Pm_Close(input);
        err = Pm_OpenInput(&input, port, 0, MIDI_BUFSIZ, 0, 0);
        // TODO check error
        break;
      default:
        printf("# error: unknown subcommand %s\n", words[1]);
        break;
      }
      break;
    case 's':
      if (input == 0)
        fprintf(stderr, "please select an input port first\n");
      else
        send_file_or_bytes(input, &words[1]);
      break;
    case 'r':
      if (output == 0)
        fprintf(stderr, "please select an output port first\n");
      else
        receive_and_print_bytes(output);
      break;
    case 'p':
      for (i = 1; words[i] != 0; ++i) {
        if (i > 1) putchar(' ');
        puts(words[i]);
      }
      putchar('\n');
      break;
    case 'x':
      if (input == 0 || output == 0)
        fprintf(stderr, "please select output and inport ports first\n");
      else {
        send_file_or_bytes(input, &words[1]);
        receive_and_print_bytes(output);
      }
      break;
    case 'h': case '?':
      help();
      break;
    case 'q':
      return;
    default:
      if (words[0][0] != 0)
        fprintf(stderr, "unknown command %s, type 'h' for help\n", words[0]);
      break;
    }       
  }
}

int main(int argc, char * const *argv) {
  initialize();
  run();
  exit(0);
  return 0;
}
