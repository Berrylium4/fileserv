#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

/* Windows Sockets TCP/IP primitive data types */
typedef int socklen_t;

#pragma comment(lib, "ws2_32.lib")

#include <WinSock2.h> // Window version of sys/socket.h  netinet/in.h  arpa/inet.h
#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "dirent.h" // navigate directory
#include <vector>
#include <fstream> // file stream

using namespace std;

SOCKET client_socket;

bool HandShakeClient(); // start of session

bool DisconnectProtocol(); // end of session

void GetCommand(char* string, size_t size);

bool CheckStatus(int status_code);

bool DownloadFile(char* buffer, size_t size);

std::vector<std::string> FileNameToVector();

std::string GetFileName(char* buffer, size_t size);

bool SendFile(char* buffer, size_t buffer_size, std::string filename);


int main(int argc, char* argv[])
{
    // Init WINSOCK ==================================================
    // REQUIRED FOR WINSOCK2
    WSADATA wsaData;
    WORD DllVersion = MAKEWORD(2, 2);
    if (WSAStartup(DllVersion, &wsaData) != 0)
    {
        printf("WINSOCK initialization failed ending program");
        ExitProcess(EXIT_FAILURE);
    }
    //================================================================

    struct sockaddr_in server_address;
    char serverIP[29];
    int portNumber;

    const size_t buffer_size = 1000;
    char send_buffer[buffer_size];
    char recv_buffer[buffer_size];
    memset(send_buffer, 0, buffer_size);
    memset(recv_buffer, 0, buffer_size);

    int send_status = 0;
    int recv_status = 0;
    bool session = false;
    std::string string_temp;

    if (argc < 3) {
        printf("usage is client <ipaddr> <port>\n");
        ExitProcess(EXIT_SUCCESS);
    }

    //sd = socket(AF_INET, SOCK_DGRAM, 0);
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    portNumber = strtol(argv[2], NULL, 10);
    strcpy(serverIP, argv[1]);
    // ./client 127.0.0.1 24000

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portNumber);
    server_address.sin_addr.s_addr = inet_addr(serverIP);

    if (connect(client_socket, (SOCKADDR*)&server_address, sizeof(server_address)) < 0)
    {
        // BUSY
        closesocket(client_socket);
        printf("server is busy/offline, try again later.");
    }
    else
    {
        session = HandShakeClient();
    }

    //rc = sendto(sd, buffer, strlen(buffer), 0, (SOCKADDR*) & server_address, sizeof(server_address)); UDP SEND

    while (session)
    {
        // User input command
        memset(send_buffer, 0, buffer_size);
        memset(recv_buffer, 0, buffer_size);
        GetCommand(send_buffer, buffer_size);

        // Sends commands to server
        if (strcmp(send_buffer, "bye") == 0)
        {
            // Needs to call DisconnectProtocol() after sending bye
            send_status = send(client_socket, "bye", 3, 0);
            DisconnectProtocol();
            printf("Disconnected from server successsfully\n");
            session = false;
            break;
        }
        else if (
            send_buffer[0] == 'u' &&
            send_buffer[1] == 'p' &&
            send_buffer[2] == 'l' &&
            send_buffer[3] == 'o' &&
            send_buffer[4] == 'a' &&
            send_buffer[5] == 'd' &&
            send_buffer[6] == ' ')
        {
            string_temp = "";
            string_temp = GetFileName(send_buffer, strlen(send_buffer));
            if (string_temp == "") // file not found
            {
                printf("file not found in own directory\n");
            }
            else
            {
                send_status = send(client_socket, send_buffer, strlen(send_buffer), 0); // let server know of file upload
                if (!CheckStatus(send_status))
                {
                    session = false;
                    break;
                }
                memset(send_buffer, 0, buffer_size);
                if (SendFile(send_buffer, buffer_size, string_temp)) // successfully sending
                {
                    recv_status = recv(client_socket, recv_buffer, buffer_size, 0); // wait for status from server
                    if (!CheckStatus(recv_status))
                    {
                        session = false;
                        break;
                    }
                    printf("%s\n", recv_buffer);
                }
                else
                {
                    printf("file upload to server failed\n");
                }
                // send the file
            }
            continue;
        }

        send_status = send(client_socket, send_buffer, strlen(send_buffer), 0); // TCP send
        if (!CheckStatus(send_status))
        {
            session = false;
            break;
        }
        // printf("sent %d bytes\n", send_status); // debugging use

        // Wait for response
        recv_status = recv(client_socket, recv_buffer, buffer_size, 0);
        if (strcmp(recv_buffer, "filesend") == 0) // Server indicate start of file transfer
        {
            if (DownloadFile(recv_buffer, buffer_size))
            {
                printf("file downloaded\n");
            }
            else
            {
                printf("download failed\n");
            }
            continue;
        }

        // print response from server
        printf("%s\n", recv_buffer);
    }
    WSACleanup();
    ExitProcess(EXIT_SUCCESS);
}

bool HandShakeClient()
{
    int buffer_size = 20;
    char send_buffer[20] = "ackhandshake";
    char rec_buffer[20];
    memset(rec_buffer, 0, 20);

    // Wait for server to notify
    int recv_status = recv(client_socket, rec_buffer, buffer_size, 0);
    if (recv_status == SOCKET_ERROR)
    {
        printf("handshake notify error %d\n", WSAGetLastError());
        return false;
    }

    if (strcmp(rec_buffer, "connection accepted") != 0)
    {
        printf("handshake unknown notify\n");
        return false;
    }

    // We have to ack the notification
    int send_status = send(client_socket, send_buffer, strlen(send_buffer), 0);
    if (send_status == SOCKET_ERROR)
    {
        printf("handshake ack error %d\n", WSAGetLastError());
        return false;
    }

    // After sending the ack, we completed our side of the handshake
    printf("connection complete\n");
    return true;
}

bool DisconnectProtocol()
{
    char send_buffer[31] = "Internet copy client is down!\n";
    char recv_buffer[50];
    memset(recv_buffer, 0, 50);
    bool error = false;

    int recv_status = recv(client_socket, recv_buffer, 50, 0);
    if (recv_status == SOCKET_ERROR)
    {
        printf("Disconnect notify error %d\n", WSAGetLastError());
        error = true;
    }

    int send_status = send(client_socket, send_buffer, strlen(send_buffer), 0);
    if (send_status == SOCKET_ERROR)
    {
        printf("Disconnect goodbye error %d\n", WSAGetLastError());
        error = true;
    }

    printf("Internet copy client is down!\n");
    printf("Received:%s\n", recv_buffer);
    closesocket(client_socket);
    return (!error);
}

void GetCommand(char* string, size_t size) {
    std::string temp;
    bool done = false;
    while (!done)
    {
        cout << "->";
        getline(cin, temp);
        if (temp.size() > size)
        {
            printf("command is too long");
        }
        done = true;
    }
    strcpy(string, temp.c_str());
}

bool CheckStatus(int status_code)
{
    if (status_code == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error == 10054 || error == 10038)
        {
            // WSAECONNRESET
            // Connection reset by peer. (Client dced)
            printf("disconnected from server\n");
            return false;
        }
        printf("status error %d\n", error); // Print if some other error
        return false;
    }
    else
    {
        return true;
    }
}

bool DownloadFile(char* buffer, size_t size)
{
    ofstream OutFile;
    string filename = "";
    memset(buffer, 0, size);
    bool done = false;

    int send_status = send(client_socket, "ready", 5, 0); // Let server know we're ready for file
    if (!CheckStatus(send_status))
    {
        return false;
    }

    // EXPECTING name of file
    int recv_status = recv(client_socket, buffer, size, 0);
    if (!CheckStatus(recv_status))
    {
        return false;
    }

    // Putting name of file into string
    for (size_t i = 0; i < strlen(buffer); i++)
    {
        filename += buffer[i];
    }

    OutFile.open(filename.c_str(), std::ios::binary); // open file

    send_status = send(client_socket, "ack", 3, 0); // Let server know we're ready for next msg
    if (!CheckStatus(send_status))
    {
        OutFile.close();
        return false;
    }

    // Receiving file itself
    while (true) // loop terminates when all of file received or an error occured
    {
        memset(buffer, 0, size);
        recv_status = recv(client_socket, buffer, size, 0);
        if (!CheckStatus(recv_status))
        {
            OutFile.close();
            return false;
        }

        // THREE POSSIBLE THINGS FROM SERVER
        // 1. "sending"
        // 2. file data
        // 3. "end"
        if (strcmp(buffer, "sending") == 0) // server about to send
        {
            memset(buffer, 0, size);
            send_status = send(client_socket, "ack", 3, 0); // Let server know we're ready for next msg
            if (!CheckStatus(send_status))
            {
                OutFile.close();
                return false;
            }

            recv_status = recv(client_socket, buffer, size, 0); // get data from server
            if (!CheckStatus(recv_status))
            {
                OutFile.close();
                return false;
            }
            OutFile.write(buffer, size); // Write to file
        }
        else if (strcmp(buffer, "end") == 0) // server indicate end of file
        {
            memset(buffer, 0, size);
            OutFile.close(); // close ofstream
            return true;
        }
        else // Some error
        {
            memset(buffer, 0, size); // clear buffer
            OutFile.close(); // close ofstream
            return false;
        }

        send_status = send(client_socket, "ack", 3, 0); // Let server know we're ready for next msg
        if (!CheckStatus(send_status))
        {
            OutFile.close();
            return false;
        }
    } // while loop
}

std::vector<std::string> FileNameToVector()
{
    std::vector<std::string> arr;
    DIR* directory;
    struct dirent* file;
    directory = opendir(".");
    while ((file = readdir(directory)) != NULL)
    {
        std::string temp = "";
        for (size_t i = 0; i < strlen(file->d_name); i++)
        {
            temp += file->d_name[i];
        }
        if (temp == "." || temp == "..")
        {
            continue;
        }
        arr.push_back(temp);
    }
    closedir(directory); // prevent memory leak
    return arr;
}

std::string GetFileName(char* buffer, size_t size)
{
    std::string filename = "";
    std::vector<std::string> filearr;
    bool match = false;
    for (size_t i = 7; i < size; i++)
    {
        filename += buffer[i];
    }
    filearr = FileNameToVector(); // Get vector of all files

    for (size_t i = 0; i < filearr.size(); i++)
    {
        if (filearr[i] == filename)
        {
            match = true;
        }
    }
    return match ? filename : "";
}

bool SendFile(char* buffer, size_t buffer_size, std::string filename)
{
    std::ifstream File(filename.c_str(), std::ios::binary);
    File.seekg(0, File.end); // go to end of file
    int size = File.tellg(); // get size of file
    File.seekg(0, File.beg); // go back to beginning of file
    bool done = false;
    int send_status;
    int recv_status;
    memset(buffer, 0, buffer_size); // Reset buffer

    // Need to wait for server to say it's ready
    recv_status = recv(client_socket, buffer, buffer_size, 0);
    if (!CheckStatus(recv_status))
    {
        File.close();
        return false;
    }

    if (strcmp(buffer, "ready") != 0) // something other than ready
    {
        File.close();
        return false;
    }

    // START OF FILE TRANSFER

    while (!done)
    {
        if (File.peek() == EOF) // Check if at end of file
        {
            done = true;
            break;
        }
        send_status = send(client_socket, "sending", 7, 0); // Let client know next send is part of file
        memset(buffer, 0, buffer_size); // Reset buffer
        if (!CheckStatus(send_status))
        {
            File.close();
            return false;
        }

        // WAIT FOR ACK
        recv_status = recv(client_socket, buffer, buffer_size, 0);
        if (!CheckStatus(recv_status))
        {
            File.close();
            return false;
        }

        if (strcmp(buffer, "ack") != 0) // something other than ack
        {
            File.close();
            return false;
        }

        // NOW SEND FILE ITSELF
        memset(buffer, 0, buffer_size); // Reset buffer
        File.read(buffer, buffer_size); // Read 1000 bytes into the buffer
        send_status = send(client_socket, buffer, buffer_size, 0); // Send to client
        memset(buffer, 0, buffer_size); // Reset buffer


        // WAIT FOR ACK
        recv_status = recv(client_socket, buffer, buffer_size, 0);
        if (!CheckStatus(recv_status))
        {
            File.close();
            return false;
        }

        if (strcmp(buffer, "ack") != 0) // something other than ack
        {
            File.close();
            return false;
        }
    }
    // by now entire file is sent
    send_status = send(client_socket, "end", 3, 0); // Let client know all of file is sent

    File.close();
    return true;
}
