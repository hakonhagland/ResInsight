/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2021 Equinor ASA
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

#include "RicMswTableFormatterTools.h"

#include "RiaLogging.h"

#include "RicMswCompletions.h"
#include "RicMswExportInfo.h"

#include "RifTextDataTableFormatter.h"

#include "RigWellPath.h"

#include "RimWellPath.h" // TODO: Consider adding wellnameforexport to RicMswExportInfo to avoid these includes
#include "RimWellPathCompletionSettings.h"

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::generateWelsegsTable( RifTextDataTableFormatter& formatter,
                                                      RicMswExportInfo&          exportInfo,
                                                      double                     maxSegmentLength )
{
    formatter.keyword( "WELSEGS" );

    double startMD  = exportInfo.mainBoreBranch()->startMD();
    double startTVD = exportInfo.mainBoreBranch()->startTVD();

    {
        std::vector<RifTextDataTableColumn> header = {
            RifTextDataTableColumn( "Name" ),
            RifTextDataTableColumn( "Dep 1" ),
            RifTextDataTableColumn( "Tlen 1" ),
            RifTextDataTableColumn( "Vol 1" ),
            RifTextDataTableColumn( "Len&Dep" ),
            RifTextDataTableColumn( "PresDrop" ),
        };
        formatter.header( header );

        formatter.add( exportInfo.mainBoreBranch()->wellPath()->completionSettings()->wellNameForExport() );
        formatter.add( startTVD );
        formatter.add( startMD );
        formatter.addValueOrDefaultMarker( exportInfo.topWellBoreVolume(), RicMswExportInfo::defaultDoubleValue() );
        formatter.add( exportInfo.lengthAndDepthText() );
        formatter.add( QString( "'%1'" ).arg( exportInfo.pressureDropText() ) );

        formatter.rowCompleted();
    }

    {
        std::vector<RifTextDataTableColumn> header =
            { RifTextDataTableColumn( "First Seg" ),
              RifTextDataTableColumn( "Last Seg" ),
              RifTextDataTableColumn( "Branch Num" ),
              RifTextDataTableColumn( "Outlet Seg" ),
              RifTextDataTableColumn( "Length" ),
              RifTextDataTableColumn( "Depth Change" ),
              RifTextDataTableColumn( "Diam" ),
              RifTextDataTableColumn( "Rough", RifTextDataTableDoubleFormatting( RIF_FLOAT, 7 ) ) };
        formatter.header( header );
    }

    int segmentNumber = 2; // There's an implicit segment number 1.

    RicMswSegment* parentSegment = nullptr;
    writeWelsegsSegmentsRecursively( formatter,
                                     exportInfo,
                                     exportInfo.mainBoreBranch(),
                                     &segmentNumber,
                                     maxSegmentLength,
                                     parentSegment );

    formatter.tableCompleted();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::writeWelsegsSegmentsRecursively( RifTextDataTableFormatter&   formatter,
                                                                 RicMswExportInfo&            exportInfo,
                                                                 gsl::not_null<RicMswBranch*> branch,
                                                                 gsl::not_null<int*>          segmentNumber,
                                                                 double                       maxSegmentLength,
                                                                 RicMswSegment*               connectedToSegment )
{
    auto outletSegment = connectedToSegment;

    RicMswValve* outletValve = nullptr;

    auto branchSegments = branch->segments();
    auto it             = branchSegments.begin();
    if ( outletValve = dynamic_cast<RicMswTieInICV*>( branch.get() ); outletValve != nullptr )
    {
        writeValveWelsegsSegment( outletSegment, outletValve, formatter, exportInfo, maxSegmentLength, segmentNumber );

        auto valveSegments = outletValve->segments();
        outletSegment      = valveSegments.front();

        *segmentNumber = outletSegment->segmentNumber() + 1;
        ++it; // skip segment below
    }

    formatter.addOptionalComment( QString( "Segments on branch %1" ).arg( branch->label() ) );

    for ( ; it != branchSegments.end(); ++it )
    {
        auto segment = *it;
        segment->setSegmentNumber( *segmentNumber );

        if ( segment->subIndex() != cvf::UNDEFINED_SIZE_T )
        {
            QString comment = segment->label() + QString( ", sub %1" ).arg( segment->subIndex() );
            formatter.addOptionalComment( comment );
        }

        writeWelsegsSegment( segment, outletSegment, formatter, exportInfo, maxSegmentLength, branch, segmentNumber );
        outletSegment = segment;

        for ( auto& completion : segment->completions() )
        {
            // For a well with perforation intervals, the WELSEGS segments are reported twice if if we include the
            // RicMswPerforation completions. Investigate when this class is intended to be exported to file
            auto performationMsw = dynamic_cast<RicMswPerforation*>( completion );
            if ( performationMsw ) continue;

            auto segmentValve = dynamic_cast<RicMswValve*>( completion );
            if ( segmentValve != nullptr )
            {
                writeValveWelsegsSegment( segment, segmentValve, formatter, exportInfo, maxSegmentLength, segmentNumber );
                outletValve   = segmentValve;
                outletSegment = segment;
            }
            else
            {
                // If we have a valve, the outlet segment is the valve's segment
                RicMswSegment* outletSegment =
                    outletValve && outletValve->segmentCount() > 0 ? outletValve->segments().front() : segment;
                writeCompletionWelsegsSegments( outletSegment, completion, formatter, exportInfo, maxSegmentLength, segmentNumber );
            }
        }
    }

    for ( auto childBranch : branch->branches() )
    {
        RicMswSegment* tieInSegmentOnParentBranch = nullptr;

        {
            // The the tie-in branch is connected to the segment of parent branch with the closest midpoint

            double closestSegmentMidPointDistance = std::numeric_limits<double>::infinity();
            double branchStartMD                  = childBranch->startMD();
            for ( auto seg : branch->segments() )
            {
                double midpointMD = 0.5 * ( seg->startMD() + seg->endMD() );

                double candidateDistance = std::abs( midpointMD - branchStartMD );
                if ( candidateDistance < closestSegmentMidPointDistance )
                {
                    tieInSegmentOnParentBranch     = seg;
                    closestSegmentMidPointDistance = candidateDistance;
                }
            }
        }

        RicMswSegment* outletSegmentForChildBranch = outletSegment;
        if ( tieInSegmentOnParentBranch ) outletSegmentForChildBranch = tieInSegmentOnParentBranch;

        writeWelsegsSegmentsRecursively( formatter,
                                         exportInfo,
                                         childBranch,
                                         segmentNumber,
                                         maxSegmentLength,
                                         outletSegmentForChildBranch );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::writeWelsegsCompletionCommentHeader( RifTextDataTableFormatter&        formatter,
                                                                     RigCompletionData::CompletionType completionType )
{
    if ( completionType == RigCompletionData::CT_UNDEFINED )
    {
        formatter.addOptionalComment( "Main stem" );
    }
    else if ( completionType == RigCompletionData::FISHBONES_ICD )
    {
        formatter.addOptionalComment( "Fishbone Laterals" );
        formatter.addOptionalComment( "Diam: MSW - Tubing Radius" );
        formatter.addOptionalComment( "Rough: MSW - Open Hole Roughness Factor" );
    }
    else if ( RigCompletionData::isPerforationValve( completionType ) )
    {
        formatter.addOptionalComment( "Perforation Valve Segments" );
        formatter.addOptionalComment( "Diam: MSW - Tubing Radius" );
        formatter.addOptionalComment( "Rough: MSW - Open Hole Roughness Factor" );
    }
    else if ( completionType == RigCompletionData::FRACTURE )
    {
        formatter.addOptionalComment( "Fracture Segments" );
        formatter.addOptionalComment( "Diam: MSW - Default Dummy" );
        formatter.addOptionalComment( "Rough: MSW - Default Dummy" );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::generateCompsegTables( RifTextDataTableFormatter& formatter, RicMswExportInfo& exportInfo )
{
    /*
     * TODO: Creating the regular perforation COMPSEGS table should come in here, before the others
     * should take precedence by appearing later in the output. See #3230.
     */

    bool headerGenerated = false;

    std::set<cvf::Vec3st, CvfVec3stComparator> intersectedCells;

    {
        std::set<RigCompletionData::CompletionType> perforationTypes = { RigCompletionData::PERFORATION,
                                                                         RigCompletionData::PERFORATION_ICD,
                                                                         RigCompletionData::PERFORATION_ICV,
                                                                         RigCompletionData::PERFORATION_AICD };
        generateCompsegTable( formatter,
                              exportInfo,
                              exportInfo.mainBoreBranch(),
                              false,
                              perforationTypes,
                              &headerGenerated,
                              &intersectedCells );
        if ( exportInfo.hasSubGridIntersections() )
        {
            generateCompsegTable( formatter,
                                  exportInfo,
                                  exportInfo.mainBoreBranch(),
                                  true,
                                  perforationTypes,
                                  &headerGenerated,
                                  &intersectedCells );
        }
    }

    {
        std::set<RigCompletionData::CompletionType> fishbonesTypes = { RigCompletionData::FISHBONES_ICD,
                                                                       RigCompletionData::FISHBONES };
        generateCompsegTable( formatter,
                              exportInfo,
                              exportInfo.mainBoreBranch(),
                              false,
                              fishbonesTypes,
                              &headerGenerated,
                              &intersectedCells );
        if ( exportInfo.hasSubGridIntersections() )
        {
            generateCompsegTable( formatter,
                                  exportInfo,
                                  exportInfo.mainBoreBranch(),
                                  true,
                                  fishbonesTypes,
                                  &headerGenerated,
                                  &intersectedCells );
        }
    }

    {
        std::set<RigCompletionData::CompletionType> fractureTypes = { RigCompletionData::FRACTURE };
        generateCompsegTable( formatter,
                              exportInfo,
                              exportInfo.mainBoreBranch(),
                              false,
                              fractureTypes,
                              &headerGenerated,
                              &intersectedCells );
        if ( exportInfo.hasSubGridIntersections() )
        {
            generateCompsegTable( formatter,
                                  exportInfo,
                                  exportInfo.mainBoreBranch(),
                                  true,
                                  fractureTypes,
                                  &headerGenerated,
                                  &intersectedCells );
        }
    }

    if ( headerGenerated )
    {
        formatter.tableCompleted();
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::generateCompsegTable(
    RifTextDataTableFormatter&                                 formatter,
    RicMswExportInfo&                                          exportInfo,
    gsl::not_null<const RicMswBranch*>                         branch,
    bool                                                       exportSubGridIntersections,
    const std::set<RigCompletionData::CompletionType>&         exportCompletionTypes,
    gsl::not_null<bool*>                                       headerGenerated,
    gsl::not_null<std::set<cvf::Vec3st, CvfVec3stComparator>*> intersectedCells )
{
    for ( auto segment : branch->segments() )
    {
        for ( auto completion : segment->completions() )
        {
            if ( completion->segments().empty() || !exportCompletionTypes.count( completion->completionType() ) )
                continue;

            if ( !*headerGenerated )
            {
                generateCompsegHeader( formatter, exportInfo, completion->completionType(), exportSubGridIntersections );
                *headerGenerated = true;
            }

            bool isPerforationValve = completion->completionType() == RigCompletionData::PERFORATION_ICD ||
                                      completion->completionType() == RigCompletionData::PERFORATION_AICD ||
                                      completion->completionType() == RigCompletionData::PERFORATION_ICV;

            for ( auto subSegment : completion->segments() )
            {
                for ( auto intersection : subSegment->intersections() )
                {
                    bool isSubGridIntersection = !intersection->gridName().isEmpty();
                    if ( isSubGridIntersection != exportSubGridIntersections ) continue;

                    double startLength = subSegment->startMD();
                    double endLength   = subSegment->endMD();
                    if ( isPerforationValve )
                    {
                        startLength = segment->startMD();
                        endLength   = segment->endMD();
                    }

                    cvf::Vec3st ijk = intersection->gridLocalCellIJK();
                    if ( !intersectedCells->count( ijk ) )
                    {
                        if ( exportSubGridIntersections )
                        {
                            formatter.add( intersection->gridName() );
                        }

                        formatter.addOneBasedCellIndex( ijk.x() ).addOneBasedCellIndex( ijk.y() ).addOneBasedCellIndex(
                            ijk.z() );
                        formatter.add( completion->branchNumber() );

                        formatter.add( startLength );
                        formatter.add( endLength );

                        formatter.rowCompleted();
                        intersectedCells->insert( ijk );
                    }
                }
            }
        }
    }

    for ( auto childBranch : branch->branches() )
    {
        generateCompsegTable( formatter,
                              exportInfo,
                              childBranch,
                              exportSubGridIntersections,
                              exportCompletionTypes,
                              headerGenerated,
                              intersectedCells );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::generateCompsegHeader( RifTextDataTableFormatter&        formatter,
                                                       RicMswExportInfo&                 exportInfo,
                                                       RigCompletionData::CompletionType completionType,
                                                       bool                              exportSubGridIntersections )
{
    if ( exportSubGridIntersections )
    {
        formatter.keyword( "COMPSEGL" );
    }
    else
    {
        formatter.keyword( "COMPSEGS" );
    }

    if ( completionType == RigCompletionData::FISHBONES_ICD )
    {
        formatter.comment( "Fishbones" );
    }
    else if ( completionType == RigCompletionData::FRACTURE )
    {
        formatter.comment( "Fractures" );
    }

    {
        std::vector<RifTextDataTableColumn> header = { RifTextDataTableColumn( "Name" ) };
        formatter.header( header );
        formatter.add( exportInfo.mainBoreBranch()->wellPath()->completionSettings()->wellNameForExport() );
        formatter.rowCompleted();
    }

    {
        std::vector<RifTextDataTableColumn> allHeaders;
        if ( exportSubGridIntersections )
        {
            allHeaders.push_back( RifTextDataTableColumn( "Grid" ) );
        }

        std::vector<RifTextDataTableColumn> commonHeaders = { RifTextDataTableColumn( "I" ),
                                                              RifTextDataTableColumn( "J" ),
                                                              RifTextDataTableColumn( "K" ),
                                                              RifTextDataTableColumn( "Branch no" ),
                                                              RifTextDataTableColumn( "Start Length" ),
                                                              RifTextDataTableColumn( "End Length" ),
                                                              RifTextDataTableColumn( "Dir Pen" ),
                                                              RifTextDataTableColumn( "End Range" ),
                                                              RifTextDataTableColumn( "Connection Depth" ) };
        allHeaders.insert( allHeaders.end(), commonHeaders.begin(), commonHeaders.end() );
        formatter.header( allHeaders );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::generateWsegvalvTable( RifTextDataTableFormatter& formatter, RicMswExportInfo& exportInfo )
{
    bool foundValve = false;

    generateWsegvalvTableRecursively( formatter,
                                      exportInfo.mainBoreBranch(),
                                      foundValve,
                                      exportInfo.mainBoreBranch()->wellPath()->completionSettings()->wellNameForExport() );

    if ( foundValve )
    {
        formatter.tableCompleted();
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::generateWsegvalvTableRecursively( RifTextDataTableFormatter&   formatter,
                                                                  gsl::not_null<RicMswBranch*> branch,
                                                                  bool&                        isHeaderWritten,
                                                                  const QString&               wellNameForExport )
{
    {
        auto tieInValve = dynamic_cast<RicMswTieInICV*>( branch.get() );
        if ( tieInValve && !tieInValve->segments().empty() )
        {
            if ( !isHeaderWritten )
            {
                writeWsegvalHeader( formatter );

                isHeaderWritten = true;
            }

            auto firstSubSegment = tieInValve->segments().front();
            CAF_ASSERT( tieInValve->completionType() == RigCompletionData::PERFORATION_ICV );
            {
                formatter.addOptionalComment( tieInValve->label() );
            }
            formatter.add( wellNameForExport );
            formatter.add( firstSubSegment->segmentNumber() );
            formatter.add( tieInValve->flowCoefficient() );
            formatter.add( QString( "%1" ).arg( tieInValve->area(), 8, 'g', 4 ) );
            formatter.rowCompleted();
        }
    }

    for ( auto segment : branch->segments() )
    {
        for ( auto completion : segment->completions() )
        {
            if ( RigCompletionData::isWsegValveTypes( completion->completionType() ) )
            {
                if ( !isHeaderWritten )
                {
                    writeWsegvalHeader( formatter );

                    isHeaderWritten = true;
                }

                auto wsegValve = static_cast<RicMswWsegValve*>( completion );
                if ( !wsegValve->segments().empty() )
                {
                    CVF_ASSERT( wsegValve->segments().size() == 1u );

                    auto firstSubSegment = wsegValve->segments().front();

                    // TODO: The following line was blocking export of valves for fishbones
                    // Unclear why this line was included. Remove when MSW export has ben verified correctly
                    // if ( !firstSubSegment->intersections().empty() )
                    {
                        if ( wsegValve->completionType() == RigCompletionData::PERFORATION_ICD ||
                             wsegValve->completionType() == RigCompletionData::PERFORATION_ICV )
                        {
                            formatter.addOptionalComment( wsegValve->label() );
                        }
                        formatter.add( wellNameForExport );
                        formatter.add( firstSubSegment->segmentNumber() );
                        formatter.add( wsegValve->flowCoefficient() );
                        formatter.add( QString( "%1" ).arg( wsegValve->area(), 8, 'g', 4 ) );
                        formatter.rowCompleted();
                    }
                }
            }
        }
    }

    for ( auto childBranch : branch->branches() )
    {
        generateWsegvalvTableRecursively( formatter, childBranch, isHeaderWritten, wellNameForExport );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::generateWsegAicdTable( RifTextDataTableFormatter& formatter, RicMswExportInfo& exportInfo )
{
    RifTextDataTableFormatter tighterFormatter( formatter );
    tighterFormatter.setColumnSpacing( 1 );
    tighterFormatter.setTableRowPrependText( "   " );

    bool foundValve = false;

    for ( auto segment : exportInfo.mainBoreBranch()->segments() )
    {
        for ( auto completion : segment->completions() )
        {
            if ( completion->completionType() == RigCompletionData::PERFORATION_AICD )
            {
                auto aicd = static_cast<RicMswPerforationAICD*>( completion );
                if ( aicd->isValid() )
                {
                    if ( !foundValve )
                    {
                        std::vector<QString> columnDescriptions =
                            { "Well Name",
                              "Segment Number",
                              "Segment Number",
                              "Strength of AICD",
                              "Flow Scaling Factor for AICD",
                              "Density of Calibration Fluid",
                              "Viscosity of Calibration Fluid",
                              "Critical water in liquid fraction for emulsions viscosity model",
                              "Emulsion viscosity transition region",
                              "Max ratio of emulsion viscosity to continuous phase viscosity",
                              "Flow scaling factor method",
                              "Maximum flow rate for AICD device",
                              "Volume flow rate exponent, x",
                              "Viscosity function exponent, y",
                              "Device OPEN/SHUT",
                              "Exponent of the oil flowing fraction in the density mixture calculation",
                              "Exponent of the water flowing fraction in the density mixture calculation",
                              "Exponent of the gas flowing fraction in the density mixture calculation",
                              "Exponent of the oil flowing fraction in the density viscosity calculation",
                              "Exponent of the water flowing fraction in the density viscosity calculation",
                              "Exponent of the gas flowing fraction in the density viscosity calculation" };

                        tighterFormatter.keyword( "WSEGAICD" );
                        tighterFormatter.comment( "Column Overview:" );
                        for ( size_t i = 0; i < columnDescriptions.size(); ++i )
                        {
                            tighterFormatter.comment(
                                QString( "%1: %2" ).arg( i + 1, 2, 10, QChar( '0' ) ).arg( columnDescriptions[i] ) );
                        }

                        std::vector<RifTextDataTableColumn> header;
                        for ( size_t i = 1; i <= 21; ++i )
                        {
                            QString                cName = QString( "%1" ).arg( i, 2, 10, QChar( '0' ) );
                            RifTextDataTableColumn col( cName,
                                                        RifTextDataTableDoubleFormatting(
                                                            RifTextDataTableDoubleFormat::RIF_CONSISE ),
                                                        RIGHT );
                            header.push_back( col );
                        }
                        tighterFormatter.header( header );

                        foundValve = true;
                    }
                    if ( !aicd->segments().empty() )
                    {
                        CVF_ASSERT( aicd->segments().size() == 1u );
                        tighterFormatter.comment( aicd->label() );
                        tighterFormatter.add(
                            exportInfo.mainBoreBranch()->wellPath()->completionSettings()->wellNameForExport() ); // #1
                        tighterFormatter.add( aicd->segments().front()->segmentNumber() );
                        tighterFormatter.add( aicd->segments().front()->segmentNumber() );

                        std::array<double, AICD_NUM_PARAMS> values = aicd->values();
                        tighterFormatter.add( values[AICD_STRENGTH] );

                        tighterFormatter.add( aicd->flowScalingFactor() ); // #5 Flow scaling factor used when item
                                                                           // #11 is set to '1'

                        tighterFormatter.add( values[AICD_DENSITY_CALIB_FLUID] );
                        tighterFormatter.add( values[AICD_VISCOSITY_CALIB_FLUID] );
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_CRITICAL_WATER_IN_LIQUID_FRAC],
                                                                  RicMswExportInfo::defaultDoubleValue() );
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_EMULSION_VISC_TRANS_REGION],
                                                                  RicMswExportInfo::defaultDoubleValue() );
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_MAX_RATIO_EMULSION_VISC],
                                                                  RicMswExportInfo::defaultDoubleValue() ); // #10

                        tighterFormatter.add( 1 ); // #11 : Always use method "b. Scale factor". The value of the
                                                   // scale factor is given in item #5

                        tighterFormatter.addValueOrDefaultMarker( values[AICD_MAX_FLOW_RATE],
                                                                  RicMswExportInfo::defaultDoubleValue() );
                        tighterFormatter.add( values[AICD_VOL_FLOW_EXP] );
                        tighterFormatter.add( values[AICD_VISOSITY_FUNC_EXP] );
                        tighterFormatter.add( aicd->isOpen() ? "OPEN" : "SHUT" ); // #15
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_EXP_OIL_FRAC_DENSITY],
                                                                  RicMswExportInfo::defaultDoubleValue() );
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_EXP_WATER_FRAC_DENSITY],
                                                                  RicMswExportInfo::defaultDoubleValue() );
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_EXP_GAS_FRAC_DENSITY],
                                                                  RicMswExportInfo::defaultDoubleValue() );
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_EXP_OIL_FRAC_VISCOSITY],
                                                                  RicMswExportInfo::defaultDoubleValue() );
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_EXP_WATER_FRAC_VISCOSITY],
                                                                  RicMswExportInfo::defaultDoubleValue() ); // #20
                        tighterFormatter.addValueOrDefaultMarker( values[AICD_EXP_GAS_FRAC_VISCOSITY],
                                                                  RicMswExportInfo::defaultDoubleValue() );
                        tighterFormatter.rowCompleted();
                    }
                }
                else
                {
                    RiaLogging::error( QString( "Export AICD Valve (%1): Valve is invalid. At least one required "
                                                "template parameter is not set." )
                                           .arg( aicd->label() ) );
                }
            }
        }
    }
    if ( foundValve )
    {
        tighterFormatter.tableCompleted();
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::writeWelsegsSegment( RicMswSegment*               segment,
                                                     const RicMswSegment*         previousSegment,
                                                     RifTextDataTableFormatter&   formatter,
                                                     RicMswExportInfo&            exportInfo,
                                                     double                       maxSegmentLength,
                                                     gsl::not_null<RicMswBranch*> branch,
                                                     int*                         segmentNumber )
{
    CVF_ASSERT( segment && segmentNumber );

    double startMD = segment->startMD();
    double endMD   = segment->endMD();

    std::vector<std::pair<double, double>> segments = createSubSegmentMDPairs( startMD, endMD, maxSegmentLength );

    CVF_ASSERT( branch->wellPath() );
    auto wellPathGeometry = branch->wellPath()->wellPathGeometry();
    CVF_ASSERT( wellPathGeometry );

    double prevOutMD  = branch->startMD();
    double prevOutTVD = branch->startTVD();
    if ( previousSegment )
    {
        prevOutMD  = previousSegment->outputMD();
        prevOutTVD = previousSegment->outputTVD();
    }

    auto outletSegment = previousSegment;
    for ( const auto& [subStartMD, subEndMD] : segments )
    {
        double depth  = 0;
        double length = 0;

        double midPointMD  = 0.5 * ( subStartMD + subEndMD );
        double midPointTVD = tvdFromMeasuredDepth( branch->wellPath(), midPointMD );

        if ( exportInfo.lengthAndDepthText() == QString( "INC" ) )
        {
            depth  = midPointTVD - prevOutTVD;
            length = midPointMD - prevOutMD;
        }
        else
        {
            depth  = midPointTVD;
            length = midPointMD;
        }
        segment->setOutputMD( midPointMD );
        segment->setOutputTVD( midPointTVD );
        segment->setSegmentNumber( *segmentNumber );

        formatter.add( *segmentNumber ).add( *segmentNumber );
        formatter.add( branch->branchNumber() );
        if ( outletSegment )
            formatter.add( outletSegment->segmentNumber() );
        else
            formatter.add( 1 );
        formatter.add( length );
        formatter.add( depth );
        formatter.add( exportInfo.linerDiameter() );
        formatter.add( exportInfo.roughnessFactor() );
        formatter.rowCompleted();
        ( *segmentNumber )++;
        outletSegment = segment;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::writeValveWelsegsSegment( const RicMswSegment*       outletSegment,
                                                          RicMswValve*               valve,
                                                          RifTextDataTableFormatter& formatter,
                                                          RicMswExportInfo&          exportInfo,
                                                          double                     maxSegmentLength,
                                                          int*                       segmentNumber )
{
    CVF_ASSERT( valve );
    if ( !valve->isValid() ) return;

    CVF_ASSERT( !valve->label().isEmpty() );
    CVF_ASSERT( valve->wellPath() );

    formatter.addOptionalComment( valve->label() );

    auto segments = valve->segments();

    auto subSegment = segments.front();
    subSegment->setSegmentNumber( *segmentNumber );

    double startMD = subSegment->startMD();
    double endMD   = subSegment->endMD();

    double midPointMD  = 0.5 * ( startMD + endMD );
    double midPointTVD = tvdFromMeasuredDepth( valve->wellPath(), midPointMD );

    subSegment->setOutputMD( midPointMD );
    subSegment->setOutputTVD( midPointTVD );

    std::vector<std::pair<double, double>> splitSegments = createSubSegmentMDPairs( startMD, endMD, maxSegmentLength );

    auto wellPathGeometry = valve->wellPath()->wellPathGeometry();
    CVF_ASSERT( wellPathGeometry );

    for ( const auto& [subStartMD, subEndMD] : splitSegments )
    {
        int subSegmentNumber = ( *segmentNumber )++;

        double subStartTVD = tvdFromMeasuredDepth( valve->wellPath(), subStartMD );
        double subEndTVD   = tvdFromMeasuredDepth( valve->wellPath(), subEndMD );

        double depth  = 0;
        double length = 0;

        if ( exportInfo.lengthAndDepthText() == QString( "INC" ) )
        {
            depth  = subEndTVD - subStartTVD;
            length = subEndMD - subStartMD;
        }
        else
        {
            depth  = subEndTVD;
            length = subEndMD;
        }

        formatter.add( subSegmentNumber );
        formatter.add( subSegmentNumber );
        formatter.add( valve->branchNumber() );
        formatter.add( outletSegment->segmentNumber() );

        formatter.add( length );
        formatter.add( depth );
        formatter.add( exportInfo.linerDiameter() );
        formatter.add( exportInfo.roughnessFactor() );
        formatter.rowCompleted();
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::writeCompletionWelsegsSegments( gsl::not_null<const RicMswSegment*>    outletSegment,
                                                                gsl::not_null<const RicMswCompletion*> completion,
                                                                RifTextDataTableFormatter&             formatter,
                                                                RicMswExportInfo&                      exportInfo,
                                                                double                                 maxSegmentLength,
                                                                int*                                   segmentNumber )
{
    writeWelsegsCompletionCommentHeader( formatter, completion->completionType() );

    if ( completion->completionType() == RigCompletionData::FISHBONES )
    {
        formatter.addOptionalComment(
            QString( "Sub index %1 - %2" ).arg( outletSegment->subIndex() ).arg( completion->label() ) );
    }
    else if ( completion->completionType() == RigCompletionData::FRACTURE )
    {
        formatter.addOptionalComment(
            QString( "%1 connected to segment %2" ).arg( completion->label() ).arg( outletSegment->segmentNumber() ) );
    }

    CVF_ASSERT( completion->wellPath() );

    int outletSegmentNumber = outletSegment->segmentNumber();

    for ( auto segment : completion->segments() )
    {
        double startMD = segment->startMD();
        double endMD   = segment->endMD();

        std::vector<std::pair<double, double>> splitSegments = createSubSegmentMDPairs( startMD, endMD, maxSegmentLength );

        for ( const auto& [subStartMD, subEndMD] : splitSegments )
        {
            int subSegmentNumber = ( *segmentNumber )++;

            double subStartTVD = tvdFromMeasuredDepth( completion->wellPath(), subStartMD );
            double subEndTVD   = tvdFromMeasuredDepth( completion->wellPath(), subEndMD );

            double depth  = 0;
            double length = 0;

            if ( exportInfo.lengthAndDepthText() == QString( "INC" ) )
            {
                depth  = subEndTVD - subStartTVD;
                length = subEndMD - subStartMD;
            }
            else
            {
                depth  = subEndTVD;
                length = subEndMD;
            }
            formatter.add( subSegmentNumber );
            formatter.add( subSegmentNumber );
            formatter.add( completion->branchNumber() );
            formatter.add( outletSegmentNumber );
            formatter.add( length );
            formatter.add( depth );
            formatter.add( outletSegment->effectiveDiameter() );
            formatter.add( outletSegment->openHoleRoughnessFactor() );
            formatter.rowCompleted();
            outletSegmentNumber = subSegmentNumber;
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<std::pair<double, double>>
    RicMswTableFormatterTools::createSubSegmentMDPairs( double startMD, double endMD, double maxSegmentLength )
{
    int subSegmentCount = (int)( std::trunc( ( endMD - startMD ) / maxSegmentLength ) + 1 );

    double subSegmentLength = ( endMD - startMD ) / subSegmentCount;

    std::vector<std::pair<double, double>> subSegmentMDPairs;

    double subStartMD = startMD;
    double subEndMD   = startMD + subSegmentLength;
    for ( int i = 0; i < subSegmentCount; ++i )
    {
        subSegmentMDPairs.push_back( std::make_pair( subStartMD, subEndMD ) );
        subStartMD += subSegmentLength;
        subEndMD += std::min( subSegmentLength, endMD );
    }
    return subSegmentMDPairs;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RicMswTableFormatterTools::tvdFromMeasuredDepth( gsl::not_null<const RimWellPath*> wellPath, double measuredDepth )
{
    auto wellPathGeometry = wellPath->wellPathGeometry();
    CVF_ASSERT( wellPathGeometry );

    double tvdValue = -wellPathGeometry->interpolatedPointAlongWellPath( measuredDepth ).z();

    return tvdValue;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RicMswTableFormatterTools::writeWsegvalHeader( RifTextDataTableFormatter& formatter )
{
    formatter.keyword( "WSEGVALV" );
    std::vector<RifTextDataTableColumn> header = {
        RifTextDataTableColumn( "Well Name" ),
        RifTextDataTableColumn( "Seg No" ),
        RifTextDataTableColumn( "Cv" ),
        RifTextDataTableColumn( "Ac" ),
    };
    formatter.header( header );
}