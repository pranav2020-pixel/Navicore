#ifndef PATHFINDER_H
#define PATHFINDER_H

#include "Graph.h"
#include <vector>
#include <string>

struct RouteResult {
    bool success;
    std::string startId;
    std::string targetId;
    std::string algorithmUsed;
    std::vector<std::string> pathNodes;
    std::vector<std::string> pathRoads;
    double totalDistanceMeters;
    double totalTimeSeconds;
    int nodesExplored;
    long long executionMicroseconds;
    std::string statusMessage;

    RouteResult()
        : success(false), startId(""), targetId(""), algorithmUsed(""),
          totalDistanceMeters(0.0), totalTimeSeconds(0.0), nodesExplored(0),
          executionMicroseconds(0), statusMessage("") {}
};

class PathFinder {
public:
    enum class CostMetric {
        SHORTEST_DISTANCE,
        FASTEST_TIME
    };

    static RouteResult findRouteAStar(const Graph& graph,
                                      const std::string& startId,
                                      const std::string& targetId,
                                      CostMetric metric = CostMetric::SHORTEST_DISTANCE);

    static RouteResult findRouteDijkstra(const Graph& graph,
                                         const std::string& startId,
                                         const std::string& targetId,
                                         CostMetric metric = CostMetric::SHORTEST_DISTANCE);

    static double calculateEuclideanDistance(const Landmark& a, const Landmark& b);
};

#endif // PATHFINDER_H
