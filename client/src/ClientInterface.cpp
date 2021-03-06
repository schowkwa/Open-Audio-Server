/**
 * @file    ClientInterface.cpp
 * @author  Shreenidhi Chowkwale
 */

#include "ClientInterface.h"

using namespace oasclient;

// statics
int ClientInterface::_socketFD = -1;

bool ClientInterface::initialize(const std::string &host, unsigned short port)
{
    if (isInitialized())
        return true;

    struct sockaddr_in stSockAddr;
    int socketFD;

    // Set the internal socketFD to -1
    _socketFD = -1;

    // Set the address info struct to 0
    memset(&stSockAddr, 0, sizeof(stSockAddr));

    // Set the values for the address struct
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(port);

    // Convert address from text to binary
    if (0 >= inet_pton(AF_INET, host.c_str(), &stSockAddr.sin_addr))
    {
        return false;
    }

    // Create the socket
    socketFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (-1 == socketFD)
    {
        return false;
    }

    int one = 1;

    setsockopt(socketFD, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));


    // Connect to the established socket
    if (-1 == connect(socketFD, (struct sockaddr *) &stSockAddr, sizeof(stSockAddr)))
    {
        close(socketFD);
        return false;
    }

    ClientInterface::_socketFD = socketFD;
    return true;
}

bool ClientInterface::isInitialized()
{
    // Initialized only if socketFD
    return (-1 != _socketFD);
}

bool ClientInterface::shutdown()
{
    if ((-1 == ClientInterface::_socketFD)
        || (ClientInterface::writeToServer("QUIT")
            && (0 == close(ClientInterface::_socketFD))))
    {
        ClientInterface::_socketFD = -1;
        return true;
    }
    else
    {
        return false;
    }
}

bool ClientInterface::writeToServer(const char *format, ...)
{
    if (!format || !isInitialized())
    {
        return false;
    }

    // Create a buffer that should be more than long enough to fit data
    char buf[PACKET_SIZE * 2] = {0};
    int bufSizeAttempt, length;
    va_list args;

    // Read var args, put the formatted string into buf
    va_start(args, format);
    bufSizeAttempt = vsnprintf(buf, PACKET_SIZE, format, args);
    va_end(args);

    // If the formatted string exceeds the message packet size, then we cannot send
    if (PACKET_SIZE <= bufSizeAttempt)
    {
        return false;
    }

    if (-1 == write(ClientInterface::_socketFD, buf, bufSizeAttempt + 1))
    {
        return false;
    }

    return true;
}

bool ClientInterface::readFromServer(char *&data, size_t &count)
{
    char buf[PACKET_SIZE] = {0};
    int retval;

    data = NULL;
    count = 0;

    if (-1 == ClientInterface::_socketFD)
    {
        return false;
    }

    retval = read(ClientInterface::_socketFD,
                  buf,
                  ClientInterface::PACKET_SIZE);

    if (-1 == retval || 0 == retval)
    {
        return false;
    }

    count = retval;

    char *newData = new char[count];
    memcpy(newData, buf, count);
    
    data = newData;
    return true;
}

bool ClientInterface::readIntegerFromServer(int &value)
{
    char *data;
    size_t bytesRead;
    bool success;

    value = -1;
    if (!readFromServer(data, bytesRead))
        return false;

    if (!data)
        return false;

    std::string tempString = data;
    delete[] data;

    std::istringstream converter(tempString);
    int result = -1;

    if (!(converter >> result))
        return false;

    value = result;
    return true;
}

bool ClientInterface::sendFile(const std::string &sPath, const std::string &sFilename)
{
    int fileSize;
    struct stat fileInfo;
    char *data;
    const char *filePath;

    // If the connection to the server is not established, fail early
    if (-1 == ClientInterface::_socketFD)
    {
        return false;
    }

    // Append the path and filename together, and assign the filePath pointer to the appended string
    std::string sFilePath = sPath + "/" + sFilename;
    filePath = sFilePath.c_str();

    // Retrieve file information. Note: stat() returns 0 on success
    if (0 != stat(filePath, &fileInfo))
    {
        return false;
    }

    fileSize = fileInfo.st_size;

    // Send the PTFI message to the server
    char buf[PACKET_SIZE + 1] = {0};
    int sizeAttempt = snprintf(buf, PACKET_SIZE, "PTFI %s %d", sFilename.c_str(), fileSize);

    // If the amount written to buffer is greater than what we can send, return false
    if (PACKET_SIZE <= sizeAttempt)
    {
        return false;
    }

    if (!ClientInterface::writeToServer(buf))
    {
        return false;
    }

    // Read file from disk
    data = new char[fileSize];
    std::ifstream fileIn(filePath, std::ios::in | std::ios::binary);
    fileIn.read(data, fileInfo.st_size);
    
    // Send the file over the socket
    int bytesLeft, bytesWritten;
    char *dataPtr;
    
    bytesLeft = fileSize;
    dataPtr = data;

    // While we still have bytes/data to write...
    while (bytesLeft > 0)
    {
        // Write a chunk of data out to the socket
        bytesWritten = write(ClientInterface::_socketFD, dataPtr, bytesLeft);

        // If an error occurred, return failure
        if (bytesWritten == 0 || bytesWritten == -1)
        {
            return false;
        }

        // Move the data pointer up by the number of bytes written
        dataPtr += bytesWritten;

        // Reduce the number of bytes remaining by the number of bytes written
        bytesLeft -= bytesWritten;
    }
   
    delete[] data;

    return true;
}

