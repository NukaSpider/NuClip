#include "platform/DeviceCatalog.h"

#include "recording/FfmpegLocator.h"

#include <QProcess>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <dxgi.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#include <wrl/client.h>

namespace nuclip {
namespace {

using Microsoft::WRL::ComPtr;

QVariantMap deviceItem(const QString &id, const QString &label)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), id);
    map.insert(QStringLiteral("label"), label);
    return map;
}

QString probeCodecName(const QString &encoder, const QString &codec)
{
    const bool hevc = codec == QLatin1String("hevc");
    const bool av1 = codec == QLatin1String("av1");
    if (encoder == QLatin1String("nvenc"))
        return hevc ? QStringLiteral("hevc_nvenc") : av1 ? QStringLiteral("av1_nvenc") : QStringLiteral("h264_nvenc");
    if (encoder == QLatin1String("amf"))
        return hevc ? QStringLiteral("hevc_amf") : av1 ? QStringLiteral("av1_amf") : QStringLiteral("h264_amf");
    if (encoder == QLatin1String("qsv"))
        return hevc ? QStringLiteral("hevc_qsv") : av1 ? QStringLiteral("av1_qsv") : QStringLiteral("h264_qsv");
    if (hevc)
        return QStringLiteral("libx265");
    if (av1)
        return QStringLiteral("libsvtav1");
    return QStringLiteral("libx264");
}

QString friendlyName(IMMDevice *device)
{
    if (!device)
        return {};
    ComPtr<IPropertyStore> store;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &store)))
        return {};
    PROPVARIANT value;
    PropVariantInit(&value);
    QString name;
    if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR && value.pwszVal)
        name = QString::fromWCharArray(value.pwszVal);
    PropVariantClear(&value);
    return name;
}

void collectEndpoints(IMMDeviceEnumerator *enumerator, EDataFlow flow, QVariantList *out)
{
    ComPtr<IMMDeviceCollection> collection;
    if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection)))
        return;
    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device)))
            continue;
        LPWSTR rawId = nullptr;
        if (FAILED(device->GetId(&rawId)) || !rawId)
            continue;
        const QString id = QString::fromWCharArray(rawId);
        CoTaskMemFree(rawId);
        const QString name = friendlyName(device.Get());
        if (name.isEmpty())
            continue;
        out->append(deviceItem(id, name));
    }
}

} // namespace

DeviceCatalog::DeviceCatalog(QObject *parent)
    : QObject(parent)
{
    m_probe = new QProcess(this);
    connect(m_probe, &QProcess::finished, this, [this](int code) {
        const QString encoder = m_probe->property("encoderId").toString();
        const QString codec = m_probe->property("codecId").toString();
        if (code == 0) {
            if (codec == QLatin1String("h264")) {
                if (encoder == QLatin1String("nvenc"))
                    m_nvenc = true;
                else if (encoder == QLatin1String("amf"))
                    m_amf = true;
                else if (encoder == QLatin1String("qsv"))
                    m_qsv = true;
            } else if (!encoder.isEmpty() && !codec.isEmpty()) {
                m_supported.insert(encoder + QLatin1Char('/') + codec);
            }
            emit encodersChanged();
        }
        probeNext();
    });
}

void DeviceCatalog::refresh()
{
    enumerateMonitors();
    enumerateAudio();
}

void DeviceCatalog::enumerateMonitors()
{
    QVariantList next;
    ComPtr<ID3D11Device> device;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, nullptr);
    ComPtr<IDXGIAdapter> adapter;
    if (SUCCEEDED(hr) && device) {
        ComPtr<IDXGIDevice> dxgiDevice;
        if (SUCCEEDED(device.As(&dxgiDevice)))
            dxgiDevice->GetParent(IID_PPV_ARGS(&adapter));
    }
    if (!adapter) {
        ComPtr<IDXGIFactory1> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            factory->EnumAdapters(0, &adapter);
    }
    if (adapter) {
        DXGI_ADAPTER_DESC adapterDesc{};
        if (SUCCEEDED(adapter->GetDesc(&adapterDesc)))
            m_adapterVendor = adapterDesc.VendorId;
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIOutput> output;
            if (adapter->EnumOutputs(index, &output) == DXGI_ERROR_NOT_FOUND)
                break;
            DXGI_OUTPUT_DESC desc{};
            if (FAILED(output->GetDesc(&desc)))
                continue;
            const int width = int(desc.DesktopCoordinates.right - desc.DesktopCoordinates.left);
            const int height = int(desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top);
            if (width <= 0 || height <= 0)
                continue;
            const bool primary = desc.DesktopCoordinates.left <= 0 && desc.DesktopCoordinates.right > 0
                && desc.DesktopCoordinates.top <= 0 && desc.DesktopCoordinates.bottom > 0;
            int refresh = 0;
            DEVMODEW mode{};
            mode.dmSize = sizeof(mode);
            if (EnumDisplaySettingsW(desc.DeviceName, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency >= 30)
                refresh = int(mode.dmDisplayFrequency);
            QString label = QStringLiteral("Display %1 · %2×%3").arg(index + 1).arg(width).arg(height);
            if (primary)
                label += QStringLiteral(" · Primary");
            QVariantMap item;
            item.insert(QStringLiteral("index"), int(index));
            item.insert(QStringLiteral("label"), label);
            item.insert(QStringLiteral("width"), width);
            item.insert(QStringLiteral("height"), height);
            item.insert(QStringLiteral("refresh"), refresh);
            item.insert(QStringLiteral("primary"), primary);
            item.insert(QStringLiteral("x"), int(desc.DesktopCoordinates.left));
            item.insert(QStringLiteral("y"), int(desc.DesktopCoordinates.top));
            item.insert(QStringLiteral("right"), int(desc.DesktopCoordinates.right));
            item.insert(QStringLiteral("bottom"), int(desc.DesktopCoordinates.bottom));
            next.append(item);
        }
    }
    if (next.isEmpty()) {
        QVariantMap item;
        item.insert(QStringLiteral("index"), 0);
        item.insert(QStringLiteral("label"), QStringLiteral("Primary display"));
        item.insert(QStringLiteral("width"), 0);
        item.insert(QStringLiteral("height"), 0);
        item.insert(QStringLiteral("refresh"), 0);
        item.insert(QStringLiteral("primary"), true);
        next.append(item);
    }
    m_monitors = next;
    emit monitorsChanged();
}

void DeviceCatalog::enumerateAudio()
{
    QVariantList mics;
    QVariantList outputs;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(hr);
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) {
        ComPtr<IMMDevice> defaultRender;
        ComPtr<IMMDevice> defaultCapture;
        QString renderName = QStringLiteral("Default output");
        QString captureName = QStringLiteral("Default microphone");
        if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultRender))) {
            const QString name = friendlyName(defaultRender.Get());
            if (!name.isEmpty())
                renderName = QStringLiteral("Default — %1").arg(name);
        }
        if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &defaultCapture))) {
            const QString name = friendlyName(defaultCapture.Get());
            if (!name.isEmpty())
                captureName = QStringLiteral("Default — %1").arg(name);
        }
        outputs.append(deviceItem(QString(), renderName));
        mics.append(deviceItem(QString(), captureName));
        collectEndpoints(enumerator.Get(), eRender, &outputs);
        collectEndpoints(enumerator.Get(), eCapture, &mics);
    }
    if (uninit)
        CoUninitialize();
    m_outputs = outputs;
    m_microphones = mics;
    emit audioChanged();
}

QVariantList DeviceCatalog::encoders() const
{
    QVariantList list;
    const QString chosen = resolveEncoder(QStringLiteral("auto"));
    list.append(deviceItem(QStringLiteral("auto"), QStringLiteral("Auto · %1").arg(encoderLabel(chosen))));
    if (m_nvenc)
        list.append(deviceItem(QStringLiteral("nvenc"), QStringLiteral("NVIDIA NVENC")));
    if (m_amf)
        list.append(deviceItem(QStringLiteral("amf"), QStringLiteral("AMD AMF")));
    if (m_qsv)
        list.append(deviceItem(QStringLiteral("qsv"), QStringLiteral("Intel Quick Sync")));
    list.append(deviceItem(QStringLiteral("x264"), QStringLiteral("Software")));
    return list;
}

QString DeviceCatalog::encoderLabel(const QString &id) const
{
    if (id == QLatin1String("nvenc"))
        return QStringLiteral("NVIDIA NVENC");
    if (id == QLatin1String("amf"))
        return QStringLiteral("AMD AMF");
    if (id == QLatin1String("qsv"))
        return QStringLiteral("Intel Quick Sync");
    if (id == QLatin1String("x264"))
        return QStringLiteral("Software");
    return encoderLabel(resolveEncoder(QStringLiteral("auto")));
}

bool DeviceCatalog::encoderAvailable(const QString &id) const
{
    if (id == QLatin1String("nvenc"))
        return m_nvenc;
    if (id == QLatin1String("amf"))
        return m_amf;
    if (id == QLatin1String("qsv"))
        return m_qsv;
    return id == QLatin1String("x264") || id == QLatin1String("auto");
}

QString DeviceCatalog::vendorEncoder() const
{
    if (m_adapterVendor == 0x1002)
        return QStringLiteral("amf");
    if (m_adapterVendor == 0x10DE)
        return QStringLiteral("nvenc");
    if (m_adapterVendor == 0x8086)
        return QStringLiteral("qsv");
    return QStringLiteral("amf");
}

bool DeviceCatalog::familySupports(const QString &encoder, const QString &codec) const
{
    const QString want = codec.isEmpty() ? QStringLiteral("h264") : codec;
    if (encoder == QLatin1String("x264"))
        return want == QLatin1String("h264") || m_supported.contains(QStringLiteral("x264/") + want);
    if (!encoderAvailable(encoder))
        return false;
    if (want == QLatin1String("h264"))
        return true;
    return m_supported.contains(encoder + QLatin1Char('/') + want);
}

QString DeviceCatalog::resolveEncoder(const QString &requested, const QString &codec) const
{
    const QString want = codec.isEmpty() ? QStringLiteral("h264") : codec;
    if (requested != QLatin1String("auto") && familySupports(requested, want))
        return requested;
    QStringList ranked;
    const QString prefer = vendorEncoder();
    if (familySupports(prefer, want))
        ranked << prefer;
    const QStringList rest{QStringLiteral("amf"), QStringLiteral("nvenc"), QStringLiteral("qsv"), QStringLiteral("x264")};
    for (const QString &id : rest) {
        if (!ranked.contains(id))
            ranked << id;
    }
    for (const QString &id : ranked) {
        if (familySupports(id, want))
            return id;
    }
    return QStringLiteral("x264");
}

bool DeviceCatalog::codecSupported(const QString &encoder, const QString &codec) const
{
    const QString id = encoder == QLatin1String("auto") ? resolveEncoder(encoder, codec) : encoder;
    return familySupports(id, codec);
}

QVariantList DeviceCatalog::codecsFor(const QString &encoder) const
{
    QVariantList list;
    const QStringList codecs{QStringLiteral("h264"), QStringLiteral("hevc"), QStringLiteral("av1")};
    for (const QString &codec : codecs) {
        const QString resolved = resolveEncoder(encoder, codec);
        if (!familySupports(resolved, codec))
            continue;
        const bool hardware = resolved != QLatin1String("x264");
        QString detail;
        if (codec == QLatin1String("hevc"))
            detail = QStringLiteral("Better compression at comparable visual quality.");
        else if (codec == QLatin1String("av1"))
            detail = hardware
                ? QStringLiteral("Excellent compression and visual quality. %1.").arg(encoderLabel(resolved))
                : QStringLiteral("Excellent compression and visual quality. Encoded on the CPU.");
        else
            detail = QStringLiteral("Best compatibility.");
        if (codec != QLatin1String("av1")) {
            if (hardware)
                detail += QLatin1Char(' ') + encoderLabel(resolved) + QLatin1Char('.');
            else
                detail += QStringLiteral(" Encoded on the CPU.");
        }
        QVariantMap item;
        item.insert(QStringLiteral("id"), codec);
        item.insert(QStringLiteral("label"), codec == QLatin1String("hevc") ? QStringLiteral("H.265")
                                      : codec == QLatin1String("av1")      ? QStringLiteral("AV1")
                                                                            : QStringLiteral("H.264"));
        item.insert(QStringLiteral("detail"), detail);
        item.insert(QStringLiteral("hardware"), hardware);
        list.append(item);
    }
    return list;
}

bool DeviceCatalog::monitorGeometry(int index, int *width, int *height, int *refresh) const
{
    for (const QVariant &value : m_monitors) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("index")).toInt() != index)
            continue;
        if (width)
            *width = item.value(QStringLiteral("width")).toInt();
        if (height)
            *height = item.value(QStringLiteral("height")).toInt();
        if (refresh)
            *refresh = item.value(QStringLiteral("refresh")).toInt();
        return true;
    }
    if (width)
        *width = 0;
    if (height)
        *height = 0;
    if (refresh)
        *refresh = 0;
    return false;
}

int DeviceCatalog::monitorRefresh(int index) const
{
    int refresh = 0;
    monitorGeometry(index, nullptr, nullptr, &refresh);
    return refresh;
}

bool DeviceCatalog::fpsSupported(int fps, int monitorIndex) const
{
    const int refresh = monitorRefresh(monitorIndex);
    if (refresh <= 0)
        return fps <= 60;
    return fps <= refresh + 3;
}

int DeviceCatalog::supportedFps(int fps, int monitorIndex) const
{
    static const int rates[] = {30, 60, 90, 120, 144, 165, 240};
    int best = 30;
    for (int rate : rates) {
        if (rate <= fps && fpsSupported(rate, monitorIndex))
            best = rate;
    }
    return best;
}

int DeviceCatalog::indexForPoint(int x, int y) const
{
    for (const QVariant &value : m_monitors) {
        const QVariantMap item = value.toMap();
        const int left = item.value(QStringLiteral("x")).toInt();
        const int top = item.value(QStringLiteral("y")).toInt();
        const int right = item.value(QStringLiteral("right")).toInt();
        const int bottom = item.value(QStringLiteral("bottom")).toInt();
        if (x >= left && x < right && y >= top && y < bottom)
            return item.value(QStringLiteral("index")).toInt();
    }
    return 0;
}

QString DeviceCatalog::monitorLabel(int index) const
{
    for (const QVariant &value : m_monitors) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("index")).toInt() == index)
            return item.value(QStringLiteral("label")).toString();
    }
    if (!m_monitors.isEmpty())
        return m_monitors.first().toMap().value(QStringLiteral("label")).toString();
    return QStringLiteral("Display");
}

void DeviceCatalog::startProbe()
{
    if (m_probing)
        return;
    const QString ffmpeg = ffmpegExecutable();
    if (ffmpeg.isEmpty()) {
        emit probeFinished();
        return;
    }
    m_nvenc = false;
    m_amf = false;
    m_qsv = false;
    m_supported.clear();
    m_probeQueue.clear();
    const QStringList families{QStringLiteral("nvenc"), QStringLiteral("amf"), QStringLiteral("qsv")};
    const QStringList codecs{QStringLiteral("h264"), QStringLiteral("hevc"), QStringLiteral("av1")};
    for (const QString &family : families) {
        for (const QString &codec : codecs)
            m_probeQueue << family + QLatin1Char('|') + codec;
    }
    m_probeQueue << QStringLiteral("x264|hevc") << QStringLiteral("x264|av1");
    m_probing = true;
    emit probingChanged();
    probeNext();
}

void DeviceCatalog::probeNext()
{
    if (m_probeQueue.isEmpty()) {
        m_probing = false;
        emit probingChanged();
        emit encodersChanged();
        emit probeFinished();
        return;
    }
    const QString job = m_probeQueue.takeFirst();
    const int split = job.indexOf(QLatin1Char('|'));
    const QString encoder = job.left(split);
    const QString codec = job.mid(split + 1);
    const QString ffmpegCodec = probeCodecName(encoder, codec);
    m_probe->setProperty("encoderId", encoder);
    m_probe->setProperty("codecId", codec);
    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
                     QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
                     QStringLiteral("color=c=black:s=256x144:r=30"), QStringLiteral("-frames:v"),
                     QStringLiteral("2"), QStringLiteral("-c:v"), ffmpegCodec};
    if (ffmpegCodec == QLatin1String("libsvtav1"))
        args << QStringLiteral("-preset") << QStringLiteral("12");
    args << QStringLiteral("-b:v") << QStringLiteral("1000k") << QStringLiteral("-f") << QStringLiteral("null")
         << QStringLiteral("-");
    m_probe->start(ffmpegExecutable(), args);
}

} // namespace nuclip
