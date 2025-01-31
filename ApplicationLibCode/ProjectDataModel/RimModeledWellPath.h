/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2018-     Equinor ASA
//
//  ResInsight is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  ResInsight is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.
//
//  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
//  for more details.
//
/////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "RimWellPath.h"

#include "cafPdmChildField.h"
#include "cafPdmPtrField.h"

class RimWellPathTarget;
class RimWellPath;
class RimWellPathGeometryDef;

class RimModeledWellPath : public RimWellPath
{
    CAF_PDM_HEADER_INIT;

public:
    RimModeledWellPath();
    ~RimModeledWellPath() override;

    void                    createWellPathGeometry();
    void                    updateWellPathVisualization();
    void                    scheduleUpdateOfDependentVisualization();
    RimWellPathGeometryDef* geometryDefinition() const;
    QString                 wellPlanText();
    void                    updateTieInLocationFromParentWell();

private:
    void defineUiTreeOrdering( caf::PdmUiTreeOrdering& uiTreeOrdering, QString uiConfigName ) override;
    void defineUiOrdering( QString uiConfigName, caf::PdmUiOrdering& uiOrdering ) override;
    void onGeometryDefinitionChanged( const caf::SignalEmitter* emitter, bool fullUpdate );

    void fieldChangedByUi( const caf::PdmFieldHandle* changedField, const QVariant& oldValue, const QVariant& newValue ) override;

    QList<caf::PdmOptionItemInfo> calculateValueOptions( const caf::PdmFieldHandle* fieldNeedingOptions,
                                                         bool*                      useOptionsOnly ) override;

    void updateGeometry( bool fullUpdate );

private:
    caf::PdmChildField<RimWellPathGeometryDef*> m_geometryDefinition;
};
