#ifndef RGB_TO_SPECTRUM_H
#define RGB_TO_SPECTRUM_H

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <map>
#include <numeric>
#include <algorithm>
#include <QImage>

struct Pixel {
    unsigned char r, g, b;
};

std::vector<double> spectrt_it(const QImage& currentFrame, size_t w, size_t h);

#endif // RGB_TO_SPECTRUM_H
