//
// Created by volund on 11/13/21.
//

#include "mudlink/mudlink.h"


int main(int argc, char **argv) {
    mudlink::createListener("0.0.0.0", 2008, mudlink::ClientType::TcpTelnet);
    mudlink::run(1);
}