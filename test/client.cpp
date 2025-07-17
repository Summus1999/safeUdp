#pragma once
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <glog/logging.h>
#include "udp_client.h"

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = google::GLOG_INFO;
  LOG(INFO) << "Starting the client !!!";
  if (argc != 7) {
    LOG(ERROR) << "Please provide format: <server-ip> <server-port> "
                  "<file-name> <receiver-window> <control-param> <drop/delay%>";
    exit(1);
  }

  safe_udp::UdpClient *udp_client = new safe_udp::UdpClient();
  std::string server_ip(argv[1]);
  std::string port_num(argv[2]);
  std::string file_name(argv[3]);
  udp_client->receiverWindow = atoi(argv[4]);

  int control_param = atoi(argv[5]);
  LOG(INFO) << "control_param: " << control_param;
  if (control_param == 0) {
    udp_client->isDelay = false;
    udp_client->isPacketDrop = false;
  } else if (control_param == 1) {
    udp_client->isPacketDrop = true;
    udp_client->isDelay = false;
  } else if (control_param == 2) {
    udp_client->isPacketDrop = false;
    udp_client->isDelay = true;
  } else if (control_param == 3) {
    udp_client->isPacketDrop = true;
    udp_client->isDelay = true;
  } else {
    LOG(ERROR) << "Invalid argument, should be range in 0-3 !!!";
    return 0;
  }

  int drop_percentage = atoi(argv[6]);
  udp_client->probValue = drop_percentage;

  udp_client->CreateSocketAndServerConnection(server_ip, port_num);
  udp_client->SendFileRequest(file_name);

  free(udp_client);
  return 0;
}