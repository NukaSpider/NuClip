#pragma once

#include <QString>

#include <memory>

namespace nuclip {

class WasapiInput {
public:
    struct Config {
        QString deviceId;
        bool loopback = false;
        QString pipeName;
    };

    WasapiInput();
    ~WasapiInput();

    bool start(const Config &config, QString *error);
    void stop();
    void setVolume(float volume);
    float level() const;
    bool failed() const;
    QString error() const;
    QString pipeName() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace nuclip
