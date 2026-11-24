#include "audio/WasapiInput.h"

#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <windows.h>
#include <wrl/client.h>

namespace nuclip {
namespace {

using Microsoft::WRL::ComPtr;

constexpr int kRate = 48000;
constexpr int kChunkFrames = 480;

class SampleRing {
public:
    explicit SampleRing(int frames = kRate)
        : m_buffer(size_t(frames * 2))
    {
    }

    void push(const float *samples, int frames)
    {
        QMutexLocker lock(&m_mutex);
        const int incoming = frames * 2;
        for (int i = 0; i < incoming; ++i) {
            if (m_count == int(m_buffer.size())) {
                m_read = (m_read + 1) % int(m_buffer.size());
                --m_count;
            }
            m_buffer[size_t(m_write)] = samples[i];
            m_write = (m_write + 1) % int(m_buffer.size());
            ++m_count;
        }
    }

    void pop(float *destination, int frames)
    {
        QMutexLocker lock(&m_mutex);
        const int need = frames * 2;
        const int have = std::min(need, m_count);
        for (int i = 0; i < have; ++i) {
            destination[i] = m_buffer[size_t(m_read)];
            m_read = (m_read + 1) % int(m_buffer.size());
        }
        m_count -= have;
        if (have < need)
            std::fill(destination + have, destination + need, 0.f);
    }

    void clear()
    {
        QMutexLocker lock(&m_mutex);
        m_read = 0;
        m_write = 0;
        m_count = 0;
    }

private:
    QMutex m_mutex;
    std::vector<float> m_buffer;
    int m_read = 0;
    int m_write = 0;
    int m_count = 0;
};

struct Format {
    int rate = 48000;
    int channels = 2;
    int bits = 32;
    bool floating = true;
    int blockAlign = 8;
};

Format parseFormat(const WAVEFORMATEX *format)
{
    Format parsed;
    parsed.rate = int(format->nSamplesPerSec);
    parsed.channels = format->nChannels;
    parsed.bits = format->wBitsPerSample;
    parsed.blockAlign = format->nBlockAlign;
    parsed.floating = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
        const auto *extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format);
        parsed.floating = extensible->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT;
    }
    return parsed;
}

float sampleAt(const BYTE *frame, const Format &format, int channel)
{
    const BYTE *sample = frame + channel * (format.bits / 8);
    if (format.floating && format.bits == 32)
        return *reinterpret_cast<const float *>(sample);
    if (!format.floating && format.bits == 16)
        return *reinterpret_cast<const int16_t *>(sample) / 32768.f;
    if (!format.floating && format.bits == 32)
        return *reinterpret_cast<const int32_t *>(sample) / 2147483648.f;
    return 0.f;
}

void toStereo(const BYTE *data, int frames, const Format &format, std::vector<float> *out)
{
    out->resize(size_t(frames * 2));
    for (int frame = 0; frame < frames; ++frame) {
        const BYTE *row = data + frame * format.blockAlign;
        float left = 0.f;
        float right = 0.f;
        if (format.channels <= 1) {
            left = right = sampleAt(row, format, 0);
        } else if (format.channels == 2) {
            left = sampleAt(row, format, 0);
            right = sampleAt(row, format, 1);
        } else if (format.channels >= 6) {
            const float fl = sampleAt(row, format, 0);
            const float fr = sampleAt(row, format, 1);
            const float center = sampleAt(row, format, 2);
            const float backL = format.channels > 4 ? sampleAt(row, format, 4) : 0.f;
            const float backR = format.channels > 5 ? sampleAt(row, format, 5) : 0.f;
            left = fl + 0.707f * center + 0.707f * backL;
            right = fr + 0.707f * center + 0.707f * backR;
        } else {
            left = sampleAt(row, format, 0);
            right = sampleAt(row, format, 1);
        }
        (*out)[size_t(frame * 2)] = left;
        (*out)[size_t(frame * 2 + 1)] = right;
    }
}

class RateConverter {
public:
    void reset()
    {
        m_position = 0;
        m_haveLast = false;
    }

    void convert(const std::vector<float> &input, int sourceRate, std::vector<float> *output)
    {
        if (sourceRate == kRate || sourceRate <= 0) {
            *output = input;
            return;
        }
        const double step = double(sourceRate) / double(kRate);
        const int inputFrames = int(input.size() / 2);
        output->clear();
        if (!m_haveLast && inputFrames > 0) {
            m_lastL = input[0];
            m_lastR = input[1];
            m_haveLast = true;
        }
        while (m_position < inputFrames) {
            const int index = int(m_position);
            const float fraction = float(m_position - index);
            const float left0 = index <= 0 ? m_lastL : input[size_t((index - 1) * 2)];
            const float right0 = index <= 0 ? m_lastR : input[size_t((index - 1) * 2 + 1)];
            const float left1 = input[size_t(index * 2)];
            const float right1 = input[size_t(index * 2 + 1)];
            output->push_back(left0 + (left1 - left0) * fraction);
            output->push_back(right0 + (right1 - right0) * fraction);
            m_position += step;
        }
        m_position -= inputFrames;
        if (inputFrames > 0) {
            m_lastL = input[size_t((inputFrames - 1) * 2)];
            m_lastR = input[size_t((inputFrames - 1) * 2 + 1)];
        }
    }

private:
    double m_position = 0;
    float m_lastL = 0;
    float m_lastR = 0;
    bool m_haveLast = false;
};

QString hrText(HRESULT hr, const QString &action)
{
    return QStringLiteral("%1 failed (%2)").arg(action, QString::number(uint32_t(hr), 16));
}

} // namespace

struct WasapiInput::Impl {
    Config config;
    QString pipeName;
    std::atomic<bool> running{false};
    std::atomic<bool> failed{false};
    std::atomic<float> volume{1.f};
    std::atomic<float> level{0.f};
    QMutex errorMutex;
    QString error;
    SampleRing ring;
    RateConverter converter;
    std::thread capture;
    std::thread writer;
    HANDLE pipe = nullptr;

    void fail(const QString &message)
    {
        failed = true;
        QMutexLocker lock(&errorMutex);
        error = message;
    }

    void captureMain()
    {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool uninit = SUCCEEDED(init);
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioCaptureClient> captureClient;
        WAVEFORMATEX *mix = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
        if (FAILED(hr)) {
            fail(hrText(hr, QStringLiteral("Audio device enumeration")));
        } else if (config.deviceId.isEmpty()) {
            hr = enumerator->GetDefaultAudioEndpoint(config.loopback ? eRender : eCapture, eConsole, &device);
        } else {
            hr = enumerator->GetDevice(reinterpret_cast<LPCWSTR>(config.deviceId.utf16()), &device);
        }
        if (!failed && FAILED(hr))
            fail(hrText(hr, QStringLiteral("Opening the audio device")));
        if (!failed)
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void **>(client.GetAddressOf()));
        if (!failed && FAILED(hr))
            fail(hrText(hr, QStringLiteral("Activating the audio client")));
        if (!failed)
            hr = client->GetMixFormat(&mix);
        if (!failed && (FAILED(hr) || !mix))
            fail(hrText(hr, QStringLiteral("Reading the mix format")));
        if (!failed) {
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, config.loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0,
                                    2000000, 0, mix, nullptr);
        }
        if (!failed && FAILED(hr))
            fail(hrText(hr, QStringLiteral("Initializing audio capture")));
        if (!failed)
            hr = client->GetService(IID_PPV_ARGS(&captureClient));
        if (!failed && FAILED(hr))
            fail(hrText(hr, QStringLiteral("Opening the capture client")));
        if (!failed)
            hr = client->Start();
        if (!failed && FAILED(hr))
            fail(hrText(hr, QStringLiteral("Starting audio capture")));

        Format format = mix ? parseFormat(mix) : Format{};
        std::vector<float> stereo;
        std::vector<float> resampled;
        while (running && !failed) {
            UINT32 packet = 0;
            if (FAILED(captureClient->GetNextPacketSize(&packet)))
                break;
            while (packet > 0 && running) {
                BYTE *data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                if (FAILED(captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                    break;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT || !data) {
                    stereo.assign(size_t(frames * 2), 0.f);
                } else {
                    toStereo(data, int(frames), format, &stereo);
                }
                captureClient->ReleaseBuffer(frames);
                float energy = 0.f;
                for (float sample : stereo)
                    energy += sample * sample;
                const float rms = stereo.empty() ? 0.f : std::sqrt(energy / float(stereo.size()));
                const float previous = level.load();
                level = previous * 0.65f + std::min(1.f, rms * 4.f) * 0.35f;
                converter.convert(stereo, format.rate, &resampled);
                if (!resampled.empty())
                    ring.push(resampled.data(), int(resampled.size() / 2));
                if (FAILED(captureClient->GetNextPacketSize(&packet))) {
                    packet = 0;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (client)
            client->Stop();
        if (mix)
            CoTaskMemFree(mix);
        if (uninit)
            CoUninitialize();
    }

    void writerMain(HANDLE ready)
    {
        const std::wstring name = pipeName.toStdWString();
        pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                                1 << 20, 1 << 20, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            pipe = nullptr;
            fail(QStringLiteral("Could not create the audio pipe."));
            SetEvent(ready);
            return;
        }
        SetEvent(ready);
        const BOOL connected = ConnectNamedPipe(pipe, nullptr);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            if (running)
                fail(QStringLiteral("FFmpeg did not connect to the audio pipe."));
            CloseHandle(pipe);
            pipe = nullptr;
            return;
        }
        if (!running) {
            CloseHandle(pipe);
            pipe = nullptr;
            return;
        }
        ring.clear();
        std::vector<float> chunk(size_t(kChunkFrames * 2));
        auto next = std::chrono::steady_clock::now();
        while (running) {
            next += std::chrono::milliseconds(10);
            ring.pop(chunk.data(), kChunkFrames);
            const float gain = volume.load();
            for (float &sample : chunk)
                sample *= gain;
            DWORD written = 0;
            if (!WriteFile(pipe, chunk.data(), DWORD(chunk.size() * sizeof(float)), &written, nullptr))
                break;
            std::this_thread::sleep_until(next);
        }
        CloseHandle(pipe);
        pipe = nullptr;
    }
};

WasapiInput::WasapiInput()
    : m_impl(std::make_unique<Impl>())
{
}

WasapiInput::~WasapiInput()
{
    stop();
}

bool WasapiInput::start(const Config &config, QString *error)
{
    stop();
    m_impl->config = config;
    m_impl->pipeName = config.pipeName;
    m_impl->failed = false;
    m_impl->volume = 1.f;
    m_impl->level = 0.f;
    {
        QMutexLocker lock(&m_impl->errorMutex);
        m_impl->error.clear();
    }
    m_impl->converter.reset();
    m_impl->ring.clear();
    m_impl->running = true;
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_impl->writer = std::thread([this, ready] { m_impl->writerMain(ready); });
    WaitForSingleObject(ready, 2000);
    CloseHandle(ready);
    if (m_impl->failed) {
        if (error)
            *error = this->error();
        stop();
        return false;
    }
    m_impl->capture = std::thread([this] { m_impl->captureMain(); });
    return true;
}

void WasapiInput::stop()
{
    if (!m_impl)
        return;
    m_impl->running = false;
    if (m_impl->pipeName.size()) {
        HANDLE client = CreateFileW(reinterpret_cast<LPCWSTR>(m_impl->pipeName.utf16()), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (client != INVALID_HANDLE_VALUE)
            CloseHandle(client);
    }
    if (m_impl->writer.joinable())
        m_impl->writer.join();
    if (m_impl->capture.joinable())
        m_impl->capture.join();
}

void WasapiInput::setVolume(float volume)
{
    m_impl->volume = std::clamp(volume, 0.f, 1.f);
}

float WasapiInput::level() const
{
    return m_impl->level.load();
}

bool WasapiInput::failed() const
{
    return m_impl->failed.load();
}

QString WasapiInput::error() const
{
    QMutexLocker lock(&m_impl->errorMutex);
    return m_impl->error;
}

QString WasapiInput::pipeName() const
{
    return m_impl->pipeName;
}

} // namespace nuclip
