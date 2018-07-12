#include "utility/helpers.h"
#include <opendht.h>


int main() {
#ifdef WIN32
    gnutls_global_init();
#endif

    auto dht = std::make_shared<dht::DhtRunner>();



    dht->join();

#ifdef WIN32
    gnutls_global_deinit();
#endif
    return 0;
}
