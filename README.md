# pmserver

`pmserver` is a server that listens for text commands and can send and
receive MIDI data through PortMidi. It works with one input and one output
device at a time.

This utility is best used to send and receive sysex messages or as a MIDI
monitor. You can send any MIDI message you want, but it only makes sense for
`pmserver` to explicitly receive sysex messages because otherwise it won't
know when to stop listening.

`pmserver` listens for commands on `stdin` and writes received sysex
messages (in response to a read command) or all message (the monitor
command) to `stdout`. Command error messages are written to `stdout`
prefixed by the string "# ". Because `pmserver` responds to text commands on
`stdin`, it is scriptable.

A sample session:

```
$ pmserver
> help
...help message...
> list
...list of input and output devices...
> open input 1
> open output My Device
> send 90 64 127    # note on
> send 80 64 127    # note off
> send f0 42 30 68 37 0d 0 f7 # set list bank digest request
> receive
f0 42 30 68 38 0d 0 ...23 bytes... f7
> quit
```

# The Commands

All commands and subcommands can be abbreviated to one character.

All lists of bytes are displayed in hexadecimal.

## l[ist]

List all open input and output ports.

## o[pen] i[nput] INPUT

Opens an input port. `INPUT` can either be a port number or name.

## o[pen] o[utput] OUTPUT

Opens an input port. `OUTPUT` can either be a port number or name.

## s[end] @file | .file | b[ b...]

Sends either the contents of a file (@ for ASCII hex bytes, . for binary) or
bytes to the open input. Byte values `b` are in hex.

Bytes can be strung together without spaces between them. If a byte string
is more than one character long (as all status and system message start
bytes will be) then all hex numbers in it must be two hex digits long.
Examples:
- `b90` `b30` `bff` sends a note on message
- `b80` `b30` `bff` sends the corresponding note off message
- `b9030ff` sends the same note on, and `b8030ff` the same note off

## r[eceive]

Receives sysex from the open output and returns it as a string of ASCII hex
bytes separated by spaces.

## w[rite] file

Receives sysex from the open input and saves it to a file.

## m[onitor]

Listens for and prints all incoming MIDI messages. This is a superset of the
`receive` command.

Type `^C` to stop monitoring. (NOTE: this will quit pmserver now. That is a
bug that I will fix ASAP.)

## x @file | .file | b[, b...]

Sends either the contents of a file (@ for ASCII hex bytes, . for binary) or
bytes to the open input, presumed to be a sysex message, and receives and
prints the reply.

## f outfile @infile | .infile | b[, b...]

Sends either the contents of infile (@ for ASCII hex bytes, . for binary) or
bytes to the open input, presumed to be a sysex message, and receives and
saves the reply to outfile.

## p words...

Prints out words. Useful when running a script passed in to stdin.

## q[uit]

# Limitations

Only one input and output can be open at a time.

# Example

```sh
$ ./pmserver <<EOS
o i 1
o o 3
p request for Kronos set list digest
x f0 42 30 68 37 0d 0 f7
q
EOS
```
