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

#include "RimWellPathTieIn.h"

#include "RimModeledWellPath.h"
#include "RimTools.h"
#include "RimWellPathValve.h"

#include "cafPdmFieldScriptingCapability.h"
#include "cafPdmObjectScriptingCapability.h"
#include "cafPdmUiDoubleValueEditor.h"

CAF_PDM_SOURCE_INIT( RimWellPathTieIn, "RimWellPathTieIn" );

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimWellPathTieIn::RimWellPathTieIn()
{
    CAF_PDM_InitObject( "Well Path Tie In", ":/NotDefined.png", "", "Well Path Tie In description" );

    CAF_PDM_InitFieldNoDefault( &m_parentWell, "ParentWellPath", "ParentWellPath", "", "", "" );
    CAF_PDM_InitFieldNoDefault( &m_childWell, "ChildWellPath", "ChildWellPath", "", "", "" );
    CAF_PDM_InitFieldNoDefault( &m_tieInMeasuredDepth, "TieInMeasuredDepth", "TieInMeasuredDepth", "", "", "" );
    m_tieInMeasuredDepth.uiCapability()->setUiEditorTypeName( caf::PdmUiDoubleValueEditor::uiEditorTypeName() );

    CAF_PDM_InitScriptableField( &m_addValveAtConnection,
                                 "AddValveAtConnection",
                                 false,
                                 "Add Outlet Valve for Branches",
                                 "",
                                 "",
                                 "" );

    CAF_PDM_InitScriptableFieldNoDefault( &m_valve, "Valve", "Branch Outlet Valve", "", "", "" );

    m_valve = new RimWellPathValve;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellPathTieIn::connectWellPaths( RimWellPath* parentWell, RimWellPath* childWell, double tieInMeasuredDepth )
{
    m_parentWell         = parentWell;
    m_childWell          = childWell;
    m_tieInMeasuredDepth = tieInMeasuredDepth;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimWellPath* RimWellPathTieIn::parentWell() const
{
    return m_parentWell();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimWellPathTieIn::tieInMeasuredDepth() const
{
    return m_tieInMeasuredDepth();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimWellPath* RimWellPathTieIn::childWell() const
{
    return m_childWell();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellPathTieIn::updateChildWellGeometry()
{
    auto modeledWellPath = dynamic_cast<RimModeledWellPath*>( m_childWell() );
    if ( modeledWellPath )
    {
        modeledWellPath->updateTieInLocationFromParentWell();
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const RimWellPathValve* RimWellPathTieIn::outletValve() const
{
    return m_addValveAtConnection() && m_valve() && m_valve->valveTemplate() ? m_valve() : nullptr;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellPathTieIn::defineUiOrdering( QString uiConfigName, caf::PdmUiOrdering& uiOrdering )
{
    uiOrdering.add( &m_parentWell );
    uiOrdering.add( &m_tieInMeasuredDepth );

    auto valveGroup = uiOrdering.addNewGroup( "Valve Settings" );
    valveGroup->add( &m_addValveAtConnection );
    if ( m_addValveAtConnection )
    {
        m_valve->uiOrdering( "TemplateOnly", *valveGroup );
    }

    uiOrdering.skipRemainingFields();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimWellPathTieIn::fieldChangedByUi( const caf::PdmFieldHandle* changedField,
                                         const QVariant&            oldValue,
                                         const QVariant&            newValue )
{
    updateChildWellGeometry();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QList<caf::PdmOptionItemInfo> RimWellPathTieIn::calculateValueOptions( const caf::PdmFieldHandle* fieldNeedingOptions,
                                                                       bool*                      useOptionsOnly )
{
    QList<caf::PdmOptionItemInfo> options;

    if ( fieldNeedingOptions == &m_parentWell )
    {
        std::vector<RimWellPath*> wellPathsToExclude = { m_childWell() };
        RimTools::wellPathOptionItemsSubset( wellPathsToExclude, &options );

        options.push_front( caf::PdmOptionItemInfo( "None", nullptr ) );
    }

    return options;
}
