#pragma once

#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

#include "SettingsGroup.h"

/// STRATUM admin-only settings. The group itself is standard QSettings-backed Facts
/// (required PX4/NX versions, strict-gate toggle). Visibility of the editor is gated
/// behind a hidden unlock gesture on the STRATUM wordmark and a SHA-256 password
/// check implemented in this class. The password hash is baked in at compile time
/// and can be overridden per build via -DSTRATUM_ADMIN_PASSWORD_HASH=<hex> in CMake.
class AdminSettings : public SettingsGroup
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(bool unlocked READ unlocked NOTIFY unlockedChanged)
    Q_PROPERTY(bool lockedOut READ lockedOut NOTIFY lockedOutChanged)
public:
    AdminSettings(QObject* parent = nullptr);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(requiredPx4MajorVersion)
    DEFINE_SETTINGFACT(requiredPx4MinorVersion)
    DEFINE_SETTINGFACT(requiredPx4PatchVersion)
    DEFINE_SETTINGFACT(requiredStratumSchemaMajor)
    DEFINE_SETTINGFACT(requiredStratumNxMajor)
    DEFINE_SETTINGFACT(strictCompatibilityGate)
    DEFINE_SETTINGFACT(passwordHashOverride)

    bool unlocked() const { return _unlocked; }
    bool lockedOut() const { return _wrongAttempts >= kMaxWrongAttempts; }

    /// Verifies the plaintext against either the runtime override hash (if set) or
    /// the compile-time SHA-256 hash. On success, flips the session unlock flag. On
    /// failure, increments a session-scoped wrong attempt counter; after
    /// kMaxWrongAttempts the dialog refuses further tries until the app is restarted.
    Q_INVOKABLE bool verifyPassword(const QString& plaintext);

    /// Replaces the stored admin password at runtime. Requires the current password
    /// to be presented again (even if the session is already unlocked) and the new
    /// password to pass minimum-strength rules. The new SHA-256 hash is persisted in
    /// the passwordHashOverride fact; from then on the compile-time hash is ignored
    /// until the override is cleared. Returns true on success. On any failure the
    /// stored hash is left untouched.
    Q_INVOKABLE bool changePassword(const QString& currentPlaintext, const QString& newPlaintext);

    /// Minimum plaintext length enforced by changePassword.
    static constexpr int kMinPasswordLength = 8;

    /// Clears the session unlock flag. Called from the dialog's close path so the
    /// password must be re-entered next time.
    Q_INVOKABLE void relock();

signals:
    void unlockedChanged();
    void lockedOutChanged();

private:
    static constexpr int kMaxWrongAttempts = 3;

    bool _unlocked = false;
    int  _wrongAttempts = 0;
};
