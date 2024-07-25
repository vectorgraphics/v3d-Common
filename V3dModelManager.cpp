#include "V3dModelManager.h"

#include <iostream>

#include <QApplication>
#include <QBoxLayout>

#include "Utility/EventFilter.h"
#include "Utility/ProtectedFunctionCaller.h"

V3dModelManager::V3dModelManager() {
    m_PageView = GetPageViewWidget();

    m_EventFilter = new EventFilter(m_PageView, this);
    m_PageView->viewport()->installEventFilter(m_EventFilter);
}

void V3dModelManager::AddModel(V3dModel model, size_t pageNumber) {
    m_Models.resize(pageNumber + 1);
    m_Models[pageNumber].emplace_back(std::move(model));

    m_Models[pageNumber].back().initProjection();
}

QImage V3dModelManager::RenderModel(size_t pageNumber, size_t modelIndex, int width, int height) {
    std::vector<float> vertices = m_Models[pageNumber][modelIndex].file->vertices;
    std::vector<unsigned int> indices = m_Models[pageNumber][modelIndex].file->indices;

    VkSubresourceLayout imageSubresourceLayout;

    // Model
    glm::mat4 model = glm::mat4{ 1.0f };

    // Projection
    m_Models[pageNumber][modelIndex].setProjection(glm::vec2{ width, height });

	glm::mat4 mvp = m_Models[pageNumber][modelIndex].projectionMatrix * m_Models[pageNumber][modelIndex].viewMatrix * model;

    unsigned char* imageData = m_HeadlessRenderer->render(width, height, &imageSubresourceLayout, vertices, indices, mvp);

    unsigned char* imgDatatmp = imageData;

    size_t finalImageSize = width * height * 4;

    std::vector<unsigned char> vectorData;
    vectorData.reserve(finalImageSize);

    int x = 0;
    unsigned int* oldRow;
    bool done = false;
    for (int32_t y = 0; y < height; y++) {
        unsigned int *row = (unsigned int*)imgDatatmp;
        for (int32_t x = 0; x < width; x++) {
            unsigned char* charRow = (unsigned char*)row;
            vectorData.push_back(charRow[2]);
            vectorData.push_back(charRow[1]);
            vectorData.push_back(charRow[0]);
            vectorData.push_back(charRow[3]);

            row++;
        }
        imgDatatmp += imageSubresourceLayout.rowPitch;
    }
    delete imageData;

    QImage image{ vectorData.data(), width, height, QImage::Format_ARGB32 };

    image = image.mirrored(false, true);

    return image;
}

V3dModel& V3dModelManager::Model(size_t pageNumber, size_t modelIndex) {
    return m_Models[pageNumber][modelIndex];
}

std::vector<V3dModel>& V3dModelManager::Models(size_t pageNumber) {
    return m_Models[pageNumber];
}

glm::vec2 V3dModelManager::GetPageSize(size_t pageNumber) {
    return glm::vec2{ m_Models[pageNumber][0].file->headerInfo.canvasWidth, m_Models[pageNumber][0].file->headerInfo.canvasHeight };
}

bool V3dModelManager::mouseMoveEvent(QMouseEvent* event) {
    m_MousePosition.x = event->globalPos().x();
    m_MousePosition.y = event->globalPos().y();

    if (m_MouseDown == false) {
        return true;
    }

    glm::vec2 normalizedMousePosition{ };
    glm::vec2 lastNormalizedMousePosition{ };

    glm::vec2 pageViewSize = { m_PageView->width(), m_PageView->height() };
    glm::vec2 halfPageViewDimensions = pageViewSize / 2.0f;

    normalizedMousePosition.x = (float)(m_MousePosition.x - halfPageViewDimensions.x) / halfPageViewDimensions.x;
    normalizedMousePosition.y = (float)(m_MousePosition.y - halfPageViewDimensions.y) / halfPageViewDimensions.y;

    lastNormalizedMousePosition.x = (float)(m_LastMousePosition.x - halfPageViewDimensions.x) / halfPageViewDimensions.x;
    lastNormalizedMousePosition.y = (float)(m_LastMousePosition.y - halfPageViewDimensions.y) / halfPageViewDimensions.y;

    switch (m_DragMode) {
        case DragMode::SHIFT: {
            m_Models[0][0].dragModeShift(normalizedMousePosition, lastNormalizedMousePosition, pageViewSize);
            break;
        }
        case DragMode::ZOOM: {
            m_Models[0][0].dragModeZoom(normalizedMousePosition, lastNormalizedMousePosition, pageViewSize);
            break;
        }
        case DragMode::PAN: {
            m_Models[0][0].dragModePan(normalizedMousePosition, lastNormalizedMousePosition, pageViewSize);
            break;
        }
        case DragMode::ROTATE: {
            m_Models[0][0].dragModeRotate(normalizedMousePosition, lastNormalizedMousePosition, pageViewSize);
            break;
        }
    }

    m_LastMousePosition.x = m_MousePosition.x;
    m_LastMousePosition.y = m_MousePosition.y;

    m_Models[0][0].setProjection(GetPageSize(0)); // TODO
    requestPixmapRefresh();

    return true;
}

bool V3dModelManager::mouseButtonPressEvent(QMouseEvent* event) {
    if (m_MouseDown != false) {
        return true;
    }

    m_LastMousePosition.x = m_MousePosition.x;
    m_LastMousePosition.y = m_MousePosition.y;

    m_MouseDown = true;

    bool controlKey = event->modifiers() & Qt::ControlModifier;
    bool shiftKey = event->modifiers() & Qt::ShiftModifier;
    bool altKey = event->modifiers() & Qt::AltModifier;

    if (controlKey && !shiftKey && !altKey) {
        m_DragMode = DragMode::SHIFT;
    } else if (!controlKey && shiftKey && !altKey) {
        m_DragMode = DragMode::ZOOM;
    } else if (!controlKey && !shiftKey && altKey) {
        m_DragMode = DragMode::PAN;
    } else {
        m_DragMode = DragMode::ROTATE;
    }

    return true;
}

bool V3dModelManager::mouseButtonReleaseEvent(QMouseEvent* event) {
    if (m_MouseDown != true) {
        return true;
    }

    m_MouseDown = false;

    return true;
}

QAbstractScrollArea* V3dModelManager::GetPageViewWidget() {
    QAbstractScrollArea* pageView = nullptr;

    for (QWidget* widget : QApplication::allWidgets()) {
        bool hasScrollArea = false;
        bool parentIsWidget = false;
        bool has8Children = false;
        bool has1QVboxChild = false;
        bool has5QFrameChild = false;

        QAbstractScrollArea* scrollArea = dynamic_cast<QAbstractScrollArea*>(widget);

        if (scrollArea != nullptr) {
            hasScrollArea = true;
        } else {
            continue;
        }

        QWidget* parent = dynamic_cast<QWidget*>(widget->parent());

        if (parent != nullptr) {
            parentIsWidget = true;
        } else {
            continue;
        }

        if (parent->children().size() == 9) {
            has8Children = true;
        } else {
            continue;
        }

        int QBoxLayoutCount = 0;
        for (auto child : parent->children()) {
            QBoxLayout* qBox = dynamic_cast<QBoxLayout*>(child);

            if (qBox != nullptr) {
                QBoxLayoutCount += 1;
            }
        }

        if (QBoxLayoutCount == 1) {
            has1QVboxChild = true;
        } else {
            continue;
        }

        int QFrameCount = 0;
        for (auto child : parent->children()) {
            QFrame* qFrame = dynamic_cast<QFrame*>(child);

            if (qFrame != nullptr) {
                QFrameCount += 1;
            }
        }

        if (QFrameCount == 6) {
            has5QFrameChild = true;
        } else {
            continue;
        }

        if (hasScrollArea && parentIsWidget && has8Children && has1QVboxChild && has5QFrameChild) {
            if (pageView != nullptr) {
                std::cout << "ERROR, multiple pageViews found" << std::endl;
            }

            pageView = dynamic_cast<QAbstractScrollArea*>(widget);
        }
    }

    return pageView;
}

void V3dModelManager::requestPixmapRefresh() {
    auto elapsedTime = std::chrono::system_clock::now() - m_LastPixmapRefreshTime;

    auto elapsedTimeSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(elapsedTime);

    if (elapsedTimeSeconds > m_MinTimeBetweenRefreshes) {
        refreshPixmap();
        m_LastPixmapRefreshTime = std::chrono::system_clock::now();
    }
}

void V3dModelManager::refreshPixmap() {
    static bool shouldZoomIn = true;

    int zoom = 0;
    if (shouldZoomIn) {
        zoom = 1;
        shouldZoomIn = false;
    } else {
        zoom = -1;
        shouldZoomIn = true;
    }

    QWheelEvent* wheelEvent = new QWheelEvent(
        QPointF{},            // pos
        QPointF{},            // globalPos
        QPoint{},             // pixelDelta
        QPoint{ zoom, zoom }, // angleDelta
        0,                    // buttons
        Qt::ControlModifier,  // modifiers
        Qt::NoScrollPhase,    // phase
        false                 // inverted
    );

    QMouseEvent* mouseEvent = new QMouseEvent(
        QEvent::MouseButtonRelease,     // type
        QPointF{ },                     // localPos
        QPointF{ },                     // globalPos
        Qt::MiddleButton,               // button
        0,                              // buttons
        Qt::NoModifier                  // modifiers
    );

    ProtectedFunctionCaller::callWheelEvent(m_PageView, wheelEvent);
    ProtectedFunctionCaller::callMouseReleaseEvent(m_PageView, mouseEvent);
}
