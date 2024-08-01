#pragma once

#include <vector>
#include <string>
#include <memory>

#include <QImage>
#include <QtGui/QMouseEvent>
#include <QAbstractScrollArea>
#include <page.h>

#include <document.h>

#include "V3dModel.h"
#include "Rendering/renderheadless.h"

class EventFilter;

class V3dModelManager {
public:
    V3dModelManager(const Okular::Document* document, const std::string& shaderPath);

    void AddModel(V3dModel model, size_t pageNumber);

    QImage RenderModel(size_t pageNumber, size_t modelIndex, int width, int height);

    V3dModel& Model(size_t pageNumber, size_t modelIndex);
    std::vector<V3dModel>& Models(size_t pageNumber);

    glm::vec2 GetPageSize(size_t pageNumber);

    void SetDocument(const Okular::Document* document);

    bool mouseMoveEvent(QMouseEvent* event);
    bool mouseButtonPressEvent(QMouseEvent* event);
    bool mouseButtonReleaseEvent(QMouseEvent* event);

    void CacheRequestSize(size_t pageNumber, int width, int height, int priority);
    void CachePage(size_t pageNumber, Okular::Page* page);

private:
    const Okular::Document* m_Document;

    QAbstractScrollArea* GetPageViewWidget();

    std::chrono::duration<double> m_MinTimeBetweenRefreshes{ 1.0 / 60.0 }; // In Seconds
    std::chrono::time_point<std::chrono::system_clock> m_LastPixmapRefreshTime;

    void requestPixmapRefresh();
    void refreshPixmap();

    std::vector<std::vector<V3dModel>> m_Models;
    std::vector<std::vector<QImage>> m_ModelImages;

    std::unique_ptr<HeadlessRenderer> m_HeadlessRenderer;

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

    struct RequestCache {
        glm::ivec2 size;
        int priority{ std::numeric_limits<int>::max() };
    };

    std::vector<RequestCache> m_CachedRequestSizes;
    std::vector<Okular::Page*> m_Pages;

    V3dModel* m_ActiveModel;
    glm::ivec2 m_ActiveModelInfo;
};