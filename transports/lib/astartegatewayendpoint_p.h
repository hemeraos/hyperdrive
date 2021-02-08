#ifndef ASTARTE_GATEWAYENDPOINT_P_H
#define ASTARTE_GATEWAYENDPOINT_P_H

#include "astartegatewayendpoint.h"

#include <HemeraCore/Operation>

#include "astarteendpoint_p.h"

class QNetworkAccessManager;

namespace Astarte {

class GatewayEndpointPrivate : public EndpointPrivate {

public:
    GatewayEndpointPrivate(GatewayEndpoint *q) : EndpointPrivate(q) {}

    Q_DECLARE_PUBLIC(GatewayEndpoint)

    QUrl mqttBroker;
};

}

#endif // ASTARTE_GATEWAYENDPOINT_P_H
