/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2011-     Statoil ASA
//  Copyright (C) 2013-     Ceetron Solutions AS
//  Copyright (C) 2011-2012 Ceetron AS
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

#include "RimCellRangeFilter.h"

#include "RiaApplication.h"
#include "RigActiveCellInfo.h"
#include "RigReservoirGridTools.h"
#include "Rim3dView.h"
#include "RimCase.h"

#include "cafPdmUiSliderEditor.h"

#include "cvfAssert.h"
#include "cvfStructGrid.h"

CAF_PDM_SOURCE_INIT( RimCellRangeFilter, "CellRangeFilter" );

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimCellRangeFilter::RimCellRangeFilter()
{
    CAF_PDM_InitObject( "Cell Range Filter", ":/CellFilter_Range.png", "", "" );

    CAF_PDM_InitField( &startIndexI, "StartIndexI", 1, "Start Index I", "", "", "" );
    startIndexI.uiCapability()->setUiEditorTypeName( caf::PdmUiSliderEditor::uiEditorTypeName() );

    CAF_PDM_InitField( &cellCountI, "CellCountI", 1, "Cell Count I", "", "", "" );
    cellCountI.uiCapability()->setUiEditorTypeName( caf::PdmUiSliderEditor::uiEditorTypeName() );

    CAF_PDM_InitField( &startIndexJ, "StartIndexJ", 1, "Start Index J", "", "", "" );
    startIndexJ.uiCapability()->setUiEditorTypeName( caf::PdmUiSliderEditor::uiEditorTypeName() );

    CAF_PDM_InitField( &cellCountJ, "CellCountJ", 1, "Cell Count J", "", "", "" );
    cellCountJ.uiCapability()->setUiEditorTypeName( caf::PdmUiSliderEditor::uiEditorTypeName() );

    CAF_PDM_InitField( &startIndexK, "StartIndexK", 1, "Start Index K", "", "", "" );
    startIndexK.uiCapability()->setUiEditorTypeName( caf::PdmUiSliderEditor::uiEditorTypeName() );

    CAF_PDM_InitField( &cellCountK, "CellCountK", 1, "Cell Count K", "", "", "" );
    cellCountK.uiCapability()->setUiEditorTypeName( caf::PdmUiSliderEditor::uiEditorTypeName() );

    updateIconState();
    setDeletable( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimCellRangeFilter::~RimCellRangeFilter()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimCellRangeFilter::fullName() const
{
    QString postfix;
    if ( ( cellCountI == 1 ) && ( cellCountJ > 1 ) && ( cellCountK > 1 ) )
    {
        postfix = QString( "I-slice %1" ).arg( QString::number( startIndexI ) );
    }
    else if ( ( cellCountJ == 1 ) && ( cellCountI > 1 ) && ( cellCountK > 1 ) )
    {
        postfix = QString( "J-slice %1" ).arg( QString::number( startIndexJ ) );
    }
    else if ( ( cellCountK == 1 ) && ( cellCountI > 1 ) && ( cellCountJ > 1 ) )
    {
        postfix = QString( "K-slice %1" ).arg( QString::number( startIndexK ) );
    }
    else
    {
        QString irange =
            QString( "I=%1-%2" ).arg( QString::number( startIndexI ), QString::number( startIndexI + cellCountI - 1 ) );
        QString jrange =
            QString( "J=%1-%2" ).arg( QString::number( startIndexJ ), QString::number( startIndexJ + cellCountJ - 1 ) );
        QString krange =
            QString( "K=%1-%2" ).arg( QString::number( startIndexK ), QString::number( startIndexK + cellCountK - 1 ) );

        postfix = QString( "%1 %2 %3" ).arg( irange, jrange, krange );
    }

    return QString( "%1 [%2]" ).arg( RimCellFilter::fullName(), postfix );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimCellRangeFilter::fieldChangedByUi( const caf::PdmFieldHandle* changedField,
                                           const QVariant&            oldValue,
                                           const QVariant&            newValue )
{
    if ( changedField == &m_gridIndex )
    {
        const cvf::StructGridInterface* grid = selectedGrid();

        if ( grid && grid->cellCountI() > 0 && grid->cellCountJ() > 0 && grid->cellCountK() > 0 )
        {
            cellCountI  = static_cast<int>( grid->cellCountI() );
            startIndexI = 1;

            cellCountJ  = static_cast<int>( grid->cellCountJ() );
            startIndexJ = 1;

            cellCountK  = static_cast<int>( grid->cellCountK() );
            startIndexK = 1;
        }

        filterChanged.send();
        return;
    }

    if ( changedField != &m_name )
    {
        computeAndSetValidValues();
        filterChanged.send();
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimCellRangeFilter::computeAndSetValidValues()
{
    const cvf::StructGridInterface* grid = selectedGrid();
    if ( grid && grid->cellCountI() > 0 && grid->cellCountJ() > 0 && grid->cellCountK() > 0 )
    {
        cellCountI  = std::clamp( cellCountI.v(), 1, static_cast<int>( grid->cellCountI() ) );
        startIndexI = std::clamp( startIndexI.v(), 1, static_cast<int>( grid->cellCountI() ) );

        cellCountJ  = std::clamp( cellCountJ.v(), 1, static_cast<int>( grid->cellCountJ() ) );
        startIndexJ = std::clamp( startIndexJ.v(), 1, static_cast<int>( grid->cellCountJ() ) );

        cellCountK  = std::clamp( cellCountK.v(), 1, static_cast<int>( grid->cellCountK() ) );
        startIndexK = std::clamp( startIndexK.v(), 1, static_cast<int>( grid->cellCountK() ) );
    }
    this->updateIconState();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimCellRangeFilter::setDefaultValues( int sliceDirection, int defaultSlice )
{
    const cvf::StructGridInterface* grid = selectedGrid();

    if ( !grid ) return;

    Rim3dView* rimView = nullptr;
    this->firstAncestorOrThisOfTypeAsserted( rimView );
    auto actCellInfo = RigReservoirGridTools::activeCellInfo( rimView );

    RimCase* rimCase = nullptr;
    this->firstAncestorOrThisOfTypeAsserted( rimCase );

    const cvf::StructGridInterface* mainGrid = RigReservoirGridTools::mainGrid( rimCase );

    if ( grid == mainGrid && actCellInfo )
    {
        cvf::Vec3st min, max;
        actCellInfo->IJKBoundingBox( min, max );

        // Adjust to Eclipse indexing
        min.x() = min.x() + 1;
        min.y() = min.y() + 1;
        min.z() = min.z() + 1;

        max.x() = max.x() + 1;
        max.y() = max.y() + 1;
        max.z() = max.z() + 1;

        startIndexI = static_cast<int>( min.x() );
        startIndexJ = static_cast<int>( min.y() );
        startIndexK = static_cast<int>( min.z() );
        cellCountI  = static_cast<int>( max.x() - min.x() + 1 );
        cellCountJ  = static_cast<int>( max.y() - min.y() + 1 );
        cellCountK  = static_cast<int>( max.z() - min.z() + 1 );
    }
    else
    {
        startIndexI = 1;
        startIndexJ = 1;
        startIndexK = 1;
        cellCountI  = static_cast<int>( grid->cellCountI() );
        cellCountJ  = static_cast<int>( grid->cellCountJ() );
        cellCountK  = static_cast<int>( grid->cellCountK() );
    }

    switch ( sliceDirection )
    {
        case 0:
            cellCountI = 1;
            if ( defaultSlice > 0 ) startIndexI = defaultSlice;
            break;
        case 1:
            cellCountJ = 1;
            if ( defaultSlice > 0 ) startIndexJ = defaultSlice;
            break;
        case 2:
            cellCountK = 1;
            if ( defaultSlice > 0 ) startIndexK = defaultSlice;
            break;
        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimCellRangeFilter::defineEditorAttribute( const caf::PdmFieldHandle* field,
                                                QString                    uiConfigName,
                                                caf::PdmUiEditorAttribute* attribute )
{
    caf::PdmUiSliderEditorAttribute* myAttr = dynamic_cast<caf::PdmUiSliderEditorAttribute*>( attribute );
    if ( !myAttr )
    {
        return;
    }

    const cvf::StructGridInterface* grid = selectedGrid();

    if ( !grid ) return;

    if ( field == &startIndexI || field == &cellCountI )
    {
        myAttr->m_minimum = 1;
        myAttr->m_maximum = static_cast<int>( grid->cellCountI() );
    }
    else if ( field == &startIndexJ || field == &cellCountJ )
    {
        myAttr->m_minimum = 1;
        myAttr->m_maximum = static_cast<int>( grid->cellCountJ() );
    }
    else if ( field == &startIndexK || field == &cellCountK )
    {
        myAttr->m_minimum = 1;
        myAttr->m_maximum = static_cast<int>( grid->cellCountK() );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimCellRangeFilter::defineUiOrdering( QString uiConfigName, caf::PdmUiOrdering& uiOrdering )
{
    RimCellFilter::defineUiOrdering( uiConfigName, uiOrdering );

    const cvf::StructGridInterface* grid = selectedGrid();

    RimCase* rimCase = nullptr;
    this->firstAncestorOrThisOfTypeAsserted( rimCase );
    const cvf::StructGridInterface* mainGrid = RigReservoirGridTools::mainGrid( rimCase );

    Rim3dView* rimView = nullptr;
    this->firstAncestorOrThisOfTypeAsserted( rimView );
    auto actCellInfo = RigReservoirGridTools::activeCellInfo( rimView );

    if ( grid == mainGrid && actCellInfo )
    {
        cvf::Vec3st min, max;
        actCellInfo->IJKBoundingBox( min, max );

        // Adjust to Eclipse indexing
        min.x() = min.x() + 1;
        min.y() = min.y() + 1;
        min.z() = min.z() + 1;

        max.x() = max.x() + 1;
        max.y() = max.y() + 1;
        max.z() = max.z() + 1;

        startIndexI.uiCapability()->setUiName( QString( "I Start (%1)" ).arg( min.x() ) );
        startIndexJ.uiCapability()->setUiName( QString( "J Start (%1)" ).arg( min.y() ) );
        startIndexK.uiCapability()->setUiName( QString( "K Start (%1)" ).arg( min.z() ) );
        cellCountI.uiCapability()->setUiName( QString( "  Width (%1)" ).arg( max.x() - min.x() + 1 ) );
        cellCountJ.uiCapability()->setUiName( QString( "  Width (%1)" ).arg( max.y() - min.y() + 1 ) );
        cellCountK.uiCapability()->setUiName( QString( "  Width (%1)" ).arg( max.z() - min.z() + 1 ) );
    }
    else
    {
        startIndexI.uiCapability()->setUiName( QString( "I Start" ) );
        startIndexJ.uiCapability()->setUiName( QString( "J Start" ) );
        startIndexK.uiCapability()->setUiName( QString( "K Start" ) );
        cellCountI.uiCapability()->setUiName( QString( "  Width" ) );
        cellCountJ.uiCapability()->setUiName( QString( "  Width" ) );
        cellCountK.uiCapability()->setUiName( QString( "  Width" ) );
    }

    auto group = uiOrdering.addNewGroup( "Range Selection" );

    group->add( &startIndexI );
    group->add( &cellCountI );
    group->add( &startIndexJ );
    group->add( &cellCountJ );
    group->add( &startIndexK );
    group->add( &cellCountK );

    uiOrdering.skipRemainingFields( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimCellRangeFilter::defineUiTreeOrdering( caf::PdmUiTreeOrdering& uiTreeOrdering, QString uiConfigName )
{
    RimCellFilter::defineUiTreeOrdering( uiTreeOrdering, uiConfigName );

    updateActiveState( isFilterControlled() );
    updateIconState();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimCellRangeFilter::updateCompundFilter( cvf::CellRangeFilter* cellRangeFilter )
{
    CVF_ASSERT( cellRangeFilter );

    if ( filterMode() == RimCellFilter::INCLUDE )
    {
        cellRangeFilter->addCellIncludeRange( static_cast<size_t>( startIndexI ) - 1,
                                              static_cast<size_t>( startIndexJ ) - 1,
                                              static_cast<size_t>( startIndexK ) - 1,
                                              static_cast<size_t>( startIndexI ) + cellCountI - 1,
                                              static_cast<size_t>( startIndexJ ) + cellCountJ - 1,
                                              static_cast<size_t>( startIndexK ) + cellCountK - 1,
                                              propagateToSubGrids() );
    }
    else
    {
        cellRangeFilter->addCellExcludeRange( static_cast<size_t>( startIndexI ) - 1,
                                              static_cast<size_t>( startIndexJ ) - 1,
                                              static_cast<size_t>( startIndexK ) - 1,
                                              static_cast<size_t>( startIndexI ) + cellCountI - 1,
                                              static_cast<size_t>( startIndexJ ) + cellCountJ - 1,
                                              static_cast<size_t>( startIndexK ) + cellCountK - 1,
                                              propagateToSubGrids() );
    }
}
