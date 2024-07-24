#include "EventFilter.h"

#include <qcoreevent.h>
#include <qevent.h>

#include "V3dModelManager.h"

EventFilter::EventFilter(QObject* parent, V3dModelManager* modelManager) 
    : QObject(parent), modelManager(modelManager) { }

bool EventFilter::eventFilter(QObject *object, QEvent *event) {
    if (modelManager == nullptr) {
        return false;
    }

    if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseMove = dynamic_cast<QMouseEvent*>(event);

        if (mouseMove != nullptr) {
            return modelManager->mouseMoveEvent(mouseMove);
        }

    } else if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mousePress = dynamic_cast<QMouseEvent*>(event);

        if (mousePress != nullptr) {
            return modelManager->mouseButtonPressEvent(mousePress);
        }

    } else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseRelease = dynamic_cast<QMouseEvent*>(event);

        if (mouseRelease != nullptr) {
            return modelManager->mouseButtonReleaseEvent(mouseRelease);
        }
    }

    return false;
}