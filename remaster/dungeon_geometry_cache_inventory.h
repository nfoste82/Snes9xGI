// Test-only complete semantic map inventory (never compare struct padding).
#include <sstream>
inline std::string dungeonMapInventory(const RemasterDungeonHeightMap &m) {
 std::ostringstream o;
 auto v=[&](auto x){o<<+x<<' ';};
 auto array=[&](const auto &a){for(auto x:a)v(x);};
 array(m.producerHashes);array(m.producerFlips);array(m.producerZeroFloor);array(m.producerObstacle);
 v(m.roomIndex);array(m.offsets);for(auto &a:m.planeOffsets)array(a);
 v(m.componentCount);v(m.constraintCount);v(m.conflictingComponentCount);array(m.solvedPlaneCells);
 v(m.hasPlaneOffsets);v(m.hasRoomGeometry);array(m.fallbackDirections);
 for(auto &a:m.structuralPlaneOffsets)array(a);for(auto &a:m.mixedFloorBases)array(a);
 v(m.mixedFloorUnknown);v(m.mixedFloorConflicts);array(m.requiredPlaneCells);array(m.unknownPlaneCells);
 v(m.structuralConflicts);array(m.persistentSupportRequired);array(m.persistentSupportMissing);
 v(m.persistentSupport.size());for(auto &r:m.persistentSupport){v(r.cell);v(r.plane);v(r.component);v(r.base);}
 v(m.solvedWallCells);v(m.unknownWallCells);array(m.candidateFloorCells);array(m.unobservedFloorCells);
 v(m.disconnectedPlaneCandidates);v(m.planeRelationshipUnresolved);
 v(m.geometryConflicts.size());for(auto &r:m.geometryConflicts){v(r.node);v(r.other);v(r.expected);v(r.actual);}
 v(m.cornerBranches.size());for(auto &r:m.cornerBranches){v(r.corner);v(r.plane);v(r.branch);v(r.base);v(r.support);v(r.rise);}
 v(m.doorframeAttachments.size());for(auto &r:m.doorframeAttachments){v(r.cell);v(r.plane);v(r.base);v(r.mask);v(r.source);v(r.visible);}
 v(m.localHeightPreviews.size());for(auto &r:m.localHeightPreviews){v(r.cell);array(r.height);}
 v(m.unknownDoorframeCells);v(m.conflictingDoorframeCells);
 v(m.geometryIssuePlacements.size());for(auto &r:m.geometryIssuePlacements)array(r);
 v(m.relativeAnchors.size());for(auto &r:m.relativeAnchors){v(r.node);v(r.minimum);v(r.supportNodes);v(r.isolatedFlat);}
 v(m.geometryGroupIssues.size());for(auto &r:m.geometryGroupIssues){v(r.start);v(r.nodes);v(r.mismatches);v(r.negative);v(r.overflow);v(r.isolatedFlat);}
 v(m.authoredSupportRelations.size());for(auto &r:m.authoredSupportRelations){v(r.a);v(r.b);v(r.rise);}
 v(m.authoredWallSupportRelations.size());for(auto &r:m.authoredWallSupportRelations){v(r.wall);v(r.support);v(r.rise);v(r.word);array(r.height);}
 v(m.sharedStructuralFaces);v(m.configuredStairFallbacks);v(m.entrySupportConsumed);
 v(bool(m.roomDiagnostics));if(m.roomDiagnostics)for(auto &d:m.roomDiagnostics->cells) {
  v(d.floorId);v(d.floorBase);v(d.lowerFloorId);v(d.lowerFloorBase);v(d.lowerFloorDistance);
  v(d.room);v(d.cell);v(d.plane);v(d.background);v(d.uphillDirections);v(d.wall);
  v(d.heightConflict);v(d.axisUnknown);v(d.planeObserved);v(int(d.wallSource));v(d.wallTemplateOffset);
 }
 return o.str();
}
