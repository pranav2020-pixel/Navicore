#ifndef NAVIGATIONENGINE_H
#define NAVIGATIONENGINE_H

#include "Graph.h"
#include "PathFinder.h"
#include <string>
#include <vector>

struct TurnInstruction {
    int stepNumber;
    std::string landmarkId;
    std::string landmarkName;
    std::string action;        // "Depart", "Turn Left", "Turn Right", "Continue Straight", "Arrive"
    std::string detailedText;  // "At Central Library, turn Left towards CS Dept"
    double segmentDistanceMeters;
};

class NavigationEngine {
public:
    static std::vector<TurnInstruction> generateDirections(const Graph& graph,
                                                           const RouteResult& route);

    static std::string exportRouteToJSON(const Graph& graph,
                                         const RouteResult& route,
                                         const std::vector<TurnInstruction>& directions);

    static std::string exportGraphToJSON(const Graph& graph);
};

#endif // NAVIGATIONENGINE_H
