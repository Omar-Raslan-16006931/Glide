// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// The picture. Once per display refresh it asks the engine what to show at that moment and draws it
// with the interpolation shader (shaders/frame.frag) through Qt's renderer (Vulkan or OpenGL on Linux,
// Metal on macOS). On Qt's software renderer it shows the real frames only.
#include "veyra/portable/Engine.h"

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <memory>
#include <vector>

class QSGTexture;

namespace pushfps {

class FrameView : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY pictureChanged)
    Q_PROPERTY(qreal contrast READ contrast WRITE setContrast NOTIFY pictureChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY pictureChanged)
    Q_PROPERTY(qreal gamma READ gamma WRITE setGamma NOTIFY pictureChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
public:
    explicit FrameView(QQuickItem* parent = nullptr);
    ~FrameView() override;

    qreal brightness() const { return brightness_; }
    qreal contrast() const { return contrast_; }
    qreal saturation() const { return saturation_; }
    qreal gamma() const { return gamma_; }
    bool running() const { return running_; }
    void setBrightness(qreal v);
    void setContrast(qreal v);
    void setSaturation(qreal v);
    void setGamma(qreal v);
    void setRunning(bool v);

    // The engine every FrameView draws from (set once by the application).
    static void setEngine(std::shared_ptr<veyra::portable::Engine> engine);
    // GPU check: draw this fixed pick instead of asking the engine.
    static void setFixedPick(std::shared_ptr<veyra::portable::Pick> pick);

signals:
    void pictureChanged();
    void runningChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void releaseResources() override;

private:
    struct CachedTexture {
        quint64 key = 0;
        QSGTexture* texture = nullptr;
        quint64 lastUse = 0;
    };
    QSGTexture* cached(quint64 key, const std::function<QImage()>& make, bool nearest);
    void dropTextures();

    qreal brightness_ = 0, contrast_ = 1, saturation_ = 1, gamma_ = 1;
    bool running_ = true;
    std::vector<CachedTexture> cache_;
    quint64 frameCounter_ = 0;
    QSGTexture* black_ = nullptr;
    QMetaObject::Connection swapped_;
};

}  // namespace pushfps
