# netput

Cross platform C++ framework for streaming input and UI events over capnproto RPC.

## Requirements

To build netput, the following is required:

+ C++17(14 might work but I doubt it)
+ CMake

## Usage

netput was designed to resemble SDL events, so the simplest use case is
to capture events in SDL and then send them over the netput client.

