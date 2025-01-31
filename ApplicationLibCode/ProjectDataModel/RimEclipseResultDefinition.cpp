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

#include "RimEclipseResultDefinition.h"

#include "RiaColorTables.h"
#include "RiaLogging.h"
#include "RiaNncDefines.h"
#include "RiaQDateTimeTools.h"

#include "RicfCommandObject.h"

#include "RigActiveCellInfo.h"
#include "RigAllanDiagramData.h"
#include "RigCaseCellResultsData.h"
#include "RigEclipseCaseData.h"
#include "RigEclipseResultInfo.h"
#include "RigFlowDiagResultAddress.h"
#include "RigFlowDiagResults.h"
#include "RigFormationNames.h"
#include "RigVisibleTracerFilter.h"

#include "Rim3dView.h"
#include "Rim3dWellLogCurve.h"
#include "RimCellEdgeColors.h"
#include "RimColorLegend.h"
#include "RimContourMapProjection.h"
#include "RimEclipseCase.h"
#include "RimEclipseCellColors.h"
#include "RimEclipseContourMapProjection.h"
#include "RimEclipseContourMapView.h"
#include "RimEclipseFaultColors.h"
#include "RimEclipseInputProperty.h"
#include "RimEclipseInputPropertyCollection.h"
#include "RimEclipsePropertyFilter.h"
#include "RimEclipseResultCase.h"
#include "RimEclipseView.h"
#include "RimFlowDiagSolution.h"
#include "RimGridCrossPlot.h"
#include "RimGridCrossPlotDataSet.h"
#include "RimGridTimeHistoryCurve.h"
#include "RimIntersectionCollection.h"
#include "RimIntersectionResultDefinition.h"
#include "RimPlotCurve.h"
#include "RimProject.h"
#include "RimReservoirCellResultsStorage.h"
#include "RimSimWellInView.h"
#include "RimSimWellInViewCollection.h"
#include "RimTernaryLegendConfig.h"
#include "RimViewLinker.h"
#include "RimWellLogExtractionCurve.h"
#include "RimWellLogTrack.h"

#ifdef USE_QTCHARTS
#include "RimGridStatisticsPlot.h"
#endif

#include "cafCategoryMapper.h"
#include "cafPdmFieldScriptingCapability.h"
#include "cafPdmUiListEditor.h"
#include "cafPdmUiToolButtonEditor.h"
#include "cafPdmUiTreeSelectionEditor.h"
#include "cafUtils.h"

#include <QRegularExpression>

namespace caf
{
template <>
void RimEclipseResultDefinition::FlowTracerSelectionEnum::setUp()
{
    addItem( RimEclipseResultDefinition::FLOW_TR_INJ_AND_PROD, "FLOW_TR_INJ_AND_PROD", "All Injectors and Producers" );
    addItem( RimEclipseResultDefinition::FLOW_TR_PRODUCERS, "FLOW_TR_PRODUCERS", "All Producers" );
    addItem( RimEclipseResultDefinition::FLOW_TR_INJECTORS, "FLOW_TR_INJECTORS", "All Injectors" );
    addItem( RimEclipseResultDefinition::FLOW_TR_BY_SELECTION, "FLOW_TR_BY_SELECTION", "By Selection" );

    setDefault( RimEclipseResultDefinition::FLOW_TR_INJ_AND_PROD );
}
} // namespace caf

CAF_PDM_SOURCE_INIT( RimEclipseResultDefinition, "ResultDefinition" );

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimEclipseResultDefinition::RimEclipseResultDefinition( caf::PdmUiItemInfo::LabelPosType labelPosition )
    : m_isDeltaResultEnabled( false )
    , m_labelPosition( labelPosition )
    , m_ternaryEnabled( true )
{
    CAF_PDM_InitScriptableObjectWithNameAndComment( "Result Definition", "", "", "", "EclipseResult", "An eclipse result definition" );

    CAF_PDM_InitScriptableFieldNoDefault( &m_resultType, "ResultType", "Type", "", "", "" );
    m_resultType.uiCapability()->setUiHidden( true );

    CAF_PDM_InitScriptableFieldNoDefault( &m_porosityModel, "PorosityModelType", "Porosity", "", "", "" );
    m_porosityModel.uiCapability()->setUiHidden( true );

    CAF_PDM_InitScriptableField( &m_resultVariable,
                                 "ResultVariable",
                                 RiaResultNames::undefinedResultName(),
                                 "Variable",
                                 "",
                                 "",
                                 "" );
    m_resultVariable.uiCapability()->setUiHidden( true );

    CAF_PDM_InitFieldNoDefault( &m_flowSolution, "FlowDiagSolution", "Solution", "", "", "" );
    m_flowSolution.uiCapability()->setUiHidden( true );

    CAF_PDM_InitField( &m_timeLapseBaseTimestep,
                       "TimeLapseBaseTimeStep",
                       RigEclipseResultAddress::noTimeLapseValue(),
                       "Base Time Step",
                       "",
                       "",
                       "" );

    CAF_PDM_InitFieldNoDefault( &m_differenceCase, "DifferenceCase", "Difference Case", "", "", "" );

    CAF_PDM_InitField( &m_divideByCellFaceArea, "DivideByCellFaceArea", false, "Divide By Area", "", "", "" );

    CAF_PDM_InitScriptableFieldNoDefault( &m_selectedInjectorTracers, "SelectedInjectorTracers", "Injector Tracers", "", "", "" );
    m_selectedInjectorTracers.uiCapability()->setUiHidden( true );

    CAF_PDM_InitScriptableFieldNoDefault( &m_selectedProducerTracers, "SelectedProducerTracers", "Producer Tracers", "", "", "" );
    m_selectedProducerTracers.uiCapability()->setUiHidden( true );

    CAF_PDM_InitScriptableFieldNoDefault( &m_selectedSouringTracers, "SelectedSouringTracers", "Tracers", "", "", "" );
    m_selectedSouringTracers.uiCapability()->setUiHidden( true );

    CAF_PDM_InitScriptableFieldNoDefault( &m_flowTracerSelectionMode, "FlowTracerSelectionMode", "Tracers", "", "", "" );
    CAF_PDM_InitScriptableFieldNoDefault( &m_phaseSelection, "PhaseSelection", "Phases", "", "", "" );
    m_phaseSelection.uiCapability()->setUiLabelPosition( m_labelPosition );

    CAF_PDM_InitScriptableField( &m_showOnlyVisibleTracersInLegend,
                                 "ShowOnlyVisibleTracersInLegend",
                                 true,
                                 "Show Only Visible Tracers In Legend",
                                 "",
                                 "",
                                 "" );

    // Ui only fields

    CAF_PDM_InitFieldNoDefault( &m_resultTypeUiField, "MResultType", "Type", "", "", "" );
    m_resultTypeUiField.xmlCapability()->disableIO();
    m_resultTypeUiField.uiCapability()->setUiLabelPosition( m_labelPosition );

    CAF_PDM_InitFieldNoDefault( &m_porosityModelUiField, "MPorosityModelType", "Porosity", "", "", "" );
    m_porosityModelUiField.xmlCapability()->disableIO();
    m_porosityModelUiField.uiCapability()->setUiLabelPosition( m_labelPosition );

    CAF_PDM_InitField( &m_resultVariableUiField,
                       "MResultVariable",
                       RiaResultNames::undefinedResultName(),
                       "Result Property",
                       "",
                       "",
                       "" );
    m_resultVariableUiField.xmlCapability()->disableIO();
    m_resultVariableUiField.uiCapability()->setUiEditorTypeName( caf::PdmUiListEditor::uiEditorTypeName() );
    m_resultVariableUiField.uiCapability()->setUiLabelPosition( m_labelPosition );

    CAF_PDM_InitFieldNoDefault( &m_inputPropertyFileName, "InputPropertyFileName", "File Name", "", "", "" );
    m_inputPropertyFileName.xmlCapability()->disableIO();
    m_inputPropertyFileName.uiCapability()->setUiReadOnly( true );

    CAF_PDM_InitFieldNoDefault( &m_flowSolutionUiField, "MFlowDiagSolution", "Solution", "", "", "" );
    m_flowSolutionUiField.xmlCapability()->disableIO();
    m_flowSolutionUiField.uiCapability()->setUiHidden( true ); // For now since there are only one to choose from

    CAF_PDM_InitField( &m_syncInjectorToProducerSelection, "MSyncSelectedInjProd", false, "Add Communicators ->", "", "", "" );
    m_syncInjectorToProducerSelection.uiCapability()->setUiEditorTypeName( caf::PdmUiToolButtonEditor::uiEditorTypeName() );

    CAF_PDM_InitField( &m_syncProducerToInjectorSelection, "MSyncSelectedProdInj", false, "<- Add Communicators", "", "", "" );
    m_syncProducerToInjectorSelection.uiCapability()->setUiEditorTypeName( caf::PdmUiToolButtonEditor::uiEditorTypeName() );

    CAF_PDM_InitFieldNoDefault( &m_selectedInjectorTracersUiField, "MSelectedInjectorTracers", "Injector Tracers", "", "", "" );
    m_selectedInjectorTracersUiField.xmlCapability()->disableIO();
    m_selectedInjectorTracersUiField.uiCapability()->setUiEditorTypeName(
        caf::PdmUiTreeSelectionEditor::uiEditorTypeName() );
    m_selectedInjectorTracersUiField.uiCapability()->setUiLabelPosition( caf::PdmUiItemInfo::HIDDEN );

    CAF_PDM_InitFieldNoDefault( &m_selectedProducerTracersUiField, "MSelectedProducerTracers", "Producer Tracers", "", "", "" );
    m_selectedProducerTracersUiField.xmlCapability()->disableIO();
    m_selectedProducerTracersUiField.uiCapability()->setUiEditorTypeName(
        caf::PdmUiTreeSelectionEditor::uiEditorTypeName() );
    m_selectedProducerTracersUiField.uiCapability()->setUiLabelPosition( caf::PdmUiItemInfo::HIDDEN );

    CAF_PDM_InitFieldNoDefault( &m_selectedSouringTracersUiField, "MSelectedSouringTracers", "Tracers", "", "", "" );
    m_selectedSouringTracersUiField.xmlCapability()->disableIO();
    m_selectedSouringTracersUiField.uiCapability()->setUiEditorTypeName( caf::PdmUiListEditor::uiEditorTypeName() );
    m_selectedSouringTracersUiField.uiCapability()->setUiLabelPosition( m_labelPosition );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimEclipseResultDefinition::~RimEclipseResultDefinition()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::simpleCopy( const RimEclipseResultDefinition* other )
{
    this->setResultVariable( other->resultVariable() );
    this->setPorosityModel( other->porosityModel() );
    this->setResultType( other->resultType() );
    this->setFlowSolution( other->m_flowSolution() );
    this->setSelectedInjectorTracers( other->m_selectedInjectorTracers() );
    this->setSelectedProducerTracers( other->m_selectedProducerTracers() );
    this->setSelectedSouringTracers( other->m_selectedSouringTracers() );
    m_flowTracerSelectionMode = other->m_flowTracerSelectionMode();
    m_phaseSelection          = other->m_phaseSelection;

    m_differenceCase        = other->m_differenceCase();
    m_timeLapseBaseTimestep = other->m_timeLapseBaseTimestep();
    m_divideByCellFaceArea  = other->m_divideByCellFaceArea();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setEclipseCase( RimEclipseCase* eclipseCase )
{
    m_eclipseCase = eclipseCase;

    assignFlowSolutionFromCase();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimEclipseCase* RimEclipseResultDefinition::eclipseCase() const
{
    return m_eclipseCase;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RigCaseCellResultsData* RimEclipseResultDefinition::currentGridCellResults() const
{
    if ( !m_eclipseCase ) return nullptr;

    return m_eclipseCase->results( m_porosityModel() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::fieldChangedByUi( const caf::PdmFieldHandle* changedField,
                                                   const QVariant&            oldValue,
                                                   const QVariant&            newValue )
{
    if ( &m_flowSolutionUiField == changedField || &m_resultTypeUiField == changedField ||
         &m_porosityModelUiField == changedField )
    {
        // If the user are seeing the list with the actually selected result,
        // select that result in the list. Otherwise select nothing.

        QStringList varList = getResultNamesForResultType( m_resultTypeUiField(), this->currentGridCellResults() );

        bool isFlowDiagFieldsRelevant = ( m_resultType() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS );

        if ( ( m_flowSolutionUiField() == m_flowSolution() || !isFlowDiagFieldsRelevant ) &&
             m_resultTypeUiField() == m_resultType() && m_porosityModelUiField() == m_porosityModel() )
        {
            if ( varList.contains( resultVariable() ) )
            {
                m_resultVariableUiField = resultVariable();
            }

            if ( isFlowDiagFieldsRelevant )
            {
                m_selectedInjectorTracersUiField = m_selectedInjectorTracers();
                m_selectedProducerTracersUiField = m_selectedProducerTracers();
            }
            else
            {
                m_selectedInjectorTracersUiField = std::vector<QString>();
                m_selectedProducerTracersUiField = std::vector<QString>();
            }
        }
        else
        {
            m_resultVariableUiField          = "";
            m_selectedInjectorTracersUiField = std::vector<QString>();
            m_selectedProducerTracersUiField = std::vector<QString>();
        }
    }

    if ( &m_resultVariableUiField == changedField )
    {
        m_porosityModel  = m_porosityModelUiField;
        m_resultType     = m_resultTypeUiField;
        m_resultVariable = m_resultVariableUiField;

        if ( m_resultTypeUiField() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
        {
            m_flowSolution            = m_flowSolutionUiField();
            m_selectedInjectorTracers = m_selectedInjectorTracersUiField();
            m_selectedProducerTracers = m_selectedProducerTracersUiField();
        }
        else if ( m_resultTypeUiField() == RiaDefines::ResultCatType::INJECTION_FLOODING )
        {
            m_selectedSouringTracers = m_selectedSouringTracersUiField();
        }
        else if ( m_resultTypeUiField() == RiaDefines::ResultCatType::INPUT_PROPERTY )
        {
            m_inputPropertyFileName = getInputPropertyFileName( newValue.toString() );
        }
        loadDataAndUpdate();
    }

    if ( &m_porosityModelUiField == changedField )
    {
        m_porosityModel         = m_porosityModelUiField;
        m_resultVariableUiField = resultVariable();

        RimEclipseView* eclipseView = nullptr;
        this->firstAncestorOrThisOfType( eclipseView );
        if ( eclipseView )
        {
            // Active cells can be different between matrix and fracture, make sure all geometry is recreated
            eclipseView->scheduleReservoirGridGeometryRegen();
        }

        loadDataAndUpdate();
    }

    RimEclipseContourMapView* contourMapView = nullptr;
    this->firstAncestorOrThisOfType( contourMapView );

    if ( &m_differenceCase == changedField )
    {
        m_timeLapseBaseTimestep = RigEclipseResultAddress::noTimeLapseValue();

        if ( contourMapView )
        {
            contourMapView->contourMapProjection()->updatedWeightingResult();
        }

        loadDataAndUpdate();
    }

    if ( &m_timeLapseBaseTimestep == changedField )
    {
        if ( contourMapView )
        {
            contourMapView->contourMapProjection()->updatedWeightingResult();
        }

        loadDataAndUpdate();
    }

    if ( &m_divideByCellFaceArea == changedField )
    {
        loadDataAndUpdate();
    }

    if ( &m_flowTracerSelectionMode == changedField )
    {
        loadDataAndUpdate();
    }

    if ( &m_selectedInjectorTracersUiField == changedField )
    {
        changedTracerSelectionField( true );
    }

    if ( &m_selectedProducerTracersUiField == changedField )
    {
        changedTracerSelectionField( false );
    }

    if ( &m_syncInjectorToProducerSelection == changedField )
    {
        syncInjectorToProducerSelection();
        m_syncInjectorToProducerSelection = false;
    }

    if ( &m_syncProducerToInjectorSelection == changedField )
    {
        syncProducerToInjectorSelection();
        m_syncProducerToInjectorSelection = false;
    }

    if ( &m_selectedSouringTracersUiField == changedField )
    {
        if ( !m_resultVariable().isEmpty() )
        {
            m_selectedSouringTracers = m_selectedSouringTracersUiField();
            loadDataAndUpdate();
        }
    }

    if ( &m_phaseSelection == changedField )
    {
        if ( m_phaseSelection() != RigFlowDiagResultAddress::PHASE_ALL )
        {
            m_resultType            = m_resultTypeUiField;
            m_resultVariable        = RIG_FLD_TOF_RESNAME;
            m_resultVariableUiField = RIG_FLD_TOF_RESNAME;
        }
        loadDataAndUpdate();
    }

    updateAnyFieldHasChanged();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::changedTracerSelectionField( bool injector )
{
    m_flowSolution = m_flowSolutionUiField();

    std::vector<QString>& selectedTracers = injector ? m_selectedInjectorTracers.v() : m_selectedProducerTracers.v();
    const std::vector<QString>& selectedTracersUi = injector ? m_selectedInjectorTracersUiField.v()
                                                             : m_selectedProducerTracersUiField.v();

    selectedTracers = selectedTracersUi;

    loadDataAndUpdate();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::updateAnyFieldHasChanged()
{
    RimEclipsePropertyFilter* propFilter = nullptr;
    this->firstAncestorOrThisOfType( propFilter );
    if ( propFilter )
    {
        propFilter->updateConnectedEditors();
    }

    RimEclipseFaultColors* faultColors = nullptr;
    this->firstAncestorOrThisOfType( faultColors );
    if ( faultColors )
    {
        faultColors->updateConnectedEditors();
    }

    RimCellEdgeColors* cellEdgeColors = nullptr;
    this->firstAncestorOrThisOfType( cellEdgeColors );
    if ( cellEdgeColors )
    {
        cellEdgeColors->updateConnectedEditors();
    }

    RimEclipseCellColors* cellColors = nullptr;
    this->firstAncestorOrThisOfType( cellColors );
    if ( cellColors )
    {
        cellColors->updateConnectedEditors();
    }

    RimIntersectionResultDefinition* intersectResDef = nullptr;
    this->firstAncestorOrThisOfType( intersectResDef );
    if ( intersectResDef )
    {
        intersectResDef->updateConnectedEditors();
    }

    RimGridCrossPlotDataSet* crossPlotCurveSet = nullptr;
    this->firstAncestorOrThisOfType( crossPlotCurveSet );
    if ( crossPlotCurveSet )
    {
        crossPlotCurveSet->updateConnectedEditors();
    }

    RimPlotCurve* curve = nullptr;
    this->firstAncestorOrThisOfType( curve );
    if ( curve )
    {
        curve->updateConnectedEditors();
    }

    Rim3dWellLogCurve* rim3dWellLogCurve = nullptr;
    this->firstAncestorOrThisOfType( rim3dWellLogCurve );
    if ( rim3dWellLogCurve )
    {
        rim3dWellLogCurve->resetMinMaxValues();
    }

    RimEclipseContourMapProjection* contourMap = nullptr;
    this->firstAncestorOrThisOfType( contourMap );
    if ( contourMap )
    {
        contourMap->updatedWeightingResult();
    }

    RimWellLogTrack* wellLogTrack = nullptr;
    this->firstAncestorOrThisOfType( wellLogTrack );
    if ( wellLogTrack )
    {
        wellLogTrack->loadDataAndUpdate();
        wellLogTrack->updateEditors();
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setTofAndSelectTracer( const QString& tracerName )
{
    setResultType( RiaDefines::ResultCatType::FLOW_DIAGNOSTICS );
    setResultVariable( "TOF" );
    setFlowDiagTracerSelectionType( FLOW_TR_BY_SELECTION );

    if ( m_flowSolution() == nullptr )
    {
        assignFlowSolutionFromCase();
    }

    if ( m_flowSolution() )
    {
        RimFlowDiagSolution::TracerStatusType tracerStatus = m_flowSolution()->tracerStatusOverall( tracerName );

        std::vector<QString> tracers;
        tracers.push_back( tracerName );
        if ( ( tracerStatus == RimFlowDiagSolution::TracerStatusType::INJECTOR ) ||
             ( tracerStatus == RimFlowDiagSolution::TracerStatusType::VARYING ) )
        {
            setSelectedInjectorTracers( tracers );
        }

        if ( ( tracerStatus == RimFlowDiagSolution::TracerStatusType::PRODUCER ) ||
             ( tracerStatus == RimFlowDiagSolution::TracerStatusType::VARYING ) )
        {
            setSelectedProducerTracers( tracers );
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::loadDataAndUpdate()
{
    Rim3dView* view = nullptr;
    this->firstAncestorOrThisOfType( view );

    loadResult();

    RimEclipsePropertyFilter* propFilter = nullptr;
    this->firstAncestorOrThisOfType( propFilter );
    if ( propFilter )
    {
        propFilter->setToDefaultValues();
        propFilter->updateFilterName();

        if ( view )
        {
            view->scheduleGeometryRegen( PROPERTY_FILTERED );
            view->scheduleCreateDisplayModelAndRedraw();
        }
    }

    RimEclipseCellColors* cellColors = nullptr;
    this->firstAncestorOrThisOfType( cellColors );
    if ( cellColors )
    {
        this->updateLegendCategorySettings();

        if ( view )
        {
            RimViewLinker* viewLinker = view->assosiatedViewLinker();
            if ( viewLinker )
            {
                viewLinker->updateCellResult();
            }
            RimGridView* eclView = dynamic_cast<RimGridView*>( view );
            if ( eclView ) eclView->intersectionCollection()->scheduleCreateDisplayModelAndRedraw2dIntersectionViews();
        }
    }

    RimIntersectionResultDefinition* sepIntersectionResDef = nullptr;
    this->firstAncestorOrThisOfType( sepIntersectionResDef );
    if ( sepIntersectionResDef && sepIntersectionResDef->isInAction() )
    {
        if ( view ) view->scheduleCreateDisplayModelAndRedraw();
        RimGridView* gridView = dynamic_cast<RimGridView*>( view );
        if ( gridView ) gridView->intersectionCollection()->scheduleCreateDisplayModelAndRedraw2dIntersectionViews();
    }

    RimCellEdgeColors* cellEdgeColors = nullptr;
    this->firstAncestorOrThisOfType( cellEdgeColors );
    if ( cellEdgeColors )
    {
        cellEdgeColors->singleVarEdgeResultColors()->updateLegendCategorySettings();
        cellEdgeColors->loadResult();

        if ( view )
        {
            view->scheduleCreateDisplayModelAndRedraw();
        }
    }

    RimGridCrossPlotDataSet* crossPlotCurveSet = nullptr;
    this->firstAncestorOrThisOfType( crossPlotCurveSet );
    if ( crossPlotCurveSet )
    {
        crossPlotCurveSet->destroyCurves();
        crossPlotCurveSet->loadDataAndUpdate( true );
    }

    RimPlotCurve* curve = nullptr;
    this->firstAncestorOrThisOfType( curve );
    if ( curve )
    {
        curve->loadDataAndUpdate( true );
    }

    Rim3dWellLogCurve* rim3dWellLogCurve = nullptr;
    this->firstAncestorOrThisOfType( rim3dWellLogCurve );
    if ( rim3dWellLogCurve )
    {
        rim3dWellLogCurve->updateCurveIn3dView();
    }

#ifdef USE_QTCHARTS
    RimGridStatisticsPlot* gridStatisticsPlot = nullptr;
    this->firstAncestorOrThisOfType( gridStatisticsPlot );
    if ( gridStatisticsPlot )
    {
        gridStatisticsPlot->loadDataAndUpdate();
    }
#endif
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QList<caf::PdmOptionItemInfo>
    RimEclipseResultDefinition::calculateValueOptions( const caf::PdmFieldHandle* fieldNeedingOptions, bool* useOptionsOnly )
{
    QList<caf::PdmOptionItemInfo> options;

    if ( fieldNeedingOptions == &m_resultTypeUiField )
    {
        bool                  hasSourSimRLFile = false;
        RimEclipseResultCase* eclResCase       = dynamic_cast<RimEclipseResultCase*>( m_eclipseCase.p() );
        if ( eclResCase && eclResCase->eclipseCaseData() )
        {
            hasSourSimRLFile = eclResCase->hasSourSimFile();
        }

#ifndef USE_HDF5
        // If using ResInsight without HDF5 support, ignore SourSim files and
        // do not show it as a result category.
        hasSourSimRLFile = false;
#endif

        bool enableSouring = false;

#ifdef USE_HDF5
        if ( m_eclipseCase.notNull() )
        {
            RigCaseCellResultsData* cellResultsData = m_eclipseCase->results( this->porosityModel() );

            if ( cellResultsData && cellResultsData->hasFlowDiagUsableFluxes() )
            {
                enableSouring = true;
            }
        }
#endif /* USE_HDF5 */

        RimGridTimeHistoryCurve* timeHistoryCurve;
        this->firstAncestorOrThisOfType( timeHistoryCurve );

        bool isSeparateFaultResult = false;
        {
            RimEclipseFaultColors* sepFaultResult;
            this->firstAncestorOrThisOfType( sepFaultResult );
            if ( sepFaultResult ) isSeparateFaultResult = true;
        }

        using ResCatEnum = caf::AppEnum<RiaDefines::ResultCatType>;
        for ( size_t i = 0; i < ResCatEnum::size(); ++i )
        {
            RiaDefines::ResultCatType resType = ResCatEnum::fromIndex( i );

            // Do not include flow diagnostics results if it is a time history curve

            if ( resType == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS && ( timeHistoryCurve ) )
            {
                continue;
            }

            if ( resType == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS && m_eclipseCase &&
                 m_eclipseCase->eclipseCaseData()->hasFractureResults() )
            {
                // Flow diagnostics is not supported for dual porosity models
                continue;
            }

            // Do not include SourSimRL if no SourSim file is loaded

            if ( resType == RiaDefines::ResultCatType::SOURSIMRL && ( !hasSourSimRLFile ) )
            {
                continue;
            }

            if ( resType == RiaDefines::ResultCatType::INJECTION_FLOODING && !enableSouring )
            {
                continue;
            }

            if ( resType == RiaDefines::ResultCatType::ALLAN_DIAGRAMS && !isSeparateFaultResult )
            {
                continue;
            }

            QString uiString = ResCatEnum::uiTextFromIndex( i );
            options.push_back( caf::PdmOptionItemInfo( uiString, resType ) );
        }
    }

    if ( m_resultTypeUiField() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
    {
        if ( fieldNeedingOptions == &m_resultVariableUiField )
        {
            options.push_back( caf::PdmOptionItemInfo( timeOfFlightString( false ), RIG_FLD_TOF_RESNAME ) );
            if ( m_phaseSelection() == RigFlowDiagResultAddress::PHASE_ALL )
            {
                options.push_back( caf::PdmOptionItemInfo( "Tracer Cell Fraction (Sum)", RIG_FLD_CELL_FRACTION_RESNAME ) );
                options.push_back(
                    caf::PdmOptionItemInfo( maxFractionTracerString( false ), RIG_FLD_MAX_FRACTION_TRACER_RESNAME ) );
                options.push_back(
                    caf::PdmOptionItemInfo( "Injector Producer Communication", RIG_FLD_COMMUNICATION_RESNAME ) );
            }
        }
        else if ( fieldNeedingOptions == &m_flowSolutionUiField )
        {
            RimEclipseResultCase* eclCase = dynamic_cast<RimEclipseResultCase*>( m_eclipseCase.p() );
            if ( eclCase )
            {
                std::vector<RimFlowDiagSolution*> flowSols = eclCase->flowDiagSolutions();
                for ( RimFlowDiagSolution* flowSol : flowSols )
                {
                    options.push_back( caf::PdmOptionItemInfo( flowSol->userDescription(), flowSol ) );
                }
            }
        }
        else if ( fieldNeedingOptions == &m_selectedInjectorTracersUiField )
        {
            options = calcOptionsForSelectedTracerField( true );
        }
        else if ( fieldNeedingOptions == &m_selectedProducerTracersUiField )
        {
            options = calcOptionsForSelectedTracerField( false );
        }
    }
    else if ( m_resultTypeUiField() == RiaDefines::ResultCatType::INJECTION_FLOODING )
    {
        if ( fieldNeedingOptions == &m_selectedSouringTracersUiField )
        {
            RigCaseCellResultsData* cellResultsStorage = currentGridCellResults();
            if ( cellResultsStorage )
            {
                QStringList dynamicResultNames =
                    cellResultsStorage->resultNames( RiaDefines::ResultCatType::DYNAMIC_NATIVE );

                for ( const QString& resultName : dynamicResultNames )
                {
                    if ( !resultName.endsWith( "F" ) || resultName == RiaResultNames::completionTypeResultName() )
                    {
                        continue;
                    }
                    options.push_back( caf::PdmOptionItemInfo( resultName, resultName ) );
                }
            }
        }
        else if ( fieldNeedingOptions == &m_resultVariableUiField )
        {
            options.push_back( caf::PdmOptionItemInfo( RIG_NUM_FLOODED_PV, RIG_NUM_FLOODED_PV ) );
        }
    }
    else
    {
        if ( fieldNeedingOptions == &m_resultVariableUiField )
        {
            options = calcOptionsForVariableUiFieldStandard( m_resultTypeUiField(),
                                                             this->currentGridCellResults(),
                                                             showDerivedResultsFirstInVariableUiField(),
                                                             addPerCellFaceOptionsForVariableUiField(),
                                                             m_ternaryEnabled );
        }
        else if ( fieldNeedingOptions == &m_differenceCase )
        {
            options.push_back( caf::PdmOptionItemInfo( "None", nullptr ) );

            RimEclipseCase* eclipseCase = nullptr;
            this->firstAncestorOrThisOfTypeAsserted( eclipseCase );
            if ( eclipseCase && eclipseCase->eclipseCaseData() && eclipseCase->eclipseCaseData()->mainGrid() )
            {
                RimProject* proj = nullptr;
                eclipseCase->firstAncestorOrThisOfTypeAsserted( proj );

                std::vector<RimEclipseCase*> allCases = proj->eclipseCases();
                for ( RimEclipseCase* otherCase : allCases )
                {
                    if ( otherCase == eclipseCase ) continue;

                    if ( otherCase->eclipseCaseData() && otherCase->eclipseCaseData()->mainGrid() )
                    {
                        options.push_back( caf::PdmOptionItemInfo( QString( "%1 (#%2)" )
                                                                       .arg( otherCase->caseUserDescription() )
                                                                       .arg( otherCase->caseId() ),
                                                                   otherCase,
                                                                   false,
                                                                   otherCase->uiIconProvider() ) );
                    }
                }
            }
        }
        else if ( fieldNeedingOptions == &m_timeLapseBaseTimestep )
        {
            RimEclipseCase* currentCase = nullptr;
            this->firstAncestorOrThisOfTypeAsserted( currentCase );

            RimEclipseCase* baseCase = currentCase;
            if ( m_differenceCase )
            {
                baseCase = m_differenceCase;
            }

            options.push_back( caf::PdmOptionItemInfo( "Disabled", RigEclipseResultAddress::noTimeLapseValue() ) );

            std::vector<QDateTime> stepDates = baseCase->timeStepDates();
            for ( size_t stepIdx = 0; stepIdx < stepDates.size(); ++stepIdx )
            {
                QString displayString = stepDates[stepIdx].toString( RiaQDateTimeTools::dateFormatString() );
                displayString += QString( " (#%1)" ).arg( stepIdx );

                options.push_back( caf::PdmOptionItemInfo( displayString, static_cast<int>( stepIdx ) ) );
            }
        }
    }

    ( *useOptionsOnly ) = true;

    return options;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RigEclipseResultAddress RimEclipseResultDefinition::eclipseResultAddress() const
{
    if ( isFlowDiagOrInjectionFlooding() ) return RigEclipseResultAddress();

    const RigCaseCellResultsData* gridCellResults = this->currentGridCellResults();
    if ( gridCellResults )
    {
        int timelapseTimeStep = RigEclipseResultAddress::noTimeLapseValue();
        int diffCaseId        = RigEclipseResultAddress::noCaseDiffValue();

        if ( isDeltaTimeStepActive() )
        {
            timelapseTimeStep = m_timeLapseBaseTimestep();
        }

        if ( isDeltaCaseActive() )
        {
            diffCaseId = m_differenceCase->caseId();
        }

        return RigEclipseResultAddress( m_resultType(),
                                        m_resultVariable(),
                                        timelapseTimeStep,
                                        diffCaseId,
                                        isDivideByCellFaceAreaActive() );
    }
    else
    {
        return RigEclipseResultAddress();
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setFromEclipseResultAddress( const RigEclipseResultAddress& address )
{
    RigEclipseResultAddress canonizedAddress = address;

    const RigCaseCellResultsData* gridCellResults = this->currentGridCellResults();
    if ( gridCellResults )
    {
        auto rinfo = gridCellResults->resultInfo( address );
        if ( rinfo ) canonizedAddress = rinfo->eclipseResultAddress();
    }

    m_resultType            = canonizedAddress.resultCatType();
    m_resultVariable        = canonizedAddress.resultName();
    m_timeLapseBaseTimestep = canonizedAddress.deltaTimeStepIndex();
    m_divideByCellFaceArea  = canonizedAddress.isDivideByCellFaceAreaActive();

    if ( canonizedAddress.isDeltaCaseActive() )
    {
        auto eclipseCases = RimProject::current()->eclipseCases();
        for ( RimEclipseCase* c : eclipseCases )
        {
            if ( c && c->caseId() == canonizedAddress.deltaCaseId() )
            {
                m_differenceCase = c;
            }
        }
    }

    this->updateUiFieldsFromActiveResult();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RigFlowDiagResultAddress RimEclipseResultDefinition::flowDiagResAddress() const
{
    CVF_ASSERT( isFlowDiagOrInjectionFlooding() );

    if ( m_resultType() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
    {
        size_t timeStep = 0;

        Rim3dView* rimView = nullptr;
        this->firstAncestorOrThisOfType( rimView );
        if ( rimView )
        {
            timeStep = rimView->currentTimeStep();
        }
        RimWellLogExtractionCurve* wellLogExtractionCurve = nullptr;
        this->firstAncestorOrThisOfType( wellLogExtractionCurve );
        if ( wellLogExtractionCurve )
        {
            timeStep = static_cast<size_t>( wellLogExtractionCurve->currentTimeStep() );
        }

        // Time history curves are not supported, since it requires the time
        // step to access to be supplied.
        RimGridTimeHistoryCurve* timeHistoryCurve = nullptr;
        this->firstAncestorOrThisOfType( timeHistoryCurve );
        CVF_ASSERT( timeHistoryCurve == nullptr );

        std::set<std::string> selTracerNames;
        if ( m_flowTracerSelectionMode == FLOW_TR_BY_SELECTION )
        {
            for ( const QString& tName : m_selectedInjectorTracers() )
            {
                selTracerNames.insert( tName.toStdString() );
            }
            for ( const QString& tName : m_selectedProducerTracers() )
            {
                selTracerNames.insert( tName.toStdString() );
            }
        }
        else
        {
            RimFlowDiagSolution* flowSol = m_flowSolution();
            if ( flowSol )
            {
                std::vector<QString> tracerNames = flowSol->tracerNames();

                if ( m_flowTracerSelectionMode == FLOW_TR_INJECTORS || m_flowTracerSelectionMode == FLOW_TR_INJ_AND_PROD )
                {
                    for ( const QString& tracerName : tracerNames )
                    {
                        RimFlowDiagSolution::TracerStatusType status =
                            flowSol->tracerStatusInTimeStep( tracerName, timeStep );
                        if ( status == RimFlowDiagSolution::TracerStatusType::INJECTOR )
                        {
                            selTracerNames.insert( tracerName.toStdString() );
                        }
                    }
                }

                if ( m_flowTracerSelectionMode == FLOW_TR_PRODUCERS || m_flowTracerSelectionMode == FLOW_TR_INJ_AND_PROD )
                {
                    for ( const QString& tracerName : tracerNames )
                    {
                        RimFlowDiagSolution::TracerStatusType status =
                            flowSol->tracerStatusInTimeStep( tracerName, timeStep );
                        if ( status == RimFlowDiagSolution::TracerStatusType::PRODUCER )
                        {
                            selTracerNames.insert( tracerName.toStdString() );
                        }
                    }
                }
            }
        }

        return RigFlowDiagResultAddress( m_resultVariable().toStdString(), m_phaseSelection(), selTracerNames );
    }
    else
    {
        std::set<std::string> selTracerNames;
        for ( const QString& selectedTracerName : m_selectedSouringTracers() )
        {
            selTracerNames.insert( selectedTracerName.toUtf8().constData() );
        }
        return RigFlowDiagResultAddress( m_resultVariable().toStdString(),
                                         RigFlowDiagResultAddress::PHASE_ALL,
                                         selTracerNames );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setFlowDiagTracerSelectionType( FlowTracerSelectionType selectionType )
{
    m_flowTracerSelectionMode = selectionType;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::resultVariableUiName() const
{
    if ( resultType() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
    {
        return flowDiagResUiText( false, 32 );
    }

    if ( isDivideByCellFaceAreaActive() )
    {
        return m_resultVariable() + " /A";
    }

    return m_resultVariable();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::resultVariableUiShortName() const
{
    if ( resultType() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
    {
        return flowDiagResUiText( true, 24 );
    }

    if ( isDivideByCellFaceAreaActive() )
    {
        return m_resultVariable() + " /A";
    }

    return m_resultVariable();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::additionalResultText() const
{
    QStringList resultText;

    if ( isDeltaTimeStepActive() )
    {
        std::vector<QDateTime>        stepDates;
        const RigCaseCellResultsData* gridCellResults = this->currentGridCellResults();
        if ( gridCellResults )
        {
            stepDates = gridCellResults->timeStepDates();
            resultText +=
                QString( "<b>Base Time Step</b>: %1" )
                    .arg( stepDates[m_timeLapseBaseTimestep()].toString( RiaQDateTimeTools::dateFormatString() ) );
        }
    }
    if ( isDeltaCaseActive() )
    {
        resultText += QString( "<b>Base Case</b>: %1" ).arg( m_differenceCase()->caseUserDescription() );
    }
    return resultText.join( "<br>" );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::additionalResultTextShort() const
{
    QString resultTextShort;
    if ( isDeltaTimeStepActive() || isDeltaCaseActive() )
    {
        QStringList resultTextLines;
        resultTextLines += QString( "\nDiff. Options:" );
        if ( isDeltaCaseActive() )
        {
            resultTextLines += QString( "Base Case: #%1" ).arg( m_differenceCase()->caseId() );
        }
        if ( isDeltaTimeStepActive() )
        {
            resultTextLines += QString( "Base Time: #%1" ).arg( m_timeLapseBaseTimestep() );
        }
        resultTextShort = resultTextLines.join( "\n" );
    }

    return resultTextShort;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
int RimEclipseResultDefinition::timeLapseBaseTimeStep() const
{
    return m_timeLapseBaseTimestep;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
int RimEclipseResultDefinition::caseDiffIndex() const
{
    if ( m_differenceCase )
    {
        return m_differenceCase->caseId();
    }
    return -1;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::loadResult()
{
    if ( isFlowDiagOrInjectionFlooding() ) return; // Will load automatically on access

    if ( m_eclipseCase )
    {
        if ( !m_eclipseCase->ensureReservoirCaseIsOpen() )
        {
            RiaLogging::error( "Could not open the Eclipse Grid file: " + m_eclipseCase->gridFileName() );
            return;
        }
    }

    if ( m_differenceCase )
    {
        if ( !m_differenceCase->ensureReservoirCaseIsOpen() )
        {
            RiaLogging::error( "Could not open the Eclipse Grid file: " + m_eclipseCase->gridFileName() );
            return;
        }
    }

    RigCaseCellResultsData* gridCellResults = this->currentGridCellResults();
    if ( gridCellResults )
    {
        if ( isDeltaTimeStepActive() || isDeltaCaseActive() || isDivideByCellFaceAreaActive() )
        {
            gridCellResults->createResultEntry( this->eclipseResultAddress(), false );
        }

        QString           resultName                    = m_resultVariable();
        std::set<QString> eclipseResultNamesWithNncData = RiaResultNames::nncResultNames();
        if ( eclipseResultNamesWithNncData.find( resultName ) != eclipseResultNamesWithNncData.end() )
        {
            eclipseCase()->ensureFaultDataIsComputed();

            bool dataWasComputed = eclipseCase()->ensureNncDataIsComputed();
            if ( dataWasComputed )
            {
                eclipseCase()->createDisplayModelAndUpdateAllViews();
            }
        }

        gridCellResults->ensureKnownResultLoaded( this->eclipseResultAddress() );
    }
}

//--------------------------------------------------------------------------------------------------
/// Returns whether the result requested by the definition is a single frame result
/// The result needs to be loaded before asking
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::hasStaticResult() const
{
    if ( isFlowDiagOrInjectionFlooding() ) return false;

    const RigCaseCellResultsData* gridCellResults       = this->currentGridCellResults();
    RigEclipseResultAddress       gridScalarResultIndex = this->eclipseResultAddress();

    if ( hasResult() && gridCellResults->timeStepCount( gridScalarResultIndex ) == 1 )
    {
        return true;
    }
    else
    {
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
/// Returns whether the result requested by the definition is loaded or possible to load from the result file
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::hasResult() const
{
    if ( isFlowDiagOrInjectionFlooding() )
    {
        if ( m_flowSolution() && !m_resultVariable().isEmpty() ) return true;
    }
    else if ( this->currentGridCellResults() )
    {
        const RigCaseCellResultsData* gridCellResults = this->currentGridCellResults();

        return gridCellResults->hasResultEntry( this->eclipseResultAddress() );
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/// Returns whether the result requested by the definition is a multi frame result
/// The result needs to be loaded before asking
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::hasDynamicResult() const
{
    if ( hasResult() )
    {
        if ( m_resultType() == RiaDefines::ResultCatType::DYNAMIC_NATIVE )
        {
            return true;
        }
        else if ( m_resultType() == RiaDefines::ResultCatType::SOURSIMRL )
        {
            return true;
        }
        else if ( m_resultType() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
        {
            return true;
        }
        else if ( m_resultType() == RiaDefines::ResultCatType::INJECTION_FLOODING )
        {
            return true;
        }

        if ( this->currentGridCellResults() )
        {
            const RigCaseCellResultsData* gridCellResults       = this->currentGridCellResults();
            RigEclipseResultAddress       gridScalarResultIndex = this->eclipseResultAddress();
            if ( gridCellResults->timeStepCount( gridScalarResultIndex ) > 1 )
            {
                return true;
            }
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::initAfterRead()
{
    if ( m_flowSolution() == nullptr )
    {
        assignFlowSolutionFromCase();
    }

    if ( m_resultVariable == "Formation Allen" )
    {
        m_resultVariable = RiaResultNames::formationAllanResultName();
        m_resultType     = RiaDefines::ResultCatType::ALLAN_DIAGRAMS;
    }
    else if ( m_resultVariable == "Binary Formation Allen" )
    {
        m_resultVariable = RiaResultNames::formationBinaryAllanResultName();
        m_resultType     = RiaDefines::ResultCatType::ALLAN_DIAGRAMS;
    }

    m_porosityModelUiField  = m_porosityModel;
    m_resultTypeUiField     = m_resultType;
    m_resultVariableUiField = m_resultVariable;

    m_flowSolutionUiField            = m_flowSolution();
    m_selectedInjectorTracersUiField = m_selectedInjectorTracers;

    this->updateUiIconFromToggleField();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setResultType( RiaDefines::ResultCatType val )
{
    m_resultType        = val;
    m_resultTypeUiField = val;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setPorosityModel( RiaDefines::PorosityModelType val )
{
    m_porosityModel        = val;
    m_porosityModelUiField = val;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setResultVariable( const QString& val )
{
    m_resultVariable        = val;
    m_resultVariableUiField = val;
}

//--------------------------------------------------------------------------------------------------
/// Return phase type if the current result is known to be of a particular
/// fluid phase type. Otherwise the method will return PHASE_NOT_APPLICABLE.
//--------------------------------------------------------------------------------------------------
RiaDefines::PhaseType RimEclipseResultDefinition::resultPhaseType() const
{
    if ( QRegularExpression( "OIL" ).match( m_resultVariable() ).hasMatch() )
    {
        return RiaDefines::PhaseType::OIL_PHASE;
    }
    else if ( QRegularExpression( "GAS" ).match( m_resultVariable() ).hasMatch() )
    {
        return RiaDefines::PhaseType::GAS_PHASE;
    }
    else if ( QRegularExpression( "WAT" ).match( m_resultVariable() ).hasMatch() )
    {
        return RiaDefines::PhaseType::WATER_PHASE;
    }
    return RiaDefines::PhaseType::PHASE_NOT_APPLICABLE;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimFlowDiagSolution* RimEclipseResultDefinition::flowDiagSolution() const
{
    return m_flowSolution();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setFlowSolution( RimFlowDiagSolution* flowSol )
{
    this->m_flowSolution        = flowSol;
    this->m_flowSolutionUiField = flowSol;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setSelectedTracers( const std::vector<QString>& selectedTracers )
{
    if ( m_flowSolution() == nullptr )
    {
        assignFlowSolutionFromCase();
    }
    if ( m_flowSolution() )
    {
        std::vector<QString> injectorTracers;
        std::vector<QString> producerTracers;
        for ( const QString& tracerName : selectedTracers )
        {
            RimFlowDiagSolution::TracerStatusType tracerStatus = m_flowSolution()->tracerStatusOverall( tracerName );
            if ( tracerStatus == RimFlowDiagSolution::TracerStatusType::INJECTOR )
            {
                injectorTracers.push_back( tracerName );
            }
            else if ( tracerStatus == RimFlowDiagSolution::TracerStatusType::PRODUCER )
            {
                producerTracers.push_back( tracerName );
            }
            else if ( tracerStatus == RimFlowDiagSolution::TracerStatusType::VARYING ||
                      tracerStatus == RimFlowDiagSolution::TracerStatusType::UNDEFINED )
            {
                injectorTracers.push_back( tracerName );
                producerTracers.push_back( tracerName );
            }
        }
        setSelectedInjectorTracers( injectorTracers );
        setSelectedProducerTracers( producerTracers );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setSelectedInjectorTracers( const std::vector<QString>& selectedTracers )
{
    this->m_selectedInjectorTracers        = selectedTracers;
    this->m_selectedInjectorTracersUiField = selectedTracers;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setSelectedProducerTracers( const std::vector<QString>& selectedTracers )
{
    this->m_selectedProducerTracers        = selectedTracers;
    this->m_selectedProducerTracersUiField = selectedTracers;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setSelectedSouringTracers( const std::vector<QString>& selectedTracers )
{
    this->m_selectedSouringTracers        = selectedTracers;
    this->m_selectedSouringTracersUiField = selectedTracers;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::updateUiFieldsFromActiveResult()
{
    m_resultTypeUiField              = m_resultType;
    m_resultVariableUiField          = resultVariable();
    m_selectedInjectorTracersUiField = m_selectedInjectorTracers;
    m_selectedProducerTracersUiField = m_selectedProducerTracers;
    m_selectedSouringTracersUiField  = m_selectedSouringTracers;
    m_porosityModelUiField           = m_porosityModel;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::enableDeltaResults( bool enable )
{
    m_isDeltaResultEnabled = enable;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isTernarySaturationSelected() const
{
    bool isTernary =
        ( m_resultType() == RiaDefines::ResultCatType::DYNAMIC_NATIVE ) &&
        ( m_resultVariable().compare( RiaResultNames::ternarySaturationResultName(), Qt::CaseInsensitive ) == 0 );

    return isTernary;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isCompletionTypeSelected() const
{
    return ( m_resultType() == RiaDefines::ResultCatType::DYNAMIC_NATIVE &&
             m_resultVariable() == RiaResultNames::completionTypeResultName() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::hasCategoryResult() const
{
    if ( this->m_resultType() == RiaDefines::ResultCatType::FORMATION_NAMES && m_eclipseCase &&
         m_eclipseCase->eclipseCaseData() && !m_eclipseCase->eclipseCaseData()->formationNames().empty() )
        return true;

    if ( this->m_resultType() == RiaDefines::ResultCatType::DYNAMIC_NATIVE &&
         this->resultVariable() == RiaResultNames::completionTypeResultName() )
        return true;

    if ( this->m_resultType() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS &&
         m_resultVariable() == RIG_FLD_MAX_FRACTION_TRACER_RESNAME )
        return true;

    if ( this->resultVariable() == RiaResultNames::formationAllanResultName() ||
         this->resultVariable() == RiaResultNames::formationBinaryAllanResultName() )
    {
        return true;
    }

    if ( !this->hasStaticResult() ) return false;

    return RiaDefines::isNativeCategoryResult( this->resultVariable() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isFlowDiagOrInjectionFlooding() const
{
    if ( this->m_resultType() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS ||
         this->m_resultType() == RiaDefines::ResultCatType::INJECTION_FLOODING )
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::defineUiOrdering( QString uiConfigName, caf::PdmUiOrdering& uiOrdering )
{
    uiOrdering.add( &m_resultTypeUiField );

    if ( hasDualPorFractureResult() )
    {
        uiOrdering.add( &m_porosityModelUiField );
    }

    if ( m_resultTypeUiField() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
    {
        uiOrdering.add( &m_flowSolutionUiField );

        uiOrdering.add( &m_flowTracerSelectionMode );

        if ( m_flowTracerSelectionMode == FLOW_TR_BY_SELECTION )
        {
            caf::PdmUiGroup* selectionGroup = uiOrdering.addNewGroup( "Tracer Selection" );
            selectionGroup->setEnableFrame( false );

            caf::PdmUiGroup* injectorGroup = selectionGroup->addNewGroup( "Injectors" );
            injectorGroup->add( &m_selectedInjectorTracersUiField );
            injectorGroup->add( &m_syncInjectorToProducerSelection );

            caf::PdmUiGroup* producerGroup = selectionGroup->addNewGroup( "Producers", false );
            producerGroup->add( &m_selectedProducerTracersUiField );
            producerGroup->add( &m_syncProducerToInjectorSelection );
        }

        uiOrdering.add( &m_phaseSelection );

        if ( m_flowSolution() == nullptr )
        {
            assignFlowSolutionFromCase();
        }
    }

    if ( m_resultTypeUiField() == RiaDefines::ResultCatType::INJECTION_FLOODING )
    {
        uiOrdering.add( &m_selectedSouringTracersUiField );
    }

    uiOrdering.add( &m_resultVariableUiField );
    if ( m_resultTypeUiField() == RiaDefines::ResultCatType::INPUT_PROPERTY )
    {
        uiOrdering.add( &m_inputPropertyFileName );
    }

    if ( isDivideByCellFaceAreaPossible() )
    {
        uiOrdering.add( &m_divideByCellFaceArea );

        QString resultPropertyLabel = "Result Property";
        if ( isDivideByCellFaceAreaActive() )
        {
            resultPropertyLabel += QString( "\nDivided by Area" );
        }
        m_resultVariableUiField.uiCapability()->setUiName( resultPropertyLabel );
    }

    caf::PdmUiGroup* legendGroup = uiOrdering.addNewGroup( "Legend" );
    legendGroup->add( &m_showOnlyVisibleTracersInLegend );

    bool showOnlyVisibleTracesOption = ( m_resultTypeUiField() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS &&
                                         m_resultVariableUiField() == RIG_FLD_MAX_FRACTION_TRACER_RESNAME );
    legendGroup->setUiHidden( !showOnlyVisibleTracesOption );

    if ( isDeltaCasePossible() || isDeltaTimeStepPossible() )
    {
        caf::PdmUiGroup* differenceGroup = uiOrdering.addNewGroup( "Difference Options" );
        differenceGroup->setUiReadOnly( !( isDeltaTimeStepPossible() || isDeltaCasePossible() ) );

        m_differenceCase.uiCapability()->setUiReadOnly( !isDeltaCasePossible() );
        m_timeLapseBaseTimestep.uiCapability()->setUiReadOnly( !isDeltaTimeStepPossible() );

        if ( isDeltaCasePossible() ) differenceGroup->add( &m_differenceCase );
        if ( isDeltaTimeStepPossible() ) differenceGroup->add( &m_timeLapseBaseTimestep );

        QString resultPropertyLabel = "Result Property";
        if ( isDeltaTimeStepActive() || isDeltaCaseActive() )
        {
            resultPropertyLabel += QString( "\n%1" ).arg( additionalResultTextShort() );
        }
        m_resultVariableUiField.uiCapability()->setUiName( resultPropertyLabel );
    }

    uiOrdering.skipRemainingFields( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::defineEditorAttribute( const caf::PdmFieldHandle* field,
                                                        QString                    uiConfigName,
                                                        caf::PdmUiEditorAttribute* attribute )
{
    if ( m_resultTypeUiField() == RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
    {
        if ( field == &m_resultVariableUiField )
        {
            caf::PdmUiListEditorAttribute* listEditAttr = dynamic_cast<caf::PdmUiListEditorAttribute*>( attribute );
            if ( listEditAttr )
            {
                listEditAttr->m_heightHint = 50;
            }
        }
        else if ( field == &m_syncInjectorToProducerSelection || field == &m_syncProducerToInjectorSelection )
        {
            caf::PdmUiToolButtonEditorAttribute* toolButtonAttr =
                dynamic_cast<caf::PdmUiToolButtonEditorAttribute*>( attribute );
            if ( toolButtonAttr )
            {
                toolButtonAttr->m_sizePolicy.setHorizontalPolicy( QSizePolicy::MinimumExpanding );
            }
        }
    }
    if ( field == &m_resultVariableUiField )
    {
        caf::PdmUiListEditorAttribute* listEditAttr = dynamic_cast<caf::PdmUiListEditorAttribute*>( attribute );
        if ( listEditAttr )
        {
            listEditAttr->m_allowHorizontalScrollBar = false;
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::TracerComp::operator()( const QString& lhs, const QString& rhs ) const
{
    if ( !lhs.endsWith( "-XF" ) && rhs.endsWith( "-XF" ) )
    {
        return true;
    }
    else if ( lhs.endsWith( "-XF" ) && !rhs.endsWith( "-XF" ) )
    {
        return false;
    }
    else
    {
        return lhs < rhs;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::assignFlowSolutionFromCase()
{
    RimFlowDiagSolution* defaultFlowDiagSolution = nullptr;

    RimEclipseResultCase* eclCase = dynamic_cast<RimEclipseResultCase*>( m_eclipseCase.p() );

    if ( eclCase )
    {
        defaultFlowDiagSolution = eclCase->defaultFlowDiagSolution();
    }
    this->setFlowSolution( defaultFlowDiagSolution );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::hasDualPorFractureResult()
{
    if ( m_eclipseCase && m_eclipseCase->eclipseCaseData() )
    {
        return m_eclipseCase->eclipseCaseData()->hasFractureResults();
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::flowDiagResUiText( bool shortLabel, int maxTracerStringLength ) const
{
    QString uiText = QString::fromStdString( flowDiagResAddress().variableName );
    if ( flowDiagResAddress().variableName == RIG_FLD_TOF_RESNAME )
    {
        uiText = timeOfFlightString( shortLabel );
    }
    else if ( flowDiagResAddress().variableName == RIG_FLD_MAX_FRACTION_TRACER_RESNAME )
    {
        uiText = maxFractionTracerString( shortLabel );
    }

    QString tracersString = selectedTracersString();

    if ( !tracersString.isEmpty() )
    {
        const QString postfix = "...";

        if ( tracersString.size() > maxTracerStringLength + postfix.size() )
        {
            tracersString = tracersString.left( maxTracerStringLength );
            tracersString += postfix;
        }
        uiText += QString( "\n%1" ).arg( tracersString );
    }
    return uiText;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QList<caf::PdmOptionItemInfo>
    RimEclipseResultDefinition::calcOptionsForVariableUiFieldStandard( RiaDefines::ResultCatType     resultCatType,
                                                                       const RigCaseCellResultsData* results,
                                                                       bool showDerivedResultsFirst,
                                                                       bool addPerCellFaceOptionItems,
                                                                       bool ternaryEnabled )
{
    CVF_ASSERT( resultCatType != RiaDefines::ResultCatType::FLOW_DIAGNOSTICS &&
                resultCatType != RiaDefines::ResultCatType::INJECTION_FLOODING );

    if ( results )
    {
        QList<caf::PdmOptionItemInfo> optionList;

        QStringList cellCenterResultNames;
        QStringList cellFaceResultNames;

        for ( const QString& s : getResultNamesForResultType( resultCatType, results ) )
        {
            if ( s == RiaResultNames::completionTypeResultName() )
            {
                if ( results->timeStepDates().empty() ) continue;
            }

            if ( RiaResultNames::isPerCellFaceResult( s ) )
            {
                cellFaceResultNames.push_back( s );
            }
            else
            {
                cellCenterResultNames.push_back( s );
            }
        }

        cellCenterResultNames.sort();
        cellFaceResultNames.sort();

        // Cell Center result names
        for ( const QString& s : cellCenterResultNames )
        {
            optionList.push_back( caf::PdmOptionItemInfo( s, s ) );
        }

        if ( addPerCellFaceOptionItems )
        {
            for ( const QString& s : cellFaceResultNames )
            {
                if ( showDerivedResultsFirst )
                {
                    optionList.push_front( caf::PdmOptionItemInfo( s, s ) );
                }
                else
                {
                    optionList.push_back( caf::PdmOptionItemInfo( s, s ) );
                }
            }

            // Ternary Result
            if ( ternaryEnabled )
            {
                bool hasAtLeastOneTernaryComponent = false;
                if ( cellCenterResultNames.contains( "SOIL" ) )
                    hasAtLeastOneTernaryComponent = true;
                else if ( cellCenterResultNames.contains( "SGAS" ) )
                    hasAtLeastOneTernaryComponent = true;
                else if ( cellCenterResultNames.contains( "SWAT" ) )
                    hasAtLeastOneTernaryComponent = true;

                if ( resultCatType == RiaDefines::ResultCatType::DYNAMIC_NATIVE && hasAtLeastOneTernaryComponent )
                {
                    optionList.push_front( caf::PdmOptionItemInfo( RiaResultNames::ternarySaturationResultName(),
                                                                   RiaResultNames::ternarySaturationResultName() ) );
                }
            }
        }

        optionList.push_front(
            caf::PdmOptionItemInfo( RiaResultNames::undefinedResultName(), RiaResultNames::undefinedResultName() ) );

        return optionList;
    }

    return QList<caf::PdmOptionItemInfo>();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::setTernaryEnabled( bool enabled )
{
    m_ternaryEnabled = enabled;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool operator<( const cvf::Color3ub first, const cvf::Color3ub second )
{
    if ( first.r() != second.r() ) return first.r() < second.r();
    if ( first.g() != second.g() ) return first.g() < second.g();
    if ( first.b() != second.b() ) return first.b() < second.b();

    return false;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
class TupleCompare
{
public:
    bool operator()( const std::tuple<QString, int, cvf::Color3ub>& t1,
                     const std::tuple<QString, int, cvf::Color3ub>& t2 ) const
    {
        using namespace std;
        if ( get<0>( t1 ) != get<0>( t2 ) ) return get<0>( t1 ) < get<0>( t2 );
        if ( get<1>( t1 ) != get<1>( t2 ) ) return get<1>( t1 ) < get<1>( t2 );
        if ( get<2>( t1 ) != get<2>( t2 ) ) return get<2>( t1 ) < get<2>( t2 );

        return false;
    }
};

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::updateRangesForExplicitLegends( RimRegularLegendConfig* legendConfigToUpdate,
                                                                 RimTernaryLegendConfig* ternaryLegendConfigToUpdate,
                                                                 int                     currentTimeStep )

{
    RimEclipseCase* rimEclipseCase = this->eclipseCase();

    if ( this->hasResult() )
    {
        if ( this->isFlowDiagOrInjectionFlooding() )
        {
            CVF_ASSERT( currentTimeStep >= 0 );

            double                   globalMin, globalMax;
            double                   globalPosClosestToZero, globalNegClosestToZero;
            RigFlowDiagResults*      flowResultsData = this->flowDiagSolution()->flowDiagResults();
            RigFlowDiagResultAddress resAddr         = this->flowDiagResAddress();

            flowResultsData->minMaxScalarValues( resAddr, currentTimeStep, &globalMin, &globalMax );
            flowResultsData->posNegClosestToZero( resAddr, currentTimeStep, &globalPosClosestToZero, &globalNegClosestToZero );

            double localMin, localMax;
            double localPosClosestToZero, localNegClosestToZero;
            if ( this->hasDynamicResult() )
            {
                flowResultsData->minMaxScalarValues( resAddr, currentTimeStep, &localMin, &localMax );
                flowResultsData->posNegClosestToZero( resAddr, currentTimeStep, &localPosClosestToZero, &localNegClosestToZero );
            }
            else
            {
                localMin = globalMin;
                localMax = globalMax;

                localPosClosestToZero = globalPosClosestToZero;
                localNegClosestToZero = globalNegClosestToZero;
            }

            CVF_ASSERT( legendConfigToUpdate );

            legendConfigToUpdate->disableAllTimeStepsRange( true );
            legendConfigToUpdate->setClosestToZeroValues( globalPosClosestToZero,
                                                          globalNegClosestToZero,
                                                          localPosClosestToZero,
                                                          localNegClosestToZero );
            legendConfigToUpdate->setAutomaticRanges( globalMin, globalMax, localMin, localMax );

            if ( this->hasCategoryResult() )
            {
                RimEclipseView* eclView = nullptr;
                this->firstAncestorOrThisOfType( eclView );

                if ( eclView )
                {
                    std::set<std::tuple<QString, int, cvf::Color3ub>, TupleCompare> categories;

                    std::vector<QString> tracerNames = this->flowDiagSolution()->tracerNames();
                    int                  tracerIndex = 0;

                    for ( const auto& tracerName : tracerNames )
                    {
                        cvf::Color3ub color( cvf::Color3::GRAY );

                        RimSimWellInView* well = eclView->wellCollection()->findWell(
                            RimFlowDiagSolution::removeCrossFlowEnding( tracerName ) );

                        if ( well ) color = cvf::Color3ub( well->wellPipeColor() );

                        categories.insert( std::make_tuple( tracerName, tracerIndex, color ) );
                        ++tracerIndex;
                    }

                    std::vector<std::tuple<QString, int, cvf::Color3ub>> categoryVector;

                    if ( m_showOnlyVisibleTracersInLegend )
                    {
                        std::set<int> visibleTracers;
                        RigVisibleTracerFilter::filterByVisibility( *eclView,
                                                                    *flowResultsData,
                                                                    resAddr,
                                                                    currentTimeStep,
                                                                    visibleTracers );
                        for ( auto tupIt : categories )
                        {
                            int tracerIndex = std::get<1>( tupIt );
                            if ( visibleTracers.count( tracerIndex ) ) categoryVector.push_back( tupIt );
                        }
                    }
                    else
                    {
                        for ( auto tupIt : categories )
                        {
                            categoryVector.push_back( tupIt );
                        }
                    }

                    legendConfigToUpdate->setCategoryItems( categoryVector );
                }
            }
        }
        else
        {
            if ( !rimEclipseCase ) return;

            RigEclipseCaseData* eclipseCaseData = rimEclipseCase->eclipseCaseData();
            if ( !eclipseCaseData ) return;

            RigCaseCellResultsData* cellResultsData = eclipseCaseData->results( this->porosityModel() );
            cellResultsData->ensureKnownResultLoaded( this->eclipseResultAddress() );

            double globalMin, globalMax;
            double globalPosClosestToZero, globalNegClosestToZero;

            cellResultsData->minMaxCellScalarValues( this->eclipseResultAddress(), globalMin, globalMax );
            cellResultsData->posNegClosestToZero( this->eclipseResultAddress(),
                                                  globalPosClosestToZero,
                                                  globalNegClosestToZero );

            double localMin, localMax;
            double localPosClosestToZero, localNegClosestToZero;
            if ( this->hasDynamicResult() && currentTimeStep >= 0 )
            {
                cellResultsData->minMaxCellScalarValues( this->eclipseResultAddress(), currentTimeStep, localMin, localMax );
                cellResultsData->posNegClosestToZero( this->eclipseResultAddress(),
                                                      currentTimeStep,
                                                      localPosClosestToZero,
                                                      localNegClosestToZero );
            }
            else
            {
                localMin = globalMin;
                localMax = globalMax;

                localPosClosestToZero = globalPosClosestToZero;
                localNegClosestToZero = globalNegClosestToZero;
            }

            CVF_ASSERT( legendConfigToUpdate );

            legendConfigToUpdate->disableAllTimeStepsRange( false );
            legendConfigToUpdate->setClosestToZeroValues( globalPosClosestToZero,
                                                          globalNegClosestToZero,
                                                          localPosClosestToZero,
                                                          localNegClosestToZero );
            legendConfigToUpdate->setAutomaticRanges( globalMin, globalMax, localMin, localMax );

            if ( this->hasCategoryResult() )
            {
                if ( this->resultType() == RiaDefines::ResultCatType::FORMATION_NAMES )
                {
                    std::vector<QString> fnVector = eclipseCaseData->formationNames();
                    legendConfigToUpdate->setNamedCategories( fnVector );
                }
                else if ( this->resultType() == RiaDefines::ResultCatType::ALLAN_DIAGRAMS )
                {
                    if ( this->resultVariable() == RiaResultNames::formationAllanResultName() )
                    {
                        const std::vector<QString> fnVector = eclipseCaseData->formationNames();
                        std::vector<int>           fnameIdxes;
                        for ( int i = 0; i < static_cast<int>( fnVector.size() ); i++ )
                        {
                            fnameIdxes.push_back( i );
                        }

                        cvf::Color3ubArray legendBaseColors = legendConfigToUpdate->colorLegend()->colorArray();

                        cvf::ref<caf::CategoryMapper> formationColorMapper = new caf::CategoryMapper;
                        formationColorMapper->setCategories( fnameIdxes );
                        formationColorMapper->setInterpolateColors( legendBaseColors );

                        const std::map<std::pair<int, int>, int>& formationCombToCategory =
                            eclipseCaseData->allanDiagramData()->formationCombinationToCategory();

                        std::vector<std::tuple<QString, int, cvf::Color3ub>> categories;
                        for ( int frmNameIdx : fnameIdxes )
                        {
                            cvf::Color3ub formationColor = formationColorMapper->mapToColor( frmNameIdx );
                            categories.emplace_back( std::make_tuple( fnVector[frmNameIdx], frmNameIdx, formationColor ) );
                        }

                        for ( auto it : formationCombToCategory )
                        {
                            int frmIdx1   = it.first.first;
                            int frmIdx2   = it.first.second;
                            int combIndex = it.second;

                            int fnVectorSize = static_cast<int>( fnVector.size() );
                            if ( frmIdx1 >= fnVectorSize || frmIdx2 >= fnVectorSize ) continue;

                            QString frmName1 = fnVector[frmIdx1];
                            QString frmName2 = fnVector[frmIdx2];

                            cvf::Color3f formationColor1 = cvf::Color3f( formationColorMapper->mapToColor( frmIdx1 ) );
                            cvf::Color3f formationColor2 = cvf::Color3f( formationColorMapper->mapToColor( frmIdx2 ) );

                            cvf::Color3ub blendColor =
                                cvf::Color3ub( cvf::Color3f( 0.5f * ( formationColor1.r() + formationColor2.r() ),
                                                             0.5f * ( formationColor1.g() + formationColor2.g() ),
                                                             0.5f * ( formationColor1.b() + formationColor2.b() ) ) );
                            categories.emplace_back( std::make_tuple( frmName1 + "-" + frmName2, combIndex, blendColor ) );
                        }

                        legendConfigToUpdate->setCategoryItems( categories );
                    }
                    else if ( this->resultVariable() == RiaResultNames::formationBinaryAllanResultName() )
                    {
                        std::vector<std::tuple<QString, int, cvf::Color3ub>> categories;
                        categories.emplace_back( std::make_tuple( "Same formation", 0, cvf::Color3ub::BROWN ) );
                        categories.emplace_back( std::make_tuple( "Different formation", 1, cvf::Color3ub::ORANGE ) );

                        legendConfigToUpdate->setCategoryItems( categories );
                    }
                }
                else if ( this->resultType() == RiaDefines::ResultCatType::DYNAMIC_NATIVE &&
                          this->resultVariable() == RiaResultNames::completionTypeResultName() )
                {
                    const std::vector<int>& visibleCategories =
                        cellResultsData->uniqueCellScalarValues( this->eclipseResultAddress() );

                    std::vector<RiaDefines::WellPathComponentType> supportedCompletionTypes =
                        { RiaDefines::WellPathComponentType::WELL_PATH,
                          RiaDefines::WellPathComponentType::FISHBONES,
                          RiaDefines::WellPathComponentType::PERFORATION_INTERVAL,
                          RiaDefines::WellPathComponentType::FRACTURE };

                    RiaColorTables::WellPathComponentColors colors = RiaColorTables::wellPathComponentColors();

                    std::vector<std::tuple<QString, int, cvf::Color3ub>> categories;
                    for ( auto completionType : supportedCompletionTypes )
                    {
                        if ( std::find( visibleCategories.begin(),
                                        visibleCategories.end(),
                                        static_cast<int>( completionType ) ) != visibleCategories.end() )
                        {
                            QString categoryText =
                                caf::AppEnum<RiaDefines::WellPathComponentType>::uiText( completionType );
                            categories.push_back( std::make_tuple( categoryText,
                                                                   static_cast<int>( completionType ),
                                                                   colors[completionType] ) );
                        }
                    }

                    legendConfigToUpdate->setCategoryItems( categories );
                }
                else
                {
                    legendConfigToUpdate->setIntegerCategories(
                        cellResultsData->uniqueCellScalarValues( this->eclipseResultAddress() ) );
                }
            }
        }
    }

    // Ternary legend update
    {
        if ( !rimEclipseCase ) return;

        RigEclipseCaseData* eclipseCase = rimEclipseCase->eclipseCaseData();
        if ( !eclipseCase ) return;

        RigCaseCellResultsData* cellResultsData = eclipseCase->results( this->porosityModel() );

        size_t maxTimeStepCount = cellResultsData->maxTimeStepCount();
        if ( this->isTernarySaturationSelected() && maxTimeStepCount > 1 )
        {
            RigCaseCellResultsData* gridCellResults = this->currentGridCellResults();
            {
                RigEclipseResultAddress resAddr( RiaDefines::ResultCatType::DYNAMIC_NATIVE, "SOIL" );

                if ( gridCellResults->ensureKnownResultLoaded( resAddr ) )
                {
                    double globalMin = 0.0;
                    double globalMax = 1.0;
                    double localMin  = 0.0;
                    double localMax  = 1.0;

                    cellResultsData->minMaxCellScalarValues( resAddr, globalMin, globalMax );
                    cellResultsData->minMaxCellScalarValues( resAddr, currentTimeStep, localMin, localMax );

                    ternaryLegendConfigToUpdate->setAutomaticRanges( RimTernaryLegendConfig::TERNARY_SOIL_IDX,
                                                                     globalMin,
                                                                     globalMax,
                                                                     localMin,
                                                                     localMax );
                }
            }

            {
                RigEclipseResultAddress resAddr( RiaDefines::ResultCatType::DYNAMIC_NATIVE, "SGAS" );

                if ( gridCellResults->ensureKnownResultLoaded( resAddr ) )
                {
                    double globalMin = 0.0;
                    double globalMax = 1.0;
                    double localMin  = 0.0;
                    double localMax  = 1.0;

                    cellResultsData->minMaxCellScalarValues( resAddr, globalMin, globalMax );
                    cellResultsData->minMaxCellScalarValues( resAddr, currentTimeStep, localMin, localMax );

                    ternaryLegendConfigToUpdate->setAutomaticRanges( RimTernaryLegendConfig::TERNARY_SGAS_IDX,
                                                                     globalMin,
                                                                     globalMax,
                                                                     localMin,
                                                                     localMax );
                }
            }

            {
                RigEclipseResultAddress resAddr( RiaDefines::ResultCatType::DYNAMIC_NATIVE, "SWAT" );

                if ( gridCellResults->ensureKnownResultLoaded( resAddr ) )
                {
                    double globalMin = 0.0;
                    double globalMax = 1.0;
                    double localMin  = 0.0;
                    double localMax  = 1.0;

                    cellResultsData->minMaxCellScalarValues( resAddr, globalMin, globalMax );
                    cellResultsData->minMaxCellScalarValues( resAddr, currentTimeStep, localMin, localMax );

                    ternaryLegendConfigToUpdate->setAutomaticRanges( RimTernaryLegendConfig::TERNARY_SWAT_IDX,
                                                                     globalMin,
                                                                     globalMax,
                                                                     localMin,
                                                                     localMax );
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::updateLegendTitle( RimRegularLegendConfig* legendConfig, const QString& legendHeading )
{
    QString title = legendHeading + this->resultVariableUiName();
    if ( !this->additionalResultTextShort().isEmpty() )
    {
        title += additionalResultTextShort();
    }

    if ( this->hasDualPorFractureResult() )
    {
        QString porosityModelText = caf::AppEnum<RiaDefines::PorosityModelType>::uiText( this->porosityModel() );

        title += QString( "\nDual Por : %1" ).arg( porosityModelText );
    }

    legendConfig->setTitle( title );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QList<caf::PdmOptionItemInfo> RimEclipseResultDefinition::calcOptionsForSelectedTracerField( bool injector )
{
    QList<caf::PdmOptionItemInfo> options;

    RimFlowDiagSolution* flowSol = m_flowSolutionUiField();
    if ( flowSol )
    {
        std::set<QString, TracerComp> sortedTracers = setOfTracersOfType( injector );

        for ( const QString& tracerName : sortedTracers )
        {
            QString                               postfix;
            RimFlowDiagSolution::TracerStatusType status = flowSol->tracerStatusOverall( tracerName );
            if ( status == RimFlowDiagSolution::TracerStatusType::VARYING )
            {
                postfix = " [I/P]";
            }
            else if ( status == RimFlowDiagSolution::TracerStatusType::UNDEFINED )
            {
                postfix = " [U]";
            }
            options.push_back( caf::PdmOptionItemInfo( tracerName + postfix, tracerName ) );
        }
    }
    return options;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::timeOfFlightString( bool shorter ) const
{
    QString tofString;
    bool    multipleSelected = false;
    if ( injectorSelectionState() != NONE_SELECTED && producerSelectionState() != NONE_SELECTED )
    {
        tofString        = shorter ? "Res.Time" : "Residence Time";
        multipleSelected = true;
    }
    else if ( injectorSelectionState() != NONE_SELECTED )
    {
        tofString = shorter ? "Fwd.TOF" : "Forward Time of Flight";
    }
    else if ( producerSelectionState() != NONE_SELECTED )
    {
        tofString = shorter ? "Rev.TOF" : "Reverse Time of Flight";
    }
    else
    {
        tofString = shorter ? "TOF" : "Time of Flight";
    }

    multipleSelected = multipleSelected || injectorSelectionState() >= MULTIPLE_SELECTED ||
                       producerSelectionState() >= MULTIPLE_SELECTED;

    if ( multipleSelected && !shorter )
    {
        tofString += " (Average)";
    }

    tofString += " [days]";
    // Conversion from seconds in flow module to days is done in RigFlowDiagTimeStepResult::setTracerTOF()

    return tofString;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::maxFractionTracerString( bool shorter ) const
{
    QString mfString;
    if ( injectorSelectionState() >= ONE_SELECTED && producerSelectionState() == NONE_SELECTED )
    {
        mfString = shorter ? "FloodReg" : "Flooding Region";
        if ( injectorSelectionState() >= MULTIPLE_SELECTED ) mfString += "s";
    }
    else if ( injectorSelectionState() == NONE_SELECTED && producerSelectionState() >= ONE_SELECTED )
    {
        mfString = shorter ? "DrainReg" : "Drainage Region";
        if ( producerSelectionState() >= MULTIPLE_SELECTED ) mfString += "s";
    }
    else
    {
        mfString = shorter ? "Drain&FloodReg" : "Drainage/Flooding Regions";
    }
    return mfString;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::selectedTracersString() const
{
    QStringList fullTracersList;

    FlowTracerSelectionState injectorState = injectorSelectionState();
    FlowTracerSelectionState producerState = producerSelectionState();

    if ( injectorState == ALL_SELECTED && producerState == ALL_SELECTED )
    {
        fullTracersList += caf::AppEnum<FlowTracerSelectionType>::uiText( FLOW_TR_INJ_AND_PROD );
    }
    else
    {
        if ( injectorState == ALL_SELECTED )
        {
            fullTracersList += caf::AppEnum<FlowTracerSelectionType>::uiText( FLOW_TR_INJECTORS );
        }

        if ( producerState == ALL_SELECTED )
        {
            fullTracersList += caf::AppEnum<FlowTracerSelectionType>::uiText( FLOW_TR_PRODUCERS );
        }

        if ( injectorSelectionState() == ONE_SELECTED || injectorSelectionState() == MULTIPLE_SELECTED )
        {
            QStringList listOfSelectedInjectors;
            for ( const QString& injector : m_selectedInjectorTracers() )
            {
                listOfSelectedInjectors.push_back( injector );
            }
            if ( !listOfSelectedInjectors.empty() )
            {
                fullTracersList += listOfSelectedInjectors.join( ", " );
            }
        }

        if ( producerSelectionState() == ONE_SELECTED || producerSelectionState() == MULTIPLE_SELECTED )
        {
            QStringList listOfSelectedProducers;
            for ( const QString& producer : m_selectedProducerTracers() )
            {
                listOfSelectedProducers.push_back( producer );
            }
            if ( !listOfSelectedProducers.empty() )
            {
                fullTracersList.push_back( listOfSelectedProducers.join( ", " ) );
            }
        }
    }

    QString tracersText;
    if ( !fullTracersList.empty() )
    {
        tracersText = fullTracersList.join( ", " );
    }
    return tracersText;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QStringList RimEclipseResultDefinition::getResultNamesForResultType( RiaDefines::ResultCatType     resultCatType,
                                                                     const RigCaseCellResultsData* results )
{
    if ( resultCatType != RiaDefines::ResultCatType::FLOW_DIAGNOSTICS )
    {
        if ( !results ) return QStringList();

        return results->resultNames( resultCatType );
    }
    else
    {
        QStringList flowVars;
        flowVars.push_back( RIG_FLD_TOF_RESNAME );
        flowVars.push_back( RIG_FLD_CELL_FRACTION_RESNAME );
        flowVars.push_back( RIG_FLD_MAX_FRACTION_TRACER_RESNAME );
        flowVars.push_back( RIG_FLD_COMMUNICATION_RESNAME );
        return flowVars;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<QString> RimEclipseResultDefinition::allTracerNames() const
{
    std::vector<QString> tracerNames;

    RimFlowDiagSolution* flowSol = m_flowSolutionUiField();
    if ( flowSol )
    {
        tracerNames = flowSol->tracerNames();
    }

    return tracerNames;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::set<QString, RimEclipseResultDefinition::TracerComp> RimEclipseResultDefinition::setOfTracersOfType( bool injector ) const
{
    std::set<QString, TracerComp> sortedTracers;

    RimFlowDiagSolution* flowSol = m_flowSolutionUiField();
    if ( flowSol )
    {
        std::vector<QString> tracerNames = allTracerNames();
        for ( const QString& tracerName : tracerNames )
        {
            RimFlowDiagSolution::TracerStatusType status = flowSol->tracerStatusOverall( tracerName );
            bool includeTracer                           = status == RimFlowDiagSolution::TracerStatusType::VARYING ||
                                 status == RimFlowDiagSolution::TracerStatusType::UNDEFINED;
            includeTracer |= injector && status == RimFlowDiagSolution::TracerStatusType::INJECTOR;
            includeTracer |= !injector && status == RimFlowDiagSolution::TracerStatusType::PRODUCER;

            if ( includeTracer )
            {
                sortedTracers.insert( tracerName );
            }
        }
    }
    return sortedTracers;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimEclipseResultDefinition::FlowTracerSelectionState RimEclipseResultDefinition::injectorSelectionState() const
{
    if ( m_flowTracerSelectionMode == FLOW_TR_INJECTORS || m_flowTracerSelectionMode == FLOW_TR_INJ_AND_PROD )
    {
        return ALL_SELECTED;
    }
    else if ( m_flowTracerSelectionMode == FLOW_TR_BY_SELECTION )
    {
        if ( m_selectedInjectorTracers().size() == setOfTracersOfType( true ).size() )
        {
            return ALL_SELECTED;
        }
        else if ( m_selectedInjectorTracers().size() == (size_t)1 )
        {
            return ONE_SELECTED;
        }
        else if ( m_selectedInjectorTracers().size() > (size_t)1 )
        {
            return MULTIPLE_SELECTED;
        }
    }
    return NONE_SELECTED;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimEclipseResultDefinition::FlowTracerSelectionState RimEclipseResultDefinition::producerSelectionState() const
{
    if ( m_flowTracerSelectionMode == FLOW_TR_PRODUCERS || m_flowTracerSelectionMode == FLOW_TR_INJ_AND_PROD )
    {
        return ALL_SELECTED;
    }
    else if ( m_flowTracerSelectionMode == FLOW_TR_BY_SELECTION )
    {
        if ( m_selectedProducerTracers().size() == setOfTracersOfType( false ).size() )
        {
            return ALL_SELECTED;
        }
        else if ( m_selectedProducerTracers().size() == (size_t)1 )
        {
            return ONE_SELECTED;
        }
        else if ( m_selectedProducerTracers().size() > (size_t)1 )
        {
            return MULTIPLE_SELECTED;
        }
    }
    return NONE_SELECTED;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::syncInjectorToProducerSelection()
{
    const double epsilon = 1.0e-8;

    int timeStep = 0;

    Rim3dView* rimView = nullptr;
    this->firstAncestorOrThisOfType( rimView );
    if ( rimView )
    {
        timeStep = rimView->currentTimeStep();
    }

    RimFlowDiagSolution* flowSol = m_flowSolution();
    if ( flowSol && m_flowTracerSelectionMode == FLOW_TR_BY_SELECTION )
    {
        std::set<QString, TracerComp> producers = setOfTracersOfType( false );

        std::set<QString, TracerComp> newProducerSelection;
        for ( const QString& selectedInjector : m_selectedInjectorTracers() )
        {
            for ( const QString& producer : producers )
            {
                std::pair<double, double> commFluxes =
                    flowSol->flowDiagResults()->injectorProducerPairFluxes( selectedInjector.toStdString(),
                                                                            producer.toStdString(),
                                                                            timeStep );
                if ( std::abs( commFluxes.first ) > epsilon || std::abs( commFluxes.second ) > epsilon )
                {
                    newProducerSelection.insert( producer );
                }
            }
        }
        // Add all currently selected producers to set
        for ( const QString& selectedProducer : m_selectedProducerTracers() )
        {
            newProducerSelection.insert( selectedProducer );
        }
        std::vector<QString> newProducerVector( newProducerSelection.begin(), newProducerSelection.end() );
        setSelectedProducerTracers( newProducerVector );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimEclipseResultDefinition::syncProducerToInjectorSelection()
{
    const double epsilon = 1.0e-8;

    int timeStep = 0;

    Rim3dView* rimView = nullptr;
    this->firstAncestorOrThisOfType( rimView );
    if ( rimView )
    {
        timeStep = rimView->currentTimeStep();
    }

    RimFlowDiagSolution* flowSol = m_flowSolution();
    if ( flowSol && m_flowTracerSelectionMode == FLOW_TR_BY_SELECTION )
    {
        std::set<QString, TracerComp> injectors = setOfTracersOfType( true );

        std::set<QString, TracerComp> newInjectorSelection;
        for ( const QString& selectedProducer : m_selectedProducerTracers() )
        {
            for ( const QString& injector : injectors )
            {
                std::pair<double, double> commFluxes =
                    flowSol->flowDiagResults()->injectorProducerPairFluxes( injector.toStdString(),
                                                                            selectedProducer.toStdString(),
                                                                            timeStep );
                if ( std::abs( commFluxes.first ) > epsilon || std::abs( commFluxes.second ) > epsilon )
                {
                    newInjectorSelection.insert( injector );
                }
            }
        }
        // Add all currently selected injectors to set
        for ( const QString& selectedInjector : m_selectedInjectorTracers() )
        {
            newInjectorSelection.insert( selectedInjector );
        }
        std::vector<QString> newInjectorVector( newInjectorSelection.begin(), newInjectorSelection.end() );
        setSelectedInjectorTracers( newInjectorVector );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isDeltaResultEnabled() const
{
    return m_isDeltaResultEnabled;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isDeltaTimeStepPossible() const
{
    return isDeltaResultEnabled() && m_resultTypeUiField() == RiaDefines::ResultCatType::DYNAMIC_NATIVE &&
           !isTernarySaturationSelected();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isDeltaTimeStepActive() const
{
    return isDeltaTimeStepPossible() && m_timeLapseBaseTimestep() >= 0;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isDeltaCasePossible() const
{
    return isDeltaResultEnabled() && !isTernarySaturationSelected() &&
           ( m_resultTypeUiField() == RiaDefines::ResultCatType::DYNAMIC_NATIVE ||
             m_resultTypeUiField() == RiaDefines::ResultCatType::STATIC_NATIVE ||
             m_resultTypeUiField() == RiaDefines::ResultCatType::GENERATED );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isDeltaCaseActive() const
{
    return isDeltaCasePossible() && m_differenceCase() != nullptr;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isDivideByCellFaceAreaPossible() const
{
    QString str = m_resultVariable;

    // TODO : Move to RiaDefines or a separate file for cell face results

    if ( str == "FLRWATI+" ) return true;
    if ( str == "FLRWATJ+" ) return true;
    if ( str == "FLRWATK+" ) return true;

    if ( str == "FLROILI+" ) return true;
    if ( str == "FLROILJ+" ) return true;
    if ( str == "FLROILK+" ) return true;

    if ( str == "FLRGASI+" ) return true;
    if ( str == "FLRGASJ+" ) return true;
    if ( str == "FLRGASK+" ) return true;

    if ( str == "TRANX" ) return true;
    if ( str == "TRANY" ) return true;
    if ( str == "TRANZ" ) return true;

    if ( str == "riTRANX" ) return true;
    if ( str == "riTRANY" ) return true;
    if ( str == "riTRANZ" ) return true;

    return false;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::isDivideByCellFaceAreaActive() const
{
    return isDivideByCellFaceAreaPossible() && m_divideByCellFaceArea;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::showDerivedResultsFirstInVariableUiField() const
{
    // Cell Face result names
    bool                   showDerivedResultsFirstInList = false;
    RimEclipseFaultColors* rimEclipseFaultColors         = nullptr;
    this->firstAncestorOrThisOfType( rimEclipseFaultColors );

    if ( rimEclipseFaultColors ) showDerivedResultsFirstInList = true;

    return showDerivedResultsFirstInList;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimEclipseResultDefinition::addPerCellFaceOptionsForVariableUiField() const
{
    RimPlotCurve* curve = nullptr;
    this->firstAncestorOrThisOfType( curve );

    RimEclipsePropertyFilter* propFilter = nullptr;
    this->firstAncestorOrThisOfType( propFilter );

    RimCellEdgeColors* cellEdge = nullptr;
    this->firstAncestorOrThisOfType( cellEdge );

    if ( propFilter || curve || cellEdge )
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimEclipseResultDefinition::getInputPropertyFileName( const QString& resultName ) const
{
    RimEclipseCase* eclipseCase;
    this->firstAncestorOrThisOfType( eclipseCase );

    if ( eclipseCase )
    {
        RimEclipseInputPropertyCollection* inputPropertyCollection = eclipseCase->inputPropertyCollection();
        if ( inputPropertyCollection )
        {
            RimEclipseInputProperty* inputProperty = inputPropertyCollection->findInputProperty( resultName );
            if ( inputProperty )
            {
                return inputProperty->fileName.v().path();
            }
        }
    }

    return "";
}
