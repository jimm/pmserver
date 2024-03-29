#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <vector>
#include "portmidi.h"

typedef unsigned char byte;

typedef enum SysexState {
  SYSEX_WAITING,
  SYSEX_PROCESSING,
  SYSEX_DONE
} SysexState;

class Server {
public:
  Server();

  void list_all_devices();
  void send_file_or_bytes(char **words);

  PmError open_input(const char *port_num_or_name);
  PmError open_output(const char *port_num_or_name);

  void receive_and_print_sysex_bytes();
  void receive_and_save_sysex_bytes(const char * const output_path);
  void monitor_midi();

  bool is_input_open() { return input != nullptr; }
  bool is_output_open() { return output != nullptr; }

  // only public for testing
  void hex_word_to_bytes(const char * const word, std::vector<byte> &bytes);

protected:
  PortMidiStream *input;
  PortMidiStream *output;
  SysexState sysex_state;
  size_t sysex_offset;
  byte sysex_bytes[16];

  void list_devices(const char *title, std::vector<PmDeviceInfo *> &devices, bool inputs);
  int port_number_matching_name(const char *name, bool match_inputs);
  byte char_to_nibble(const char ch);
  void send_hex_file_bytes(char *fname);
  void send_bin_file_bytes(char *fname);
  void send_hex_bytes(char **words);
  void send_bytes(std::vector<byte> &bytes);
  void read_and_process_sysex();
  void read_and_save_sysex(FILE *fp);
  void read_and_process_any_message();
  void print_note(PmMessage msg, const char * const name);
  void print_three_byte_chan(PmMessage msg, const char * const name);
  void print_two_byte(PmMessage msg, const char * const name);
  void print_four_sysex_bytes(PmMessage msg);
  void print_sys_common(PmMessage msg);

  void print_sysex_byte(byte b);
  void print_sysex_line();
  void end_sysex_print();
};

#endif /* SERVER_H */
