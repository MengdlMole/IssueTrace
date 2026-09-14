#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <array>

namespace {
QByteArray renderPng(QSvgRenderer& renderer, int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    painter.end();

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) return {};
    return png;
}

bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    if (argc != 5) return 2;

    QSvgRenderer renderer(QString::fromLocal8Bit(argv[1]));
    if (!renderer.isValid()) return 3;

    const QString iconsetPath = QString::fromLocal8Bit(argv[2]);
    const QString icoPath = QString::fromLocal8Bit(argv[3]);
    const QString icnsPath = QString::fromLocal8Bit(argv[4]);
    if (!QDir().mkpath(iconsetPath)) return 4;

    struct IconsetFile { const char* name; int size; };
    constexpr std::array iconsetFiles{
        IconsetFile{"icon_16x16.png", 16},
        IconsetFile{"icon_16x16@2x.png", 32},
        IconsetFile{"icon_32x32.png", 32},
        IconsetFile{"icon_32x32@2x.png", 64},
        IconsetFile{"icon_128x128.png", 128},
        IconsetFile{"icon_128x128@2x.png", 256},
        IconsetFile{"icon_256x256.png", 256},
        IconsetFile{"icon_256x256@2x.png", 512},
        IconsetFile{"icon_512x512.png", 512},
        IconsetFile{"icon_512x512@2x.png", 1024},
    };
    for (const auto& entry : iconsetFiles) {
        if (!writeFile(QDir(iconsetPath).filePath(QString::fromLatin1(entry.name)),
                       renderPng(renderer, entry.size))) {
            return 5;
        }
    }

    constexpr std::array icoSizes{16, 32, 48, 64, 128, 256};
    std::array<QByteArray, icoSizes.size()> images;
    for (qsizetype index = 0; index < static_cast<qsizetype>(icoSizes.size()); ++index) {
        images[index] = renderPng(renderer, icoSizes[index]);
        if (images[index].isEmpty()) return 6;
    }

    QFile ico(icoPath);
    if (!ico.open(QIODevice::WriteOnly)) return 7;
    QDataStream stream(&ico);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint16(0) << quint16(1) << quint16(icoSizes.size());
    quint32 offset = 6 + static_cast<quint32>(16 * icoSizes.size());
    for (qsizetype index = 0; index < static_cast<qsizetype>(icoSizes.size()); ++index) {
        const auto size = icoSizes[index];
        stream << quint8(size == 256 ? 0 : size) << quint8(size == 256 ? 0 : size)
               << quint8(0) << quint8(0) << quint16(1) << quint16(32)
               << quint32(images[index].size()) << offset;
        offset += static_cast<quint32>(images[index].size());
    }
    for (const auto& bytes : images) {
        if (ico.write(bytes) != bytes.size()) return 8;
    }

    struct IcnsImage { const char* type; int size; };
    constexpr std::array icnsImages{
        IcnsImage{"icp4", 16}, IcnsImage{"icp5", 32}, IcnsImage{"icp6", 64},
        IcnsImage{"ic07", 128}, IcnsImage{"ic08", 256}, IcnsImage{"ic09", 512},
        IcnsImage{"ic10", 1024},
    };
    std::array<QByteArray, icnsImages.size()> icnsPngs;
    quint32 icnsLength = 8;
    for (qsizetype index = 0; index < static_cast<qsizetype>(icnsImages.size()); ++index) {
        icnsPngs[index] = renderPng(renderer, icnsImages[index].size);
        if (icnsPngs[index].isEmpty()) return 9;
        icnsLength += 8 + static_cast<quint32>(icnsPngs[index].size());
    }
    QFile icns(icnsPath);
    if (!icns.open(QIODevice::WriteOnly)) return 10;
    QDataStream icnsStream(&icns);
    icnsStream.setByteOrder(QDataStream::BigEndian);
    icnsStream.writeRawData("icns", 4);
    icnsStream << icnsLength;
    for (qsizetype index = 0; index < static_cast<qsizetype>(icnsImages.size()); ++index) {
        icnsStream.writeRawData(icnsImages[index].type, 4);
        icnsStream << quint32(8 + icnsPngs[index].size());
        if (icns.write(icnsPngs[index]) != icnsPngs[index].size()) return 11;
    }
    return 0;
}
