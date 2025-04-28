#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <algorithm>
#include "StudyBuddy.h"
//#include <Utilities.cpp>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

/*================================================================================*/
// Winsock Initialization

void initializeWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        exit(1);
    }
}

/*================================================================================*/
// Socket Creation

SOCKET createUdpSocket() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(1);
    }
    return s;
}

/*================================================================================*/
// Bind Socket

void bindSocket(SOCKET s, int port) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cout << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(s);
        WSACleanup();
        exit(1);
    }
}

/*================================================================================*/
// Wait Function (Adapted from StudyBuddy)

//create async timer using <chrono> and <thread> that will wait for a specified time while waiting for messages on the socket
void waitForMessage(SOCKET s, int seconds) {
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(s, &readfds);
	timeval timeout;
	timeout.tv_sec = seconds;
	timeout.tv_usec = 0;
	int result = select(0, &readfds, NULL, NULL, &timeout);
    if (result == SOCKET_ERROR) {
		std::cout << "Select failed: " << WSAGetLastError() << std::endl;
		closesocket(s);
		WSACleanup();
		exit(1);
	}
}

/*================================================================================*/
// Get Broadcast Address (Adapted from StudyBuddy)

sockaddr_in GetBroadcastAddress() {
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*)HeapAlloc(GetProcessHeap(), 0, sizeof(IP_ADAPTER_INFO));
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        HeapFree(GetProcessHeap(), 0, pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)HeapAlloc(GetProcessHeap(), 0, ulOutBufLen);
    }
    sockaddr_in addr{};
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == NO_ERROR && pAdapterInfo) {
        unsigned long ip, mask;
        inet_pton(AF_INET, pAdapterInfo->IpAddressList.IpAddress.String, &ip);
        inet_pton(AF_INET, pAdapterInfo->IpAddressList.IpMask.String, &mask);
        unsigned long bcast = ip | (mask ^ 0xffffffff);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(DEFAULT_PORT);
        addr.sin_addr.s_addr = bcast;
    }
    if (pAdapterInfo) HeapFree(GetProcessHeap(), 0, pAdapterInfo);
    return addr.sin_addr.s_addr == INADDR_ANY ? addr : addr; // Fallback to 255.255.255.255 if needed
}
/*================================================================================*/
// Discover Servers (Adapted from StudyBuddy)

int getServers(SOCKET s, ServerStruct servers[]) {
    int numServers = 0;
    BOOL bOptVal = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, sizeof(BOOL));
    sockaddr_in broadcastAddr = GetBroadcastAddress();
    sendto(s, QUERY, strlen(QUERY) + 1, 0, (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
    char recvBuf[MAX_BUFFER];
    int status = wait(s, DISCOVERY_TIMEOUT, 0);
    while (status > 0) {
        sockaddr_in addr;
        int addrSize = sizeof(addr);
        int len = recvfrom(s, recvBuf, MAX_BUFFER, 0, (sockaddr*)&addr, &addrSize);
        if (len > 0 && strncmp(recvBuf, NAME_PREFIX, 5) == 0) {
            strncpy_s(servers[numServers].name, recvBuf + 5, MAX_BUFFER - 5);
            servers[numServers].name[MAX_BUFFER - 5] = '\0';
            servers[numServers].addr = addr;
            numServers++;
        }
        status = wait(s, 0, 100); // Short wait for additional responses
    }
    return numServers;
}
/*================================================================================*/
// Send Message

void sendMessage(SOCKET s, const std::string& message, sockaddr_in dest) {
    sendto(s, message.c_str(), message.length() + 1, 0, (sockaddr*)&dest, sizeof(dest));
}

/*================================================================================*/
// Receive Message

std::string receiveMessage(SOCKET s, sockaddr_in& sender, int seconds = TIMEOUT_SECONDS) {
    char buf[MAX_BUFFER];
    int senderLen = sizeof(sender);
    if (wait(s, seconds, 0) > 0) {
        int len = recvfrom(s, buf, MAX_BUFFER, 0, (sockaddr*)&sender, &senderLen);
        if (len > 0) {
            buf[len] = '\0';
            return std::string(buf);
        }
    }
    return "";
}
/*================================================================================*/
// User Input Functions

std::string getUserName() {
    std::cout << "Enter your name (max 80 chars): ";
    std::string name;
    std::getline(std::cin, name);
    return name.length() > 80 ? name.substr(0, 80) : name;
}

char getModeChoice() {
    std::cout << "Would you like to (H)ost or (C)hallenge a game? (Q to quit): ";
    char choice;
    std::cin >> choice;
    return toupper(choice);
}

/*================================================================================*/
// Client Negotiation

bool clientNegotiate(SOCKET s, const std::string& clientName, GameState& game, sockaddr_in& serverAddr) {
    // Fetch servers
    ServerStruct servers[MAX_SERVERS];
    int numServers = getServers(s, servers);
    if (numServers == 0) {
        std::cout << "No servers found.\n";
        return false;
    }
    
    // Give client the option to pick a server to challenge
    std::cout << "Available servers:\n";
    for (int i = 0; i < numServers; i++) {
        std::cout << i + 1 << ". " << servers[i].name << "\n";
    }
    std::cout << "Select a server (1-" << numServers << "): ";
    int choice;
    std::cin >> choice;
    std::cin.ignore();
    if (choice < 1 || choice > numServers) return false;
    
    // Notify the server that the client picked to challenge
    serverAddr = servers[choice - 1].addr;
    game.opponentName = servers[choice - 1].name;
    std::string challenge = PLAYER_PREFIX + clientName;
    sendMessage(s, challenge, serverAddr);
    std::string response = receiveMessage(s, serverAddr, CHALLENGE_TIMEOUT);
    
    // If the server agrees to play we start the game
    if (response == "YES") {
        sendMessage(s, "GREAT!", serverAddr);
        // Recieve the game board from the server and ensure it is valid
        std::string board = receiveMessage(s, serverAddr);
        if (board.empty() || board[0] < '3' || board[0] > '9' || board.length() != (board[0] - '0') * 2 + 1) { 
            std::cout << "Game over: Invalid/no board received. You win.\n";
            return false;
        }
        // ????? What is does this code do from 200-207 ?????
        game.piles.clear();
        for (size_t i = 1; i < board.length(); i += 2) {
            int rocks = std::stoi(board.substr(i, 2));
            if (rocks < 1 || rocks > 20) return false;
            game.piles.push_back(rocks);
        }
        game.myTurn = true;
        return true;
    }
    
    // If the server does not accept the client's challenge we return
    std::cout << game.opponentName << " refused the challenge.\n";
    return false;
}

/*================================================================================*/
// Server Negotiation

bool serverNegotiate(SOCKET s, const std::string& serverName, GameState& game, sockaddr_in& clientAddr) {
    bindSocket(s, DEFAULT_PORT);
    while (true) { // TODO: Put a control flag here for readability
        char buf[MAX_BUFFER];
        int addrSize = sizeof(clientAddr);
        if (wait(s, TIMEOUT_SECONDS, 0) > 0) {
            int len = recvfrom(s, buf, MAX_BUFFER, 0, (sockaddr*)&clientAddr, &addrSize);
            if (len > 0) {
                buf[len] = '\0';
                // Check if the client asked the server "Who?"
                if (strcmp(buf, QUERY) == 0) {
                    std::string response = NAME_PREFIX + serverName;
                    sendMessage(s, response, clientAddr);
                } 
                else if (strncmp(buf, PLAYER_PREFIX, 7) == 0) { // Check if the client sent the challenge message "Player=client_name"
                    game.opponentName = std::string(buf + 7);
                    std::cout << "Challenged by " << game.opponentName << ". Accept? (y/n): ";
                    std::string choice;
                    std::getline(std::cin, choice);
                    // Check if the server user accepted the challenge and respond to client
                    if (choice == "y" || choice == "Y") { // !!!!!!!EDIT!!!!!!!: We can just use toupper() here since we do that for getModeChoice()
                        sendMessage(s, "YES", clientAddr);
                        std::string response = receiveMessage(s, clientAddr, GREAT_TIMEOUT);
                        if (response == "GREAT!") {
                            // Ask server user to enter the number of piles. If they enter a number outside the range 3-9 ask them again
                            int piles;
                            do {
                                std::cout << "Enter number of piles (3-9): ";
                                std::cin >> piles;
                            } while (piles < 3 || piles > 9); 
                            game.piles.resize(piles);
                            
                            // Ask server user to enter the number of rocks for each pile
                            for (int i = 0; i < piles; i++) {
                                do {
                                    std::cout << "Enter rocks for pile " << i + 1 << " (1-20): ";
                                    std::cin >> game.piles[i];
                                } while (game.piles[i] < 1 || game.piles[i] > 20);
                            }
                            
                            // Send the game board to the client
                            std::cin.ignore();
                            std::string board = std::to_string(piles);
                            for (int r : game.piles) board += (r < 10 ? "0" + std::to_string(r) : std::to_string(r));
                            sendMessage(s, board, clientAddr);
                            game.myTurn = false;
                            return true;
                        }
                    }
                    else { // The server user declines the challenge
                        sendMessage(s, "NO", clientAddr);
                    }
                }
            }
        }
    }
    return false;
}
/*================================================================================*/
// Game Logic

void displayBoard(const GameState& game) {
    std::cout << "Board:\n";
    for (size_t i = 0; i < game.piles.size(); i++) {
        std::cout << "Pile " << i + 1 << ": " << game.piles[i] << "\n";
    }
}

bool isGameOver(const GameState& game) {
    return std::all_of(game.piles.begin(), game.piles.end(), [](int r) { return r == 0; });
}

std::string getMove(const GameState& game) {
    int pile, rocks;
    // !!!!!!!!EDIT!!!!!!!!!! I implemented a control flag that replaces the while true loop. Only issue is if user enters the amount of piles right but the rocks wrong then it will ask for both piles and rocks again.
    // Could fix by implementing two seperate do while loops, but I am not sure about it.
    bool validMove = true;
    do {
        displayBoard(game);
        std::cout << "Enter Pile to Take From (1-" << game.piles.size() << "): ";
        std::cin >> pile;
        if (pile < 1 || pile >(int)game.piles.size() || game.piles[pile - 1] == 0) {
            std::cout << "Invalid pile.\n";
            //continue;
            validMove = false;
        }
        if(validMove) {
            std::cout << "Enter Number of Rocks to Take (1-" << game.piles[pile - 1] << "): ";
            std::cin >> rocks;
            if (rocks < 1 || rocks > game.piles[pile - 1]) {
                std::cout << "Invalid rocks.\n";
                validMove = false;
            }
        }
        // else break;
    } while (!validMove);
    
    std::cin.ignore();
    return std::to_string(pile) + (rocks < 10 ? "0" + std::to_string(rocks) : std::to_string(rocks));
}

void playGame(SOCKET s, GameState& game, sockaddr_in& opponentAddr) {
    while (!isGameOver(game)) {
        if (game.myTurn) {
            std::cout << "1. Move\n2. Chat\n3. Forfeit\nEnter Number: ";
            int choice;
            std::cin >> choice;
            std::cin.ignore();
            std::string msg;
            if (choice == 1) {
                msg = getMove(game);
                int pile = msg[0] - '0' - 1;
                int rocks = std::stoi(msg.substr(1));
                game.piles[pile] -= rocks;
            }
            else if (choice == 2) {
                std::cout << "Enter Message (max 80 chars): ";
                std::getline(std::cin, msg);
                msg = "C" + (msg.length() > 80 ? msg.substr(0, 80) : msg);
            }
            else if (choice == 3) {
                sendMessage(s, "F", opponentAddr);
                std::cout << "You forfeited. Game over.\n";
                return;
            }
            else continue;
            sendMessage(s, msg, opponentAddr);
            if (choice == 1) {
                if (isGameOver(game)) {
                    std::cout << "You win!\n";
                    return;
                }
                game.myTurn = false;
            }
        }
        else {
            std::string msg = receiveMessage(s, opponentAddr);
            if (msg.empty()) {
                std::cout << "Game over: No response. You win.\n";
                return;
            }
            if (msg == "F") {
                std::cout << game.opponentName << " forfeited. You win!\n";
                return;
            }
            if (msg[0] == 'C') {
                std::cout << game.opponentName << ": " << msg.substr(1) << "\n";
                continue;
            }
            if (msg.length() != 3 || msg[0] < '1' || msg[0] - '0' > (int)game.piles.size() ||
                std::stoi(msg.substr(1)) > game.piles[msg[0] - '0' - 1]) {
                std::cout << "Game over: Invalid move. You win.\n";
                return;
            }
            if (!all_of(msg.substr(1).begin(), msg.substr(1).end(), ::isdigit)) {
                std::cout <<  "Game over: Invalid move. You win.\n";
                return;
            }
            int pile = msg[0] - '0' - 1;
            int rocks = std::stoi(msg.substr(1));
            game.piles[pile] -= rocks;
            if (isGameOver(game)) {
                std::cout << game.opponentName << " wins.\n";
                return;
            }
            game.myTurn = true;
        }
    }
}

/*================================================================================*/
// Main Function

int main() {
    initializeWinsock();
    std::string userName = getUserName();

    char choice = getModeChoice();

    while (choice != 'Q') {
        SOCKET s = createUdpSocket();
        GameState game;
        sockaddr_in opponentAddr;

        bool gameStarted = false;
        if (choice == 'H') {
            gameStarted = serverNegotiate(s, userName, game, opponentAddr);
        }
        else if (choice == 'C') {
            gameStarted = clientNegotiate(s, userName, game, opponentAddr);
        }

        if (gameStarted) playGame(s, game, opponentAddr);
        closesocket(s);

        choice = getModeChoice();
    }
    WSACleanup();
    return 0;
}
