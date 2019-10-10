#include <iostream>
#include <iomanip>
#include <stdio.h>              // NEEDED?
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>
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

using std::cout;
using std::cerr;
using std::endl;
using std::setw;
using std::setfill;
using std::hex;
using std::flush;

typedef unsigned char byte;

typedef enum SysexState {
  SYSEX_WAITING,
  SYSEX_PROCESSING,
  SYSEX_DONE
} SysexState;

struct opts {
  bool list_devices;
  int input_port;
  int output_port;
} opts;

void list_devices(const char *title, const PmDeviceInfo *infos[], int num_devices) {
  cout << title << ":" << endl;
  for (int i = 0; i < num_devices; ++i) {
    if (infos[i] == 0)
      continue;

    const char *name = infos[i]->name;
    const char *q = (name[0] == ' ' || name[strlen(name)-1] == ' ') ? "\"" : "";
    cout << "   " << setw(2) << i << ": "
         << q << name << q
         << (infos[i]->opened ? " (open)" : "")
         << endl;
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
        cout << " " << setfill('0') << setw(2) << hex << b;
        if (b == EOX) {
          cout << endl;
          return SYSEX_DONE;
        }
      }
      else if (b == SYSEX) {
        if (sysex_state != SYSEX_WAITING)
          cerr << "Hmm, something odd here: state is not WAITING but I just saw my first SYSEX byte" << endl;
        cout << " " << setfill('0') << setw(2) << hex << b;
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
      cerr << "it's been " << WAIT_FOR_SYSEX_TIMEOUT_SECS
           << "seconds and I haven't seen a SYSEX message" << endl;
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
  cout << "list                  List all devices" << endl
       << "open input/output N   Open input or output port" << endl
       << "send file | b [b...]  Send file or bytes to open input; all b must be hex" << endl
       << "receive               Receive and print bytes from open output" << endl
       << "x file | b [b...]     Send file or bytes, then receive and print" << endl
       << "p words...            Print words (good for scripts)" << endl
       << "help                  This help" << endl
       << "quit                  Quit" << endl
       << endl
       << "all commands can be entered using the shortest unique prefix (1 char)" << endl;
}

void run(struct opts *opts) {
  char line[LINE_BUFSIZ];
  char *string, **ap, *words[MAX_WORDS];
  int port, err;
  PortMidiStream *input = 0, *output = 0;

  // TODO check error
  if (opts->input_port >= 0) {
    err = Pm_OpenInput(&input, opts->input_port, 0, MIDI_BUFSIZ, 0, 0);
    if (err != 0)
      cerr << "error opening input port " << opts->input_port << endl;
  }
  if (opts->output_port >= 0) {
    err = Pm_OpenOutput(&output, opts->output_port, 0, 128, 0, 0, 0);
    if (err != 0)
      cerr << "error opening input port " << opts->output_port << endl;
  }

  while (1) {
    cout << "> " << flush;
    if (fgets(line, LINE_BUFSIZ, stdin) == 0) {
      if (feof(stdin)) {
        cout << endl;
        return;
      }
      continue;
    }
    line[strlen(line) - 1] = 0;
    string = line;
    for (ap = words; (*ap = strsep(&string, " ")) != 0; )
      if (**ap != 0)            // skip empty entries (two spaces in a row)
        if (++ap >= &words[MAX_WORDS])
          break;
    if (ap == words || words[0][0] == '#') // if empty line or comment, done
      continue;
    *ap = 0;

    switch (words[0][0]) {
    case 'l':
      list_all_devices();
      break;
    case 'o':
      if (words[0][0] == 0 || words[1][0] == 0) {
        cerr <<  "open input/output N" << endl;
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
        cout <<  "# error: unknown subcommand " << words[1] << endl;
        break;
      }
      break;
    case 's':
      if (input == 0)
        cerr << "please select an input port first" << endl;
      else
        send_file_or_bytes(input, &words[1]);
      break;
    case 'r':
      if (output == 0)
        cerr << "please select an output port first" << endl;
      else
        receive_and_print_bytes(output);
      break;
    case 'p':
      for (int i = 1; words[i] != 0; ++i) {
        if (i > 1) cout << ' ';
        cout << words[i];
      }
      cout << endl;
      break;
    case 'x':
      if (input == 0 || output == 0)
        cerr << "please select output and inport ports first" << endl;
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
        cerr << "unknown command " << words[0] << ", type 'h' for help" << endl;
      break;
    }       
  }
}

void usage(const char *prog_name) {
  cerr << "usage: " << basename((char *)prog_name) << " [-l] [-i] [-o]\n"
       << endl
       << "    -l or --list-ports" << endl
       << "        List all attached MIDI ports" << endl
       << endl
       << "    -i or --input PORT" << endl
       << "        Use input port PORT" << endl
       << endl
       << "    -o or --output PORT" << endl
       << "        Use output port PORT" << endl
       << endl
       << "    -h or --help" << endl
       << "        This help" << endl;
}

void parse_command_line(int argc, char * const *argv, struct opts *opts) {
  int ch;
  static struct option longopts[] = {
    {"list", no_argument, 0, 'l'},
    {"input-port", required_argument, 0, 'i'},
    {"output-port", required_argument, 0, 'o'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  opts->list_devices = false;
  opts->input_port = opts->output_port = -1;
  while ((ch = getopt_long(argc, argv, "li:o:h", longopts, 0)) != -1) {
    switch (ch) {
    case 'l':
      opts->list_devices = true;
      break;
    case 'i':
      opts->input_port = atoi(optarg);
      break;
    case 'o':
      opts->output_port = atoi(optarg);
      break;
    case 'h': default:
      usage(argv[0]);
      exit(ch == '?' || ch == 'h' ? 0 : 1);
    }
  }
}

int main(int argc, char * const *argv) {
  struct opts opts;

  parse_command_line(argc, argv, &opts);
  argc -= optind;
  argv += optind;

  if (opts.list_devices) {
    list_all_devices();
    exit(0);
  }

  initialize();
  run(&opts);
  exit(0);
  return 0;
}
