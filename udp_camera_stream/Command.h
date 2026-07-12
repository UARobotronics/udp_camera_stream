#pragma once

#define MAX_COMMAND_LENGTH 3

#define CMD_BASE 0xAA

#define CMD_CONNECT_TO_SERVER     (CMD_BASE + 0)
#define CMD_SEND_ACK_TO_CLIENT (CMD_BASE + 1)

#define CMD_SEND_ACK_TO_CLIENT_SIZE 1
