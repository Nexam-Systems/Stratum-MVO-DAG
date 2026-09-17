#include "StandoffRing.h"

#include <QtCore/qmath.h>

StandoffRing::StandoffRing(QObject *parent)
    : QObject(parent)
{
}

double StandoffRing::thetaMinDeg() const
{
    // θ_min = 2·asin( R_req / (2R) )   — ICD §6.2
    if (_radiusM <= 0.0) {
        return 0.0;
    }
    const double ratio = qBound(-1.0, _rReqM / (2.0 * _radiusM), 1.0);
    return qRadiansToDegrees(2.0 * qAsin(ratio));
}

void StandoffRing::setGeometry(const QGeoCoordinate &target, double radiusMeters, double heightMeters)
{
    _target  = target;
    _radiusM = radiusMeters;
    _heightM = heightMeters;
    emit changed();
}

void StandoffRing::setReqSeparation(double rReqMeters)
{
    _rReqM = rReqMeters;
    emit changed();
}

QGeoCoordinate StandoffRing::slotCoordinate(double bearingDeg) const
{
    return _target.atDistanceAndAzimuth(_radiusM, bearingDeg);
}

bool StandoffRing::bearingAllowed(double bearingDeg, const QList<double> &others) const
{
    const double thetaMin = thetaMinDeg();
    for (const double other : others) {
        double delta = qAbs(bearingDeg - other);
        if (delta > 180.0) {
            delta = 360.0 - delta;
        }
        if (delta < thetaMin) {
            return false;
        }
    }
    return true;
}
