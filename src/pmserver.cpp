#include <iostream>
#include <libgen.h>
#include <getopt.h>
#include <unistd.h>
#include "server.h"
#include "util.h"

#define LINE_BUFSIZ 8192

using std::cout;
using std::cerr;
using std::endl;
using std::flush;

struct opts {
  bool list_devices;
  char input_port[BUFSIZ];
  char output_port[BUFSIZ];
} opts;

void help() {
  cout << "list                  List all devices" << endl
       << "open input/output N   Open input or output port" << endl
       << "send file | b [b...]  Send file or bytes to open output; all b must be hex" << endl
       << "receive               Receive and print sysex bytes from open input" << endl
       << "w outfile             Receive sysex from open input and write to a file" << endl
       << "monitor               Receive and print all MIDI messages from open input" << endl
       << "x file | b [b...]     Send file or bytes, then receive and print" << endl
       << "f outfile file | b [b...]     Send file or bytes, then receive and save in outfile" << endl
       << "p words...            Print words (good for scripts)" << endl
       << "help                  This help" << endl
       << "quit                  Quit" << endl
       << endl
       << "all commands can be entered using the shortest unique prefix (1 char)" << endl;
}

void run(Server &server, struct opts *opts) {
  char line[LINE_BUFSIZ],  *words[MAX_WORDS];
  int err;

  if (opts->input_port[0] != 0) {
    err = server.open_input(opts->input_port);
    if (err != 0)
      cerr << "# error opening input port " << opts->input_port << endl;
  }
  if (opts->output_port[0] != 0) {
    err = server.open_output(opts->output_port);
    if (err != 0)
      cerr << "# error opening input port " << opts->output_port << endl;
  }

  while (1) {
    // get input, quitting if we see EOF
    if (isatty(fileno(stdin)))
      cout << "> " << flush;
    if (fgets(line, LINE_BUFSIZ, stdin) == 0) {
      if (feof(stdin)) {
        if (isatty(fileno(stdin)))
          cout << endl;
        return;
      }
      continue;
    }
    split_line_into_words(line, words);

    // dispatch action based on first character of first word
    char cmd = words[0][0];
    std::string str;
    switch (cmd) {
    case 'l':
      server.list_all_devices();
      break;
    case 'o':
      if (words[1] == 0 || words[1][0] == 0 || words[2] == 0 || words[2] == 0) {
        cerr <<  "# open input/output N" << endl;
        break;
      }
      switch (words[1][0]) {
      case 'o':
        str = std::string(words[2]);
        for (int i = 3; words[0] != 0; ++i) {
          str += ' ';
          str += words[i];
          i += 1;
        }
        err = server.open_output(str.c_str());
        // TODO check error
        break;
      case 'i':
        str = std::string(words[2]);
        for (int i = 3; words[0] != 0; ++i) {
          str += ' ';
          str += words[i];
          i += 1;
        }
        err = server.open_input(str.c_str());
        // TODO check error
        break;
      default:
        cout <<  "# error: unknown subcommand " << words[1] << endl;
        break;
      }
      break;
    case 's':
      if (!server.is_output_open())
        cerr << "# please select an output port first" << endl;
      else
        server.send_file_or_bytes(&words[1]);
      break;
    case 'r':
      if (!server.is_input_open())
        cerr << "# please select an input port first" << endl;
      else
        server.receive_and_print_sysex_bytes();
      break;
    case 'w':
      if (!server.is_input_open())
        cerr << "# please select an input port first" << endl;
      else
        server.receive_and_save_sysex_bytes(words[1]);
      break;
    case 'm':
      if (!server.is_input_open())
        cerr << "# please select an input port first" << endl;
      else {
        cout << "type ^C to stop monitoring" << endl;
        server.monitor_midi();
      }
      break;
    case 'p':
      for (int i = 1; words[i] != 0; ++i) {
        if (i > 1) cout << ' ';
        cout << words[i];
      }
      cout << endl;
      break;
    case 'x':
      if (!server.is_input_open() || !server.is_output_open())
        cerr << "# please select output and input ports first" << endl;
      else {
        server.send_file_or_bytes(&words[1]);
        server.receive_and_print_sysex_bytes();
      }
      break;
    case 'f':
      if (!server.is_input_open())
        cerr << "# please select an inport port" << endl;
      else {
        server.send_file_or_bytes(&words[2]);
        server.receive_and_save_sysex_bytes(words[1]);
      }
      break;
    case 'h': case '?':
      help();
      break;
    case 'q':
      return;
    case '#':
      // comment, ignore
      break;
    default:
      if (cmd != 0)
        cerr << "# unknown command " << words[0] << ", type 'h' for help" << endl;
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
  opts->input_port[0] = opts->output_port[0] = 0;
  while ((ch = getopt_long(argc, argv, "li:o:h", longopts, 0)) != -1) {
    switch (ch) {
    case 'l':
      opts->list_devices = true;
      break;
    case 'i':
      strncpy(opts->input_port, optarg, BUFSIZ);
      break;
    case 'o':
      strncpy(opts->output_port, optarg, BUFSIZ);
      break;
    case 'h': default:
      usage(argv[0]);
      exit(ch == '?' || ch == 'h' ? 0 : 1);
    }
  }
}

int main(int argc, char * const *argv) {
  Server server;
  struct opts opts;

  parse_command_line(argc, argv, &opts);
  if (opts.list_devices) {
    server.list_all_devices();
    exit(0);
  }
  run(server, &opts);
  exit(0);
  return 0;
}
