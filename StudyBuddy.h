#pragma once

#define MAX_NAME         50
#define MAX_COURSENAME   10
#define MAX_SERVERS      100
#define DEFAULT_BUFLEN   512
#define DEFAULT_PORT     29333
#define Study_WHERE      "Where?"
#define Study_LOC	     "Loc="
#define Study_WHAT       "What?"
#define Study_COURSES    "Courses="
#define Study_MEMBERS    "Members?"
#define Study_MEMLIST    "Members="
#define Study_CONFIRM    "YES"
#define Study_DENY       "NO"
#define MAX_BUFFER 81
#define TIMEOUT_SECONDS 30
#define DISCOVERY_TIMEOUT 2
#define CHALLENGE_TIMEOUT 10
#define GREAT_TIMEOUT 2
#define MAX_SERVERS 100
#define QUERY "Who?"
#define NAME_PREFIX "Name="
#define PLAYER_PREFIX "Player="

struct ServerStruct {
	char name[MAX_NAME];
	sockaddr_in addr;
};

struct GameState {
    std::vector<int> piles;
    bool myTurn;
    std::string opponentName;
};

int getServers(SOCKET s, ServerStruct server[]);
int wait(SOCKET s, int seconds, int msec);
sockaddr_in GetBroadcastAddress(char* IPAddress, char* subnetMask);
sockaddr_in GetBroadcastAddressAlternate(char* IPAddress, char* subnetMask);