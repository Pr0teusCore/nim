#pragma comment(lib, "Ws2_32.lib")
const int DEFAULT_BUFLEN = 512;

#include <iostream>
#include <string>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include "StudyBuddy.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

void hostStudyGroup();
void joinStudyGroup();
string getUserInput(const string& prompt);
void sendMessage(SOCKET s, const char* message, sockaddr_in dest);
string receiveMessage(SOCKET s, sockaddr_in& sender, int timeoutSeconds = 2);

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed." << endl;
        return 1;
    }

    char choice;
    do {
        cout << "Would you like to (H)ost or (J)oin a study group? (Q to quit): ";
        cin >> choice;
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clear input buffer

        switch (toupper(choice)) {
        case 'H':
            hostStudyGroup();
            break;
        case 'J':
            joinStudyGroup();
            break;
        case 'Q':
            cout << "Quitting program." << endl;
            break;
        default:
            cout << "Invalid choice. Please enter H, J, or Q." << endl;
        }
    } while (toupper(choice) != 'Q');

    WSACleanup();
    return 0;
}

void hostStudyGroup() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        cout << "Socket creation failed." << endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(s, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Bind failed." << endl;
        closesocket(s);
        return;
    }

    string groupName = getUserInput("Enter the name of the study group: ");
    string hostName = getUserInput("Enter your name (optional, press Enter to skip): ");
    string location = getUserInput("Enter the location of the study group: ");
    string courses = getUserInput("Enter the courses (PREFIX XXXX, newline separated): ");
    string members = hostName.empty() ? "" : hostName + "\n";

    cout << "Hosting study group: " << groupName << " at " << location << " for " << courses << endl;
    cout << "Waiting for queries..." << endl;

    char recvBuf[DEFAULT_BUFLEN];
    while (true) {
        sockaddr_in sender{};
        int senderLen = sizeof(sender);
        int len = recvfrom(s, recvBuf, DEFAULT_BUFLEN, 0, (sockaddr*)&sender, &senderLen);
        if (len > 0) {
            recvBuf[len] = '\0';
            cout << "Received: " << recvBuf << endl;

            if (strcmp(recvBuf, Study_QUERY) == 0) {
                string response = string(Study_NAME) + groupName;
                sendMessage(s, response.c_str(), sender);
                cout << "Sent: " << response << endl;
            }
            else if (strcmp(recvBuf, Study_WHERE) == 0) {
                string response = string(Study_LOC) + location;
                sendMessage(s, response.c_str(), sender);
                cout << "Sent: " << response << endl;
            }
            else if (strcmp(recvBuf, Study_WHAT) == 0) {
                string response = string(Study_COURSES) + courses;
                sendMessage(s, response.c_str(), sender);
                cout << "Sent: " << response << endl;
            }
            else if (strcmp(recvBuf, Study_MEMBERS) == 0) {
                string response = string(Study_MEMLIST) + members;
                sendMessage(s, response.c_str(), sender);
                cout << "Sent: " << response << endl;
            }
            else if (strncmp(recvBuf, Study_JOIN, strlen(Study_JOIN)) == 0) {
                string newMember = string(recvBuf).substr(strlen(Study_JOIN));
                if (!newMember.empty()) {
                    members += newMember + "\n";
                    sendMessage(s, Study_CONFIRM, sender);
                    cout << "Sent: " << Study_CONFIRM << " and added " << newMember << " to members." << endl;
                }
            }
        }
    }

    closesocket(s);
}

void joinStudyGroup() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        cout << "Socket creation failed." << endl;
        return;
    }

    BOOL bOptVal = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, sizeof(BOOL));

    string clientName = getUserInput("Enter your name: ");
    ServerStruct servers[MAX_SERVERS];
    int numServers = getServers(s, servers);

    if (numServers == 0) {
        cout << "No study groups found." << endl;
        closesocket(s);
        return;
    }

    cout << "Available study groups:" << endl;
    for (int i = 0; i < numServers; i++) {
        cout << i + 1 << ". " << servers[i].name << endl;
    }

    char choice;
    do {
        cout << "Choose an action:\n1. Ask location\n2. Ask courses\n3. Ask members\n4. Join group\n5. Exit\nChoice: ";
        cin >> choice;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (choice >= '1' && choice <= '5') {
            int serverIndex;
            if (choice != '5') {
                cout << "Select a study group (1-" << numServers << "): ";
                cin >> serverIndex;
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                serverIndex--; // Convert to 0-based
                if (serverIndex < 0 || serverIndex >= numServers) {
                    cout << "Invalid selection." << endl;
                    continue;
                }
            }

            sockaddr_in serverAddr = servers[serverIndex].addr;
            string response;

            switch (choice) {
            case '1': // Ask location
                sendMessage(s, Study_WHERE, serverAddr);
                response = receiveMessage(s, serverAddr);
                if (!response.empty() && strncmp(response.c_str(), Study_LOC, strlen(Study_LOC)) == 0) {
                    cout << "Location: " << response.substr(strlen(Study_LOC)) << endl;
                }
                break;
            case '2': // Ask courses
                sendMessage(s, Study_WHAT, serverAddr);
                response = receiveMessage(s, serverAddr);
                if (!response.empty() && strncmp(response.c_str(), Study_COURSES, strlen(Study_COURSES)) == 0) {
                    cout << "Courses: " << response.substr(strlen(Study_COURSES)) << endl;
                }
                break;
            case '3': // Ask members
                sendMessage(s, Study_MEMBERS, serverAddr);
                response = receiveMessage(s, serverAddr);
                if (!response.empty() && strncmp(response.c_str(), Study_MEMLIST, strlen(Study_MEMLIST)) == 0) {
                    cout << "Members: " << response.substr(strlen(Study_MEMLIST)) << endl;
                }
                break;
            case '4': // Join group
                string joinMsg = string(Study_JOIN) + clientName;
                sendMessage(s, joinMsg.c_str(), serverAddr);
                response = receiveMessage(s, serverAddr);
                if (response == Study_CONFIRM) {
                    cout << "Successfully joined the study group!" << endl;
                    closesocket(s);
                    return; // Exit after joining
                }
                break;
            case '5': // Exit
                break;
            }
        }
        else {
            cout << "Invalid choice. Please enter 1-5." << endl;
        }
    } while (choice != '5');

    closesocket(s);
}

string getUserInput(const string& prompt) {
    string input;
    cout << prompt;
    getline(cin, input);
    return input;
}

void sendMessage(SOCKET s, const char* message, sockaddr_in dest) {
    sendto(s, message, strlen(message) + 1, 0, (sockaddr*)&dest, sizeof(dest));
    cout << "Sent: " << message << endl;
}

string receiveMessage(SOCKET s, sockaddr_in& sender, int timeoutSeconds) {
    char buf[DEFAULT_BUFLEN];
    int senderLen = sizeof(sender);
    int len = recvfrom(s, buf, DEFAULT_BUFLEN, 0, (sockaddr*)&sender, &senderLen);
    if (len > 0) {
        buf[len] = '\0';
        cout << "Received: " << buf << endl;
        return string(buf);
    }
    return "";
}