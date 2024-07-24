#pragma once

#include <qabstractscrollarea.h>

class ProtectedFunctionCaller : public QAbstractScrollArea {
public:
    static void callWheelEvent(QAbstractScrollArea* obj, QWheelEvent* event);

    static void callMouseReleaseEvent(QAbstractScrollArea* obj, QMouseEvent* event);
};