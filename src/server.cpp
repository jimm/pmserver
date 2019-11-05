#include <iostream>
#include <iomanip>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "portmidi.h"
#include "consts.h"
#include "server.h"
#include "util.h"

#define BYTES_BUFSIZ 8192
#define MIDI_BUFSIZ 128
#define PM_EVENT_BUFSIZ 256
#define WAIT_FOR_SYSEX_TIMEOUT_SECS 10
// 10 milliseconds, in nanoseconds
#define SLEEP_NANOSECS 1e7
#define HEX_FILE_NAME_INDICATOR_CHAR '@'
#define BIN_FILE_NAME_INDICATOR_CHAR '.'

using std::cout;
using std::cerr;
using std::endl;
using std::setw;
using std::setfill;
using std::hex;
using std::vector;

typedef unsigned char byte;

void cleanup() {
  Pm_Terminate();
}

Server::Server() : input(0), output(0), sysex_state(SYSEX_WAITING) {
  Pm_Initialize();
  atexit(cleanup);
}

void Server::list_devices(const char *title, vector<PmDeviceInfo *> &devices, bool print_inputs) {
  cout << title << ":" << endl;
  vector<PmDeviceInfo *>::iterator iter = devices.begin();
  for (int i = 0; iter != devices.end(); ++iter, ++i) {
    if ((print_inputs && !(*iter)->input) || (!print_inputs && !(*iter)->output))
      continue;

    const char *name = (*iter)->name;
    const char *q = (name[0] == ' ' || name[strlen(name)-1] == ' ') ? "\"" : "";
    cout << "   " << setw(2) << i << ": "
         << q << name << q
         << ((*iter)->opened ? " (open)" : "")
         << endl;
  }
}

void Server::list_all_devices() {
  vector<PmDeviceInfo *>devices;
  int num_devices = Pm_CountDevices();

  for (int i = 0; i < num_devices; ++i)
    devices.push_back((PmDeviceInfo * const)Pm_GetDeviceInfo(i));
  list_devices("Inputs", devices, true);
  list_devices("Outputs", devices, false);
}

PmError Server::open_input(int port) {
  if (input != 0)
    Pm_Close(input);
  return Pm_OpenInput(&input, port, 0, MIDI_BUFSIZ, 0, 0);
}

PmError Server::open_output(int port) {
  if (output != 0)
    Pm_Close(output);
  return Pm_OpenOutput(&output, port, 0, 128, 0, 0, 0);
}

byte Server::char_to_nibble(const char ch) {
    switch (ch) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return (byte)(ch - '0');
      break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      return (byte)(ch - 'a' + 10);
      break;
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
      return (byte)(ch - 'A' + 10);
      break;
    default:
      throw "error";
    }
}

/*
 * Turns `word` into one or more bytes and stores them into `bytes`. Returns
 * a pointer to the byte AFTER the last byte stored in `bytes`.
 *
 * If `word` contains an odd number of characters greater than 1, an error
 * message is output and the last character is ignored.
 */
void Server::hex_word_to_bytes(const char * const word, vector<byte> &bytes) {
  if (*word == 0)
    return;

  if (word[1] == 0) {
    bytes.push_back(char_to_nibble(word[0]));
    return;
  }

  const char *p = (const char *)word;
  while (*p) {
    if (*(p+1) == 0) {
      fprintf(stderr, "error: odd number of hex digits seen, last byte dropped\n");
      return;
    }
    bytes.push_back((char_to_nibble(*p) << 4) + char_to_nibble(*(p+1)));
    p += 2;
  }
}

void Server::send_hex_file_bytes(char *fname) {
  vector<byte> bytes;
  char line[BUFSIZ], *words[MAX_WORDS];
  int i, offset = 0;

  FILE *fp = fopen(fname, "r");
  while (fgets(line, BUFSIZ, fp) != 0) {
    split_line_into_words(line, words);
    for (i = 0; words[i]; ++i) {
      if (words[i][0] == '#')
        break;
      hex_word_to_bytes(words[i], bytes);
    }
  }
  Pm_WriteSysEx(output, 0, bytes.data());
  fclose(fp);
}

void Server::send_bin_file_bytes(char *fname) {
  vector<byte> bytes;
  byte buf[BYTES_BUFSIZ];
  size_t num_read;
  int i, offset = 0;

  FILE *fp = fopen(fname, "rb");
  while ((num_read = fread(buf, 1, BYTES_BUFSIZ, fp)) > 0) {
    for (int j = 0; j < num_read; ++j)
      bytes.push_back(buf[j]);
  }
  Pm_WriteSysEx(output, 0, bytes.data());
  fclose(fp);
}

void Server::send_hex_bytes(char **words) {
  vector<byte> bytes;
  int i;

  for (i = 0; words[i]; ++i)
    hex_word_to_bytes(words[i], bytes);
  Pm_WriteSysEx(output, 0, bytes.data());
}

void Server::send_file_or_bytes(char **words) {
  switch (words[0][0]) {
  case HEX_FILE_NAME_INDICATOR_CHAR:
    send_hex_file_bytes(&words[0][1]);
    break;
  case BIN_FILE_NAME_INDICATOR_CHAR:
    send_bin_file_bytes(&words[0][1]);
    break;
  default:
    send_hex_bytes(words);
    break;
  }
}

void Server::read_and_process_sysex() {
  PmEvent events[PM_EVENT_BUFSIZ];

  int num_read = Pm_Read(input, events, PM_EVENT_BUFSIZ);
  for (int i = 0; i < num_read; ++i) {
    PmMessage msg = events[i].message;
    byte *bp = (byte *)&msg;
    for (int j = 0; j < 4; ++j) {
      byte b = bp[j];
      if (sysex_state == SYSEX_PROCESSING) {
        // cout << " " << setfill('0') << setw(2) << hex << b;
        printf(" %02x", b);
        if (b == EOX) {
          cout << endl;
          sysex_state = SYSEX_DONE;
          return;
        }
      }
      else if (b == SYSEX) {
        if (sysex_state != SYSEX_WAITING)
          cerr << "Hmm, something odd here: state is not WAITING but I just saw my first SYSEX byte" << endl;
        // cout << " " << setfill('0') << setw(2) << hex << b;
        printf(" %02x", b);
        sysex_state = SYSEX_PROCESSING;
      }
    }
  }
}

void Server::receive_and_print_bytes() {
  struct timespec rqtp = {0, SLEEP_NANOSECS};
  time_t start_time = time(0);

  sysex_state = SYSEX_WAITING;
  while (sysex_state != SYSEX_DONE) {
    if (difftime(time(0), start_time) >= WAIT_FOR_SYSEX_TIMEOUT_SECS) {
      cerr << "it's been " << WAIT_FOR_SYSEX_TIMEOUT_SECS << " seconds";
      switch (sysex_state) {
      case SYSEX_WAITING:
        cerr << " and I haven't seen a SYSEX message" << endl;
        return;
      case SYSEX_PROCESSING:
        cerr << " and I'm still getting SYSEX!" << endl;
        break;
      default:
        break;
      }
    }
    if (Pm_Poll(input) == TRUE)
      read_and_process_sysex();
    else {
      if (nanosleep(&rqtp, 0) == -1)
        return;                 // TODO handle error
    }
  }
}
