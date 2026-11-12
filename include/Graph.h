#ifndef GRAPH_H
#define GRAPH_H

#include "Landmark.h"
#include "Edge.h"
#include <unordered_map>
#include <vector>
#include <string>

class Graph {
private:
    std::unordered_map<std::string, Landmark> landmarks;
    std::unordered_map<std::string, std::vector<Edge>> adjList;
    std::vector<Edge> allEdges;

    // Derived from the loaded data so the A* heuristic stays admissible on any map:
    // the smallest ratio of road length (metres) to straight-line coordinate distance,
    // and the highest speed limit on any road.
    double minMetresPerUnit;
    double maxSpeedKmh;

public:
    Graph();

    bool loadFromCSV(const std::string& nodesCsvPath, const std::string& edgesCsvPath);

    void addLandmark(const Landmark& lm);
    void addEdge(const Edge& edge);

    bool toggleRoadBlock(const std::string& roadId, bool blockStatus);
    bool setRoadBlockBetween(const std::string& u, const std::string& v, bool blockStatus);

    const std::unordered_map<std::string, Landmark>& getLandmarks() const;
    const std::vector<Edge>& getNeighbors(const std::string& nodeId) const;
    const std::vector<Edge>& getAllEdges() const;

    bool hasLandmark(const std::string& id) const;
    const Landmark& getLandmark(const std::string& id) const;

    int getVertexCount() const { return static_cast<int>(landmarks.size()); }
    int getEdgeCount() const { return static_cast<int>(allEdges.size()); }

    double getMinMetresPerUnit() const { return minMetresPerUnit; }
    double getMaxSpeedKmh() const { return maxSpeedKmh; }

    // Length in metres of the road `roadId` leaving `from`, or a negative value if absent.
    double roadLength(const std::string& from, const std::string& roadId) const;
};

#endif // GRAPH_H
