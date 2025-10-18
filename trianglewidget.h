#ifndef TRIANGLEWIDGET_H
#define TRIANGLEWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QTimer>
// Qt Multimedia headers for camera access (Updated for QVideoSink)
#include <QCamera>
#include <QVideoSink> // Replaces QVideoProbe in modern Qt
#include <QVideoFrame>
#include <QImage>
#include <QOpenGLTexture>
#include <QMediaCaptureSession> // Required to link QCamera to QVideoSink
#include <QMediaDevices> // Replaces QCameraInfo in modern Qt6

constexpr double PI = 3.14159265358979323846;
constexpr int MAX_POINTS = 300;

class TriangleWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    TriangleWidget(QWidget *parent = nullptr);
    ~TriangleWidget();

    void combo_changed(const QString& s);

    const QImage& frame()const{return m_currentFrame;}

    void setFrame(QImage& frame);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // --------------------- 1. Draw Grid Lines ---------------------
    void drawGridLines();
    void drawAxes();
    void drawSineWave();
    void drawLabels();
    void drawTextLabel(QPainter& painter, double x_gl, double y_gl, const QString& text, Qt::Alignment align);

    void processFrame(const QVideoFrame &frame);
    void setupCamera();
    void drawCameraFeed();
    void showEvent(QShowEvent* event);

    void calculateHistogram();
    void drawHistogram();
    void drawSpectrum();
    void drawSpectrum2();
    void calculateSpectrum();

    QColor wavelengthToRGB(double lambda);


private:
    // Projection limits for the Histogram Overlay (0-255 intensity, 0-1 normalized freq)
    const double m_X_MIN = 300.0; // Margin left
    const double m_X_MAX = 815.0; // Margin right
    const double m_Y_MIN = -0.1;  // Margin below 0
    const double m_Y_MAX = 1.1;   // 0 to 1 (normalized) + margin top

    int m_histogramR[256] = {0};
    int m_histogramG[256] = {0};
    int m_histogramB[256] = {0};
    int gap[128] = {0};
    int m_maxFrequency = 1; // Used for Y-axis scaling (dynamic max cou

    QCamera *m_camera = nullptr;
    QVideoSink *m_sink = nullptr;
    QMediaCaptureSession *m_captureSession = nullptr; // Links camera to sink
    QOpenGLTexture *m_videoTexture = nullptr;
    QImage m_currentFrame;
    bool m_textureDirty = false;
    QTimer *m_timer = nullptr;
    bool cam_initialised_=false;

    const double M_WAVELENGTH_MIN = 315.0; // nm (Near UV)
    const double M_WAVELENGTH_MAX = 800.0; // nm (Near IR)
    const int M_WAVELENGTH_BINS = MAX_POINTS; // Number of points to plot
    const double M_WAVELENGTH_STEP = (M_WAVELENGTH_MAX - M_WAVELENGTH_MIN) / (M_WAVELENGTH_BINS - 1);

    float m_spectrum[MAX_POINTS]; // Stores estimated normalized intensity for each bin
    float m_maxSpectrumIntensity = 0.01f; // Used for Y-axis scaling (dynamic max value in array
    double maxy_scale_ = 0.0;
    std::vector<double> spectrt_it_;//(const QImage& currentFrame, size_t w, size_t h)
};

#endif // TRIANGLEWIDGET_H
