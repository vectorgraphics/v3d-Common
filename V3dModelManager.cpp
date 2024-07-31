#include "V3dModelManager.h"

#include <iostream>

#include <QApplication>
#include <QBoxLayout>

#include <area.h>
#include <document.h>

#include "Utility/EventFilter.h"
#include "Utility/ProtectedFunctionCaller.h"

V3dModelManager::V3dModelManager(const Okular::Document* document, const std::string& shaderPath) 
    : m_Document(document)
    , m_HeadlessRenderer(std::make_unique<HeadlessRenderer>(shaderPath)) {
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

    for (int32_t y = 0; y < height; y++) {
        unsigned int *row = (unsigned int*)imgDatatmp;
        size_t rowBytes = width * 4;
    
        vectorData.resize(vectorData.size() + rowBytes);
        std::memcpy(vectorData.data() + vectorData.size() - rowBytes, (unsigned char*)row, rowBytes);

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

void V3dModelManager::SetDocument(const Okular::Document* document) {
    m_Document = document;
}

bool V3dModelManager::mouseMoveEvent(QMouseEvent* event) {
    m_MousePosition.x = event->x();
    m_MousePosition.y = event->y();

    if (m_MouseDown == false) {
        return true;
    }

    const QVector<Okular::VisiblePageRect*> visiblePages = m_Document->visiblePageRects();

    bool shouldRefresh = false;

    for (size_t i = 0; i < m_Models.size(); ++i) {
        int pageNumber = (int)i;

        bool foundAPage = false;
        Okular::VisiblePageRect* visiblePage = nullptr;
        for (auto& page : visiblePages) {
            if (page->pageNumber == i) {
                foundAPage = true;
                visiblePage = page;
                break;
            }
        }

        if (!foundAPage) {
            continue;
        }

        Okular::NormalizedRect rect = visiblePage->rect;

        glm::vec2 normalizedPositionOnPage{ };
        glm::vec2 lastNormalizedPositionOnPage{ };

        if (rect.left == 0.0 && rect.right == 1.0 && rect.top == 0.0 && rect.bottom == 1.0) {
            // The full page is visible
            int horizontalMargin = (m_PageView->width() - m_CachedRequestSizes[i].size.x) / 2;
            int verticalMargin = (m_PageView->height() - m_CachedRequestSizes[i].size.y) / 2;

            bool horizontalyOnPage = m_MousePosition.x > horizontalMargin && m_MousePosition.x < horizontalMargin + m_CachedRequestSizes[i].size.x;
            bool verticalyOnPage = m_MousePosition.y > verticalMargin && m_MousePosition.y < verticalMargin + m_CachedRequestSizes[i].size.y;

            if (!(horizontalyOnPage && verticalyOnPage)) {
                continue;
            }

            normalizedPositionOnPage = {
                (float)(m_MousePosition.x - horizontalMargin) / (float)m_CachedRequestSizes[i].size.x,
                (float)(m_MousePosition.y - verticalMargin) / (float)m_CachedRequestSizes[i].size.y // TODO this assumes we are verticaly centerd
            };

            lastNormalizedPositionOnPage = {
                (float)(m_LastMousePosition.x - horizontalMargin) / (float)m_CachedRequestSizes[i].size.x,
                (float)(m_LastMousePosition.y - verticalMargin) / (float)m_CachedRequestSizes[i].size.y
            };
        } else {
            // Only a section of the page is visible
            float leftPixel = (float)rect.left * (float)m_CachedRequestSizes[i].size.x;
            float topPixel = (float)rect.top * (float)m_CachedRequestSizes[i].size.y;

            normalizedPositionOnPage = {
                (float)(m_MousePosition.x + leftPixel) / (float)m_CachedRequestSizes[i].size.x,
                (float)(m_MousePosition.y + topPixel) / (float)m_CachedRequestSizes[i].size.y
            };

            lastNormalizedPositionOnPage = {
                (float)(m_LastMousePosition.x + leftPixel) / (float)m_CachedRequestSizes[i].size.x,
                (float)(m_LastMousePosition.y + topPixel) / (float)m_CachedRequestSizes[i].size.y
            };
        }

        for (auto& model : m_Models[i]) {
            glm::vec2 normalizedPositionOnModel = {
                (normalizedPositionOnPage.x - model.minBound.x) / (model.maxBound.x - model.minBound.x),
                (normalizedPositionOnPage.y - model.minBound.y) / (model.maxBound.y - model.minBound.y)
            };

            glm::vec2 lastNormalizedPositionOnModel = {
                (lastNormalizedPositionOnPage.x - model.minBound.x) / (model.maxBound.x - model.minBound.x),
                (lastNormalizedPositionOnPage.y - model.minBound.y) / (model.maxBound.y - model.minBound.y)
            };

            bool horizontalyOnModel = normalizedPositionOnPage.x > model.minBound.x && normalizedPositionOnPage.x < model.maxBound.x;
            bool verticalyOnModel = normalizedPositionOnPage.y > model.minBound.y && normalizedPositionOnPage.y < model.maxBound.y;

            if (!(horizontalyOnModel && verticalyOnModel)) {
                continue;
            }

            glm::vec2 pageViewSize = { m_PageView->width(), m_PageView->height() };

            shouldRefresh = true;

            switch (m_DragMode) {
                case DragMode::SHIFT: {
                    model.dragModeShift(normalizedPositionOnModel, lastNormalizedPositionOnModel, pageViewSize);
                    break;
                }
                case DragMode::ZOOM: {
                    model.dragModeZoom(normalizedPositionOnModel, lastNormalizedPositionOnModel, pageViewSize);
                    break;
                }
                case DragMode::PAN: {
                    model.dragModePan(normalizedPositionOnModel, lastNormalizedPositionOnModel, pageViewSize);
                    break;
                }
                case DragMode::ROTATE: {
                    model.dragModeRotate(normalizedPositionOnModel, lastNormalizedPositionOnModel, pageViewSize);
                    break;
                }
            }
        }
    }

    if (shouldRefresh) {
        requestPixmapRefresh();
    }

    m_LastMousePosition.x = m_MousePosition.x;
    m_LastMousePosition.y = m_MousePosition.y;

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

void V3dModelManager::CacheRequestSize(size_t pageNumber, int width, int height, int priority) {
    m_CachedRequestSizes.resize(pageNumber + 1);

    if (priority <= m_CachedRequestSizes[pageNumber].priority) {
        m_CachedRequestSizes[pageNumber] = RequestCache{ glm::ivec2{ width, height }, priority };
    }
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
