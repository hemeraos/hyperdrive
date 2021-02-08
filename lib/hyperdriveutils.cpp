#include "hyperdriveutils.h"

#include <QtCore/QDateTime>

namespace Hyperdrive {

namespace Utils {

void seedRNG()
{
    qsrand(QDateTime::currentMSecsSinceEpoch());
}

qreal randomizedInterval(qreal minimumInterval, qreal delayCoefficient)
{
    return minimumInterval + (minimumInterval * delayCoefficient * ((qreal)qrand() / RAND_MAX));
}

} // Utils

} // Hyperdrive
