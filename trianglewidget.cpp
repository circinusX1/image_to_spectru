#include "trianglewidget.h"
#include <QOpenGLShader>
#include <QDebug>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QComboBox>
#include <cmath>
#include <QString>
#include "dialog.h"
#include "ui_dialog.h"
#include "GL/glu.h"
#include "rgb_to_spectrum.h"

TriangleWidget::TriangleWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setWindowTitle("OpenGL Sine Wave Plotter (with Webcam)");
    //resize(800, 600);

    // Setup timer for continuous updates to drive the video playback
    m_timer = new QTimer(this);
    // Connect timer timeout to QWidget::update() which forces paintGL()
    connect(m_timer, &QTimer::timeout, this, QOverload<>::of(&TriangleWidget::update));
    m_timer->start(33); // ~30 FPS

}

TriangleWidget::~TriangleWidget()
{


    if(m_captureSession)
    {
        m_captureSession->disconnect();
        delete m_captureSession;
    }
    if(m_sink)
    {
        m_sink->disconnect();
        delete m_sink;
    }
    if(m_camera)
    {
        m_camera->stop();
        delete m_camera;
    }
    makeCurrent();
    doneCurrent();
}

void TriangleWidget::processFrame(const QVideoFrame &frame)
{
    if (!frame.isValid()) {
        return;
    }

    // Use toImage() for a robust conversion to QImage format.
    // This handles the frame mapping and format conversion internally in modern Qt.
    m_currentFrame = frame.toImage().convertToFormat(QImage::Format_RGB888);

    // calculateHistogram();
    Dialog* pp = (Dialog*)this->parent();
    if(pp->uii()->widget_2)
    {
        pp->uii()->widget->setFrame(m_currentFrame);
    }
    // Mark the texture as dirty so it gets re-uploaded in paintGL
    m_textureDirty = true;
}

void TriangleWidget::setFrame(QImage& frame)
{
    //m_currentFrame = frame;
    spectrt_it_.clear();
    spectrt_it_ = ::spectrt_it(frame,  frame.width(),  frame.width());
    //calculateSpectrum();
}

void TriangleWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // Dark background if camera fails

    glEnable(GL_TEXTURE_2D);


}

void TriangleWidget::showEvent(QShowEvent* event)
{
    QOpenGLWidget::showEvent(event);
    if(cam_initialised_==false)
    {
        Dialog* pp = (Dialog*)this->parent();
        if(pp->uii()->widget_2==this)
        {
            setupCamera();
            cam_initialised_ = true;
        }
        else
            cam_initialised_ = true;
    }

}

void TriangleWidget::resizeGL(int w, int h)
{
    if (h == 0) h = 1;

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(m_X_MIN, m_X_MAX, m_Y_MIN, m_Y_MAX, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void TriangleWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    Dialog* pp = (Dialog*)this->parent();
    if(pp->uii()->widget_2==this)
    {
        drawCameraFeed();
    }
    else
    {
        drawGridLines();
        drawAxes();
        drawSpectrum2();
        //drawSpectrum();
        drawLabels();
    }
}

void TriangleWidget::drawGridLines()
{
    glColor4f(1.0f, 1.0f, 1.0f, 0.4f);
    glLineWidth(1.0f);

    // Horizontal Grid Line (y = 0.5 normalized intensity)
    glBegin(GL_LINES);
    glVertex2d(m_X_MIN, 0.5);
    glVertex2d(m_X_MAX, 0.5);
    glEnd();

    // Vertical Grid Lines at key visible spectrum points (nm)
    glBegin(GL_LINES);
    // Violet/Blue boundary
    glVertex2d(450.0, m_Y_MAX);
    glVertex2d(450.0, m_Y_MIN);
    // Cyan/Green boundary
    glVertex2d(500.0, m_Y_MAX);
    glVertex2d(500.0, m_Y_MIN);
    // Yellow/Orange boundary
    glVertex2d(580.0, m_Y_MAX);
    glVertex2d(580.0, m_Y_MIN);
    // Orange/Red boundary
    glVertex2d(620.0, m_Y_MAX);
    glVertex2d(620.0, m_Y_MIN);
    glEnd();
}

/**
     * @brief Draws the main X (y=0) and Y (x=0) axes.
     */
void TriangleWidget::drawAxes()
{
    // Set color for axes (bright white)
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);

    // X-Axis (y=0, Wavelength line)
    glBegin(GL_LINES);
    glVertex2d(m_X_MIN, 0.0);
    glVertex2d(m_X_MAX, 0.0);
    glEnd();

    // Y-Axis (x=0, Intensity line)
    glBegin(GL_LINES);
    glVertex2d(M_WAVELENGTH_MIN, m_Y_MAX);
    glVertex2d(M_WAVELENGTH_MIN, m_Y_MIN);
    glEnd();
}

/**
     * @brief Draws the sine wave from 0 to 2*PI (0 to 360 degrees).
     */
void TriangleWidget::drawSineWave()
{

}

/**
     * @brief Draws text labels (numbers) on the axes using QPainter.
     */
void TriangleWidget::drawLabels()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(255, 255, 255)); // White text for visibility

    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);

    // X-Axis Labels (Wavelength in nm)
    drawTextLabel(painter, 315.0, 0.0, "315 nm (UV)", Qt::AlignLeft | Qt::AlignTop);
    drawTextLabel(painter, 450.0, 0.0, "450 nm (Blue)", Qt::AlignHCenter | Qt::AlignTop);
    drawTextLabel(painter, 550.0, 0.0, "550 nm (Green)", Qt::AlignHCenter | Qt::AlignTop);
    drawTextLabel(painter, 650.0, 0.0, "650 nm (Red)", Qt::AlignHCenter | Qt::AlignTop);
    drawTextLabel(painter, 800.0, 0.0, "800 nm (IR)", Qt::AlignRight | Qt::AlignTop);

    drawTextLabel(painter, 550.0, -0.1, "Wavelength (nm)", Qt::AlignHCenter | Qt::AlignTop);


    // Y-Axis Labels (Normalized Intensity)
    drawTextLabel(painter, 315.0, 1.0, "Max Intensity", Qt::AlignLeft | Qt::AlignBottom);
    drawTextLabel(painter, 315.0, 0.5, "50%", Qt::AlignLeft | Qt::AlignVCenter);
    drawTextLabel(painter, 315.0, 0.0, "0", Qt::AlignLeft | Qt::AlignTop);

    // Legend
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(20, 20, "Estimated Spectral Distribution (Average Frame Color)");

    painter.end();
}

/**
     * @brief Helper function to draw text at a specific OpenGL coordinate.
     */
void TriangleWidget::drawTextLabel(QPainter& painter, double x_gl, double y_gl, const QString& text, Qt::Alignment align)
{
    double normalizedX = (x_gl - m_X_MIN) / (m_X_MAX - m_X_MIN);
    // OpenGL Y (up) -> Window Y (down), so invert the normalized Y
    double normalizedY = 1.0 - ((y_gl - m_Y_MIN) / (m_Y_MAX - m_Y_MIN));

    // 2. Map normalized coordinates to pixel coordinates
    QPointF windowPoint(
        normalizedX * width(),
        normalizedY * height()
        );

    // Define a rectangle around the point for alignment
    const int padding = 5;
    QRectF rect;

    if (align & Qt::AlignHCenter) {
        rect.setLeft(windowPoint.x() - 100);
        rect.setWidth(200);
    } else if (align & Qt::AlignLeft) {
        rect.setLeft(windowPoint.x() + padding);
        rect.setWidth(100);
    } else if (align & Qt::AlignRight) {
        rect.setLeft(windowPoint.x() - 100 - padding);
        rect.setWidth(100);
    } else {
        rect.setLeft(windowPoint.x() - 50); // Default width
        rect.setWidth(100);
    }

    if (align & Qt::AlignTop) {
        rect.setTop(windowPoint.y() + padding);
        rect.setHeight(20);
    } else if (align & Qt::AlignBottom) {
        rect.setTop(windowPoint.y() - 20 - padding);
        rect.setHeight(20);
    } else {
        rect.setTop(windowPoint.y() - 10);
        rect.setHeight(20);
    }

    painter.drawText(rect, align, text);
}


void TriangleWidget::setupCamera()
{
    // Check for available cameras using the modern QMediaDevices class (Qt 6+)
    QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        qWarning("No cameras found!");
        return;
    }

    Dialog* pp = (Dialog*)this->parent();
    QComboBox *pc = pp->uii()->comboBox;
    pc->clear();
    int index = 0;
    for(const auto& cams : cameras)
    {
        QString scam=QString::number(index++);
        scam += " ";
        scam += cams.description();
        pc->addItem(scam);
    }

}

void TriangleWidget::combo_changed(const QString& s)
{
    int index = ::atoi(s.toStdString().c_str());
    QList<QCameraDevice> cameras = QMediaDevices::videoInputs();

    if(m_captureSession)
    {
        m_captureSession->disconnect();
        delete m_captureSession;
    }
    if(m_sink)
    {
        m_sink->disconnect();
        delete m_sink;
    }
    if(m_camera)
    {
        m_camera->stop();
        delete m_camera;
    }
    // 1. Initialize Camera with the first available device
    m_camera = new QCamera(cameras.at(index), this);

    // 2. Initialize VideoSink (The replacement for QVideoProbe)
    m_sink = new QVideoSink(this);

    // 3. Initialize Capture Session to connect Camera to Sink
    m_captureSession = new QMediaCaptureSession(this);
    m_captureSession->setCamera(m_camera);
    m_captureSession->setVideoSink(m_sink);

    // 4. Connect the signal from the sink to the processing slot
    // QVideoSink uses videoFrameChanged(QVideoFrame)
    connect(m_sink, &QVideoSink::videoFrameChanged, this, &TriangleWidget::processFrame);

    m_camera->start();
    qDebug("Camera started successfully using QVideoSink.");
}



void TriangleWidget::drawCameraFeed()
{
    if (m_currentFrame.isNull()) {
        // Draw a solid color if no frame is available yet
        return;
    }

    // If the texture hasn't been created or needs re-uploading
    if (!m_videoTexture || m_textureDirty) {
        makeCurrent(); // Ensure we are in the GL context for texture operations
        delete m_videoTexture; // Delete old texture if it exists

        // Create a new texture from the QImage
        // Note: QImage will often be upside down when used directly in OpenGL.
        // We use the texture coordinates to flip it during drawing.
        m_videoTexture = new QOpenGLTexture(m_currentFrame, QOpenGLTexture::DontGenerateMipMaps);
        m_videoTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        m_videoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);

        m_textureDirty = false;
    }

    if (m_videoTexture) {
        // Bind the texture and draw a quad covering the entire viewport
        m_videoTexture->bind();

        // Set up a new projection matrix for the background texture to cover the viewport
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        // Use simple pixel mapping for the background (0, 0) to (width, height)
        gluOrtho2D(0, width(), 0, height());
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glColor3f(1.0f, 1.0f, 1.0f); // Ensure the texture is drawn with full white color

        // Draw the quad, mapping the texture to the screen space.
        // Texture coordinates are flipped vertically (0, 1) and (1, 0) to compensate for
        // the difference between QImage (origin top-left) and OpenGL (origin bottom-left).
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(0, 0); // Bottom-left (Map to texture top-left)
        glTexCoord2f(1, 1); glVertex2f(width(), 0); // Bottom-right (Map to texture top-right)
        glTexCoord2f(1, 0); glVertex2f(width(), height()); // Top-right (Map to texture bottom-right)
        glTexCoord2f(0, 0); glVertex2f(0, height()); // Top-left (Map to texture bottom-left)
        glEnd();

        m_videoTexture->release();

        // Restore the projection and modelview matrices for the sine wave plot
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }
}

static int MAX_FREQ = 1;

void TriangleWidget::calculateHistogram()
{
    // Reset histogram and max frequency
    m_maxFrequency = 1;

    std::fill(m_histogramR, m_histogramR + 256, 0);
    std::fill(m_histogramG, m_histogramG + 256, 0);
    std::fill(m_histogramB, m_histogramB + 256, 0);

    if (m_currentFrame.isNull()) return;

    // Iterate through all pixels to count color intensities
    for (int y = 0; y < m_currentFrame.height(); ++y) {
        for (int x = 0; x < m_currentFrame.width(); ++x) {
            // Get the pixel color
            QRgb rgb = m_currentFrame.pixel(x, y);

            // Extract R, G, B values (0-255)
            int r = qRed(rgb);
            int g = qGreen(rgb);
            int b = qBlue(rgb);

            // Increment counts
            m_histogramR[r]++;
            m_histogramG[g]++;
            m_histogramB[b]++;
        }
    }

    // Find the overall maximum frequency for normalization
    for (int i = 0; i < 256; ++i) {
        m_maxFrequency = std::max(m_maxFrequency, m_histogramR[i]);
        m_maxFrequency = std::max(m_maxFrequency, m_histogramG[i]);
        m_maxFrequency = std::max(m_maxFrequency, m_histogramB[i]);
    }
    qDebug("Max freq = "  + m_maxFrequency );
    MAX_FREQ = m_maxFrequency;
}

void TriangleWidget::drawHistogram()
{
    m_maxFrequency = MAX_FREQ;
    MAX_FREQ = 1;
    if (m_maxFrequency < 2) {
        // Cannot draw normalized plot if max frequency is 0 or 1
        return;
    }

    // Drawing the Red Histogram
    glColor3f(1.0f, 0.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < 256; ++i) {
        double x_intensity = (double)i;
        // Normalize frequency count to fit the Y-axis (0 to 1.0)
        double y_normalized_freq = (double)m_histogramR[i] / m_maxFrequency;
        glVertex2d(x_intensity, y_normalized_freq);
    }
    glEnd();

    // Drawing the Green Histogram
    glColor3f(0.0f, 1.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < 256; ++i) {
        double x_intensity = (double)i;
        double y_normalized_freq = (double)m_histogramG[i] / m_maxFrequency;
        glVertex2d(x_intensity, y_normalized_freq);
    }
    glEnd();

    // Drawing the Blue Histogram
    glColor3f(0.0f, 0.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < 256; ++i) {
        double x_intensity = (double)i;
        double y_normalized_freq = (double)m_histogramB[i] / m_maxFrequency;
        glVertex2d(x_intensity, y_normalized_freq);
    }
    glEnd();
}

QColor TriangleWidget::wavelengthToRGB(double lambda)
{
    // Source: Based on simple approximations of CIE curves for visualization (380-750nm)

    if (lambda < 380 || lambda > 750) {
        if (lambda < 380)
           return QColor::fromRgb(30, 30, 90, 100);
        return QColor::fromRgb(90, 30, 30, 100);
    }

    // Initialize R, G, B
    double R = 0.0, G = 0.0, B = 0.0;

    if (lambda >= 380.0 && lambda < 440.0) {
        R = -(lambda - 440.0) / (440.0 - 380.0);
        B = 1.0;
    } else if (lambda >= 440.0 && lambda < 490.0) {
        G = (lambda - 440.0) / (490.0 - 440.0);
        B = 1.0;
    } else if (lambda >= 490.0 && lambda < 510.0) {
        G = 1.0;
        B = -(lambda - 510.0) / (510.0 - 490.0);
    } else if (lambda >= 510.0 && lambda < 580.0) {
        R = (lambda - 510.0) / (580.0 - 510.0);
        G = 1.0;
    } else if (lambda >= 580.0 && lambda < 645.0) {
        R = 1.0;
        G = -(lambda - 645.0) / (645.0 - 580.0);
    } else if (lambda >= 645.0 && lambda <= 750.0) {
        R = 1.0;
    }

    // Intensity correction (adjusts brightness at the ends of the spectrum)
    double S = 1.0;
    if (lambda > 700.0) S = 0.3 + 0.7 * (750.0 - lambda) / (750.0 - 700.0);
    else if (lambda < 420.0) S = 0.3 + 0.7 * (lambda - 380.0) / (420.0 - 380.0);

    R = std::pow(R * S, 0.8) * 255;
    G = std::pow(G * S, 0.8) * 255;
    B = std::pow(B * S, 0.8) * 255;

    return QColor::fromRgb(std::clamp((int)R, 0, 255),
                           std::clamp((int)G, 0, 255),
                           std::clamp((int)B, 0, 255));
}

void TriangleWidget::calculateSpectrum()
{

    // Reset spectrum
    std::fill(m_spectrum, m_spectrum + M_WAVELENGTH_BINS, 0.0f);
    m_maxSpectrumIntensity = 0.01f; // Initialize slightly above zero

    if (m_currentFrame.isNull()) return;

    // 1. Calculate Average RGB
    long long totalR = 0, totalG = 0, totalB = 0;
    long long pixelCount = m_currentFrame.width() * m_currentFrame.height();
    if (pixelCount == 0) return;

    for (int y = 0; y < m_currentFrame.height(); ++y) {
        for (int x = 0; x < m_currentFrame.width(); ++x) {
            QRgb rgb = m_currentFrame.pixel(x, y);
            // Use normalized values (0.0 to 1.0)
            totalR += qRed(rgb);
            totalG += qGreen(rgb);
            totalB += qBlue(rgb);

        }
    }

    // Normalized average intensity (0.0 to 1.0)
    float avgR = (float)(totalR / pixelCount) / 255.0f;
    float avgG = (float)(totalG / pixelCount) / 255.0f;
    float avgB = (float)(totalB / pixelCount) / 255.0f;

    // 2. Define simplified spectral response centers and bandwidth (in nm)
    // This is a rough estimation for visualization
    const double R_CENTER = 620.0;
    const double G_CENTER = 550.0;
    const double B_CENTER = 470.0;
    const double BANDWIDTH = 80.0; // Half-width of the triangular distribution

    memset(m_spectrum,0,sizeof(m_spectrum));

    // 3. Estimate spectral contribution for each wavelength bin
    for (int i = 0; i < M_WAVELENGTH_BINS; ++i) {
        double lambda = M_WAVELENGTH_MIN + i * M_WAVELENGTH_STEP;

        // Calculates the intensity contribution of a single color channel (R, G, or B)
        // for the current wavelength (lambda) using a triangular kernel distribution.
        auto getContribution = [&](double center, float avgIntensity) -> float {
            double distance = std::abs(lambda - center);
            if (distance < BANDWIDTH) {
                // Linear decay from peak (1.0) at center to 0.0 at BANDWIDTH away
                return avgIntensity * (1.0f - (distance / BANDWIDTH));
            }
            return 0.0f;
        };

        float r_contrib = getContribution(R_CENTER, avgR);
        float g_contrib = getContribution(G_CENTER, avgG);
        float b_contrib = getContribution(B_CENTER, avgB);

        // Sum contributions (Estimated SPD)
        m_spectrum[i] = r_contrib + g_contrib + b_contrib;

        // Update maximum intensity for normalization
        m_maxSpectrumIntensity = std::max(m_maxSpectrumIntensity, m_spectrum[i]);
    }

}

void TriangleWidget::drawSpectrum()
{
    // If max intensity is near zero, the normalization will fail or be too noisy.
    if (m_maxSpectrumIntensity < 0.01f) return;

    glLineWidth(2.0f); // Thicker line for better visibility of the spectrum gradient
    glBegin(GL_LINE_STRIP);

    // Normalization factor to scale the estimated intensity (Y-axis) from 0 to 1.0
    float normalizationFactor = 1.0f / m_maxSpectrumIntensity;

    for (int i = 0; i < M_WAVELENGTH_BINS; ++i)
    {
        double lambda = M_WAVELENGTH_MIN + i * M_WAVELENGTH_STEP;
        double x_gl = lambda;

        // Normalize the spectral intensity to the Y-axis range (0 to 1.0)
        double y_normalized_intensity = m_spectrum[i] * normalizationFactor;

        // Set color based on the current wavelength (spectrum effect)
        QColor color = wavelengthToRGB(lambda);
        glColor3f((float)color.red() / 255.0f, (float)color.green() / 255.0f, (float)color.blue() / 255.0f);
        double p = y_normalized_intensity;///maxy_scale_;
        if(p>0.1)
        {
            glVertex2d(x_gl, p );
        }
        if(y_normalized_intensity > maxy_scale_)
            maxy_scale_ = y_normalized_intensity;
    }
    glEnd();

    // Reset color to white for drawing subsequent elements
    glColor3f(1.0f, 1.0f, 1.0f);
}

void TriangleWidget::drawSpectrum2()
{
    glLineWidth(6.0f);
    glBegin(GL_LINE_STRIP);

    for (int i = 0; i < spectrt_it_.size(); ++i)
    {
        double lambda = M_WAVELENGTH_MIN + i;
        QColor color = wavelengthToRGB(lambda);

        glColor3f((float)color.red() / 255.0f, (float)color.green() / 255.0f, (float)color.blue() / 255.0f);
        //glVertex2d((double)i+m_X_MIN, 0);
        glVertex2d((double)i+m_X_MIN, spectrt_it_[i]*2);
    }
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
}
