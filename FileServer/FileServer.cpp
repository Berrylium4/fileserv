#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
typedef int socklen_t; //Windows Sockets TCP/IP primitive data types
#pragma comment(lib, "ws2_32.lib")

#include <WinSock2.h> // Window version of sys/socket.h  netinet/in.h  arpa/inet.h
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "dirent.h" // navigate directory
#include <vector>
#include <fstream> // file stream
#include <direct.h> // mkdir


using namespace std;

const char localhost[10] = "127.0.0.1"; // Quick access to localhost ip
SOCKET server_socket; // socket descriptor

bool FillSockAddr_in(int portnum, struct sockaddr_in* sock, const char* ip = "", short sinFamily = AF_INET);
/*
Param ip: IPv4 dotted-decimal address
Param portnum: Port number
Param sock: A sockaddr_in passed by reference
Return: True if sock successfully filled with ip and portnum
*/

bool HandShakeServer(SOCKET client);
// Perform handshake routine with client, return true on completion

bool DisconnectProtocol(SOCKET* client); // End session

std::vector<std::string> FileNameToVector(); // helper function
// Returns vector string of all file names in current directory

bool Catalog(SOCKET client, char* buffer, size_t size); // ls, dir, catalog
// sends client list of files in current directory
// Uses passed in buffer to send list to client

bool ChangeDirectory(string newDir);

bool CheckStatus(int status_code);

std::string GetFileName(char* buffer, size_t size); // find in directory if file exist

bool SendFile(SOCKET client, char* buffer, size_t buffer_size, std::string filename); // send file to client

bool GetFileNameUpload(char* buffer, size_t size, std::string& filename);

bool RecvFile(SOCKET client, char* buffer, size_t buffer_size, std::string filename); // recv file from client

string curr_directory = "root";

bool MakeDirectory(string newDir);

int main()
{
    // Init WINSOCK ==================================================
    // REQUIRED FOR WINSOCK2
    WSADATA wsaData;
    WORD DllVersion = MAKEWORD(2, 2);
    if (WSAStartup(DllVersion, &wsaData) != 0)
    {
        printf("WINSOCK INIT FAILURE");
        ExitProcess(EXIT_FAILURE);
    }
    //================================================================
    // Initialize
    SOCKET client_socket; // socket descriptor for connected client
    struct sockaddr_in server_address; // c style struct declaration
    socklen_t serverLength = sizeof(server_address);
    unsigned short Port;
    int num_connected = 5; // Number of people connected at time for listen()
    struct sockaddr_in from_address;
    socklen_t fromLength = sizeof(from_address);
    int recv_status; // return code from recv
    int send_status; // return code from send
    char bufferIn[1000];
    char bufferOut[1000];
    size_t buffer_size = 1000;
    memset(bufferIn, 0, 1000);
    int flags = 0;
    bool session = false;
    std::string string_temp;
    bool overwrite;
    bool server_online = true;
    // Socket descriptor ============================================
    //sd = socket(AF_INET, SOCK_DGRAM, 0);
    cout << "ENTER PORT TO USE: ";
    cin >> Port;
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET)
    {
        printf("SOCKET ERROR %d\n", WSAGetLastError());
        ExitProcess(EXIT_FAILURE);
    }
    //===============================================================
    // Fill in server_address sockaddr_in
    if (!FillSockAddr_in(Port, &server_address))
    {
        cout << "Addr_in creation failure\n";
        ExitProcess(EXIT_FAILURE);
    }

    // BINDING ======================================================
    int result;
    result = bind(server_socket, (SOCKADDR*)&server_address, sizeof(server_address));
    if (result != 0) {
        printf("BIND ERROR %d\n", WSAGetLastError());
        return 1;
    }

    // Listen ========================================================
    // listen(server_socket, num_connected);
    // Handle clients
    while (server_online)
    {
        cout << "SERVER ONLINE\n";
        listen(server_socket, num_connected);
        client_socket = accept(server_socket, (SOCKADDR*)&from_address, &fromLength); // ACCEPT CONNECTION
        if (client_socket == INVALID_SOCKET) {
            printf("INVALID CONNECTION SOCKET");
            return 1;
        }
        else {
            session = HandShakeServer(client_socket);
        }
        // recvfrom() ====================================================
        while (session) // Session loop
        {
            // Receive a command
            memset(bufferIn, 0, 1000);
            memset(bufferOut, 0, 1000);
            recv_status = recv(client_socket, bufferIn, buffer_size, flags);
            if (!CheckStatus(recv_status)) {
                break;
            }

            // Process the command
            printf("RECEIVED %d BYTES: \n%s\n", recv_status, bufferIn);
            if (strcmp(bufferIn, "bye") == 0) // client wants to disconnect
            {
                DisconnectProtocol(&client_socket);
                printf("CLIENT DISCONNECTED SUCCESSFULLY\n");
                session = false;
                break;
            }
            else if (strcmp(bufferIn, "catalog") == 0 || strcmp(bufferIn, "ls") == 0 ||
                strcmp(bufferIn, "dir") == 0) {
                Catalog(client_socket, bufferOut, buffer_size);
            }
            else if (strcmp(bufferIn, "pwd") == 0 || strcmp(bufferIn, "chdir") == 0)
            {
                send_status = send(client_socket, curr_directory.c_str(), strlen(curr_directory.c_str()), 0);
                if (!CheckStatus(send_status)) {
                    session = false;
                    break;
                }
                cout << "CURRENT WORKING DIRECTORY PRINTED: " << curr_directory << endl;
            }
            else if (  // MKDIR
                bufferIn[0] == 'm' &&
                bufferIn[1] == 'k' &&
                bufferIn[2] == 'd' &&
                bufferIn[3] == 'i' &&
                bufferIn[4] == 'r' &&
                bufferIn[5] == ' '
                )
            {
                string_temp = "";
                for (int i = 6; i < strlen(bufferIn); i++)
                {
                    string_temp += bufferIn[i];
                }
                if (MakeDirectory(string_temp) == 0) // success
                {
                    string resp = "directory: " + string_temp + ": created";
                    send_status = send(client_socket, resp.c_str(), strlen(resp.c_str()), 0);
                    if (!CheckStatus(send_status)) {
                        session = false;
                        break;
                    }
                    cout << "MAKE DIRECTORY: " << string_temp << "DONE\n";
                }
                else // fail
                {
                    string resp = "mkdir: " + string_temp + ": Failed";
                    send_status = send(client_socket, resp.c_str(), strlen(resp.c_str()), 0);
                    if (!CheckStatus(send_status)) {
                        session = false;
                        break;
                    }
                    cout << "FAILED TO MAKE DIRECTORY: " << string_temp << endl;
                }
            }
            else if ( // CHANGE DIRECTORY
                bufferIn[0] == 'c' &&
                bufferIn[1] == 'd' &&
                bufferIn[2] == ' '
                )
            {
                string_temp = "";
                for (int i = 3; i < strlen(bufferIn); i++)
                {
                    string_temp += bufferIn[i];
                }

                if (string_temp == ".." || string_temp == "../") // user want to go to root
                {
                    curr_directory = "root";
                    string resp = "working directory changed to: root";
                    send_status = send(client_socket, resp.c_str(), strlen(resp.c_str()), 0);
                    if (!CheckStatus(send_status)) {
                        session = false;
                        break;
                    }
                    cout << "WORKING DIRECTORY CHANGED TO: " << curr_directory << endl;
                }
                else
                {
                    string temp2 = curr_directory + "/" + string_temp;
                    if (ChangeDirectory(temp2)) // change directory success
                    {
                        curr_directory = temp2;
                        string resp = "working directory changed to: " + temp2;
                        send_status = send(client_socket, resp.c_str() , strlen(resp.c_str()), 0);
                        if (!CheckStatus(send_status)) {
                            session = false;
                            break;
                        }
                        cout << "WORKING DIRECTORY CHANGED TO: " << curr_directory << endl;
                    }
                    else // change directory fail
                    {
                        string resp = "cd: " + string_temp + ": No such file or directory";
                        send_status = send(client_socket, resp.c_str(), strlen(resp.c_str()), 0);
                        if (!CheckStatus(send_status)) {
                            session = false;
                            break;
                        }
                        cout << "FAILED TO CHANGE DIRECTORY TO " << temp2 << endl;
                    }
                }
            }
            else if (
                bufferIn[0] == 'd' &&
                bufferIn[1] == 'o' &&
                bufferIn[2] == 'w' &&
                bufferIn[3] == 'n' &&
                bufferIn[4] == 'l' &&
                bufferIn[5] == 'o' &&
                bufferIn[6] == 'a' &&
                bufferIn[7] == 'd' &&
                bufferIn[8] == ' '
                ) {
                string_temp = GetFileName(bufferIn, strlen(bufferIn));
                if (string_temp == "") // No matching file
                {
                    send_status = send(client_socket, "File not found", 14, 0);
                    if (!CheckStatus(send_status)) {
                        session = false;
                        break;
                    }
                    printf("FILE NOT FOUND\n");
                }
                else {
                    cout << "FILE FOUND, UPLOADING: " << string_temp << std::endl;
                    if (SendFile(client_socket, bufferOut, buffer_size, string_temp)) {
                        printf("FILE TRANSFER SUCCESS\n");
                    }
                    else {
                        printf("FILE TRANSFER FAILED\n");
                    }
                }
            } // DOWNLOAD
            else if (
                bufferIn[0] == 'u' &&
                bufferIn[1] == 'p' &&
                bufferIn[2] == 'l' &&
                bufferIn[3] == 'o' &&
                bufferIn[4] == 'a' &&
                bufferIn[5] == 'd' &&
                bufferIn[6] == ' '
                ) {
                string_temp = "";
                overwrite = false;
                // get name of file and see if there's overwrite
                overwrite = GetFileNameUpload(bufferIn, strlen(bufferIn), string_temp);
                // download the file from client
                if (RecvFile(client_socket, bufferIn, buffer_size, string_temp)) {
                    printf("FILE UPLOAD DONE\n");
                    if (overwrite) {
                        std::string temp = "\"" + string_temp + "\" file has been overwritten";
                        cout << temp << endl;
                        send_status = send(client_socket, temp.c_str(), temp.size(), 0); // send warning
                        if (!CheckStatus(send_status)) {
                            session = false;
                            break;
                        }
                    }
                    else {
                        string_temp += " uploaded to server";
                        send_status = send(client_socket, string_temp.c_str(), string_temp.size(), 0); // notify
                        if (!CheckStatus(send_status)) {
                            session = false;
                            break;
                        }
                    }
                }
                else {
                    send_status = send(client_socket, "file upload failed", 18, 0); // notify client
                    if (!CheckStatus(send_status)) {
                        session = false;
                        break;
                    }
                    printf("FILE UPLOAD FAILED\n");
                }
            } // upload file


            else // Unknown command
            {
                send_status = send(client_socket, "Unknown Command", 15, 0);
                if (!CheckStatus(send_status)) {
                    session = false;
                    break;
                }
                printf("UNKNOWN COMMAND\n");
            }

        }

        curr_directory = "root"; //reset working directory to the root for next user
        cout << "SESSION WITH CLIENT ENDED\n";
        cout << "START SESSION WITH ANOTHER CLIENT? Y/N\n";
        char resp;
        cin >> resp;
        switch (resp) {
        case 'Y':
        case 'y':
            continue;
        case 'N':
        case 'n':
            server_online = false;
        default:
            server_online = false;
        }
    } //while (server_online)
    //rc = sendto(sd, buffer, strlen(buffer), 0, (SOCKADDR*) & server_address, sizeof(server_address)); UDP SEND
    //rc = send(sd,buffer, strlen(buffer),0); // TCP send

    //rc = recvfrom(server_socket, buffer, Buflen, flags, (SOCKADDR *) & from_address, &fromLength); UDP recv
    //rc = recv(connected_sd, buffer, Buflen, flags); // TCP recv
    WSACleanup();
    return 0;
}
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
bool FillSockAddr_in(int portnum, struct sockaddr_in* sock, const char* ip, short sinFamily) {
    bool result = true;
    bool any = false;
    if (ip == "")
    {
        any = true;
    }
    else if (inet_addr(ip) == INADDR_NONE)
    {
        printf("invalid ip\n");
        result = false;
    }

    if (portnum > 65535 || portnum < 0)
    {
        printf("invalid port\n");
        result = false;
    }
    if (!result) // If failure
    {
        return result;
    }
    else
    {
        unsigned short Port;
        Port = (unsigned short)portnum;
        sock->sin_family = sinFamily;
        sock->sin_port = htons(Port); // host to network short
        if (any)
        {
            sock->sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else
        {
            sock->sin_addr.s_addr = inet_addr(ip);
        }


        return result;
    }
}

bool HandShakeServer(SOCKET client)
{
    int buffer_size = 20;
    char send_buffer[20] = "connection accepted";
    char rec_buffer[20];
    memset(rec_buffer, 0, 20);

    // We already accepted their connection
    // Notify that we accepted their connection
    int send_status = send(client, send_buffer, strlen(send_buffer), 0);

    if (send_status == SOCKET_ERROR)
    {
        printf("handshake notify error %d\n", WSAGetLastError());
        return false;
    }

    // We should receive an acknowledgement back from them

    int recv_status = recv(client, rec_buffer, buffer_size, 0);
    if (recv_status == SOCKET_ERROR)
    {
        printf("handshake ack error %d\n", WSAGetLastError());
        return false;
    }
    if (strcmp(rec_buffer, "ackhandshake") != 0)
    {
        printf("handshake unknown ack\n");
        return false;
    }
    // If we made it this far, the handshake is done
    printf("CLIENT CONNECTED\n");
    return true;
    //    send_status = send(sd,buffer, strlen(buffer),0); // TCP send
    //    rc = recv(connected_sd, buffer, Buflen, flags); // TCP recv
}

bool DisconnectProtocol(SOCKET* client)
{
    char send_buffer[26] = "File copy server is down!";
    char recv_buffer[50];
    memset(recv_buffer, 0, 50);

    int send_status = send(*client, send_buffer, strlen(send_buffer), 0);
    if (send_status == SOCKET_ERROR)
    {
        printf("Disconnect goodbye error %d\n", WSAGetLastError());
    }

    int recv_status = recv(*client, recv_buffer, 50, 0);
    if (recv_status == SOCKET_ERROR)
    {
        printf("Disconnect ack error %d\n", WSAGetLastError());
    }
    printf("Received:%s", recv_buffer);
    printf("File copy server is down!\n");

    closesocket(*client);
    curr_directory = "root"; //reset working directory to the root for next user
    return true;

    // After socket closed, any interaction result in error 10038
}
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
bool ChangeDirectory(string newDir) {
    DIR* directory;
    struct dirent* file;
    if ((directory = opendir(newDir.c_str())) == NULL) {
        return false;
    }
    else
    {
        closedir(directory); // prevent memory leak
        return true;
    }
}

bool MakeDirectory(string newDir) {
    string temp = curr_directory + "/" + newDir;
    return _mkdir(temp.c_str());
}

std::vector<std::string> FileNameToVector()
{
    std::vector<std::string> arr;
    DIR* directory;
    struct dirent* file;
    directory = opendir(curr_directory.c_str());
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

bool Catalog(SOCKET client, char* buffer, size_t size)
{
    // Get name of all files in directory
    std::vector<std::string> arr = FileNameToVector();
    if (arr.size() == 0) // blank directory
    {
        arr.push_back(" ");
    }

    // Adding all the filenames to buffer
    memset(buffer, 0, size);
    int longest = arr[0].size();
    std::string temp = "";
    for (size_t i = 0; i < arr.size(); i++)
    {
        temp += arr[i];
        if (i != arr.size() - 1) // dont add end line to the last one
        {
            temp += "\n";
        }
    }
    for (size_t i = 0; i < temp.size(); i++)
    {
        buffer[i] = temp[i];
    }
    int send_status = send(client, buffer, strlen(buffer), 0);
    if (send_status == SOCKET_ERROR)
    {
        printf("SEND ERROR %d\n", WSAGetLastError());
        return false;
    }
    return true;
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
            printf("CLIENT DISCONNECTED\n");
            return false;
        }
        printf("STATUS ERROR %d\n", error); // Print if some other error
        return false;
    }
    else
    {
        return true;
    }
}

std::string GetFileName(char* buffer, size_t size)
{
    std::string filename = "";
    std::vector<std::string> filearr;
    bool match = false;
    for (size_t i = 9; i < size; i++)
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

bool SendFile(SOCKET client, char* buffer, size_t buffer_size, std::string filename)
{
    std::ifstream File(filename.c_str(), std::ios::binary);
    File.seekg(0, File.end); // go to end of file
    int size = File.tellg(); // get size of file
    File.seekg(0, File.beg); // go back to beginning of file
    bool done = false;
    int send_status = send(client, "filesend", 8, 0); // Let client know file transfer process is starting
    memset(buffer, 0, buffer_size); // Reset buffer
    if (!CheckStatus(send_status))
    {
        File.close();
        return false;
    }

    // Need to wait for client to say it's ready
    int recv_status = recv(client, buffer, buffer_size, 0);
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
    // SEND FILE NAME
    send_status = send(client, filename.c_str(), filename.size(), 0); // Send name of file
    if (!CheckStatus(send_status))
    {
        File.close();
        return false;
    }

    // WAITING FOR ACK
    memset(buffer, 0, buffer_size); // Reset buffer
    recv_status = recv(client, buffer, buffer_size, 0);
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

    // START OF FILE SEND
    while (!done)
    {
        if (File.peek() == EOF) // Check if at end of file
        {
            done = true;
            break;
        }
        send_status = send(client, "sending", 7, 0); // Let client know next send is part of file
        memset(buffer, 0, buffer_size); // Reset buffer
        if (!CheckStatus(send_status))
        {
            File.close();
            return false;
        }

        // WAIT FOR ACK
        recv_status = recv(client, buffer, buffer_size, 0);
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
        send_status = send(client, buffer, buffer_size, 0); // Send to client
        memset(buffer, 0, buffer_size); // Reset buffer


        // WAIT FOR ACK
        recv_status = recv(client, buffer, buffer_size, 0);
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
    send_status = send(client, "end", 3, 0); // Let client know all of file is sent

    File.close();
    return true;
}

bool GetFileNameUpload(char* buffer, size_t size, std::string& filename)
{
    filename = "";
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
    return match;
}

bool RecvFile(SOCKET client, char* buffer, size_t buffer_size, std::string filename) // recv file from client
{
    // Initialization
    string fileloc = curr_directory + "/" + filename;
    ofstream OutFile(fileloc.c_str(), std::ios::binary);
    memset(buffer, 0, buffer_size);
    bool done = false;
    int send_status;
    int recv_status;

    // Begin of transfer
    send_status = send(client, "ready", 5, 0); // tells client we are ready for file upload
    if (!CheckStatus(send_status))
    {
        return false;
    }

    while (true)
    {
        memset(buffer, 0, buffer_size);
        recv_status = recv(client, buffer, buffer_size, 0); // wait for client to send
        if (!CheckStatus(recv_status))
        {
            OutFile.close();
            return false;
        }

        // THREE possibilities
        // 1. "sending"
        // 2. file data
        // 3. "end"
        if (strcmp(buffer, "sending") == 0) // client about to send
        {
            memset(buffer, 0, buffer_size);
            send_status = send(client, "ack", 3, 0); // ready to receive file data
            if (!CheckStatus(send_status))
            {
                OutFile.close();
                return false;
            }
            recv_status = recv(client, buffer, buffer_size, 0); // get file data
            if (!CheckStatus(recv_status))
            {
                OutFile.close();
                return false;
            }
            OutFile.write(buffer, buffer_size); // write to file
        }
        else if (strcmp(buffer, "end") == 0) // client indicate end of file
        {
            memset(buffer, 0, buffer_size);
            OutFile.close(); // close ofstream
            return true;
        }
        else // error
        {
            memset(buffer, 0, buffer_size);
            OutFile.close(); // close ofstream
            return false;
        }

        send_status = send(client, "ack", 3, 0); // let server know we're ready for next transfer
        if (!CheckStatus(send_status))
        {
            OutFile.close();
            return false;
        }
    }
}