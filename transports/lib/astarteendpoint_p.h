#ifndef ASTARTE_ENDPOINT_P_H
#define ASTARTE_ENDPOINT_P_H

#include "astartehttpendpoint.h"

#include <HemeraCore/Operation>

#include <private/HemeraCore/hemeraasyncinitobject_p.h>

namespace Astarte {

class EndpointPrivate : public Hemera::AsyncInitObjectPrivate {
public:
    EndpointPrivate(Endpoint *q) : AsyncInitObjectPrivate(q) {}

    Q_DECLARE_PUBLIC(Endpoint)

    QUrl endpoint;
};

}

#endif // ASTARTE_ENDPOINT_P_H
