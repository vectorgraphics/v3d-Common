#include "ProtectedFunctionCaller.h"

void ProtectedFunctionCaller::callWheelEvent(QAbstractScrollArea* obj, QWheelEvent* event) {
    void (QAbstractScrollArea::*funcPtr)(QWheelEvent*) = &ProtectedFunctionCaller::wheelEvent;

    (obj->*funcPtr)(event);
}

void ProtectedFunctionCaller::callMouseReleaseEvent(QAbstractScrollArea* obj, QMouseEvent* event) {
    void (QAbstractScrollArea::*funcPtr)(QMouseEvent*) = &ProtectedFunctionCaller::mouseReleaseEvent;

    (obj->*funcPtr)(event);
}
