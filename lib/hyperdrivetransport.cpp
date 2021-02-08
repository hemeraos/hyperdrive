/*
 *
 */

#include "hyperdrivetransport_p.h"
#include "hyperdrivecore.h"

#include <QtCore/QDebug>
#include <QtCore/QUuid>
#include <QtCore/QEventLoop>

namespace Hyperdrive
{

void TransportPrivate::initHook()
{
    Hemera::AsyncInitObjectPrivate::initHook();
}

Transport::Transport(TransportPrivate &dd, const QString &name, QObject *parent)
    : AsyncInitObject(dd, parent)
{
    Q_D(Transport);
    d->name = name;
}

Transport::~Transport()
{
}

QString Transport::name() const
{
    Q_D(const Transport);
    return d->name;
}

}
