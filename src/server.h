#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include "portmidi.h"

#define MAX_WORDS 1024

typedef unsigned char byte;

extern void initialize();
extern void list_all_devices();
extern void send_file_or_bytes(PortMidiStream *output, char **words);
extern void receive_and_print_bytes(PortMidiStream *input);

// For testing
extern void hex_word_to_bytes(const char * const word, std::vector<byte> &bytes);

#endif /* SERVER_H */
