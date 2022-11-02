//
// Created by volund on 11/13/21.
//

#include "mudlink/mudlink.h"


int main(int argc, char **argv) {

    auto exEp = mudlink::createEndpoint("0.0.0.0", 2008);
    auto inEp = mudlink::createEndpoint("127.0.0.1", 2009);

    mudlink::MudLink link(exEp, inEp);
    link.start(1);
}