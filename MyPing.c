#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define BILLION 1000000000L
#define DEFAULT_PORT 100
#define DEFAULT_DATA_LENGTH 56

unsigned short addOneComplement16Bit(unsigned short x, unsigned short y){
    unsigned int sum;
    sum = x + y;
    sum = (sum & 0xFFFF) + (sum >> 16);
    return (sum & 0xFFFF);
}

unsigned short calculateCheckSum(void* headerAddress, int length)
{   
    unsigned short checkSum = 0;
    unsigned short* pointer = (unsigned short*) headerAddress;
    for (int i = 0; i < length/2; i++)
    {
        checkSum = addOneComplement16Bit(checkSum, *pointer);
        pointer++;
    }
    if (1 == length%2)
    {
        // process remain odd byte 
        unsigned char* tempPointer = (unsigned char*) pointer;
        unsigned short tempValue = 0x0;
        tempValue += (*tempPointer) << 8;
        checkSum = addOneComplement16Bit(checkSum, tempValue);
    }
    return ~checkSum;
}

struct icmphdr* createICMPMessage(int sequenceNumber, int dataLength)
{
    int messageLength = dataLength + sizeof(struct icmphdr); // in byte
    struct icmphdr* icmpMessage = (struct icmphdr*) calloc(messageLength,1);
    memset(icmpMessage, 0xff, messageLength);
    // We are sending a ICMP_ECHO ICMP packet
    icmpMessage->type = ICMP_ECHO;
    icmpMessage->code = 0;
    icmpMessage->checksum = 0;
    icmpMessage->un.echo.sequence = htons(sequenceNumber);
    // We don't need set the identifier becasue IPPROTO_ICMP socket will do it
    // compute ICMP checksum here
    icmpMessage->checksum = calculateCheckSum(icmpMessage, messageLength);
    return icmpMessage;
}

int main(int argc, char **argv)
{
    // Init socket
    struct timeval begin, end;
    double timeSpent;
    int pingSocket, addressLength;
    struct sockaddr_in sourceAddress;
    struct sockaddr_in destinationAddress;
    memset(&sourceAddress, 0, sizeof(struct sockaddr_in));
    memset(&destinationAddress, 0, sizeof(struct sockaddr_in));
    sourceAddress.sin_family = AF_INET;
    destinationAddress.sin_family = AF_INET;
    destinationAddress.sin_port = htons(DEFAULT_PORT);
    // take arg[1] as Ipv4 address
    if (0 ==inet_aton(argv[1], &destinationAddress.sin_addr))
    {
        printf("Wrong IPv4 address, please put IPv4 as argument 1\n");
        exit(1);
    }
    pingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (-1 == pingSocket)
    {
        printf("Cannot create socket\n");
        exit(1);
    }
    if ( -1 == getsockname(pingSocket, (struct sockaddr*)&sourceAddress, &addressLength))
    {
        printf("Cannot getsockname\n");
        exit(1);
    }


    // Init some buffer and variable for send mesasge and revice
    int dataLength = DEFAULT_DATA_LENGTH;
    int messageLength = dataLength + sizeof(struct icmphdr);
    int sequenceNumber = 1;
    struct icmphdr* imcpMessage = createICMPMessage(sequenceNumber, dataLength);

    struct msghdr meassageHeader;
    int polling;
    char addressBuffer[128];
    struct iovec iov;
    unsigned char* repliedMessage = (unsigned char*)malloc(messageLength);
    struct icmphdr *icmpRepliedMessage;

    iov.iov_base = (char *) repliedMessage;
    iov.iov_len = messageLength;

    memset(&meassageHeader, 0, sizeof(meassageHeader));
    meassageHeader.msg_name = addressBuffer;
    meassageHeader.msg_namelen = sizeof(addressBuffer);
    meassageHeader.msg_iov = &iov;
    meassageHeader.msg_iovlen = 1;
    icmpRepliedMessage = meassageHeader.msg_iov->iov_base;
    polling = MSG_WAITALL;

    int i;
    gettimeofday(&begin, NULL);
    // Send ECHO Message
    i = sendto(pingSocket, imcpMessage, (messageLength), 0, (struct sockaddr*)&destinationAddress, sizeof(destinationAddress));
    printf("Sent ICMP_ECHO Message\n");
    // Wait to receive message
    i = recvmsg(pingSocket, &meassageHeader, polling);
    gettimeofday(&end, NULL);

    // Check results
    if ( 0 > i)
    {
        printf("Error in recvfrom\n");
        exit(1);
    };
    if (! 0 == calculateCheckSum(icmpRepliedMessage, dataLength))
    {
        printf("Checksum status : BAD\n");
        exit(1);
    }

    if (icmpRepliedMessage->type == ICMP_ECHOREPLY) 
    {
        timeSpent =   (double)(end.tv_usec - begin.tv_usec) / 1000 
                    + (double)(end.tv_sec - begin.tv_sec) * 1000;  // uint in ms
        printf("Reiveied ICMP_ECHOREPLY sequence number = %d\n", ntohs(icmpRepliedMessage->un.echo.sequence));
        printf("Round trip time = %lf ms \n", timeSpent);
    } 
    else 
    {
        printf("Not received message ICMP_ECHOREPLY\n");
    }
    
    // Release dynamic memory
    free(repliedMessage);
    free(imcpMessage);
    return 0;
}