# Simple IRC Server

A lightweight IRC (Internet Relay Chat) server implementation in C, designed for educational purposes. This server supports basic chat functionality, private messaging, and user management.

## Features

- Multiple concurrent client connections using poll()
- Username validation and management
- Broadcast messaging to all connected clients
- Private messaging between users
- User listing functionality
- Signal handling for graceful shutdown
- Maximum of 10 concurrent clients

## Technical Specifications

- Port: 2000 (default)
- Maximum clients: 10
- Maximum message buffer: 1024 bytes
- Maximum username length: 20 characters
- Username requirements: Alphanumeric characters only, 1-10 characters long

## Building

To compile the server, use gcc:

```bash
gcc -o irc_server main.c
```

## Running the Server

Start the server by running:

```bash
./irc_server
```

The server will listen on port 2000 by default.

## Client Commands

The following commands are available to clients:

- `/list` - Shows all currently connected users
- `/w username message` - Sends a private message to the specified user

## Connection Flow

1. Client connects to server
2. Server prompts for username
3. Client sends username
    - If valid: Client joins chat
    - If invalid: Server requests new username
4. Client can now send/receive messages and use commands

## Message Types

The server supports several message formats:

- Broadcast messages: `username: message`
- Private messages: `username to you: message`
- Server messages: `Server to you: message`
- System messages: `username has joined/disconnected`

## Error Handling

The server includes error handling for:
- Invalid usernames
- Connection limits
- Client disconnections
- Signal interrupts (SIGINT, SIGTERM)
- Socket errors

## Technical Implementation Details

- Uses poll() for non-blocking I/O
- Implements SO_REUSEADDR for socket reuse
- Handles partial reads and writes
- Manages dynamic memory for usernames
- Uses regex for username validation
- Implements graceful shutdown on signals

## Security Considerations

- Username validation prevents injection attacks
- Buffer sizes are strictly enforced
- Memory is properly freed on disconnection
- Socket connections are properly closed

## Limitations

- No persistent storage
- No channel support
- Basic authentication (username only)
- Fixed buffer sizes
- No encryption
- No file transfer capabilities

## Contributing

This is an educational project. Feel free to fork and extend the functionality while maintaining the educational value of the codebase.


#### Note

README created by leveraging generative AI.