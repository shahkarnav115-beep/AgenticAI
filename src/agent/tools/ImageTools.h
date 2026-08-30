#ifndef IMAGETOOLS_H
#define IMAGETOOLS_H

#include <QString>

class ImageTools {
public:
    static QString generateImage(const QString &path, const QString &prompt, const QString &style = "digital");
};

#endif // IMAGETOOLS_H
