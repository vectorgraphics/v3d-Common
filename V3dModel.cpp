#include "V3dModel.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Utility/Arcball.h"
#include "3rdParty/xstream.h"

V3dModel::V3dModel(const std::string& filePath, const glm::vec2& minBound, const glm::vec2& maxBound) 
    : minBound(minBound), maxBound(maxBound) {
        
    file = std::make_unique<V3dFile>(filePath);

    initProjection();
}


V3dModel::V3dModel(xdr::memixstream& xdrFile, const glm::vec2& minBound, const glm::vec2& maxBound) 
    : minBound(minBound), maxBound(maxBound) {

    file = std::make_unique<V3dFile>(xdrFile);

    initProjection();
}

void V3dModel::initProjection() {
    h = -std::tan(0.5f * file->headerInfo.angleOfView) * file->headerInfo.maxBound.z;

    center.x = 0.0f;
    center.y = 0.0f;

    center.z = 0.5f * (file->headerInfo.minBound.z + file->headerInfo.maxBound.z);

    zoom = file->headerInfo.initialZoom;
    lastZoom = file->headerInfo.initialZoom;

    viewParam.minValues.z = file->headerInfo.minBound.z;
    viewParam.maxValues.z = file->headerInfo.maxBound.z;

    shift.x = 0.0f;
    shift.y = 0.0f;
}

void V3dModel::setProjection(const glm::vec2& displayDimensions) {
    setDimensions(displayDimensions.x, displayDimensions.y, shift.x, shift.y);

    if (file->headerInfo.orthographic) {
        projectionMatrix = glm::orthoRH_ZO(viewParam.minValues.x, viewParam.maxValues.x, viewParam.minValues.y, viewParam.maxValues.y, -viewParam.maxValues.z, -viewParam.minValues.z);

    } else {
        projectionMatrix = glm::frustumRH_ZO(viewParam.minValues.x, viewParam.maxValues.x, viewParam.minValues.y, viewParam.maxValues.y, -viewParam.maxValues.z, -viewParam.minValues.z);
    }

    updateViewMatrix();
}

void V3dModel::setDimensions(float width, float height, float X, float Y) {
    float Aspect = width / height;

    xShift = (X / width + file->headerInfo.viewportShift.x) * zoom;
    yShift = (Y / height + file->headerInfo.viewportShift.y) * zoom;

    float zoomInv = 1.0f / zoom;

    if (file->headerInfo.orthographic) {
        float xsize = file->headerInfo.maxBound.x - file->headerInfo.minBound.x;
        float ysize = file->headerInfo.maxBound.y - file->headerInfo.minBound.y;

        if (xsize < ysize * Aspect) {
            float r = 0.5f * ysize * Aspect * zoomInv;

            float X0 = 2.0f * r * xShift;
            float Y0 = ysize * zoomInv * yShift;

            viewParam.minValues.x = -r - X0;
            viewParam.maxValues.x = r - X0;
            viewParam.minValues.y = file->headerInfo.minBound.y * zoomInv - Y0;
            viewParam.maxValues.y = file->headerInfo.maxBound.y * zoomInv - Y0;
        } else {
            float r = 0.5f * xsize * zoomInv / Aspect;

            float X0 = xsize * zoomInv * xShift;
            float Y0 = 2 * r * yShift;

            viewParam.minValues.x = file->headerInfo.minBound.x * zoomInv - X0;
            viewParam.maxValues.x = file->headerInfo.maxBound.x * zoomInv - X0;
            viewParam.minValues.y = -r -Y0;
            viewParam.maxValues.y = r - Y0;
        }
    } else {
        float r = h * zoomInv;
        float rAspect = r * Aspect;

        float X0 = 2.0f * rAspect * xShift;
        float Y0 = 2 * r * yShift;

        viewParam.minValues.x = -rAspect-X0;
        viewParam.maxValues.x = rAspect-X0;
        viewParam.minValues.y = -r - Y0;
        viewParam.maxValues.y = r - Y0;
    }
}

void V3dModel::updateViewMatrix() {
    glm::mat4 temp{ 1.0f };
    temp = glm::translate(temp, center);
    glm::mat4 cjmatInv = glm::inverse(temp);

    viewMatrix = rotationMatrix * cjmatInv;
    viewMatrix = temp * viewMatrix;

    viewMatrix = glm::translate(viewMatrix, { center.x, center.y, 0.0f });
    m_HasChanged = true;
}

void V3dModel::dragModeShift(const glm::vec2& normalizedMousePosition, const glm::vec2& lastNormalizedMousePosition, const glm::vec2& displayDimensions) {
    float zoomInv = 1 / zoom;
    shift.x += (normalizedMousePosition.x - lastNormalizedMousePosition.x) * zoomInv * (displayDimensions.x / 2.0f);
    shift.y -= (normalizedMousePosition.y - lastNormalizedMousePosition.y) * zoomInv * (displayDimensions.y / 2.0f);

    m_HasChanged = true;
}

void V3dModel::dragModeZoom(const glm::vec2& normalizedMousePosition, const glm::vec2& lastNormalizedMousePosition, const glm::vec2& pageViewSize) {
    float diff = lastNormalizedMousePosition.y - normalizedMousePosition.y;

    float stepPower = file->headerInfo.zoomStep * (pageViewSize.y / 2.0f) * diff;
    const float limit = std::log(0.1f * std::numeric_limits<float>::max()) / std::log(file->headerInfo.zoomFactor);

    if (std::abs(stepPower) < limit) {
        zoom *= std::pow(file->headerInfo.zoomFactor, stepPower);

        float maxZoom = std::sqrt(std::numeric_limits<float>::max());
        float minZoom = 1 / maxZoom;

        if (zoom <= minZoom) {
            zoom = minZoom;
        } else if (zoom >= maxZoom) {
            zoom = maxZoom;
        }

        m_HasChanged = true;
    }
}

void V3dModel::dragModePan(const glm::vec2& normalizedMousePosition, const glm::vec2& lastNormalizedMousePosition, const glm::vec2& pageViewSize) {
    if (file->headerInfo.orthographic) {
        dragModeShift(normalizedMousePosition, lastNormalizedMousePosition, pageViewSize);
    } else {
        center.x += (normalizedMousePosition.x - lastNormalizedMousePosition.x) * (viewParam.maxValues.x - viewParam.minValues.x);
        center.y -= (normalizedMousePosition.y - lastNormalizedMousePosition.y) * (viewParam.maxValues.y - viewParam.minValues.y);
    }

    m_HasChanged = true;
}

void V3dModel::dragModeRotate(const glm::vec2& normalizedMousePosition, const glm::vec2& lastNormalizedMousePosition, const glm::vec2& pageViewSize) {
    float arcballFactor = 1.0f;

    if (normalizedMousePosition == lastNormalizedMousePosition) { return; }

    Arcball arcball{ { lastNormalizedMousePosition.x, -lastNormalizedMousePosition.y }, { normalizedMousePosition.x, -normalizedMousePosition.y} };
    float angle = arcball.angle;
    glm::vec3 axis = arcball.axis;

    float angleRadians = 2.0f * angle / zoom * arcballFactor;
    glm::mat4 temp = glm::rotate(glm::mat4(1.0f), angleRadians, axis);
    rotationMatrix = temp * rotationMatrix;

    m_HasChanged = true;
}
