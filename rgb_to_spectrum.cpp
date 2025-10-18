
#include <QImage>
#include "rgb_to_spectrum.h"



// Define the range for the spectrum calculation
constexpr int MIN_WAVELENGTH = 300; // nm
constexpr int MAX_WAVELENGTH = 800; // nm


static size_t w_;
static size_t h_;

static std::vector<std::vector<Pixel>> mock_;

std::vector<std::vector<Pixel>> create_mock_image(size_t w, size_t h)
{
    w_=w;
    h_=h;
    // Initialize image with the new dimensions: rows = HEIGHT, columns = WIDTH
    std::vector<std::vector<Pixel>> image(h, std::vector<Pixel>(w));

    for (int i = 0; i < h; ++i) { // Loop over rows (Height)
        for (int j = 0; j < w; ++j) { // Loop over columns (Width)
            // Create a diagonal blue gradient for visual interest in the mock data
            // Simplified calculation to prevent overflow and provide smooth mock data
            int intensity = (i + j) % 200;

            image[i][j] = {
                (unsigned char)(intensity * 0.2), // Low Red
                (unsigned char)(intensity * 0.5), // Medium Green
                (unsigned char)(intensity * 0.9)  // High Blue
            };
        }
    }
    return image;
}

std::tuple<double, double, double>
calculate_average_normalized_rgb(const QImage& currentFrame)
{

    long long sum_r = 0;
    long long sum_g = 0;
    long long sum_b = 0;
    // Calculate total pixels using the new dimensions
    long long total_pixels = (long long)w_ * (long long)h_;

    for (int y = 0; y < currentFrame.height(); ++y) {
        for (int x = 0; x < currentFrame.width(); ++x) {
            QRgb rgb = currentFrame.pixel(x, y);
            // Use normalized values (0.0 to 1.0)
            sum_r += qRed(rgb);
            sum_g += qGreen(rgb);
            sum_b += qBlue(rgb);
        }
    }

    // Calculate average intensity for each channel (0-255)
    double avg_r = (double)sum_r / total_pixels;
    double avg_g = (double)sum_g / total_pixels;
    double avg_b = (double)sum_b / total_pixels;

    // Normalize the average intensity to the target range [0.0, 1.0]
    double r_norm = avg_r / 255.0;
    double g_norm = avg_g / 255.0;
    double b_norm = avg_b / 255.0;

    return {r_norm, g_norm, b_norm};
}

double gaussian_function(int lambda, double mu, double sigma) {
    // Standard Gaussian formula: e ^ ( -(lambda - mu)^2 / (2 * sigma^2) )
    double exponent = -pow((lambda - mu), 2) / (2.0 * pow(sigma, 2));
    return exp(exponent);
}


std::vector<double> calculate_spectrum(double r_norm, double g_norm, double b_norm) {

    const double B_PEAK = 450.0; // Blue center wavelength
    const double G_PEAK = 550.0; // Green center wavelength
    const double R_PEAK = 650.0; // Red center wavelength

    // Define the bandwidth/spread (sigma) for the Gaussian curves
    // Blue is generally narrower, Red is often wider
    const double B_SPREAD = 30.0;
    const double G_SPREAD = 40.0;
    const double R_SPREAD = 45.0;

    std::vector<double> spectrum;

    // Iterate over every single nanometer (1nm granularity)
    for (int lambda = MIN_WAVELENGTH; lambda <= MAX_WAVELENGTH; ++lambda) {

        // 1. Calculate the raw spectral response for each channel using the Gaussian model
        double blue_contribution = b_norm * gaussian_function(lambda, B_PEAK, B_SPREAD);
        double green_contribution = g_norm * gaussian_function(lambda, G_PEAK, G_SPREAD);
        double red_contribution = r_norm * gaussian_function(lambda, R_PEAK, R_SPREAD);

        // 2. Sum the contributions to get the total estimated intensity at this wavelength
        double intensity = blue_contribution + green_contribution + red_contribution;

        // 3. Normalize the result.
        // Since the sum of three Gaussians can exceed 1.0 (when R, G, and B are all high),
        // we must ensure the final output is clamped to the required [0.0, 1.0] range.
        intensity = std::max(0.0, std::min(1.0, intensity));

        // Store the result for every single nanometer (1nm granularity).
        spectrum.push_back(intensity);
    }
    return spectrum;
}

std::vector<double> spectrt_it(const QImage& currentFrame, size_t w, size_t h)
{
    if(mock_.empty())
        mock_ = ::create_mock_image(w,h);
    double r_norm, g_norm, b_norm;
    std::tie(r_norm, g_norm, b_norm) = calculate_average_normalized_rgb(currentFrame);
    std::vector<double> spectrum_data = calculate_spectrum(r_norm, g_norm, b_norm);
    return spectrum_data;
}
