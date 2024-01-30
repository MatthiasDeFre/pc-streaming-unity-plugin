// PCStreamingPlugin.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "PCStreamingPlugin.h"
#include "packet_data_defs.hpp"
#include "FrameBuffer.hpp"
#include "DataParser.hpp"
#define SERVER "172.22.107.250"	//ip address of udp server
#define BUFLEN 1300	//Max length of buffer
#define PORT 8000	//The port on which to listen for incoming data

static int sum = 0;
static struct sockaddr_in si_other;
static int s, slen = sizeof(si_other);
static char* buf = (char*)malloc(BUFLEN);
static char* buf_ori = buf;
static WSADATA wsa;
static std::thread wrk;
static std::map<uint32_t, ReceivedFrame> recv_frames;
static FrameBuffer frame_buffer;
static DataParser data_parser;
static int p_c = 0;
static bool keep_working = true;
static std::string server = "172.22.107.250";
enum CONNECTION_SETUP_CODE : int
{
    ConnectionSuccess = 0,
    StartUpError = 1,
    SocketCreationError = 2,
    SendToError = 3
};

int setup_connection(char* server_str, uint32_t port) {
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        return StartUpError;
    }

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
    {
        WSACleanup();
        return SocketCreationError;
    }
    ULONG buf_size = 524288000;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&buf_size, sizeof(ULONG));

    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(port);
    inet_pton(AF_INET, server_str, &si_other.sin_addr.S_un.S_addr);

    char t[BUFLEN] = { 0 };
    t[0] = 'a';
    if (sendto(s, t, BUFLEN, 0, (struct sockaddr*)&si_other, slen) == SOCKET_ERROR)
    {
        WSACleanup();
        return SendToError;
    }

    return ConnectionSuccess;
}

void listen_work() {
    // TODO: add poll for performance, maybe
    while (keep_working) {
        size_t size = 0;    

        if ((size = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr*)&si_other, &slen)) == SOCKET_ERROR)
        {

            return;
           // printf("recvfrom() failed with error code : %d", WSAGetLastError());
           // exit(EXIT_FAILURE);
        }

        struct PacketType p_type(&buf, size);
        // Check if frame=>0 or control=>1
        if (p_type.type == 0) {
            // Parse frame packet
            struct PacketHeader p_header(&buf, size);
            auto frame = recv_frames.find(p_header.framenr);
            if (frame == recv_frames.end()) {
                auto e = recv_frames.emplace(p_header.framenr, ReceivedFrame(p_header.framelen, p_header.framenr));
                frame = e.first;
            }
            frame->second.insert(buf, p_header.frameoffset, p_header.packetlen, size);
            if (frame->second.is_complete()) {
                frame_buffer.insert_frame(frame->second);
                recv_frames.erase(p_header.framenr);

            }
        }
        buf = buf_ori;
    }
}

void start_listening() {
    wrk = std::thread(listen_work);
}

int next_frame() {
    if (frame_buffer.get_buffer_size() == 0)
        return -1;
    ReceivedFrame f = frame_buffer.next();
    data_parser.set_current_frame(f);
    return data_parser.get_current_frame_size();
}
void set_data(void* d) {
    data_parser.fill_data_array(d);
}

void clean_up() {
    keep_working = false;
    if(wrk.joinable())
        wrk.join();
    WSACleanup();
    free(buf);
}

int send_data_to_server(void* data, uint32_t size) {
    if (size > BUFLEN)
        return -1;
    uint32_t current_offset = 0;
    int size_send = 0;
    char buf_msg[BUFLEN] = { 0 };
    char* temp_d = reinterpret_cast<char*>(data);
    memcpy(buf_msg, temp_d, size);
    if ((size_send = sendto(s, buf_msg, BUFLEN, 0, (struct sockaddr*)&si_other, slen)) == SOCKET_ERROR)
    {
        return -1;
    }
    return size_send;
}