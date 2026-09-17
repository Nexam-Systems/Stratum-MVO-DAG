#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtPositioning/QGeoCoordinate>
#include <QtQmlIntegration/QtQmlIntegration>

/// StandoffRing — derived ring geometry (ICD §3.3).
/// A pure function of (target, R, H); never stored independently of the target,
/// so it can never disagree with it.
class StandoffRing : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(QGeoCoordinate target      READ target      NOTIFY changed)
    Q_PROPERTY(double         radiusM     READ radiusM     NOTIFY changed)  ///< R
    Q_PROPERTY(double         heightM     READ heightM     NOTIFY changed)  ///< H above home
    Q_PROPERTY(double         thetaMinDeg READ thetaMinDeg NOTIFY changed)  ///< enforced slot spacing (ICD §6.2)

public:
    explicit StandoffRing(QObject *parent = nullptr);

    QGeoCoordinate target()      const { return _target; }
    double         radiusM()     const { return _radiusM; }
    double         heightM()     const { return _heightM; }
    double         thetaMinDeg() const;

    void setGeometry(const QGeoCoordinate &target, double radiusMeters, double heightMeters);
    void setReqSeparation(double rReqMeters);   ///< R_req drives θ_min (ICD §4, §6.2)

    /// target.atDistanceAndAzimuth(R, bearing) — the slot hold coordinate.
    Q_INVOKABLE QGeoCoordinate slotCoordinate(double bearingDeg) const;
    /// true if bearingDeg respects θ_min against every entry in `others` (ICD §3.3).
    Q_INVOKABLE bool bearingAllowed(double bearingDeg, const QList<double> &others) const;

signals:
    void changed();

private:
    QGeoCoordinate _target;
    double _radiusM = 0.0;
    double _heightM = 0.0;
    double _rReqM   = 7.0;   ///< operative separation minimum; ratified in flight test (ICD §4)
};
