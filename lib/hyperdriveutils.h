#ifndef _HYPERDRIVEUTILS_H_
#define _HYPERDRIVEUTILS_H_

#include <QtCore/QtGlobal>

namespace Hyperdrive {

namespace Utils {

    void seedRNG();
    qreal randomizedInterval(qreal minimumInterval, qreal delayCoefficient);

}

}

#endif
