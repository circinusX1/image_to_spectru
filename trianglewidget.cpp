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

// --- Spectral Locus Data (using 10nm steps) ---
const QVector<QVector<double>> SPECTRAL_LOCUS_DATA = {
    // {Wavelength, x, y}
    {400, 0.1741, 0.0075}, {410, 0.1691, 0.0016}, {420, 0.1714, 0.0039},
    {430, 0.1804, 0.0084}, {440, 0.1912, 0.0125}, {450, 0.2033, 0.0163},
    {460, 0.2185, 0.0226}, {470, 0.2401, 0.0381}, {480, 0.2655, 0.0632},
    {490, 0.2941, 0.1011}, {500, 0.3281, 0.1702}, {510, 0.3599, 0.2709},
    {520, 0.3807, 0.3875}, {530, 0.3946, 0.5100}, {540, 0.4087, 0.6133},
    {550, 0.4210, 0.7016}, {560, 0.4326, 0.7770}, {570, 0.4430, 0.8351},
    {580, 0.4549, 0.8756}, {590, 0.4705, 0.8984}, {600, 0.4916, 0.8967},
    {610, 0.5168, 0.8727}, {620, 0.5484, 0.8341}, {630, 0.5828, 0.7844},
    {640, 0.6167, 0.7314}, {650, 0.6482, 0.6800}, {660, 0.6750, 0.6320},
    {670, 0.6973, 0.5880}, {680, 0.7134, 0.5500}, {690, 0.7258, 0.5218},
    {700, 0.7347, 0.4997}
};
const int NUM_SPECTRAL_POINTS = SPECTRAL_LOCUS_DATA.size();


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

double TriangleWidget::srgb_to_linear(int c)
{
    double val = c / 255.0;
    if (val <= 0.04045) {
        return val / 12.92;
    } else {
        return std::pow((val + 0.055) / 1.055, 2.4);
    }
}

double TriangleWidget::interpolate_wavelength(double x, double y, double xw, double yw,
                              const QVector<QVector<double>>& locus_data, int num_points)
{
    // Check for achromatic color
    double dist_sq = (x - xw) * (x - xw) + (y - yw) * (y - yw);
    if (dist_sq < 1e-6) return 550.0;

    // Line A (White to Color) parameters
    double dx_A = x - xw;
    double dy_A = y - yw;

    // Iterate through the segments of the spectral locus
    for (int i = 0; i < num_points - 1; ++i) {
        double x1 = locus_data[i][1];
        double y1 = locus_data[i][2];
        double wavel1 = locus_data[i][0];

        double x2 = locus_data[i+1][1];
        double y2 = locus_data[i+1][2];
        double wavel2 = locus_data[i+1][0];

        // Line B (Spectral Segment) parameters
        double dx_B = x2 - x1;
        double dy_B = y2 - y1;
        double det = dx_A * dy_B - dy_A * dx_B; // Determinant

        if (std::abs(det) < 1e-9) continue; // Parallel lines

        // Solve for 's' (parameter along Line B) and 't' (parameter along Line A)
        double s_numerator = dx_A * (y1 - yw) - dy_A * (x1 - xw);
        double s = s_numerator / det;

        double t_numerator = dx_B * (y1 - yw) - dy_B * (x1 - xw);
        double t = t_numerator / det;

        // Intersection found if:
        // 1. Intersection is on the spectral segment (0 <= s <= 1)
        // 2. Color point (x, y) is between white point and intersection (t >= 0)
        if (s >= 0.0 && s <= 1.0 && t >= 0.0) {
            // Interpolate wavelength
            return wavel1 + s * (wavel2 - wavel1);
        }
    }

    // Handle non-spectral (purple line) colors
    return 400.0; // Default to lowest visible end for non-spectral
}

double TriangleWidget::rgb_to_approx_wavelength(int r_srgb, int g_srgb, int b_srgb)
{
    // 1. Convert sRGB to linear RGB
    double r = srgb_to_linear(r_srgb);
    double g = srgb_to_linear(g_srgb);
    double b = srgb_to_linear(b_srgb);

    // 2. Apply the sRGB to CIE XYZ (D65 white point) matrix:
    double X = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
    double Y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
    double Z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;

    if (X + Y + Z < 1e-6) return 550.0;

    // 3. Normalize to get CIE xy Chromaticity Coordinates
    double x = X / (X + Y + Z);
    double y = Y / (X + Y + Z);

    // D65 White Point (xw, yw) for sRGB
    const double xw = 0.3127;
    const double yw = 0.3290;

    // 4. Calculate dominant wavelength via spectral locus interpolation
    return interpolate_wavelength(x, y, xw, yw, SPECTRAL_LOCUS_DATA, NUM_SPECTRAL_POINTS);
}

QColor TriangleWidget::wavelength_to_rgb(double wavel)
{
    // This is a simplified function to VISUALIZE the wavelength,
    // not a rigorous colorimetric conversion.
    int R=0, G=0, B=0;

    if (wavel >= 645 && wavel <= 780) { // Red
        R = 255;
    } else if (wavel >= 580 && wavel < 645) { // Orange/Yellow-Red
        R = 255;
        G = (int)(255 * (wavel - 580) / 65);
    } else if (wavel >= 510 && wavel < 580) { // Green
        R = (int)(255 * (580 - wavel) / 70);
        G = 255;
    } else if (wavel >= 470 && wavel < 510) { // Cyan/Blue-Green
        G = 255;
        B = (int)(255 * (wavel - 470) / 40);
    } else if (wavel >= 400 && wavel < 470) { // Blue/Violet
        G = (int)(255 * (470 - wavel) / 70);
        B = 255;
    } else {
        return QColor(0, 0, 0, 0); // Outside range or non-spectral
    }

    // Apply scaling to simulate intensity drop at spectrum ends
    double factor = 1.0;
    if (wavel < 420) factor = 0.3 + 0.7 * (wavel - 400) / 20.0;
    else if (wavel > 680) factor = 0.3 + 0.7 * (700 - wavel) / 20.0;

    return QColor((int)(R * factor), (int)(G * factor), (int)(B * factor));
}


void TriangleWidget::calculateWavelengthHistogram(const QImage& tempImage)
{
    spectrt_it_.reserve(NUM_BINS);
    spectrt_it_.assign(NUM_BINS,0);
    double bin_width = (MAX_WAVEL - MIN_WAVEL) / NUM_BINS;

    // Ensure the image is in a convenient format for pixel access

    int STEP=8;
    double smax = 0;
    for (int y = 0; y < tempImage.height(); y+=STEP)
    {
        for (int x = 0; x < tempImage.width(); x+=STEP)
        {
            QRgb pixel = tempImage.pixel(x, y);
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            double wavelength = rgb_to_approx_wavelength(r, g, b);

            if (wavelength >= MIN_WAVEL && wavelength < MAX_WAVEL) {
                double relative_wavel = wavelength - MIN_WAVEL;
                int bin_index = static_cast<int>(relative_wavel / bin_width);

                if (bin_index >= NUM_BINS) bin_index = NUM_BINS - 1;

                spectrt_it_[bin_index]++;
                smax = std::max(smax, spectrt_it_[bin_index]);
            }
        }
    }

    for(auto& a : spectrt_it_)
    {
        a /= smax;
    }
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
        pp->uii()->widget->calculateWavelengthHistogram(m_currentFrame);
    }
    // Mark the texture as dirty so it gets re-uploaded in paintGL
    m_textureDirty = true;
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
    glVertex2d(MIN_WAVEL, m_Y_MAX);
    glVertex2d(MIN_WAVEL, m_Y_MIN);
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

void TriangleWidget::drawSpectrum2()
{
    if(spectrt_it_.size())
    {
        glPushMatrix();
        glLoadIdentity();
        float sx = (float)this->size().width()/(float)NUM_BINS;
        glScalef(sx,1,1);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);

        spectrt_it_[int(smax_)]=1.0;
        for (int i = 0; i < spectrt_it_.size(); ++i)
        {
            double lambda = MIN_WAVEL + i*10;
            QColor color = wavelengthToRGB(lambda);

            glColor3f((float)color.red() / 255.0f, (float)color.green() / 255.0f, (float)color.blue() / 255.0f);
            //glVertex2d((double)i+m_X_MIN, 0);
            glVertex2d((double)i, spectrt_it_[i]);
        }
        glEnd();
        glColor3f(1.0f, 1.0f, 1.0f);
        glPopMatrix();
    }
}
