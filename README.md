# Reliability over UDP

## Introduction

Reliability over UDP is a project designed to enhance the reliability of data transfer over the User Datagram Protocol (UDP). UDP is traditionally known for its connectionless and unreliable nature. This report provides an in-depth analysis of the design and implementation of both the server and client components of the project.

## Project Overview

The project employs a client-server architecture where the server sends a large file (video) to the client, ensuring reliability through acknowledgment mechanisms. Both the server and client use a custom packet structure containing a sequence number, data size, and payload. The client sends acknowledgments for received packets, and the server retransmits unacknowledged packets.

## How to Run

To run the `UDP_SERVER`:

1. Compile the server code using the following command in the terminal:
   ```bash
   gcc udp_server.c -lpthread -o udp_serv
   ```

2. Execute the compiled server program:
   ```bash
   ./udp_serv
   ```

To run the `UDP_CLIENT`:

1. Compile the client code using the following command in the terminal:
   ```bash
   gcc udp_client.c -lpthread -o udp_cli
   ```

2. Execute the compiled client program, providing the server IP address as a command-line argument (in this example, it is set to `127.0.0.1`):
   ```bash
   ./udp_cli 127.0.0.1
   ```

**Note:** Ensure that the required dependencies, including the `gcc` compiler, are installed on your Linux system before running the commands.


## Design and Implementation

### Server Code Overview

#### Initialization and Setup

The server initializes a UDP socket, binding it to a specified port. A thread is used to continuously receive packets and manage acknowledgment mechanisms, enhancing reliability.

#### Packet Structure

The `Packet` structure used by the server includes sequence number, data size, and payload. The sequence number uniquely identifies each packet, and a size of -1 indicates the last packet in the series.

#### Thread for Receiving Packets

The `receivePackets` function runs in a separate thread, responsible for receiving packets from the client. It utilizes sequence numbers for selective repeat, handling duplicates, updating acknowledgment arrays, and sending duplicate acknowledgments when necessary.

#### Main Function

The main function orchestrates the entire process, managing socket setup, file size exchange, packet reception, acknowledgment mechanisms, and file writing. The selective repeat mechanism, window size management, and handling of the last packet are crucial for achieving reliable data transfer over UDP.

### Client Code Overview

#### Initialization and Setup

Similar to the server, the client initializes a UDP socket and uses a thread for acknowledgment reception. This ensures the client is ready to promptly respond to packets from the server.

#### Packet Structure

The client's `Packet` structure includes a timestamp for RTT calculation, in addition to sequence numbers and data size information for reliable data transmission.

#### Thread for Receiving Acknowledgments

The `receiveAcks` function operates in a thread, focusing on receiving acknowledgments from the server. It updates acknowledgment arrays and calculates RTT for each acknowledged packet, crucial for assessing efficiency and ensuring timely retransmissions.

#### Main Function

The main function on the client side manages file reading, packet creation, transmission, acknowledgment, and RTT calculation. The implementation adheres to a selective repeat mechanism, ensuring lost packets are retransmitted, improving overall reliability.

### Reliability Enhancements

To make UDP reliable, implemented functionalities include:

- **Sequence Numbers**: Uniquely identify each packet, aiding in the selective repeat mechanism.
- **Retransmission (Selective Repeat)**: Triggered by lost or unacknowledged packets, enhancing data transfer reliability.
- **Window Size (Stop-and-Wait)**: Controls the number of unacknowledged packets, preventing congestion and ensuring reliable delivery.
- **Re-ordering on the Receiver Side**: The selective repeat mechanism inherently handles the reordering of out-of-sequence packets, contributing to overall reliability.

### RTT Considerations

Round-Trip Time (RTT) calculation is a central focus in both server and client implementations. Timestamps allow precise measurement of the time taken for a packet to travel to the server and back. The achieved RTT in this reliable UDP implementation is smaller compared to basic UDP due to the efficiency of the implemented mechanisms.


## Output Screenshots

![Server Output](/assests/output-server.jpg)

![Client Output](/assests/output-client.jpg)


## Contributors
Made with <3 by
Umer Mehmood and Adnan Hassnain





