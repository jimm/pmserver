#ifndef SERVER_H
#define SERVER_H

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

  PmError open_input(int port);
  PmError open_output(int port);

  void receive_and_print_sysex_bytes();
  void receive_and_print_all_messages();

  bool is_input_open() { return input != 0; }
  bool is_output_open() { return output != 0; }

  // only public for testing
  void hex_word_to_bytes(const char * const word, std::vector<byte> &bytes);

protected:
  PortMidiStream *input;
  PortMidiStream *output;
  SysexState sysex_state;

  void list_devices(const char *title, std::vector<PmDeviceInfo *> &devices, bool inputs);
  byte char_to_nibble(const char ch);
  void send_hex_file_bytes(char *fname);
  void send_bin_file_bytes(char *fname);
  void send_hex_bytes(char **words);
  void read_and_process_sysex();
  void read_and_process_any_message();
  void print_note(PmMessage msg, const char * const name);
  void print_three_byte_chan(PmMessage msg, const char * const name);
  void print_two_byte(PmMessage msg, const char * const name);
  void print_sys_common(PmMessage msg);
};

#endif /* SERVER_H */
