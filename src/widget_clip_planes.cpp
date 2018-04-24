/****************************************************************************
** Copyright (c) 2016, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
**     1. Redistributions of source code must retain the above copyright
**        notice, this list of conditions and the following disclaimer.
**
**     2. Redistributions in binary form must reproduce the above
**        copyright notice, this list of conditions and the following
**        disclaimer in the documentation and/or other materials provided
**        with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
****************************************************************************/

#include "widget_clip_planes.h"

#include "ui_widget_clip_planes.h"
#include "bnd_utils.h"
#include "gpx_utils.h"
#include "math_utils.h"
#include "options.h"

#include <algorithm>
#include <Bnd_Box.hxx>
#include <Graphic3d_ClipPlane.hxx>

namespace Mayo {

WidgetClipPlanes::WidgetClipPlanes(const Handle_V3d_View& view3d, QWidget* parent)
    : QWidget(parent),
      m_ui(new Ui_WidgetClipPlanes),
      m_view(view3d)
{
    m_ui->setupUi(this);

    m_vecClipPlaneData = {
        { new Graphic3d_ClipPlane(gp_Pln({}, gp_Dir(1, 0, 0))),
          UiClipPlane(m_ui->check_X, m_ui->widget_X) },
        { new Graphic3d_ClipPlane(gp_Pln({}, gp_Dir(0, 1, 0))),
          UiClipPlane(m_ui->check_Y, m_ui->widget_Y) },
        { new Graphic3d_ClipPlane(gp_Pln({}, gp_Dir(0, 0, 1))),
          UiClipPlane(m_ui->check_Z, m_ui->widget_Z) },
        { new Graphic3d_ClipPlane(gp_Pln({}, gp_Dir(1, 1, 1))),
          UiClipPlane(m_ui->check_Custom, m_ui->widget_Custom) }
    };

    const Options* opts = Options::instance();
    for (ClipPlaneData& data : m_vecClipPlaneData) {
        data.ui.widget_Control->setEnabled(data.ui.check_On->isChecked());
        this->connectUi(&data);
        data.gpx->SetCapping(opts->isClipPlaneCappingOn());
        if (data.gpx->IsCapping())
            GpxUtils::Gpx3dClipPlane_setCappingHatch(data.gpx, opts->clipPlaneCappingHatch());
        Graphic3d_MaterialAspect clipMaterial(Graphic3d_NOM_STEEL);
        Quantity_NameOfColor colorName;
        if (&data == &m_vecClipPlaneData.at(0))
            colorName = Quantity_NOC_RED1;
        else if (&data == &m_vecClipPlaneData.at(1))
            colorName = Quantity_NOC_GREEN1;
        else if (&data == &m_vecClipPlaneData.at(2))
            colorName = Quantity_NOC_BLUE1;
        else
            colorName = Quantity_NOC_GRAY;
        clipMaterial.SetColor(Quantity_Color(colorName));
        data.gpx->SetCappingMaterial(clipMaterial);
    }

    QObject::connect(opts, &Options::clipPlaneCappingToggled, [=](bool on) {
        for (ClipPlaneData& data : m_vecClipPlaneData)
            data.gpx->SetCapping(on);
        m_view->Redraw();
    });
    QObject::connect(
                opts, &Options::clipPlaneCappingHatchChanged,
                [=](Aspect_HatchStyle hatch) {
        for (ClipPlaneData& data : m_vecClipPlaneData)
            GpxUtils::Gpx3dClipPlane_setCappingHatch(data.gpx, hatch);
        m_view->Redraw();
    });

    m_ui->widget_CustomDir->setVisible(false);
}

WidgetClipPlanes::~WidgetClipPlanes()
{
    delete m_ui;
}

void WidgetClipPlanes::setRanges(const Bnd_Box &bndBox)
{
    m_bndBox = bndBox;
    const bool isBndBoxVoid = bndBox.IsVoid();
    const auto bbc = BndBoxCoords::get(bndBox);
    for (ClipPlaneData& data : m_vecClipPlaneData) {
        const gp_Dir& n = data.gpx->ToPlane().Axis().Direction();
        this->setPlaneRange(&data, MathUtils::planeRange(bbc, n));
        data.ui.check_On->setEnabled(!isBndBoxVoid);
        if (isBndBoxVoid)
            data.ui.check_On->setChecked(false);
    }
    m_view->Redraw();
}

void WidgetClipPlanes::setClippingOn(bool on)
{
    for (ClipPlaneData& data : m_vecClipPlaneData)
        data.gpx->SetOn(on ? data.ui.check_On->isChecked() : false);
    m_view->Redraw();
}

void WidgetClipPlanes::connectUi(ClipPlaneData* data)
{
    UiClipPlane& ui = data->ui;
    const Handle_Graphic3d_ClipPlane& gpx = data->gpx;
    QAbstractSlider* posSlider = ui.posSlider();
    QDoubleSpinBox* posSpin = ui.posSpin();
    auto signalSpinValueChanged =
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged);

    QObject::connect(ui.check_On, &QCheckBox::clicked, [=](bool on) {
        ui.widget_Control->setEnabled(on);
        this->setPlaneOn(gpx, on);
        m_view->Redraw();
    });

    if (data->ui.customXDirSpin() != nullptr) {
        auto widgetDir = ui.widget_Control->findChild<QWidget*>("widget_CustomDir");
        QObject::connect(ui.check_On, &QAbstractButton::clicked, [=](bool on) {
            widgetDir->setVisible(on);
            QWidget* panel =
                    this->parentWidget() != nullptr ? this->parentWidget() : this;
            if (on) {
                panel->adjustSize();
            }
            else {
                const QSize sz = panel->size();
                const int offset = ui.customXDirSpin()->frameGeometry().height();
                panel->resize(sz.width(), sz.height() - offset);
            }
        });
    }

    QObject::connect(posSpin, signalSpinValueChanged, [=](double pos) {
        QSignalBlocker sigBlock(posSlider); Q_UNUSED(sigBlock);
        const double dPct = ui.spinValueToSliderValue(pos);
        posSlider->setValue(qRound(dPct));
        GpxUtils::Gpx3dClipPlane_setPosition(gpx, pos);
        m_view->Redraw();
    });

    QObject::connect(posSlider, &QSlider::valueChanged, [=](int pct) {
        const double pos = ui.sliderValueToSpinValue(pct);
        QSignalBlocker sigBlock(posSpin); Q_UNUSED(sigBlock);
        posSpin->setValue(pos);
        GpxUtils::Gpx3dClipPlane_setPosition(gpx, pos);
        m_view->Redraw();
    });

    QObject::connect(ui.inverseBtn(), &QAbstractButton::clicked, [=]{
        const gp_Dir invNormal = gpx->ToPlane().Axis().Direction().Reversed();
        GpxUtils::Gpx3dClipPlane_setNormal(gpx, invNormal);
        GpxUtils::Gpx3dClipPlane_setPosition(gpx, data->ui.posSpin()->value());
        m_view->Redraw();
    });

    // Custom plane normal
    QDoubleSpinBox* customXDirSpin = ui.customXDirSpin();
    QDoubleSpinBox* customYDirSpin = ui.customYDirSpin();
    QDoubleSpinBox* customZDirSpin = ui.customZDirSpin();
    auto funcConnectDirSpin = [=](QDoubleSpinBox* dirSpin) {
        QObject::connect(dirSpin, signalSpinValueChanged, [=]{
            const gp_Vec vecNormal(
                        customXDirSpin->value(),
                        customYDirSpin->value(),
                        customZDirSpin->value());
            if (vecNormal.Magnitude() > Precision::Confusion()) {
                const gp_Dir normal(vecNormal);
                const auto bbc = BndBoxCoords::get(m_bndBox);
                this->setPlaneRange(data, MathUtils::planeRange(bbc, normal));
                GpxUtils::Gpx3dClipPlane_setNormal(gpx, normal);
                m_view->Redraw();
            }
        });
    };
    if (customXDirSpin != nullptr) {
        funcConnectDirSpin(customXDirSpin);
        funcConnectDirSpin(customYDirSpin);
        funcConnectDirSpin(customZDirSpin);
        QObject::connect(ui.inverseBtn(), &QAbstractButton::clicked, [=]{
            QSignalBlocker sigBlockX(customXDirSpin); Q_UNUSED(sigBlockX);
            QSignalBlocker sigBlockY(customYDirSpin); Q_UNUSED(sigBlockY);
            QSignalBlocker sigBlockZ(customZDirSpin); Q_UNUSED(sigBlockZ);
            const gp_Dir& n = gpx->ToPlane().Axis().Direction();
            customXDirSpin->setValue(n.X());
            customYDirSpin->setValue(n.Y());
            customZDirSpin->setValue(n.Z());
        });
    }
}

void WidgetClipPlanes::setPlaneOn(const Handle_Graphic3d_ClipPlane& plane, bool on)
{
    plane->SetOn(on);
    if (!GpxUtils::V3dView_hasClipPlane(m_view, plane))
        m_view->AddClipPlane(plane);
}

void WidgetClipPlanes::setPlaneRange(ClipPlaneData *data, const Range& range)
{
    const double rmin = range.first;
    const double rmax = range.second;
    QDoubleSpinBox* posSpin = data->ui.posSpin();
    QAbstractSlider* posSlider = data->ui.posSlider();
    const double currPlanePos = posSpin->value();
    const double gap = (rmax - rmin) * 0.01;
    const double mid = rmin + (rmax - rmin) / 2.;
    const bool isCurrPlanePosValid = rmin <= currPlanePos && currPlanePos <= rmax;
    const bool isEmptyPosSpinRange =
            std::abs(posSpin->maximum() - posSpin->minimum()) < Precision::Confusion();
    const bool useMidValue = isEmptyPosSpinRange || !isCurrPlanePosValid;
    const double newPlanePos = useMidValue ? mid : currPlanePos;
    posSpin->setRange(rmin - gap, rmax + gap);
    posSpin->setSingleStep(std::abs(posSpin->maximum() - posSpin->minimum()) / 100.);
    if (useMidValue) {
        GpxUtils::Gpx3dClipPlane_setPosition(data->gpx, newPlanePos);
        posSpin->setValue(newPlanePos);
        posSlider->setValue(data->ui.spinValueToSliderValue(newPlanePos));
    }
}

WidgetClipPlanes::UiClipPlane::UiClipPlane(QCheckBox *checkOn, QWidget *widgetControl)
    : check_On(checkOn), widget_Control(widgetControl)
{ }

QDoubleSpinBox *WidgetClipPlanes::UiClipPlane::posSpin() const {
    return this->widget_Control->findChild<QDoubleSpinBox*>(
                QString(), Qt::FindDirectChildrenOnly);
}

QAbstractSlider *WidgetClipPlanes::UiClipPlane::posSlider() const {
    return this->widget_Control->findChild<QSlider*>(
                QString(), Qt::FindDirectChildrenOnly);
}

QAbstractButton *WidgetClipPlanes::UiClipPlane::inverseBtn() const {
    return this->widget_Control->findChild<QToolButton*>(
                QString(), Qt::FindDirectChildrenOnly);
}

QDoubleSpinBox *WidgetClipPlanes::UiClipPlane::customXDirSpin() const {
    return this->widget_Control->findChild<QDoubleSpinBox*>("spin_CustomDirX");
}

QDoubleSpinBox *WidgetClipPlanes::UiClipPlane::customYDirSpin() const {
    return this->widget_Control->findChild<QDoubleSpinBox*>("spin_CustomDirY");
}

QDoubleSpinBox *WidgetClipPlanes::UiClipPlane::customZDirSpin() const {
    return this->widget_Control->findChild<QDoubleSpinBox*>("spin_CustomDirZ");
}

double WidgetClipPlanes::UiClipPlane::spinValueToSliderValue(double val) const {
    QDoubleSpinBox* spin = this->posSpin();
    QAbstractSlider* slider = this->posSlider();
    return MathUtils::mappedValue(
        val, spin->minimum(), spin->maximum(), slider->minimum(), slider->maximum());
}

double WidgetClipPlanes::UiClipPlane::sliderValueToSpinValue(double val) const {
    QDoubleSpinBox* spin = this->posSpin();
    QAbstractSlider* slider = this->posSlider();
    return MathUtils::mappedValue(
        val, slider->minimum(), slider->maximum(), spin->minimum(), spin->maximum());
}

} // namespace Mayo
