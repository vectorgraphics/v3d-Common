#pragma once

#include <vector>
#include <string>
#include <memory>

#include <QImage>
#include <QtGui/QMouseEvent>
#include <QAbstractScrollArea>

#include "V3dModel.h"
#include "Rendering/renderheadless.h"

class EventFilter;

class V3dModelManager {
public:
    V3dModelManager();

    void AddModel(V3dModel model, size_t pageNumber);

    QImage RenderModel(size_t pageNumber, size_t modelIndex, int width, int height);

    glm::vec2 GetPageSize(size_t pageNumber);

    bool mouseMoveEvent(QMouseEvent* event);
    bool mouseButtonPressEvent(QMouseEvent* event);
    bool mouseButtonReleaseEvent(QMouseEvent* event);

private:
    QAbstractScrollArea* GetPageViewWidget();

    std::chrono::duration<double> m_MinTimeBetweenRefreshes{ 1.0 / 100.0 }; // In Seconds
    std::chrono::time_point<std::chrono::system_clock> m_LastPixmapRefreshTime;

    void requestPixmapRefresh();
    void refreshPixmap();

    std::vector<std::vector<V3dModel>> m_Models;
    std::unique_ptr<HeadlessRenderer> m_HeadlessRenderer{ std::make_unique<HeadlessRenderer>("/home/benjaminb/kde/src/okular/generators/Okular-v3d-Plugin-Code/shaders/") };

    bool m_MouseDown{ false };

    enum class DragMode {
        SHIFT,
        ZOOM,
        PAN,
        ROTATE
    };

    DragMode m_DragMode{ DragMode::ROTATE };

    glm::ivec2 m_MousePosition;
    glm::ivec2 m_LastMousePosition;

    QAbstractScrollArea* m_PageView{ nullptr };
    EventFilter* m_EventFilter{ nullptr };
};