#include <iostream>
#include <iomanip>
#include <signal.h>
#include "consts.h"
#include "portmidi.h"
#include "server.h"
#include "util.h"

#define BYTES_BUFSIZ 8192
#define MIDI_BUFSIZ 128
#define PM_EVENT_BUFSIZ 256
#define WAIT_FOR_SYSEX_TIMEOUT_SECS 10
// 10 milliseconds, in nanoseconds
#define SLEEP_NANOSECS 10000000L
#define HEX_FILE_NAME_INDICATOR_CHAR '@'
#define BIN_FILE_NAME_INDICATOR_CHAR '.'

using std::cout;
using std::cerr;
using std::endl;
using std::setw;
using std::setfill;
using std::vector;

typedef unsigned char byte;

sig_atomic_t monitoring;

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

void Server::receive_and_print_sysex_bytes() {
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

void stop_monitoring(int _sig) {
  monitoring = 0;
}

void Server::receive_and_print_all_messages() {
  struct timespec rqtp = {0, SLEEP_NANOSECS};
  time_t start_time = time(0);
  struct sigaction action = {stop_monitoring, SIGINT, SA_RESETHAND};

  sigaction(SIGINT, &action, nullptr);

  monitoring = 1;
  while (monitoring == 1) {
    if (Pm_Poll(input) == TRUE)
      read_and_process_any_message();
    else {
      if (nanosleep(&rqtp, 0) == -1)
        return;                 // TODO handle error
    }
  }
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

void Server::read_and_process_any_message() {
  PmEvent events[PM_EVENT_BUFSIZ];

  int num_read = Pm_Read(input, events, PM_EVENT_BUFSIZ);
  for (int i = 0; i < num_read; ++i) {
    PmMessage msg = events[i].message;
    switch (Pm_MessageStatus(msg) & 0xf0) {
    case NOTE_OFF:
      print_note(msg, "off");
      break;
    case NOTE_ON:
      print_note(msg, Pm_MessageData2(msg) == 0 ? "off" : "on");
      break;
    case POLY_PRESSURE:
      print_three_byte_chan(msg, "ppress");
      break;
    case CONTROLLER:
      print_three_byte_chan(msg, "cntrl");
      break;
    case PROGRAM_CHANGE:
      print_two_byte(msg, "pchg");
      break;
    case CHANNEL_PRESSURE:
      print_two_byte(msg, "cpress");
      break;
    case PITCH_BEND:
      print_three_byte_chan(msg, "pbend");
      break;
    case 0xf0:
      print_sys_common(msg);
      break;
    }
  }
}

void Server::print_note(PmMessage msg, const char * const name) {
  printf("%s\t%02d\t%d\t%d\n", name, (Pm_MessageStatus(msg) & 0x0f) + 1,
         Pm_MessageData1(msg), Pm_MessageData2(msg));
}

void Server::print_three_byte_chan(PmMessage msg, const char * const name) {
  printf("%s\t%02d\t%d\t%d\n", name, (Pm_MessageStatus(msg) & 0x0f) + 1,
         Pm_MessageData1(msg), Pm_MessageData2(msg));
}

void Server::print_two_byte(PmMessage msg, const char * const name) {
  printf("%s\t%02d\t%d\n", name, (Pm_MessageStatus(msg) & 0x0f) + 1,
         Pm_MessageData1(msg));
}

void Server::print_sys_common(PmMessage msg) {
  switch (Pm_MessageStatus(msg)) {
  case SYSEX:
    // TODO
    break;
  case SONG_POINTER:
    printf("songptr\t%d\t%d\n", Pm_MessageData1(msg), Pm_MessageData2(msg));
    break;
  case SONG_SELECT:
    printf("songsel\t%d\n", Pm_MessageData1(msg));
    break;
  case TUNE_REQUEST:
    printf("tunereq\n");
    break;
  case EOX:
    printf("eox\n");
    break;
  case CLOCK:
    printf("clock\n");
    break;
  case START:
    printf("start\n");
    break;
  case CONTINUE:
    printf("cont\n");
    break;
  case STOP:
    printf("stop\n");
    break;
  case ACTIVE_SENSE:
    printf("asense\n");
    break;
  case SYSTEM_RESET:
    printf("reset\n");
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
        if (b == EOX) {
          cout << endl;
          sysex_state = SYSEX_DONE;
          return;
        }
      }
      else if (b == SYSEX) {
        if (sysex_state != SYSEX_WAITING)
          cerr << "Hmm, something odd here: state is not WAITING but I just saw my first SYSEX byte" << endl;
        sysex_state = SYSEX_PROCESSING;
      }
    }
  }
}
