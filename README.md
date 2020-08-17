# pmserver

`pmserver` is a server that listens for text commands and can send and
receive MIDI data throughPortMidi. It works with one input and one output
device at a time.

It is best used to send and receive sysex messages, or as a MIDI monitor.
You can send any MIDI message you want, but it only makes sense for
`pmserver` to explicitly receive sysex messages because otherwise it won't
know when to stop listening.

`pmserver` listens for commands on `stdin` and writes received sysex
messages (in response to a read command) or all message (the monitor
command) to `stdout`. Command error messages are written to `stdout`
prefixed by the string "# ".

A sample session:

```
$ pmserver
> help
...help message...
> list
...list of input and output devices...
> open input 1
> send 1 90 64 127    # note on
> send 1 80 64 127    # note off
> send 1 f0 42 30 68 37 0d 0 f7 # set list bank digest request
> receive 1
f0 42 30 68 38 0d 0 ...23 bytes... f7
> quit
```

# The Commands

All commands and subcommands can be abbreviated to one character.

All lists of bytes

## l[ist]

List all open input and output ports.

## o[pen] i[nput] N

Opens input port number N.

## o[pen] o[utput] N

Opens output port number N.

## s[end] @file | .file | b[ b...]

Sends either the contents of a file (@ for ASCII hex bytes, . for binary) or
bytes to the open input. Byte values `b` are in hex. Bytes can be strung
together with our without spaces between them. If a byte string is more than
one character long then all hex numbers in it must be two hex digits long.

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
p request for set list digest
x f0 42 30 68 37 0d 0 f7
q
EOS
```
