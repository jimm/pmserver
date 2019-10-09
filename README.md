# pmserver

`pmserver` is a server that listens for text commands and can send and
receive MIDI data throughPortMidi.

It is best used to send and receive sysex messages. You can send any MIDI message
you want, but it only makes sense for `pmserver` to explicitly receive sysex
messages because otherwise it won't know when to stop listening.

`pmserver` listens for commands on `stdin` and writes received sysex
messages to `stdout` (in response to a read command). Errors are written to
`stdout` prefixed by the string "# ".

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

## l[ist]

List all open input and output ports.

## o[pen] i[nput] N

Opens input port number N.

## o[pen] o[utput] N

Opens output port number N.

## s[end] file | b[, b...]

Sends contents of file (ASCII hex bytes) or bytes to the open input. Byte
values `b` are in hex.

## r[eceive]

Receives sysex from the open output and returns it as a string of ASCII hex
bytes separated by spaces.

## x file | b[, b...]

Sends contents of file (ASCII hex bytes) or bytes to the open input,
presumed to be a sysex message, and receives and prints the reply.

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
p request for set list digest 0
x f0 42 30 68 37 0d 0 f7
p request for set list digest 1
x f0 42 30 68 37 0d 1 f7
q
EOS
```
